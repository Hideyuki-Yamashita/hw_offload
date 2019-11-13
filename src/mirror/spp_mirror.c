/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Nippon Telegraph and Telephone Corporation
 */

#include <netinet/in.h>
#include <arpa/inet.h>
#include <getopt.h>

#include <rte_common.h>
#include <rte_cycles.h>

#include "spp_mirror.h"
#include "shared/secondary/common.h"
#include "shared/secondary/add_port.h"
#include "shared/secondary/return_codes.h"
#include "shared/secondary/utils.h"
#include "shared/secondary/spp_worker_th/mirror_deps.h"
#include "shared/secondary/spp_worker_th/cmd_runner.h"
#include "shared/secondary/spp_worker_th/cmd_parser.h"
#include "shared/secondary/spp_worker_th/cmd_utils.h"
#include "shared/secondary/spp_worker_th/port_capability.h"

#ifdef SPP_RINGLATENCYSTATS_ENABLE
#include "shared/secondary/spp_worker_th/latency_stats.h"
#endif

/* Declare global variables */
#define RTE_LOGTYPE_MIRROR RTE_LOGTYPE_USER1

#define SPP_MIRROR_POOL_NAME "spp_mirror_pool"
#define SPP_MIRROR_POOL_NAME_MAX 32
#define MAX_PKT_MIRROR 4096
#define MEMPOOL_CACHE_SIZE 256
#define MIR_RX_DESC_DEFAULT 1024
#define MIR_TX_DESC_DEFAULT 1024

/* getopt_long return value for long option */
enum SPP_LONGOPT_RETVAL {
	SPP_LONGOPT_RETVAL__ = 127,

	/* Return value definition for getopt_long(). Only for long option. */
	SPP_LONGOPT_RETVAL_CLIENT_ID,    /* For `--client-id` */
	SPP_LONGOPT_RETVAL_VHOST_CLIENT  /* For `--vhost-client` */
};

/* A set of port info of rx and tx */
struct mirror_rxtx {
	struct sppwk_port_info rx; /* rx port */
	struct sppwk_port_info tx; /* tx port */
};

/* Information on the path used for mirror. */
struct mirror_path {
	char name[STR_LEN_NAME];  /* component name */
	volatile enum sppwk_worker_type wk_type;
	int nof_rx;  /* number of receive ports */
	int nof_tx;  /* number of mirror ports */
	struct mirror_rxtx ports[RTE_MAX_ETHPORTS];  /* used for mirror */
};

/* Information for mirror. */
struct mirror_info {
	volatile int ref_index; /* index to reference area */
	volatile int upd_index; /* index to update area    */
	struct mirror_path path[TWO_SIDES];
				/* Information of data path */
};

static uint16_t nb_rxd = MIR_RX_DESC_DEFAULT;
static uint16_t nb_txd = MIR_TX_DESC_DEFAULT;

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

/* mirror info */
static struct mirror_info g_mirror_info[RTE_MAX_LCORE];

/* mirror mbuf pool */
static struct rte_mempool *g_mirror_pool;

/* Print help message */
static void
usage(const char *progname)
{
	RTE_LOG(INFO, MIRROR, "Usage: %s [EAL args] --"
			" --client-id CLIENT_ID"
			" -s SERVER_IP:SERVER_PORT"
			" [--vhost-client]\n"
			" --client-id CLIENT_ID   : My client ID\n"
			" -s SERVER_IP:SERVER_PORT  : "
				"Access information to the server\n"
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
			if (ret != SPPWK_RET_OK) {
				usage(progname);
				return SPPWK_RET_NG;
			}
			set_spp_ctl_ip(ctl_ip);
			set_spp_ctl_port(ctl_port);
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
	RTE_LOG(INFO, MIRROR,
			"Parsed app args (client_id=%d, server=%s:%d, "
			"vhost_client=%d)\n",
			cli_id, ctl_ip, ctl_port, get_vhost_cli_mode());
	return SPPWK_RET_OK;
}

/* mirror mbuf pool create */
static int
mirror_pool_create(int id)
{
	unsigned int nb_mbufs;
	char pool_name[SPP_MIRROR_POOL_NAME_MAX];

	nb_mbufs = RTE_MAX(
	    (uint16_t)(nb_rxd + nb_txd + MAX_PKT_BURST + MEMPOOL_CACHE_SIZE),
									8192U);
	sprintf(pool_name, "%s_%d", SPP_MIRROR_POOL_NAME, id);
	g_mirror_pool = rte_mempool_lookup(pool_name);
	if (g_mirror_pool == NULL) {
#ifdef SPP_MIRROR_SHALLOWCOPY
		g_mirror_pool = rte_pktmbuf_pool_create(pool_name,
						nb_mbufs,
						MEMPOOL_CACHE_SIZE,
						0,
						RTE_PKTMBUF_HEADROOM,
						rte_socket_id());
#else
		g_mirror_pool = rte_pktmbuf_pool_create(pool_name,
						nb_mbufs,
						MEMPOOL_CACHE_SIZE,
						0,
						RTE_MBUF_DEFAULT_BUF_SIZE,
						rte_socket_id());
#endif /* SPP_MIRROR_SHALLOWCOPY */
	}
	if (g_mirror_pool == NULL) {
		RTE_LOG(ERR, MIRROR, "Cannot init mbuf pool\n");
		return SPPWK_RET_NG;
	}

	return SPPWK_RET_OK;
}

/* Clear info */
static void
mirror_proc_init(void)
{
	int cnt = 0;
	memset(&g_mirror_info, 0x00, sizeof(g_mirror_info));
	for (cnt = 0; cnt < RTE_MAX_LCORE; cnt++) {
		g_mirror_info[cnt].ref_index = 0;
		g_mirror_info[cnt].upd_index = 1;
	}
}

/* Update mirror info */
int
update_mirror(struct sppwk_comp_info *wk_comp)
{
	int cnt = 0;
	int nof_rx = wk_comp->nof_rx;
	int nof_tx = wk_comp->nof_tx;
	struct mirror_info *info = &g_mirror_info[wk_comp->comp_id];
	struct mirror_path *path = &info->path[info->upd_index];

	/* Check mirror has just one RX and two TX port. */
	if (unlikely(nof_rx > 1)) {
		RTE_LOG(ERR, MIRROR,
			"Invalid num of RX (id=%d, type=%d, nof_rx=%d)\n",
			wk_comp->comp_id, wk_comp->wk_type, nof_rx);
		return SPPWK_RET_NG;
	}
	if (unlikely(nof_tx > 2)) {
		RTE_LOG(ERR, MIRROR,
			"Invalid num of TX (id=%d, type=%d, nof_tx=%d)\n",
			wk_comp->comp_id, wk_comp->wk_type, nof_tx);
		return SPPWK_RET_NG;
	}

	memset(path, 0x00, sizeof(struct mirror_path));

	RTE_LOG(INFO, MIRROR,
			"Start updating mirror (id=%d, name=%s, type=%d)\n",
			wk_comp->comp_id, wk_comp->name, wk_comp->wk_type);

	memcpy(&path->name, wk_comp->name, STR_LEN_NAME);
	path->wk_type = wk_comp->wk_type;
	path->nof_rx = wk_comp->nof_rx;
	path->nof_tx = wk_comp->nof_tx;
	for (cnt = 0; cnt < nof_rx; cnt++)
		memcpy(&path->ports[cnt].rx, wk_comp->rx_ports[cnt],
				sizeof(struct sppwk_port_info));

	/* Transmit port is set according with larger nof_rx / nof_tx. */
	for (cnt = 0; cnt < nof_tx; cnt++)
		memcpy(&path->ports[cnt].tx, wk_comp->tx_ports[cnt],
				sizeof(struct sppwk_port_info));

	info->upd_index = info->ref_index;
	while (likely(info->ref_index == info->upd_index))
		rte_delay_us_block(SPPWK_UPDATE_INTERVAL);

	RTE_LOG(INFO, MIRROR,
			"Done update mirror (id=%d, name=%s, type=%d)\n",
			wk_comp->comp_id, wk_comp->name, wk_comp->wk_type);

	return SPPWK_RET_OK;
}

/* Change index of mirror info */
static inline void
change_mirror_index(int id)
{
	struct mirror_info *info = &g_mirror_info[id];
	if (info->ref_index == info->upd_index) {
	/* Change reference index of port ability. */
		sppwk_swap_two_sides(SPPWK_SWAP_REF, 0, 0);
		info->ref_index = (info->upd_index+1) % TWO_SIDES;
	}
}

/**
 * Mirroring packets as mirror_proc
 *
 * Behavior of forwarding is defined as core_info->type which is given
 * as an argument of void and typecasted to spp_config_info.
 */
static int
mirror_proc(int id)
{
	int cnt, buf;
	int nb_rx = 0;
	int nb_tx = 0;
	int nb_tx1 = 0;
	int nb_tx2 = 0;
	struct mirror_info *info = &g_mirror_info[id];
	struct mirror_path *path = NULL;
	struct sppwk_port_info *rx = NULL;
	struct sppwk_port_info *tx = NULL;
	struct rte_mbuf *bufs[MAX_PKT_BURST];
	struct rte_mbuf *copybufs[MAX_PKT_BURST];
	struct rte_mbuf *org_mbuf = NULL;

	change_mirror_index(id);
	path = &info->path[info->ref_index];

	/* Practice condition check */
	if (!(path->nof_tx == 2 && path->nof_rx == 1))
		return SPPWK_RET_OK;

	rx = &path->ports[0].rx;

#ifdef SPP_RINGLATENCYSTATS_ENABLE
	nb_rx = sppwk_eth_ring_stats_rx_burst(rx->ethdev_port_id,
			rx->iface_type, rx->iface_no, 0, bufs, MAX_PKT_BURST);
#else
	nb_rx = rte_eth_rx_burst(rx->ethdev_port_id, 0, bufs, MAX_PKT_BURST);
#endif

	if (unlikely(nb_rx == 0))
		return SPPWK_RET_OK;

	/* mirror */
	tx = &path->ports[1].tx;
	if (tx->ethdev_port_id >= 0) {
		nb_tx2 = 0;
		for (cnt = 0; cnt < nb_rx; cnt++) {
			org_mbuf = bufs[cnt];
			rte_prefetch0(rte_pktmbuf_mtod(org_mbuf, void *));
#ifdef SPP_MIRROR_SHALLOWCOPY
			/* Shallow Copy */
			copybufs[cnt] = rte_pktmbuf_clone(org_mbuf,
							g_mirror_pool);
#else
			struct rte_mbuf *mirror_mbuf = NULL;
			struct rte_mbuf **mirror_mbufs = &mirror_mbuf;
			struct rte_mbuf *copy_mbuf = NULL;
			/* Deep Copy */
			do {
				copy_mbuf = rte_pktmbuf_alloc(g_mirror_pool);
				if (unlikely(copy_mbuf == NULL)) {
					rte_pktmbuf_free(mirror_mbuf);
					mirror_mbuf = NULL;
					RTE_LOG(INFO, MIRROR,
						"copy mbuf alloc NG!\n");
					break;
				}

				copy_mbuf->data_off = org_mbuf->data_off;
				copy_mbuf->data_len = org_mbuf->data_len;
				copy_mbuf->port = org_mbuf->port;
				copy_mbuf->vlan_tci = org_mbuf->vlan_tci;
				copy_mbuf->tx_offload = org_mbuf->tx_offload;
				copy_mbuf->hash = org_mbuf->hash;

				copy_mbuf->next = NULL;
				copy_mbuf->pkt_len = org_mbuf->pkt_len;
				copy_mbuf->nb_segs = org_mbuf->nb_segs;
				copy_mbuf->ol_flags = org_mbuf->ol_flags;
				copy_mbuf->packet_type = org_mbuf->packet_type;

				rte_memcpy(rte_pktmbuf_mtod(copy_mbuf, char *),
					rte_pktmbuf_mtod(org_mbuf, char *),
					org_mbuf->data_len);

				*mirror_mbufs = copy_mbuf;
				mirror_mbufs = &copy_mbuf->next;
			} while ((org_mbuf = org_mbuf->next) != NULL);
			copybufs[cnt] = mirror_mbuf;

#endif /* SPP_MIRROR_SHALLOWCOPY */
		}

		if (cnt != 0)
#ifdef SPP_RINGLATENCYSTATS_ENABLE
			nb_tx2 = sppwk_eth_ring_stats_tx_burst(
					tx->ethdev_port_id, tx->iface_type,
					tx->iface_no, 0, copybufs, cnt);
#else
			nb_tx2 = rte_eth_tx_burst(tx->ethdev_port_id, 0,
					copybufs, cnt);
#endif
	}

	/* orginal */
	tx = &path->ports[0].tx;
	if (tx->ethdev_port_id >= 0)
#ifdef SPP_RINGLATENCYSTATS_ENABLE
		nb_tx1 = sppwk_eth_ring_stats_tx_burst(tx->ethdev_port_id,
				tx->iface_type, tx->iface_no, 0, bufs, nb_rx);
#else
		nb_tx1 = rte_eth_tx_burst(tx->ethdev_port_id, 0, bufs, nb_rx);
#endif
	nb_tx = nb_tx1;

	if (nb_tx1 != nb_tx2)
		RTE_LOG(INFO, MIRROR,
			"mirror paket drop nb_rx=%d nb_tx1=%d nb_tx2=%d\n",
							nb_rx, nb_tx1, nb_tx2);

	/* Discard remained packets to release mbuf */
	if (nb_tx1 < nb_tx2)
		nb_tx = nb_tx2;
	if (unlikely(nb_tx < nb_rx)) {
		for (buf = nb_tx; buf < nb_rx; buf++)
			rte_pktmbuf_free(bufs[buf]);
	}
	if (unlikely(nb_tx2 < nb_rx)) {
		for (buf = nb_tx2; buf < nb_rx; buf++)
			rte_pktmbuf_free(copybufs[buf]);
	}
	return SPPWK_RET_OK;
}

/* Main process of slave core */
static int
slave_main(void *arg __attribute__ ((unused)))
{
	int ret = SPPWK_RET_OK;
	int cnt = 0;
	unsigned int lcore_id = rte_lcore_id();
	enum sppwk_lcore_status status = SPPWK_LCORE_STOPPED;
	struct core_mng_info *info = &g_core_info[lcore_id];
	struct core_info *core = get_core_info(lcore_id);

	RTE_LOG(INFO, MIRROR, "Slave started on lcore %d.\n", lcore_id);
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

		for (cnt = 0; cnt < core->num; cnt++) {
			/*
			 * mirror returns at once.
			 * It is for processing multiple components.
			 */
			ret = mirror_proc(core->id[cnt]);
			if (unlikely(ret != 0))
				break;
		}
		if (unlikely(ret != 0)) {
			RTE_LOG(ERR, MIRROR,
				"Failed to forward on lcore %d (id = %d)\n",
					lcore_id, core->id[cnt]);
			break;
		}
	}

	set_core_status(lcore_id, SPPWK_LCORE_STOPPED);
	RTE_LOG(INFO, MIRROR, "Terminated slave on lcore %d.\n", lcore_id);
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
	int ret_cmd_init;
	unsigned int master_lcore;
	unsigned int lcore_id;

#ifdef SPP_DEMONIZE
	/* Daemonize process */
	int ret_daemon = daemon(0, 0);
	if (unlikely(ret_daemon != 0)) {
		RTE_LOG(ERR, MIRROR, "daemonize is failed. (ret = %d)\n",
				ret_daemon);
		return ret_daemon;
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
		/* Parse spp_mirror specific parameters */
		int ret_parse = parse_app_args(argc, argv);
		if (unlikely(ret_parse != 0))
			break;

		if (sppwk_set_mng_data(&g_iface_info, g_component_info,
					g_core_info, g_change_core,
					g_change_component,
					&g_backup_info) < 0) {
			RTE_LOG(ERR, MIRROR,
				"Failed to set management data.\n");
			break;
		}

		/* create the mbuf pool */
		ret = mirror_pool_create(get_client_id());
		if (ret == SPPWK_RET_NG) {
			RTE_LOG(ERR, MIRROR,
					"Failed to create mbuf pool.\n");
			return SPPWK_RET_NG;
		}

		int ret_mng = init_mng_data();
		if (unlikely(ret_mng != 0))
			break;

		mirror_proc_init();
		sppwk_port_capability_init();

		/* Setup connection for accepting commands from controller */
		get_spp_ctl_ip(ctl_ip);
		ctl_port = get_spp_ctl_port();
		ret_cmd_init = sppwk_cmd_runner_conn(ctl_ip, ctl_port);
		if (unlikely(ret_cmd_init != SPPWK_RET_OK))
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
		int ret_ringlatency = sppwk_init_ring_latency_stats(
				SPP_RING_LATENCY_STATS_SAMPLING_INTERVAL,
				nof_rings);
		if (unlikely(ret_ringlatency != SPPWK_RET_OK))
			break;
#endif /* SPP_RINGLATENCYSTATS_ENABLE */

		/* Start worker threads of classifier and forwarder */
		lcore_id = 0;
		RTE_LCORE_FOREACH_SLAVE(lcore_id) {
			rte_eal_remote_launch(slave_main, NULL, lcore_id);
		}

		/* Set the status of main thread to idle */
		g_core_info[master_lcore].status = SPPWK_LCORE_IDLING;
		int ret_wait = check_core_status_wait(SPPWK_LCORE_IDLING);
		if (unlikely(ret_wait != 0))
			break;

		/* Start forwarding */
		set_all_core_status(SPPWK_LCORE_RUNNING);
#ifdef SPP_MIRROR_SHALLOWCOPY
		RTE_LOG(INFO, MIRROR,
			"My ID %d start handling messagei(ShallowCopy)\n", 0);
#else
		RTE_LOG(INFO, MIRROR,
			"My ID %d start handling message(DeepCopy)\n", 0);
#endif /* #ifdef SPP_MIRROR_SHALLOWCOPY */
		RTE_LOG(INFO, MIRROR, "[Press Ctrl-C to quit ...]\n");

		/* Backup the management information after initialization */
		backup_mng_info(&g_backup_info);

		/* Enter loop for accepting commands */
		int ret_do = 0;
		while (likely(g_core_info[master_lcore].status !=
					SPPWK_LCORE_REQ_STOP))
		{
			/* Receive command */
			ret_do = sppwk_run_cmd();
			if (unlikely(ret_do != SPPWK_RET_OK))
				break;
			/*
			 * To avoid making CPU busy, this thread waits
			 * here for 100 ms.
			 */
			usleep(100);

#ifdef SPP_RINGLATENCYSTATS_ENABLE
			print_ring_latency_stats(&g_iface_info);
#endif /* SPP_RINGLATENCYSTATS_ENABLE */
		}

		if (unlikely(ret_do != SPPWK_RET_OK)) {
			set_all_core_status(SPPWK_LCORE_REQ_STOP);
			break;
		}

		ret = SPPWK_RET_OK;
		break;
	}

	/* Finalize to exit */
	g_core_info[master_lcore].status = SPPWK_LCORE_STOPPED;
	int ret_core_end = check_core_status_wait(SPPWK_LCORE_STOPPED);
	if (unlikely(ret_core_end != 0))
		RTE_LOG(ERR, MIRROR, "Failed to terminate master thread.\n");

	 /* Remove vhost sock file if not running in vhost-client mode. */
	del_vhost_sockfile(g_iface_info.vhost);

#ifdef SPP_RINGLATENCYSTATS_ENABLE
	sppwk_clean_ring_latency_stats();
#endif /* SPP_RINGLATENCYSTATS_ENABLE */

	RTE_LOG(INFO, MIRROR, "Exit spp_mirror.\n");
	return ret;
}

/* Mirror get component status */
int
get_mirror_status(unsigned int lcore_id, int id,
		struct sppwk_lcore_params *params)
{
	int ret = SPPWK_RET_NG;
	int cnt;
	const char *component_type = NULL;
	struct mirror_info *info = &g_mirror_info[id];
	struct mirror_path *path = &info->path[info->ref_index];
	struct sppwk_port_idx rx_ports[RTE_MAX_ETHPORTS];
	struct sppwk_port_idx tx_ports[RTE_MAX_ETHPORTS];

	if (unlikely(path->wk_type == SPPWK_TYPE_NONE)) {
		RTE_LOG(ERR, MIRROR,
			"Mirror is not used. (id=%d, lcore=%d, type=%d)\n",
			id, lcore_id, path->wk_type);
		return SPPWK_RET_NG;
	}

	component_type = SPPWK_TYPE_MIR_STR;

	memset(rx_ports, 0x00, sizeof(rx_ports));
	for (cnt = 0; cnt < path->nof_rx; cnt++) {
		rx_ports[cnt].iface_type = path->ports[cnt].rx.iface_type;
		rx_ports[cnt].iface_no   = path->ports[cnt].rx.iface_no;
	}

	memset(tx_ports, 0x00, sizeof(tx_ports));
	for (cnt = 0; cnt < path->nof_tx; cnt++) {
		tx_ports[cnt].iface_type = path->ports[cnt].tx.iface_type;
		tx_ports[cnt].iface_no   = path->ports[cnt].tx.iface_no;
	}

	/* Set the information with the function specified by the command. */
	ret = (*params->lcore_proc)(params, lcore_id, path->name,
			component_type, path->nof_rx, rx_ports, path->nof_tx,
			tx_ports);
	if (unlikely(ret != 0))
		return SPPWK_RET_NG;

	return SPPWK_RET_OK;
}
