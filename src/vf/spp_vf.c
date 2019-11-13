/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017-2019 Nippon Telegraph and Telephone Corporation
 */

#include <netinet/in.h>
#include <arpa/inet.h>
#include <getopt.h>

#include "classifier.h"
#include "forwarder.h"
#include "shared/secondary/common.h"
#include "shared/secondary/utils.h"
#include "shared/secondary/spp_worker_th/cmd_utils.h"
#include "shared/secondary/return_codes.h"
#include "shared/secondary/add_port.h"
#include "shared/secondary/spp_worker_th/cmd_runner.h"
#include "shared/secondary/spp_worker_th/cmd_parser.h"
#include "shared/secondary/spp_worker_th/port_capability.h"

#define RTE_LOGTYPE_SPP_VF RTE_LOGTYPE_USER1

#ifdef SPP_RINGLATENCYSTATS_ENABLE
#include "shared/secondary/spp_worker_th/latency_stats.h"
#endif

/* getopt_long return value for long option */
enum SPP_LONGOPT_RETVAL {
	SPP_LONGOPT_RETVAL__ = 127,

	/* Return value definition for getopt_long(). Only for long option. */
	SPP_LONGOPT_RETVAL_CLIENT_ID,    /* For `--client-id` */
	SPP_LONGOPT_RETVAL_VHOST_CLIENT  /* For `--vhost-client` */
};

/* Declare global variables */
/* Interface management information */
static struct iface_info g_iface_info;

/* Component management information */
static struct sppwk_comp_info g_component_info[RTE_MAX_LCORE];

/* Core management information */
static struct core_mng_info g_core_info[RTE_MAX_LCORE];

/* Array of update indicator for core management information */
static int g_change_core[RTE_MAX_LCORE];

/* Array of update indicator for component management information */
static int g_change_component[RTE_MAX_LCORE];

/* Backup information for cancel command */
static struct cancel_backup_info g_backup_info;

/* Print help message */
static void
usage(const char *progname)
{
	RTE_LOG(INFO, SPP_VF, "Usage: %s [EAL args] --"
			" --client-id CLIENT_ID"
			" -s SERVER_IP:SERVER_PORT"
			" [--vhost-client]\n"
			" --client-id CLIENT_ID   : My client ID\n"
			" -s SERVER_IP:SERVER_PORT  :"
			" Access information to the server\n"
			" --vhost-client            : Run vhost on client\n"
			, progname);
}

/* Parse options for client app */
static int
parse_app_args(int argc, char *argv[])
{
	int cli_id;  /* Client ID. */
	char *ctl_ip;  /* IP address of spp_ctl. */
	int ctl_port;  /* Port num to connect spp_ctl. */
	int ret;
	int cnt;
	int option_index, opt;

	int proc_flg = 0;
	int server_flg = 0;

	const int argcopt = argc;
	char *argvopt[argcopt];
	const char *progname = argv[0];

	static struct option lgopts[] = {
			{ "client-id", required_argument, NULL,
					SPP_LONGOPT_RETVAL_CLIENT_ID },
			{ "vhost-client", no_argument, NULL,
					SPP_LONGOPT_RETVAL_VHOST_CLIENT },
			{ 0 },
	};

	/**
	 * Save argv to argvopt to avoid losing the order of options
	 * by getopt_long()
	 */
	for (cnt = 0; cnt < argcopt; cnt++)
		argvopt[cnt] = argv[cnt];

	/* vhost_cli is disabled as default. */
	set_vhost_cli_mode(0);

	/* Check options of application */
	optind = 0;
	opterr = 0;
	while ((opt = getopt_long(argc, argvopt, "s:", lgopts,
			&option_index)) != EOF) {
		switch (opt) {
		case SPP_LONGOPT_RETVAL_CLIENT_ID:
			if (parse_client_id(&cli_id, optarg) != SPPWK_RET_OK) {
				usage(progname);
				return SPPWK_RET_NG;
			}
			set_client_id(cli_id);

			proc_flg = 1;
			break;
		case SPP_LONGOPT_RETVAL_VHOST_CLIENT:
			set_vhost_cli_mode(1);
			break;
		case 's':
			ret = parse_server(&ctl_ip, &ctl_port, optarg);
			set_spp_ctl_ip(ctl_ip);
			set_spp_ctl_port(ctl_port);
			if (ret != SPPWK_RET_OK) {
				usage(progname);
				return SPPWK_RET_NG;
			}
			server_flg = 1;
			break;
		default:
			usage(progname);
			return SPPWK_RET_NG;
		}
	}

	/* Check mandatory parameters */
	if ((proc_flg == 0) || (server_flg == 0)) {
		usage(progname);
		return SPPWK_RET_NG;
	}
	RTE_LOG(INFO, SPP_VF,
			"Parsed app args (client_id=%d,server=%s:%d,"
			"vhost_client=%d)\n",
			cli_id, ctl_ip, ctl_port, get_vhost_cli_mode());
	return SPPWK_RET_OK;
}

/* Main process of slave core */
static int
slave_main(void *arg __attribute__ ((unused)))
{
	int ret = 0;
	int cnt = 0;
	unsigned int lcore_id = rte_lcore_id();
	enum sppwk_lcore_status status = SPPWK_LCORE_STOPPED;
	struct core_mng_info *info = &g_core_info[lcore_id];
	struct core_info *core = get_core_info(lcore_id);

	RTE_LOG(INFO, SPP_VF, "Slave started on lcore %d.\n", lcore_id);
	set_core_status(lcore_id, SPPWK_LCORE_IDLING);

	while ((status = sppwk_get_lcore_status(lcore_id)) !=
			SPPWK_LCORE_REQ_STOP) {
		if (status != SPPWK_LCORE_RUNNING)
			continue;

		if (sppwk_is_lcore_updated(lcore_id) == 1) {
			/* Setting with the flush command trigger. */
			info->ref_index = (info->upd_index+1) % TWO_SIDES;
			core = get_core_info(lcore_id);
		}

		/* It is for processing multiple components. */
		for (cnt = 0; cnt < core->num; cnt++) {
			/* Component classification to call a function. */
			if (sppwk_get_comp_type(core->id[cnt]) ==
					SPPWK_TYPE_CLS) {
				/* Component type for classifier. */
				ret = classify_packets(core->id[cnt]);
				if (unlikely(ret != 0))
					break;
			} else {
				/* Component type for forward or merge. */
				ret = forward_packets(core->id[cnt]);
				if (unlikely(ret != 0))
					break;
			}
		}
		if (unlikely(ret != 0)) {
			RTE_LOG(ERR, SPP_VF, "Failed to forward on lcore %d. "
					"(id = %d).\n",
					lcore_id, core->id[cnt]);
			break;
		}
	}

	set_core_status(lcore_id, SPPWK_LCORE_STOPPED);
	RTE_LOG(INFO, SPP_VF, "Terminated slave on lcore %d.\n", lcore_id);
	return ret;
}

/**
 * Main function
 *
 * Return SPPWK_RET_NG explicitly if error is occurred.
 */
int
main(int argc, char *argv[])
{
	int ret;
	char ctl_ip[IPADDR_LEN] = { 0 };
	int ctl_port;
	unsigned int master_lcore;
	unsigned int lcore_id;

#ifdef SPP_DEMONIZE
	/* Daemonize process */
	ret = daemon(0, 0);
	if (unlikely(ret != 0)) {
		RTE_LOG(ERR, SPP_VF, "daemonize is failed. (ret = %d)\n",
				ret);
		return ret;
	}
#endif

	/* Signal handler registration (SIGTERM/SIGINT) */
	signal(SIGTERM, stop_process);
	signal(SIGINT,  stop_process);

	ret = rte_eal_init(argc, argv);
	if (unlikely(ret < 0))
		rte_exit(EXIT_FAILURE, "Invalid EAL arguments.\n");

	argc -= ret;
	argv += ret;

	/**
	 * It should be initialized outside of while loop, or failed to
	 * compile because it is referred when finalize `g_core_info`.
	 */
	master_lcore = rte_get_master_lcore();

	/**
	 * If any failure is occured in the while block, break to go the end
	 * of the block to finalize.
	 */
	while (1) {
		/* Parse spp_vf specific parameters */
		ret = parse_app_args(argc, argv);
		if (unlikely(ret != SPPWK_RET_OK))
			break;

		if (sppwk_set_mng_data(&g_iface_info, g_component_info,
					g_core_info, g_change_core,
					g_change_component,
					&g_backup_info) < SPPWK_RET_OK) {
			RTE_LOG(ERR, SPP_VF,
				"Failed to set management data.\n");
			break;
		}

		ret = init_mng_data();
		if (unlikely(ret != SPPWK_RET_OK))
			break;

		ret = init_cls_mng_info();
		if (unlikely(ret != SPPWK_RET_OK))
			break;

		init_forwarder();
		sppwk_port_capability_init();

		/* Setup connection for accepting commands from controller */
		get_spp_ctl_ip(ctl_ip);
		ctl_port = get_spp_ctl_port();
		ret = sppwk_cmd_runner_conn(ctl_ip, ctl_port);
		if (unlikely(ret != SPPWK_RET_OK))
			break;

#ifdef SPP_RINGLATENCYSTATS_ENABLE
		int port_type, port_id;
		char dev_name[RTE_DEV_NAME_MAX_LEN] = { 0 };
		int nof_rings = 0;
		for (int i = 0; i < RTE_MAX_ETHPORTS; i++) {
			if (!rte_eth_dev_is_valid_port(i))
				continue;
			rte_eth_dev_get_name_by_port(i, dev_name);
			ret = parse_dev_name(dev_name, &port_type, &port_id);
			if (port_type == RING)
				nof_rings++;
		}
		ret = sppwk_init_ring_latency_stats(
				SPP_RING_LATENCY_STATS_SAMPLING_INTERVAL,
				nof_rings);
		if (unlikely(ret != SPPWK_RET_OK))
			break;
#endif /* SPP_RINGLATENCYSTATS_ENABLE */

		/* Start worker threads of classifier and forwarder */
		RTE_LCORE_FOREACH_SLAVE(lcore_id) {
			rte_eal_remote_launch(slave_main, NULL, lcore_id);
		}

		/* Set the status of main thread to idle */
		g_core_info[master_lcore].status = SPPWK_LCORE_IDLING;
		ret = check_core_status_wait(SPPWK_LCORE_IDLING);
		if (unlikely(ret != SPPWK_RET_OK))
			break;

		/* Start forwarding */
		set_all_core_status(SPPWK_LCORE_RUNNING);
		RTE_LOG(INFO, SPP_VF, "My ID %d start handling message\n", 0);
		RTE_LOG(INFO, SPP_VF, "[Press Ctrl-C to quit ...]\n");

		/* Backup the management information after initialization */
		backup_mng_info(&g_backup_info);

		/* Enter loop for accepting commands */
		while (likely(g_core_info[master_lcore].status !=
					SPPWK_LCORE_REQ_STOP))
		{
			/* Receive command */
			ret = sppwk_run_cmd();
			if (unlikely(ret != SPPWK_RET_OK))
				break;

		       /*
			* Wait to avoid CPU overloaded.
			*/
			usleep(100);

#ifdef SPP_RINGLATENCYSTATS_ENABLE
			print_ring_latency_stats(&g_iface_info);
#endif /* SPP_RINGLATENCYSTATS_ENABLE */
		}

		if (unlikely(ret != SPPWK_RET_OK)) {
			set_all_core_status(SPPWK_LCORE_REQ_STOP);
			break;
		}

		ret = SPPWK_RET_OK;
		break;
	}

	/* Finalize to exit */
	g_core_info[master_lcore].status = SPPWK_LCORE_STOPPED;
	ret = check_core_status_wait(SPPWK_LCORE_STOPPED);
	if (unlikely(ret != SPPWK_RET_OK))
		RTE_LOG(ERR, SPP_VF, "Failed to terminate master thread.\n");

	/*
	 * Remove vhost sock file if it is not running
	 *  in vhost-client mode
	 */
	del_vhost_sockfile(g_iface_info.vhost);

#ifdef SPP_RINGLATENCYSTATS_ENABLE
	sppwk_clean_ring_latency_stats();
#endif /* SPP_RINGLATENCYSTATS_ENABLE */

	RTE_LOG(INFO, SPP_VF, "Exit spp_vf.\n");
	return ret;
}
