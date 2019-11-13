/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015-2016 Intel Corporation
 * Copyright(c) 2019 Nippon Telegraph and Telephone Corporation
 */

#include <signal.h>
#include <arpa/inet.h>
#include <inttypes.h>
#include <poll.h>
#include <fcntl.h>

#include <rte_atomic.h>
#include <rte_eth_ring.h>

#include "shared/common.h"
#include "args.h"
#include "init.h"
#include "primary.h"

#include "shared/port_manager.h"
#include "shared/secondary/add_port.h"
#include "shared/secondary/utils.h"

/*
 * Buffer sizes of status message of primary. Total number of size
 * must be equal to MSG_SIZE 2048 defined in `shared/common.h`.
 */
#define PRI_BUF_SIZE_LCORE 128
#define PRI_BUF_SIZE_PHY 512
#define PRI_BUF_SIZE_RING (MSG_SIZE - PRI_BUF_SIZE_LCORE - PRI_BUF_SIZE_PHY)

#define SPP_PATH_LEN 1024  /* seems enough for path of spp procs */
#define NOF_TOKENS 48  /* seems enough to contain tokens */
/* should be contain extra two tokens for `python` and path of launcher */
#define NOF_CMD_LIST (NOF_TOKENS + 2)

#define LAUNCH_CMD "python"
#define LCMD_LEN 8

#define LAUNCHER_NAME "sec_launcher.py"
#define LAUNCHER_LEN 16

#define POLL_TIMEOUT_MS 100

/**
 * Set of port id and type of resource UID, such as `vhost:1`. It is intended
 * to be used for mapping to ethdev ID. as port_id_list.
 */
struct port_id_map {
	int port_id;
	enum port_type type;
};

struct port_id_map port_id_list[RTE_MAX_ETHPORTS];

static sig_atomic_t on = 1;
static enum cmd_type cmd = STOP;
static struct pollfd pfd;

/* global var - extern in header */
uint8_t lcore_id_used[RTE_MAX_LCORE];

static void
turn_off(int sig)
{
	on = 0;
	RTE_LOG(INFO, PRIMARY, "terminated %d\n", sig);
}

static const char *
get_printable_mac_addr(uint16_t port)
{
	static const char err_address[] = "00:00:00:00:00:00";
	static char addresses[RTE_MAX_ETHPORTS][sizeof(err_address)];

	if (unlikely(port >= RTE_MAX_ETHPORTS))
		return err_address;

	if (unlikely(addresses[port][0] == '\0')) {
		struct rte_ether_addr mac;

		rte_eth_macaddr_get(port, &mac);
		snprintf(addresses[port], sizeof(addresses[port]),
			"%02x:%02x:%02x:%02x:%02x:%02x\n",
			mac.addr_bytes[0], mac.addr_bytes[1],
			mac.addr_bytes[2], mac.addr_bytes[3],
			mac.addr_bytes[4], mac.addr_bytes[5]);
	}

	return addresses[port];
}

/*
 * This function displays the recorded statistics for each port
 * and for each client. It uses ANSI terminal codes to clear
 * screen when called. It is called from a single non-master
 * thread in the server process, when the process is run with more
 * than one lcore enabled.
 */
static void
do_stats_display(void)
{
	const char topLeft[] = { 27, '[', '1', ';', '1', 'H', '\0' };
	const char clr[] = { 27, '[', '2', 'J', '\0' };
	unsigned int i;

	/* Clear screen and move to top left */
	printf("%s%s", clr, topLeft);

	printf("PORTS\n");
	printf("-----\n");
	for (i = 0; i < ports->num_ports; i++)
		printf("Port %u: '%s'\t", ports->id[i],
			get_printable_mac_addr(ports->id[i]));
	printf("\n\n");
	for (i = 0; i < ports->num_ports; i++) {
		printf("Port %u - rx: %9"PRIu64"\t tx: %9"PRIu64"\t"
			" tx_drop: %9"PRIu64"\n",
			ports->id[i], ports->port_stats[i].rx,
			ports->port_stats[i].tx,
			ports->client_stats[i].tx_drop);
	}

	printf("\nCLIENTS\n");
	printf("-------\n");
	for (i = 0; i < num_rings; i++) {
		printf("Client %2u - rx: %9"PRIu64", rx_drop: %9"PRIu64"\n"
			"            tx: %9"PRIu64", tx_drop: %9"PRIu64"\n",
			i, ports->client_stats[i].rx,
			ports->client_stats[i].rx_drop,
			ports->client_stats[i].tx,
			ports->client_stats[i].tx_drop);
	}

	printf("\n");
}

/*
 * The function called from each non-master lcore used by the process.
 * The test_and_set function is used to randomly pick a single lcore on which
 * the code to display the statistics will run. Otherwise, the code just
 * repeatedly sleeps.
 */
static int
sleep_lcore(void *dummy __rte_unused)
{
	/* Used to pick a display thread - static, so zero-initialised */
	static rte_atomic32_t display_stats;

	/* Only one core should display stats */
	if (rte_atomic32_test_and_set(&display_stats)) {
		const unsigned int sleeptime = 1;

		RTE_LOG(INFO, PRIMARY, "Core %u displaying statistics\n",
				rte_lcore_id());

		/* Longer initial pause so above log is seen */
		sleep(sleeptime * 3);

		/* Loop forever: sleep always returns 0 or <= param */
		while (sleep(sleeptime) <= sleeptime && on)
			do_stats_display();
	}

	return 0;
}

/* main processing loop for forwarding. */
static void
forward_loop(void)
{
	unsigned int lcore_id = rte_lcore_id();

	RTE_LOG(INFO, PRIMARY, "entering main loop on lcore %u\n", lcore_id);

	while (1) {
		if (unlikely(cmd == STOP)) {
			sleep(1);
			continue;
		} else if (cmd == FORWARD) {
			forward();
		}
	}
}

/* leading to forward loop. */
static int
main_loop(void *dummy __rte_unused)
{
	forward_loop();
	return 0;
}

/*
 * Function to set all the client statistic values to zero.
 * Called at program startup.
 */
static void
clear_stats(void)
{
	memset(ports->port_stats, 0, sizeof(struct stats) * RTE_MAX_ETHPORTS);
	memset(ports->client_stats, 0, sizeof(struct stats) * MAX_CLIENT);
}

static int
do_send(int *connected, int *sock, char *str)
{
	int ret;

	ret = send(*sock, str, MSG_SIZE, 0);
	if (ret == -1) {
		RTE_LOG(ERR, PRIMARY, "Failed to send\n");
		*connected = 0;
		return -1;
	}

	RTE_LOG(INFO, PRIMARY, "To Server: %s\n", str);

	return 0;
}

/* Get directory name of given proc_name */
static int get_sec_dir(char *proc_name, char *dir_name)
{
	if (!strcmp(proc_name, "spp_nfv")) {
		sprintf(dir_name, "%s", "nfv");
		RTE_LOG(DEBUG, PRIMARY, "Found dir 'nfv' for '%s'.\n",
				proc_name);
	} else if (!strcmp(proc_name, "spp_vf")) {
		sprintf(dir_name, "%s", "vf");
		RTE_LOG(DEBUG, PRIMARY, "Found dir 'vf' for '%s'.\n",
				proc_name);
	} else if (!strcmp(proc_name, "spp_mirror")) {
		sprintf(dir_name, "%s", "mirror");
		RTE_LOG(DEBUG, PRIMARY, "Found dir 'mirror' for '%s'.\n",
				proc_name);
	} else if (!strcmp(proc_name, "spp_pcap")) {
		sprintf(dir_name, "%s", "pcap");
		RTE_LOG(DEBUG, PRIMARY, "Found dir 'pcap' for '%s'.\n",
				proc_name);
	} else {
		RTE_LOG(DEBUG, PRIMARY, "No dir found for '%s'.\n",
				proc_name);
	}
	return 0;
}

/**
 * Launch secondary process of given name and ID.
 *
 * This function finds the path of secondary by using the path of spp_primary
 * itself and given proc name.
 *
 * Output of launched proc is sent to logfile located in `log` directory in
 * the project root, and the name of logfile is a combination of proc name
 * and ID, such as `spp_nfv-1.log`.
 */
static int
launch_sec_proc(char *sec_name, int sec_id, char **sec_args)
{
	char path_spp_pri[SPP_PATH_LEN];
	char path_spp_sec[SPP_PATH_LEN];
	char path_spp_log[SPP_PATH_LEN];

	char cmd[LCMD_LEN];  /* Command for launcher */
	char launcher[LAUNCHER_LEN];  /* Name of launcher script */
	char path_launcher[SPP_PATH_LEN];

	/* Contains elems of path_spp_pri */
	char *token_list[NOF_TOKENS] = {NULL};

	/* Contains cmd and args for execvp() */
	char *cmd_list[NOF_CMD_LIST] = {NULL};

	int num_token = 0;
	int i = 0;
	char sec_dirname[16];
	int fd;

	/* Get path of spp_primary to be used to find secondary */
	if (readlink("/proc/self/exe",
				path_spp_pri, sizeof(path_spp_pri)) == -1)
		RTE_LOG(INFO, PRIMARY,
				"Failed to find exec file of spp_primary.\n");
	else {
		/**
		 * TODO(yasufum) confirm that log dir and helper tool are
		 * existing.
		 */
		/* Tokenize path of spp_primary */
		token_list[i] = strtok(path_spp_pri, "/");
		while (token_list[i] != NULL) {
			i++;
			num_token++;
			token_list[i] = strtok(NULL, "/");
		}

		/* Get path of `src` dir used as a basename */
		for (i = 0; i < num_token - 3; i++) {
			if (i == 0)
				sprintf(path_spp_sec, "/%s/", token_list[i]);
			else
				sprintf(path_spp_sec + strlen(path_spp_sec),
						"%s/", token_list[i]);
		}

		/* Dir of logfile is located in the parent dir of src */
		sprintf(path_spp_log, "%s../log/%s-%d.log",
				path_spp_sec, sec_name, sec_id);

		/* Setup cmd list */
		memset(cmd, '\0', sizeof(cmd));
		sprintf(cmd, "%s", LAUNCH_CMD);
		cmd_list[0] = cmd;

		/* Path of launcher is `spp/tools/helpers/sec_launcher.py` */
		memset(launcher, '\0', sizeof(launcher));
		sprintf(launcher, "%s", LAUNCHER_NAME);
		memset(path_launcher, '\0', sizeof(path_launcher));
		sprintf(path_launcher, "%s../tools/helpers/%s",
				path_spp_sec, launcher);
		cmd_list[1] = path_launcher;

		/* path of sec proc */
		get_sec_dir(sec_name, sec_dirname);
		sprintf(path_spp_sec + strlen(path_spp_sec), "%s/%s/%s",
				sec_dirname, token_list[num_token-2],
				sec_name);
		cmd_list[2] = path_spp_sec;

		i = 0;
		while (sec_args[i] != NULL) {
			cmd_list[i+3] = sec_args[i];
			i++;
		}

		/* Output debug log for checking tokens of cmd_list */
		i = 0;
		while (cmd_list[i] != NULL) {
			RTE_LOG(DEBUG, PRIMARY, "launch, token[%2d] = %s\n",
					i, cmd_list[i]);
			i++;
		}

		RTE_LOG(DEBUG, PRIMARY, "sec_path: '%s'.\n", path_spp_sec);
		RTE_LOG(DEBUG, PRIMARY, "sec_log: '%s'.\n", path_spp_log);

		pid_t pid;
		pid = fork();
		if (pid < 0)
			RTE_LOG(ERR, PRIMARY,
					"Failed to open secondary proc.\n");
		else if (pid == 0) {
			/* Open log file with mode equals to 'a+' */
			fd = open(path_spp_log,
					O_RDWR | O_CREAT | O_APPEND,
					0666);

			/* change to output of stdout and stderr to logfile */
			dup2(fd, 1);
			dup2(fd, 2);
			close(fd);

			printf("%s\n", sec_args[0]);
			if (execvp(cmd_list[0], cmd_list) != 0)
				RTE_LOG(ERR, PRIMARY,
					"Failed to open child proc!\n");
		} else
			RTE_LOG(INFO, PRIMARY, "Launched '%s' with ID %d.\n",
					path_spp_sec, sec_id);
	}

	return 0;
}

/* TODO(yasufum): change to use shared */
static int
append_lcore_info_json(char *str,
		uint8_t lcore_id_used[RTE_MAX_LCORE])
{
	int i;
	sprintf(str + strlen(str), "\"master-lcore\":%d,",
			rte_get_master_lcore());
	sprintf(str + strlen(str), "\"lcores\":[");
	for (i = 0; i < RTE_MAX_LCORE; i++) {
		if (lcore_id_used[i] == 1)
			sprintf(str + strlen(str), "%d,", i);
	}

	/* Remove last ','. */
	sprintf(str + strlen(str) - 1, "%s", "]");
	return 0;
}

/* TODO(yasufum): change to use shared */
static int
append_port_info_json(char *str,
		struct port *ports_fwd_array,
		struct port_map *port_map)
{
	unsigned int i;
	unsigned int has_port = 0;  // for checking having port at last

	sprintf(str + strlen(str), "\"ports\":[");
	for (i = 0; i < RTE_MAX_ETHPORTS; i++) {

		if (ports_fwd_array[i].in_port_id == PORT_RESET)
			continue;

		has_port = 1;
		switch (port_map[i].port_type) {
		case PHY:
			sprintf(str + strlen(str), "\"phy:%u\",",
					port_map[i].id);
			break;
		case RING:
			sprintf(str + strlen(str), "\"ring:%u\",",
				port_map[i].id);
			break;
		case VHOST:
			sprintf(str + strlen(str), "\"vhost:%u\",",
				port_map[i].id);
			break;
		case PCAP:
			sprintf(str + strlen(str), "\"pcap:%u\",",
					port_map[i].id);
			break;
		case NULLPMD:
			sprintf(str + strlen(str), "\"nullpmd:%u\",",
					port_map[i].id);
			break;
		case TAP:
			sprintf(str + strlen(str), "\"tap:%u\",",
					port_map[i].id);
			break;
		case UNDEF:
			/* TODO(yasufum) Need to remove print for undefined ? */
			sprintf(str + strlen(str), "\"udf\",");
			break;
		}
	}

	/* Check if it has at least one port to remove ",". */
	if (has_port == 0) {
		sprintf(str + strlen(str), "]");
	} else {  /* Remove last ',' .*/
		sprintf(str + strlen(str) - 1, "]");
	}

	return 0;
}

/* TODO(yasufum): change to use shared */
static int
append_patch_info_json(char *str,
		struct port *ports_fwd_array,
		struct port_map *port_map)
{
	unsigned int i;
	unsigned int has_patch = 0;  // for checking having patch at last

	char patch_str[128];
	sprintf(str + strlen(str), "\"patches\":[");
	for (i = 0; i < RTE_MAX_ETHPORTS; i++) {

		if (ports_fwd_array[i].in_port_id == PORT_RESET)
			continue;

		RTE_LOG(INFO, SHARED, "Port ID %d\n", i);
		RTE_LOG(INFO, SHARED, "Status %d\n",
			ports_fwd_array[i].in_port_id);

		memset(patch_str, '\0', sizeof(patch_str));

		sprintf(patch_str, "{\"src\":");

		switch (port_map[i].port_type) {
		case PHY:
			RTE_LOG(INFO, SHARED, "Type: PHY\n");
			sprintf(patch_str + strlen(patch_str),
					"\"phy:%u\",",
					port_map[i].id);
			break;
		case RING:
			RTE_LOG(INFO, SHARED, "Type: RING\n");
			sprintf(patch_str + strlen(patch_str),
					"\"ring:%u\",",
					port_map[i].id);
			break;
		case VHOST:
			RTE_LOG(INFO, SHARED, "Type: VHOST\n");
			sprintf(patch_str + strlen(patch_str),
					"\"vhost:%u\",",
					port_map[i].id);
			break;
		case PCAP:
			RTE_LOG(INFO, SHARED, "Type: PCAP\n");
			sprintf(patch_str + strlen(patch_str),
					"\"pcap:%u\",",
					port_map[i].id);
			break;
		case NULLPMD:
			RTE_LOG(INFO, SHARED, "Type: NULLPMD\n");
			sprintf(patch_str + strlen(patch_str),
					"\"nullpmd:%u\",",
					port_map[i].id);
			break;
		case TAP:
			RTE_LOG(INFO, SHARED, "Type: TAP\n");
			sprintf(patch_str + strlen(patch_str),
					"\"tap:%u\",",
					port_map[i].id);
			break;
		case UNDEF:
			RTE_LOG(INFO, SHARED, "Type: UDF\n");
			/* TODO(yasufum) Need to remove print for undefined ? */
			sprintf(patch_str + strlen(patch_str),
					"\"udf\",");
			break;
		}

		sprintf(patch_str + strlen(patch_str), "\"dst\":");

		RTE_LOG(INFO, SHARED, "Out Port ID %d\n",
				ports_fwd_array[i].out_port_id);

		if (ports_fwd_array[i].out_port_id == PORT_RESET) {
			//sprintf(patch_str + strlen(patch_str), "%s", "\"\"");
			continue;
		} else {
			has_patch = 1;
			unsigned int j = ports_fwd_array[i].out_port_id;
			switch (port_map[j].port_type) {
			case PHY:
				RTE_LOG(INFO, SHARED, "Type: PHY\n");
				sprintf(patch_str + strlen(patch_str),
						"\"phy:%u\"",
						port_map[j].id);
				break;
			case RING:
				RTE_LOG(INFO, SHARED, "Type: RING\n");
				sprintf(patch_str + strlen(patch_str),
						"\"ring:%u\"",
						port_map[j].id);
				break;
			case VHOST:
				RTE_LOG(INFO, SHARED, "Type: VHOST\n");
				sprintf(patch_str + strlen(patch_str),
						"\"vhost:%u\"",
						port_map[j].id);
				break;
			case PCAP:
				RTE_LOG(INFO, SHARED, "Type: PCAP\n");
				sprintf(patch_str + strlen(patch_str),
						"\"pcap:%u\"",
						port_map[j].id);
				break;
			case NULLPMD:
				RTE_LOG(INFO, SHARED, "Type: NULLPMD\n");
				sprintf(patch_str + strlen(patch_str),
						"\"nullpmd:%u\"",
						port_map[j].id);
				break;
			case TAP:
				RTE_LOG(INFO, SHARED, "Type: TAP\n");
				sprintf(patch_str + strlen(patch_str),
						"\"tap:%u\"",
						port_map[j].id);
				break;
			case UNDEF:
				RTE_LOG(INFO, SHARED, "Type: UDF\n");
				/*
				 * TODO(yasufum) Need to remove print for
				 * undefined ?
				 */
				sprintf(patch_str + strlen(patch_str),
						"\"udf\"");
				break;
			}
		}

		sprintf(patch_str + strlen(patch_str), "},");

		if (has_patch != 0)
			sprintf(str + strlen(str), "%s", patch_str);
	}


	/* Check if it has at least one patch to remove ",". */
	if (has_patch == 0) {
		sprintf(str + strlen(str), "]");
	} else {  /* Remove last ','. */
		sprintf(str + strlen(str) - 1, "]");
	}

	return 0;
}

static int
forwarder_status_json(char *str)
{
	char buf_running[64];
	char buf_ports[256];
	char buf_patches[256];
	memset(buf_running, '\0', sizeof(buf_running));
	memset(buf_ports, '\0', sizeof(buf_ports));
	memset(buf_patches, '\0', sizeof(buf_patches));

	sprintf(buf_running + strlen(buf_running), "\"status\":");
	if (cmd == FORWARD)
		sprintf(buf_running + strlen(buf_running), "\"%s\"", "running");
	else
		sprintf(buf_running + strlen(buf_running), "\"%s\"", "idling");

	append_port_info_json(buf_ports, ports_fwd_array, port_map);
	append_patch_info_json(buf_patches, ports_fwd_array, port_map);

	sprintf(str, "\"forwarder\":{%s,%s,%s}", buf_running, buf_ports,
			buf_patches);
	return 0;
}

static int
phy_port_stats_json(char *str)
{
	int i;
	int buf_size = 256;  /* size of temp buffer */
	char phy_port[buf_size];
	char buf_phy_ports[PRI_BUF_SIZE_PHY];
	memset(phy_port, '\0', sizeof(phy_port));
	memset(buf_phy_ports, '\0', sizeof(buf_phy_ports));

	for (i = 0; i < ports->num_ports; i++) {

		RTE_LOG(DEBUG, PRIMARY, "Size of buf_phy_ports str: %d\n",
				(int)strlen(buf_phy_ports));

		memset(phy_port, '\0', buf_size);

		sprintf(phy_port, "{\"id\":%u,\"eth\":\"%s\","
				"\"rx\":%"PRIu64",\"tx\":%"PRIu64","
				"\"tx_drop\":%"PRIu64"}",
				ports->id[i],
				get_printable_mac_addr(ports->id[i]),
				ports->port_stats[i].rx,
				ports->port_stats[i].tx,
				ports->client_stats[i].tx_drop);

		int cur_buf_size = (int)strlen(buf_phy_ports) +
			(int)strlen(phy_port);
		if (cur_buf_size > PRI_BUF_SIZE_PHY - 1) {
			RTE_LOG(ERR, PRIMARY,
				"Cannot send all of phy_port stats (%d/%d)\n",
				i, ports->num_ports);
			sprintf(buf_phy_ports + strlen(buf_phy_ports) - 1,
					"%s", "");
			break;
		}

		sprintf(buf_phy_ports + strlen(buf_phy_ports), "%s", phy_port);

		if (i < ports->num_ports - 1)
			sprintf(buf_phy_ports, "%s,", buf_phy_ports);
	}
	sprintf(str, "\"phy_ports\":[%s]", buf_phy_ports);
	return 0;
}

static int
ring_port_stats_json(char *str)
{
	int i;
	int buf_size = 256;  /* size of temp buffer */
	char buf_ring_ports[PRI_BUF_SIZE_RING];
	char ring_port[buf_size];
	memset(ring_port, '\0', sizeof(ring_port));
	memset(buf_ring_ports, '\0', sizeof(buf_ring_ports));

	for (i = 0; i < num_rings; i++) {

		RTE_LOG(DEBUG, PRIMARY, "Size of buf_ring_ports str: %d\n",
				(int)strlen(buf_ring_ports));

		memset(ring_port, '\0', buf_size);

		sprintf(ring_port, "{\"id\":%u,\"rx\":%"PRIu64","
			"\"rx_drop\":%"PRIu64","
			"\"tx\":%"PRIu64",\"tx_drop\":%"PRIu64"}",
			i,
			ports->client_stats[i].rx,
			ports->client_stats[i].rx_drop,
			ports->client_stats[i].tx,
			ports->client_stats[i].tx_drop);

		int cur_buf_size = (int)strlen(buf_ring_ports) +
			(int)strlen(ring_port);
		if (cur_buf_size >  PRI_BUF_SIZE_RING - 1) {
			RTE_LOG(ERR, PRIMARY,
				"Cannot send all of ring_port stats (%d/%d)\n",
				i, num_rings);
			sprintf(buf_ring_ports + strlen(buf_ring_ports) - 1,
					"%s", "");
			break;
		}

		sprintf(buf_ring_ports + strlen(buf_ring_ports),
				"%s", ring_port);

		if (i < num_rings - 1)
			sprintf(buf_ring_ports, "%s,", buf_ring_ports);
	}
	sprintf(str, "\"ring_ports\":[%s]", buf_ring_ports);
	return 0;
}

/**
 * Retrieve all of statu of ports as JSON format managed by primary.
 *
 * Here is an exmaple.
 *
 * {
 *     "master-lcore": 0,
 *     "lcores": [0,1],
 *     "forwarder": {
 *         "status": "idling",
 *         "ports": ["phy:0", "phy:1"],
 *         "patches": ["src": "phy:0", "dst": "phy:1"]
 *     },
 *     "ring_ports": [
 *     {
 *         "id": 0,
 *             "rx": 0,
 *             "rx_drop": 0,
 *             "tx": 0,
 *             "tx_drop": 0
 *     },
 *     ...
 *     ],
 *     "phy_ports": [
 *     {
 *         "eth": "56:48:4f:53:54:00",
 *         "id": 0,
 *         "rx": 0,
 *         "tx": 0,
 *         "tx_drop": 0
 *     },
 *     ...
 *     ]
 * }
 */
static int
get_status_json(char *str)
{
	char buf_lcores[PRI_BUF_SIZE_LCORE];
	char buf_phy_ports[PRI_BUF_SIZE_PHY];
	char buf_ring_ports[PRI_BUF_SIZE_RING];
	memset(buf_phy_ports, '\0', PRI_BUF_SIZE_PHY);
	memset(buf_ring_ports, '\0', PRI_BUF_SIZE_RING);
	memset(buf_lcores, '\0', PRI_BUF_SIZE_LCORE);

	append_lcore_info_json(buf_lcores, lcore_id_used);
	phy_port_stats_json(buf_phy_ports);
	ring_port_stats_json(buf_ring_ports);

	RTE_LOG(INFO, PRIMARY, "%s, %s\n", buf_phy_ports, buf_ring_ports);

	if (get_forwarding_flg() == 1) {
		char tmp_buf[512];
		memset(tmp_buf, '\0', sizeof(tmp_buf));
		forwarder_status_json(tmp_buf);

		sprintf(str, "{%s,%s,%s,%s}",
				buf_lcores, tmp_buf, buf_phy_ports,
				buf_ring_ports);

	} else {
		sprintf(str, "{%s,%s,%s}",
				buf_lcores, buf_phy_ports, buf_ring_ports);
	}

	return 0;
}

/**
 * Add a port to spp_primary. Port is given as a resource UID which is a
 * combination of port type and ID like as 'ring:0'.
 */
/* TODO(yasufum) consider to merge do_add in nfv/commands.h */
static int
add_port(char *p_type, int p_id)
{
	uint16_t dev_id;
	uint16_t port_id;
	int res = 0;
	uint16_t cnt = 0;

	for (dev_id = 0; dev_id < RTE_MAX_ETHPORTS; dev_id++) {
		if (port_id_list[dev_id].port_id == PORT_RESET)
			break;
		cnt += 1;
	}

	if (!strcmp(p_type, "vhost")) {
		res = add_vhost_pmd(p_id);
		port_id_list[cnt].port_id = p_id;
		port_id_list[cnt].type = VHOST;

	} else if (!strcmp(p_type, "ring")) {
		res = add_ring_pmd(p_id);
		port_id_list[cnt].port_id = p_id;
		port_id_list[cnt].type = RING;

	} else if (!strcmp(p_type, "pcap")) {
		res = add_pcap_pmd(p_id);
		port_id_list[cnt].port_id = p_id;
		port_id_list[cnt].type = PCAP;

	} else if (!strcmp(p_type, "nullpmd")) {
		res = add_null_pmd(p_id);
		port_id_list[cnt].port_id = p_id;
		port_id_list[cnt].type = NULLPMD;
	}

	if (res < 0)
		return -1;

	port_id = (uint16_t) res;
	port_map[port_id].id = p_id;
	port_map[port_id].port_type = port_id_list[cnt].type;
	port_map[port_id].stats = &ports->client_stats[p_id];

	/* Update ports_fwd_array with port id */
	ports_fwd_array[port_id].in_port_id = port_id;
	return 0;
}

/* Find ethdev ID from resource UID to delete. */
static uint16_t
find_ethdev_id(int p_id, enum port_type ptype)
{
	int cnt;
	struct port_id_map *p_map;

	RTE_LOG(DEBUG, PRIMARY, "Finding ethdev_id, p_id: %d, ptype: %d\n",
			p_id, ptype);

	for (cnt = 0; cnt < RTE_MAX_ETHPORTS; cnt++) {
		p_map = &port_id_list[cnt];
		if (p_map == NULL)
			RTE_LOG(ERR, PRIMARY, "Failed to find port_id.\n");
		else {
			if (p_map->port_id == p_id &&
					p_map->type == ptype)
				return cnt;
		}
	}

	return PORT_RESET;
}

/* Delete port. */
/* TODO(yasufum) consider to merge do_del in nfv/commands.h */
static int
del_port(char *p_type, int p_id)
{
	uint16_t dev_id = 0;

	if (!strcmp(p_type, "vhost")) {
		dev_id = find_ethdev_id(p_id, VHOST);
		if (dev_id == PORT_RESET)
			return -1;
		dev_detach_by_port_id(dev_id);

	} else if (!strcmp(p_type, "ring")) {
		dev_id = find_ethdev_id(p_id, RING);
		if (dev_id == PORT_RESET)
			return -1;
		rte_eth_dev_stop(dev_id);
		rte_eth_dev_close(dev_id);

	} else if (!strcmp(p_type, "pcap")) {
		dev_id = find_ethdev_id(p_id, PCAP);
		if (dev_id == PORT_RESET)
			return -1;
		dev_detach_by_port_id(dev_id);

	} else if (!strcmp(p_type, "nullpmd")) {
		dev_id = find_ethdev_id(p_id, NULLPMD);
		if (dev_id == PORT_RESET)
			return -1;
		dev_detach_by_port_id(dev_id);
	}

	port_id_list[dev_id].port_id = PORT_RESET;
	port_id_list[dev_id].type = UNDEF;

	forward_array_remove(dev_id);
	port_map_init_one(dev_id);

	return 0;
}

static int
parse_command(char *str)
{
	char *token_list[MAX_PARAMETER] = {NULL};
	char sec_name[16];
	char *sec_args[NOF_TOKENS] = {NULL};
	int ret = 0;
	int max_token = 0;
	uint16_t dev_id;
	char dev_name[RTE_DEV_NAME_MAX_LEN] = { 0 };
	char result[16] = { 0 };  /* "succeeded" or "failed". */
	char port_uid[20] = { 0 };  /* "\"%s:%d\"" */
	char patch_set[64] = { 0 };  /* "{\"src\":\"%s:%d\",\"dst\":...}" */
	char *p_type;
	int p_id;

	memset(sec_name, '\0', 16);

	/* tokenize the user commands from controller */
	token_list[max_token] = strtok(str, " ");
	while (token_list[max_token] != NULL) {
		RTE_LOG(DEBUG, PRIMARY,
				"parse command, token[%2d] = %s\n",
				max_token, token_list[max_token]);
		if (max_token == 2)
			sprintf(sec_name, "%s", token_list[max_token]);
		else if (max_token > 2)
			sec_args[max_token-3] = token_list[max_token];
		max_token++;
		token_list[max_token] = strtok(NULL, " ");
	}

	if (!strcmp(token_list[0], "status")) {
		RTE_LOG(DEBUG, PRIMARY, "'status' command received.\n");

		/* Clear str and token_list nouse already */
		memset(str, '\0', MSG_SIZE);
		ret = get_status_json(str);

		/* Output all of ports under management for debugging. */
		RTE_ETH_FOREACH_DEV(dev_id) {
			rte_eth_dev_get_name_by_port(dev_id, dev_name);
			if (strlen(dev_name) > 0)
				RTE_LOG(DEBUG, PRIMARY, "Port list: %d\t%s\n",
						dev_id, dev_name);
		}

	} else if (!strcmp(token_list[0], "launch")) {
		RTE_LOG(DEBUG, PRIMARY, "'%s' command received.\n",
				token_list[0]);

		ret = launch_sec_proc(sec_name,
				strtod(token_list[1], NULL), sec_args);

		if (ret < 0) {
			RTE_LOG(ERR, PRIMARY, "Failed to launch secondary.\n");
			sprintf(result, "%s", "\"failed\"");
		} else
			sprintf(result, "%s", "\"succeeded\"");

		memset(str, '\0', MSG_SIZE);
		sprintf(str, "{%s:%s,%s:%s}",
				"\"result\"", result,
				"\"command\"", "\"launch\"");

	} else if (!strcmp(token_list[0], "stop")) {
		RTE_LOG(DEBUG, PRIMARY, "stop\n");
		cmd = STOP;
		sprintf(str, "{%s:%s,%s:%s}",
				"\"result\"", "\"succeeded\"",
				"\"command\"", "\"stop\"");

	} else if (!strcmp(token_list[0], "forward")) {
		RTE_LOG(DEBUG, PRIMARY, "forward\n");
		cmd = FORWARD;
		sprintf(str, "{%s:%s,%s:%s}",
				"\"result\"", "\"succeeded\"",
				"\"command\"", "\"forward\"");

	} else if (!strcmp(token_list[0], "add")) {
		RTE_LOG(DEBUG, PRIMARY, "'%s' command received.\n",
				token_list[0]);

		ret = parse_resource_uid(token_list[1], &p_type, &p_id);
		if (ret < 0) {
			RTE_LOG(ERR, PRIMARY, "Failed to parse RES UID.\n");
			return ret;
		}

		if (add_port(p_type, p_id) < 0) {
			RTE_LOG(ERR, PRIMARY, "Failed to add_port()\n");
			sprintf(result, "%s", "\"failed\"");
		} else
			sprintf(result, "%s", "\"succeeded\"");

		sprintf(port_uid, "\"%s:%d\"", p_type, p_id);
		memset(str, '\0', MSG_SIZE);
		sprintf(str, "{%s:%s,%s:%s,%s:%s}",
				"\"result\"", result,
				"\"command\"", "\"add\"",
				"\"port\"", port_uid);

	} else if (!strcmp(token_list[0], "del")) {
		RTE_LOG(DEBUG, PRIMARY, "Received del command\n");

		ret = parse_resource_uid(token_list[1], &p_type, &p_id);
		if (ret < 0) {
			RTE_LOG(ERR, PRIMARY, "Failed to parse RES UID.\n");
			return ret;
		}

		if (del_port(p_type, p_id) < 0) {
			RTE_LOG(ERR, PRIMARY, "Failed to del_port()\n");
			sprintf(result, "%s", "\"failed\"");
		} else
			sprintf(result, "%s", "\"succeeded\"");

		sprintf(port_uid, "\"%s:%d\"", p_type, p_id);
		memset(str, '\0', MSG_SIZE);
		sprintf(str, "{%s:%s,%s:%s,%s:%s}",
				"\"result\"", result,
				"\"command\"", "\"del\"",
				"\"port\"", port_uid);

	} else if (!strcmp(token_list[0], "patch")) {
		RTE_LOG(DEBUG, PRIMARY, "patch\n");

		if (max_token <= 1)
			return 0;

		if (strncmp(token_list[1], "reset", 5) == 0) {
			/* reset forward array*/
			forward_array_reset();
		} else {
			uint16_t in_port;
			uint16_t out_port;

			if (max_token <= 2)
				return 0;

			char *in_p_type;
			char *out_p_type;
			int in_p_id;
			int out_p_id;

			parse_resource_uid(token_list[1], &in_p_type, &in_p_id);
			in_port = find_port_id(in_p_id,
					get_port_type(in_p_type));

			parse_resource_uid(token_list[2],
					&out_p_type, &out_p_id);
			out_port = find_port_id(out_p_id,
					get_port_type(out_p_type));

			if (in_port == PORT_RESET && out_port == PORT_RESET) {
				char err_msg[128];
				memset(err_msg, '\0', sizeof(err_msg));
				sprintf(err_msg, "%s '%s:%d' and '%s:%d'",
					"Patch not found, both of",
					in_p_type, in_p_id,
					out_p_type, out_p_id);
				RTE_LOG(ERR, PRIMARY, "%s\n", err_msg);
			} else if (in_port == PORT_RESET) {
				char err_msg[128];
				memset(err_msg, '\0', sizeof(err_msg));
				sprintf(err_msg, "%s '%s:%d'",
					"Patch not found, in_port",
					in_p_type, in_p_id);
				RTE_LOG(ERR, PRIMARY, "%s\n", err_msg);
			} else if (out_port == PORT_RESET) {
				char err_msg[128];
				memset(err_msg, '\0', sizeof(err_msg));
				sprintf(err_msg, "%s '%s:%d'",
					"Patch not found, out_port",
					out_p_type, out_p_id);
				RTE_LOG(ERR, PRIMARY, "%s\n", err_msg);
			}

			if (add_patch(in_port, out_port) == 0) {
				RTE_LOG(INFO, PRIMARY,
					"Patched '%s:%d' and '%s:%d'\n",
					in_p_type, in_p_id,
					out_p_type, out_p_id);
				sprintf(result, "%s", "\"succeeded\"");
			} else {
				RTE_LOG(ERR, PRIMARY, "Failed to patch\n");
				sprintf(result, "%s", "\"failed\"");
			}

			sprintf(patch_set,
				"{\"src\":\"%s:%d\",\"dst\":\"%s:%d\"}",
				in_p_type, in_p_id, out_p_type, out_p_id);

			memset(str, '\0', MSG_SIZE);
			sprintf(str, "{%s:%s,%s:%s,%s:%s}",
					"\"result\"", result,
					"\"command\"", "\"patch\"",
					"\"ports\"", patch_set);

			ret = 0;
		}

	} else if (!strcmp(token_list[0], "exit")) {
		RTE_LOG(DEBUG, PRIMARY, "'exit' command received.\n");
		cmd = STOP;
		ret = -1;
		memset(str, '\0', MSG_SIZE);
		sprintf(str, "{%s:%s,%s:%s}",
				"\"result\"", "\"succeeded\"",
				"\"command\"", "\"exit\"");

	} else if (!strcmp(token_list[0], "clear")) {
		clear_stats();
		memset(str, '\0', MSG_SIZE);
		sprintf(str, "{%s:%s,%s:%s}",
				"\"result\"", "\"succeeded\"",
				"\"command\"", "\"clear\"");
	}

	return ret;
}

static int
do_receive(int *connected, int *sock, char *str)
{
	int ret;

	memset(str, '\0', MSG_SIZE);

	ret = poll(&pfd, 1, POLL_TIMEOUT_MS);
	if (ret <= 0) {
		if (ret < 0) {
			close(*sock);
			*sock = SOCK_RESET;
			*connected = 0;
		}
		return -1;
	}

	ret = recv(*sock, str, MSG_SIZE, 0);
	if (ret <= 0) {
		RTE_LOG(DEBUG, PRIMARY, "Receive count: %d\n", ret);

		if (ret < 0)
			RTE_LOG(ERR, PRIMARY, "Receive Fail");
		else
			RTE_LOG(INFO, PRIMARY, "Receive 0\n");

		RTE_LOG(INFO, PRIMARY, "Assume Server closed connection\n");
		close(*sock);
		*sock = SOCK_RESET;
		*connected = 0;
		return -1;
	}

	return 0;
}

static int
do_connection(int *connected, int *sock)
{
	static struct sockaddr_in servaddr;
	int ret = 0;

	if (*connected == 0) {
		if (*sock < 0) {
			RTE_LOG(INFO, PRIMARY, "Creating socket...\n");
			*sock = socket(AF_INET, SOCK_STREAM, 0);
			if (*sock < 0)
				rte_exit(EXIT_FAILURE, "socket error\n");

			/* Creation of the socket */
			memset(&servaddr, 0, sizeof(servaddr));
			servaddr.sin_family = AF_INET;
			servaddr.sin_addr.s_addr = inet_addr(server_ip);
			servaddr.sin_port = htons(server_port);

			pfd.fd = *sock;
			pfd.events = POLLIN;
		}

		RTE_LOG(INFO,
			PRIMARY, "Trying to connect ... socket %d\n", *sock);
		ret = connect(*sock, (struct sockaddr *) &servaddr,
			sizeof(servaddr));
		if (ret < 0) {
			RTE_LOG(ERR, PRIMARY, "Connection Error");
			return ret;
		}

		RTE_LOG(INFO, PRIMARY, "Connected\n");
		*connected = 1;
	}

	return ret;
}

int
main(int argc, char *argv[])
{
	int sock = SOCK_RESET;
	uint16_t dev_id;
	char dev_name[RTE_DEV_NAME_MAX_LEN] = { 0 };
	unsigned int nb_ports;
	int connected = 0;
	char str[MSG_SIZE];
	int flg_exit;  // used as res of parse_command() to exit if -1
	int ret;
	int port_type;
	unsigned int i;
	int nof_phy_port = 0;

	set_user_log_debug(1);

	/* Register signals */
	signal(SIGINT, turn_off);

	/* initialise the system */
	if (init(argc, argv) < 0)
		return -1;

	RTE_LOG(INFO, PRIMARY, "Finished Process Init.\n");

	/* clear statistics */
	clear_stats();

	memset(port_id_list, 0x00,
			sizeof(struct port_id_map) * RTE_MAX_ETHPORTS);
	for (dev_id = 0; dev_id < RTE_MAX_ETHPORTS; dev_id++) {
		ret = rte_eth_dev_get_name_by_port(dev_id, dev_name);
		if (ret > -1) {
			port_id_list[dev_id].port_id = dev_id;
			port_id_list[dev_id].type = PHY;
		} else {
			port_id_list[dev_id].port_id = PORT_RESET;
			port_id_list[dev_id].type = UNDEF;
		}
	}

	if (get_forwarding_flg() == 1) {
		/* initialize port forward array*/
		forward_array_init();
		port_map_init();

		/* Check an even number of ports to send/receive on. */
		nb_ports = rte_eth_dev_count_avail();
		if (nb_ports > RTE_MAX_ETHPORTS)
			nb_ports = RTE_MAX_ETHPORTS;

		cmd = STOP;
		/* update port_forward_array with active port. */
		for (i = 0; i < nb_ports; i++) {
			if (!rte_eth_dev_is_valid_port(i))
				continue;

			int port_id;
			rte_eth_dev_get_name_by_port(i, dev_name);
			ret = parse_dev_name(dev_name, &port_type, &port_id);
			if (ret < 0)
				RTE_LOG(ERR, PRIMARY, "Failed to parse dev_name.\n");
			if (port_type == PHY) {
				port_id = nof_phy_port;
				nof_phy_port++;
			}

			/* Update ports_fwd_array with phy port. */
			ports_fwd_array[i].in_port_id = i;
			port_map[i].port_type = port_type;
			port_map[i].id = port_id;
			port_map[i].stats = &ports->port_stats[i];

			/* TODO(yasufum) convert type of port_type to char */
			RTE_LOG(DEBUG, PRIMARY, "Add port, type: %d, id: %d\n",
					port_type, port_id);

		}

		/* do forwarding */
		rte_eal_mp_remote_launch(main_loop, NULL, SKIP_MASTER);
	} else
		/* put all other cores to sleep bar master */
		rte_eal_mp_remote_launch(sleep_lcore, NULL, SKIP_MASTER);

	while (on) {
		ret = do_connection(&connected, &sock);
		if (ret < 0) {
			usleep(CONN_RETRY_USEC);
			continue;
		}

		ret = do_receive(&connected, &sock, str);
		if (ret < 0)
			continue;

		RTE_LOG(DEBUG, PRIMARY, "Received string: %s\n", str);

		flg_exit = parse_command(str);

		/* Send the message back to client */
		ret = do_send(&connected, &sock, str);

		if (flg_exit < 0)  /* terminate process if exit is called */
			break;
		else if (ret < 0)
			continue;
	}

	/* exit */
	close(sock);
	sock = SOCK_RESET;
	RTE_LOG(INFO, PRIMARY, "spp_primary exit.\n");
	return 0;
}
