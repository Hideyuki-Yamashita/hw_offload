/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015-2016 Intel Corporation
 * Copyright(c) 2019 Nippon Telegraph and Telephone Corporation
 */

#include <arpa/inet.h>
#include <getopt.h>

#include <rte_eth_ring.h>
#include <rte_eth_vhost.h>
#include <rte_memzone.h>
#include <rte_log.h>

#include "shared/secondary/common.h"
#include "shared/secondary/utils.h"
#include "shared/secondary/add_port.h"
#include "shared/secondary/common.h"
#include "shared/common.h"
#include "shared/basic_forwarder.h"

#include "params.h"
#include "nfv_status.h"
#include "shared/port_manager.h"
#include "commands.h"

#define RTE_LOGTYPE_SPP_NFV RTE_LOGTYPE_USER1

static sig_atomic_t on = 1;

uint8_t lcore_id_used[RTE_MAX_LCORE] = {};

/*
 * Long options mapped to a short option.
 *
 * First long only option value must be >= 256, so that we won't
 * conflict with short options.
 */
enum {
	CMD_LINE_OPT_MIN_NUM = 256,
	CMD_OPT_ENABLE_VHOST_CLI,
};

static struct option lgopts[] = {
	{"vhost-client", no_argument, NULL, CMD_OPT_ENABLE_VHOST_CLI},
	{0}
};

/*
 * print a usage message
 */
static void
usage(const char *progname)
{
	RTE_LOG(INFO, SPP_NFV,
		"Usage: %s [EAL args] -- %s %s %s\n\n",
		progname, "-n <client_id>", "-s <ipaddr:port>",
		"--vhost-client");
}

/*
 * Parse the application arguments to the client app.
 */
static int
parse_app_args(int argc, char *argv[])
{
	int option_index, opt;
	int cli_id;
	char **argvopt = argv;
	const char *progname = argv[0];
	char *ctl_ip;  /* IP address of spp_ctl. */
	int ctl_port;  /* Port num to connect spp_ctl. */
	int ret;

	/* vhost_cli is disabled as default. */
	set_vhost_cli_mode(0);

	while ((opt = getopt_long(argc, argvopt, "n:s:", lgopts,
			&option_index)) != EOF) {
		switch (opt) {
		case CMD_OPT_ENABLE_VHOST_CLI:
			set_vhost_cli_mode(1);
			break;
		case 'n':
			if (parse_client_id(&cli_id, optarg) != 0) {
				usage(progname);
				return -1;
			}
			set_client_id(cli_id);
			break;
		case 's':
			ret = parse_server(&ctl_ip, &ctl_port, optarg);
			set_spp_ctl_ip(ctl_ip);
			set_spp_ctl_port(ctl_port);
			if (ret != 0) {
				usage(progname);
				return -1;
			}
			break;
		default:
			usage(progname);
			return -1;
		}
	}

	return 0;
}

/* main processing loop */
static void
nfv_loop(void)
{
	unsigned int lcore_id = rte_lcore_id();

	RTE_LOG(INFO, SPP_NFV, "entering main loop on lcore %u\n", lcore_id);

	while (1) {
		if (unlikely(cmd == STOP)) {
			sleep(1);
			/*RTE_LOG(INFO, SPP_NFV, "Idling\n");*/
			continue;
		} else if (cmd == FORWARD) {
			forward();
		}
	}
}

/* leading to nfv processing loop */
static int
main_loop(void *dummy __rte_unused)
{
	nfv_loop();
	return 0;
}

/*
 * Application main function - loops through
 * receiving and processing packets. Never returns
 */
int
main(int argc, char *argv[])
{
	const struct rte_memzone *mz;
	int sock = SOCK_RESET;
	unsigned int lcore_id;
	unsigned int nb_ports;
	int connected = 0;
	char str[MSG_SIZE] = { 0 };
	unsigned int i;
	int flg_exit;  // used as res of parse_command() to exit if -1
	int ret;
	char dev_name[RTE_DEV_NAME_MAX_LEN] = { 0 };
	int port_type;
	int nof_phy_port = 0;
	char log_msg[1024] = { 0 };  /* temporary log message */

	ret = rte_eal_init(argc, argv);
	if (ret < 0)
		rte_exit(EXIT_FAILURE, "Invalid EAL arguments\n");

	argc -= ret;
	argv += ret;

	if (parse_app_args(argc, argv) < 0)
		rte_exit(EXIT_FAILURE, "Invalid command-line arguments\n");

	if (get_vhost_cli_mode() == 1)
		RTE_LOG(INFO, SPP_NFV, "vhost client mode is enabled.\n");

	/* initialize port forward array*/
	forward_array_init();
	port_map_init();

	/* Check that there is an even number of ports to send/receive on. */
	nb_ports = rte_eth_dev_count_avail();
	if (nb_ports > RTE_MAX_ETHPORTS)
		nb_ports = RTE_MAX_ETHPORTS;

	/* set up array for port data */
	if (rte_eal_process_type() == RTE_PROC_SECONDARY) {
		mz = rte_memzone_lookup(MZ_PORT_INFO);
		if (mz == NULL)
			rte_exit(EXIT_FAILURE,
				"Cannot get port info structure\n");
		ports = mz->addr;
	} else { /* RTE_PROC_PRIMARY */
		mz = rte_memzone_reserve(MZ_PORT_INFO, sizeof(*ports),
			rte_socket_id(), NO_FLAGS);
		if (mz == NULL)
			rte_exit(EXIT_FAILURE,
				"Cannot reserve memzone for port info\n");
		memset(mz->addr, 0, sizeof(*ports));
		ports = mz->addr;
	}

	set_user_log_debug(1);

	RTE_LOG(INFO, SPP_NFV, "Number of Ports: %d\n", nb_ports);

	cmd = STOP;

	/* update port_forward_array with active port. */
	for (i = 0; i < nb_ports; i++) {
		if (!rte_eth_dev_is_valid_port(i))
			continue;

		int port_id;
		rte_eth_dev_get_name_by_port(i, dev_name);
		ret = parse_dev_name(dev_name, &port_type, &port_id);
		if (ret < 0)
			RTE_LOG(ERR, SPP_NFV, "Failed to parse dev_name.\n");
		if (port_type == PHY) {
			port_id = nof_phy_port;
			nof_phy_port++;
		}

		/* Update ports_fwd_array with phy port. */
		ports_fwd_array[i].in_port_id = i;
		port_map[i].port_type = port_type;
		port_map[i].id = port_id;
		port_map[i].stats = &ports->port_stats[i];

		/* TODO(yasufum) convert from int of port_type to char */
		RTE_LOG(DEBUG, SPP_NFV, "Add port, type: %d, id: %d\n",
				port_type, port_id);

	}

	/* Inspect lcores in use. */
	RTE_LCORE_FOREACH(lcore_id) {
		lcore_id_used[lcore_id] = 1;
	}
	sprintf(log_msg, "Used lcores: ");
	for (i = 0; i < RTE_MAX_LCORE; i++) {
		if (lcore_id_used[i] == 1)
			sprintf(log_msg + strlen(log_msg), "%d ", i);
	}
	RTE_LOG(DEBUG, SPP_NFV, "%s\n", log_msg);

	lcore_id = 0;
	RTE_LCORE_FOREACH_SLAVE(lcore_id) {
		rte_eal_remote_launch(main_loop, NULL, lcore_id);
	}

	RTE_LOG(INFO, SPP_NFV, "My ID %d start handling message\n",
			get_client_id());
	RTE_LOG(INFO, SPP_NFV, "[Press Ctrl-C to quit ...]\n");

	/* send and receive msg loop */
	while (on) {
		ret = do_connection(&connected, &sock);
		if (ret < 0) {
			usleep(CONN_RETRY_USEC);
			continue;
		}

		ret = do_receive(&connected, &sock, str);
		if (ret < 0)
			continue;

		RTE_LOG(DEBUG, SPP_NFV, "Received string: %s\n", str);

		flg_exit = parse_command(str);

		/*Send the message back to client*/
		ret = do_send(&connected, &sock, str);

		if (flg_exit < 0)  /* terminate process if exit is called */
			break;
		else if (ret < 0)
			continue;
	}

	/* exit */
	close(sock);
	sock = SOCK_RESET;
	RTE_LOG(INFO, SPP_NFV, "spp_nfv exit.\n");
	return 0;
}
