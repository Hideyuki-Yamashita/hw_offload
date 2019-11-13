/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015-2016 Intel Corporation
 * Copyright(c) 2019 Nippon Telegraph and Telephone Corporation
 */

#include <limits.h>

#include <rte_cycles.h>
#include <rte_malloc.h>
#include <rte_memzone.h>

#include "shared/common.h"
#include "args.h"
#include "init.h"
#include "primary.h"

/* array of info/queues for ring_ports */
struct ring_port *ring_ports;

/* The mbuf pool for packet rx */
static struct rte_mempool *pktmbuf_pool;

/* the port details */
struct port_info *ports;

/* global var - extern in header */
uint8_t lcore_id_used[RTE_MAX_LCORE] = {};

/**
 * Initialise the mbuf pool for packet reception for the NIC, and any other
 * buffer pools needed by the app - currently none.
 */
static int
init_mbuf_pools(void)
{
	const unsigned int num_mbufs = (num_rings * MBUFS_PER_CLIENT)
		+ (ports->num_ports * MBUFS_PER_PORT);

	/*
	 * don't pass single-producer/single-consumer flags to mbuf create as
	 * it seems faster to use a cache instead
	 */
	RTE_LOG(DEBUG, PRIMARY, "Creating mbuf pool '%s' [%u mbufs] ...\n",
		PKTMBUF_POOL_NAME, num_mbufs);

	if (rte_eal_process_type() == RTE_PROC_SECONDARY) {
		pktmbuf_pool = rte_mempool_lookup(PKTMBUF_POOL_NAME);
		if (pktmbuf_pool == NULL)
			rte_exit(EXIT_FAILURE,
				"Cannot get mempool for mbufs\n");
	} else {
		pktmbuf_pool = rte_mempool_create(PKTMBUF_POOL_NAME, num_mbufs,
			MBUF_SIZE, MBUF_CACHE_SIZE,
			sizeof(struct rte_pktmbuf_pool_private),
			rte_pktmbuf_pool_init, NULL, rte_pktmbuf_init, NULL,
			rte_socket_id(), NO_FLAGS);
	}

	return (pktmbuf_pool == NULL); /* 0  on success */
}

/**
 * Set up the DPDK rings which will be used to pass packets, via
 * pointers, between the multi-process server and client processes.
 * Each ring_port needs one RX queue.
 */
static int
init_shm_rings(void)
{
	const unsigned int ringsize = CLIENT_QUEUE_RINGSIZE;
	unsigned int socket_id;
	const char *q_name;
	unsigned int i;

	ring_ports = rte_malloc("ring_port details",
		sizeof(*ring_ports) * num_rings, 0);
	if (ring_ports == NULL)
		rte_exit(EXIT_FAILURE,
			"Cannot allocate memory for ring_port details\n");

	for (i = 0; i < num_rings; i++) {
		/* Create an RX queue for each ring_ports */
		socket_id = rte_socket_id();
		q_name = get_rx_queue_name(i);
		if (rte_eal_process_type() == RTE_PROC_SECONDARY) {
			ring_ports[i].rx_q = rte_ring_lookup(q_name);
		} else {
			ring_ports[i].rx_q = rte_ring_create(q_name,
				ringsize, socket_id,
				/* single prod, single cons */
				RING_F_SP_ENQ | RING_F_SC_DEQ);
		}
		if (ring_ports[i].rx_q == NULL)
			rte_exit(EXIT_FAILURE,
				"Cannot create rx ring queue for ring_port %u\n",
				i);
	}

	return 0;
}

/**
 * Main init function for the multi-process server app,
 * calls subfunctions to do each stage of the initialisation.
 */
int
init(int argc, char *argv[])
{
	int retval;
	int lcore_id;
	const struct rte_memzone *mz;
	uint16_t count, total_ports;
	char log_msg[1024] = { '\0' };  /* temporary log message */
	int i;

	/* init EAL, parsing EAL args */
	retval = rte_eal_init(argc, argv);
	if (retval < 0)
		return -1;

	argc -= retval;
	argv += retval;

	/* get total number of ports */
	total_ports = rte_eth_dev_count_avail();

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
				"Cannot reserve memory zone for port information\n");
		memset(mz->addr, 0, sizeof(*ports));
		ports = mz->addr;
	}

	/* Primary does forwarding without option `disp-stats` as default. */
	if (rte_lcore_count() > 1)
		set_forwarding_flg(1);
	else  /* Do not forwarding if no slave lcores. */
		set_forwarding_flg(0);

	/* Parse additional, application arguments */
	retval = parse_app_args(total_ports, argc, argv);
	if (retval != 0)
		return -1;

	/* initialise mbuf pools */
	retval = init_mbuf_pools();
	if (retval != 0)
		rte_exit(EXIT_FAILURE, "Cannot create needed mbuf pools\n");

	/* now initialise the ports we will use */
	if (rte_eal_process_type() == RTE_PROC_PRIMARY) {
		for (count = 0; count < ports->num_ports; count++) {
			retval = init_port(ports->id[count], pktmbuf_pool);
			if (retval != 0)
				rte_exit(EXIT_FAILURE,
					"Cannot initialise port %d\n", count);
		}
	}
	check_all_ports_link_status(ports, ports->num_ports, (~0x0));

	/* Initialise the ring_port. */
	init_shm_rings();

	/* Inspect lcores in use */
	RTE_LCORE_FOREACH(lcore_id) {
		lcore_id_used[lcore_id] = 1;
	}
	sprintf(log_msg, "Used lcores: ");
	for (i = 0; i < RTE_MAX_LCORE; i++) {
		if (lcore_id_used[i] == 1)
			sprintf(log_msg + strlen(log_msg), "%d ", i);
	}
	RTE_LOG(DEBUG, PRIMARY, "%s\n", log_msg);

	return 0;
}

/* Check the link status of all ports in up to 9s, and print them finally */
void
check_all_ports_link_status(struct port_info *ports, uint16_t port_num,
		uint32_t port_mask)
{
#define CHECK_INTERVAL 100 /* 100ms */
#define MAX_CHECK_TIME 90 /* 9s (90 * 100ms) in total */
	uint8_t count, all_ports_up;
	uint16_t portid;
	struct rte_eth_link link;

	RTE_LOG(INFO, PRIMARY, "Checking link status ");
	fflush(stdout);
	for (count = 0; count <= MAX_CHECK_TIME; count++) {
		all_ports_up = 1;
		for (portid = 0; portid < port_num; portid++) {
			if ((port_mask & (1 << ports->id[portid])) == 0)
				continue;

			memset(&link, 0, sizeof(link));
			rte_eth_link_get_nowait(ports->id[portid], &link);

			/* clear all_ports_up flag if any link down */
			if (link.link_status == 0) {
				all_ports_up = 0;
				break;
			}
		}

		if (all_ports_up == 0) {
			printf(".");
			fflush(stdout);
			rte_delay_ms(CHECK_INTERVAL);
		} else {
			printf("done\n");
			break;
		}
	}
	printf("\n");

	/* all ports up or timed out */
	for (portid = 0; portid < port_num; portid++) {
		if ((port_mask & (1 << ports->id[portid])) == 0)
			continue;

		memset(&link, 0, sizeof(link));
		rte_eth_link_get_nowait(ports->id[portid], &link);

		/* print link status */
		if (link.link_status)
			RTE_LOG(INFO, PRIMARY,
				"Port %d Link Up - speed %u Mbps - %s\n",
				ports->id[portid], link.link_speed,
				(link.link_duplex == ETH_LINK_FULL_DUPLEX) ?
					"full-duplex\n" : "half-duplex\n");
		else
			RTE_LOG(INFO, PRIMARY,
				"Port %d Link Down\n", ports->id[portid]);
	}
}

/**
 * Initialise an individual port:
 * - configure number of rx and tx rings
 * - set up each rx ring, to pull from the main mbuf pool
 * - set up each tx ring
 * - start the port and report its status to stdout
 */
int
init_port(uint16_t port_num, struct rte_mempool *pktmbuf_pool)
{
	/* for port configuration all features are off by default */
	const struct rte_eth_conf port_conf = {
		.rxmode = {
			.mq_mode = ETH_MQ_RX_RSS,
		},
	};
	const uint16_t rx_rings = 1, tx_rings = 1;
	const uint16_t rx_ring_size = RTE_MP_RX_DESC_DEFAULT;
	const uint16_t tx_ring_size = RTE_MP_TX_DESC_DEFAULT;
	uint16_t q;
	int retval;
	struct rte_eth_dev_info dev_info;
	struct rte_eth_conf local_port_conf = port_conf;
	struct rte_eth_txconf txq_conf;

	RTE_LOG(INFO, PRIMARY, "Port %u init ...\n", port_num);
	fflush(stdout);

	rte_eth_dev_info_get(port_num, &dev_info);
	if (dev_info.tx_offload_capa & DEV_TX_OFFLOAD_MBUF_FAST_FREE)
		local_port_conf.txmode.offloads |=
			DEV_TX_OFFLOAD_MBUF_FAST_FREE;
	txq_conf = dev_info.default_txconf;
	txq_conf.offloads = local_port_conf.txmode.offloads;

	/*
	 * Standard DPDK port initialisation - config port, then set up
	 * rx and tx rings
	 */
	retval = rte_eth_dev_configure(port_num, rx_rings, tx_rings,
		&port_conf);
	if (retval != 0)
		return retval;

	for (q = 0; q < rx_rings; q++) {
		retval = rte_eth_rx_queue_setup(port_num, q, rx_ring_size,
			rte_eth_dev_socket_id(port_num), NULL, pktmbuf_pool);
		if (retval < 0)
			return retval;
	}

	for (q = 0; q < tx_rings; q++) {
		retval = rte_eth_tx_queue_setup(port_num, q, tx_ring_size,
			rte_eth_dev_socket_id(port_num), &txq_conf);
		if (retval < 0)
			return retval;
	}

	rte_eth_promiscuous_enable(port_num);

	retval = rte_eth_dev_start(port_num);
	if (retval < 0)
		return retval;

	RTE_LOG(INFO, PRIMARY, "Port %d Init done\n", port_num);

	return 0;
}
