/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015-2016 Intel Corporation
 * Copyright(c) 2019 Nippon Telegraph and Telephone Corporation
 */

#include <arpa/inet.h>
#include <stdio.h>
#include <rte_ethdev_driver.h>
#include <rte_eth_ring.h>

#include "shared/common.h"
#include "shared/secondary/add_port.h"
#include "shared/secondary/utils.h"

char *
get_vhost_backend_name(unsigned int id)
{
	/*
	 * buffer for return value. Size calculated by %u being replaced
	 * by maximum 3 digits (plus an extra byte for safety)
	 */
	static char buffer[sizeof(VHOST_BACKEND_NAME) + 2];

	snprintf(buffer, sizeof(buffer) - 1, VHOST_BACKEND_NAME, id);
	return buffer;
}

char *
get_vhost_iface_name(unsigned int id)
{
	/*
	 * buffer for return value. Size calculated by %u being replaced
	 * by maximum 3 digits (plus an extra byte for safety)
	 */
	static char buffer[sizeof(VHOST_IFACE_NAME) + 2];

	snprintf(buffer, sizeof(buffer) - 1, VHOST_IFACE_NAME, id);
	return buffer;
}

static inline const char *
get_pcap_pmd_name(int id)
{
	static char buffer[sizeof(PCAP_PMD_DEV_NAME) + 2];
	snprintf(buffer, sizeof(buffer) - 1, PCAP_PMD_DEV_NAME, id);
	return buffer;
}

static inline const char *
get_null_pmd_name(int id)
{
	static char buffer[sizeof(NULL_PMD_DEV_NAME) + 2];
	snprintf(buffer, sizeof(buffer) - 1, NULL_PMD_DEV_NAME, id);
	return buffer;
}

/*
 * Create an empty rx pcap file to given path if it does not exit
 * Return 0 for succeeded, or -1 for failed.
 */
static int
create_pcap_rx(char *rx_fpath)
{
	int res;
	FILE *tmp_fp;
	char cmd_str[256];

	// empty file is required for 'text2pcap' command for
	// creating a pcap file.
	char template[] = "/tmp/spp-emptyfile.txt";

	// create empty file if it is not exist
	tmp_fp = fopen(template, "r");
	if (tmp_fp == NULL) {
		(tmp_fp = fopen(template, "w"));
		if (tmp_fp == NULL) {
			RTE_LOG(ERR, SHARED, "Failed to open %s\n", template);
			return -1;
		}
	}

	sprintf(cmd_str, "text2pcap %s %s", template, rx_fpath);
	res = system(cmd_str);
	if (res != 0) {
		RTE_LOG(ERR, SHARED,
				"Failed to create pcap device %s\n",
				rx_fpath);
		return -1;
	}
	RTE_LOG(INFO, SHARED, "PCAP device created\n");
	fclose(tmp_fp);
	return 0;
}

/*
 * Create a ring PMD with given ring_id.
 */
int
add_ring_pmd(int ring_id)
{
	struct rte_ring *ring;
	int res;
	char rx_queue_name[32];  /* Prefix and number like as 'eth_ring_0' */
	uint16_t port_id = PORT_RESET;
	char dev_name[RTE_ETH_NAME_MAX_LEN];

	memset(rx_queue_name, '\0', sizeof(rx_queue_name));
	sprintf(rx_queue_name, "%s", get_rx_queue_name(ring_id));

	/* Look up ring with provided ring_id */
	ring = rte_ring_lookup(rx_queue_name);
	if (ring == NULL) {
		RTE_LOG(ERR, SHARED,
			"Failed to get RX ring %s - is primary running?\n",
			rx_queue_name);
		return -1;
	}
	RTE_LOG(INFO, SHARED, "Looked up ring '%s'\n", rx_queue_name);

	/* create ring pmd*/
	snprintf(dev_name, RTE_ETH_NAME_MAX_LEN - 1, "net_ring_%s", ring->name);
	/* check whether a port already exists. */
	res = rte_eth_dev_get_port_by_name(dev_name, &port_id);
	if (port_id == PORT_RESET) {
		res = rte_eth_from_ring(ring);
		if (res < 0) {
			RTE_LOG(ERR, SHARED, "Cannot create eth dev with "
						"rte_eth_from_ring()\n");
			return -1;
		}
	} else {
		res = port_id;
		rte_eth_dev_start(res);
	}

	RTE_LOG(INFO, SHARED, "Created ring PMD: %d\n", res);

	return res;
}

int
add_vhost_pmd(int index)
{
	struct rte_eth_conf port_conf = {
		.rxmode = { .max_rx_pkt_len = RTE_ETHER_MAX_LEN }
	};
	struct rte_mempool *mp;
	uint16_t vhost_port_id;
	int nr_queues = 1;
	const char *name;
	char devargs[64];
	char *iface;
	uint16_t q;
	int ret;

	mp = rte_mempool_lookup(PKTMBUF_POOL_NAME);
	if (mp == NULL)
		rte_exit(EXIT_FAILURE, "Cannot get mempool for mbufs\n");

	/* eth_vhost0 index 0 iface /tmp/sock0 on numa 0 */
	name = get_vhost_backend_name(index);
	iface = get_vhost_iface_name(index);

	sprintf(devargs, "%s,iface=%s,queues=%d,client=%d",
			name, iface, nr_queues, get_vhost_cli_mode());
	RTE_LOG(DEBUG, SHARED, "Devargs for vhost: '%s'.\n", devargs);
	ret = dev_attach_by_devargs(devargs, &vhost_port_id);
	if (ret < 0) {
		RTE_LOG(ERR, SHARED, "Cannot attach: %s.\n", devargs);
		return ret;
	}

	ret = rte_eth_dev_configure(vhost_port_id, nr_queues, nr_queues,
		&port_conf);
	if (ret < 0) {
		RTE_LOG(ERR, SHARED, "Failed to dev configure.\n");
		return ret;
	}

	/* Allocate and set up 1 RX queue per Ethernet port. */
	for (q = 0; q < nr_queues; q++) {
		ret = rte_eth_rx_queue_setup(vhost_port_id, q, NR_DESCS,
			rte_eth_dev_socket_id(vhost_port_id), NULL, mp);
		if (ret < 0) {
			RTE_LOG(ERR, SHARED,
				"Failed to setup RX queue, "
				"port: %d, queue: %d.\n",
				vhost_port_id, q);
			return ret;
		}
	}

	/* Allocate and set up 1 TX queue per Ethernet port. */
	for (q = 0; q < nr_queues; q++) {
		ret = rte_eth_tx_queue_setup(vhost_port_id, q, NR_DESCS,
			rte_eth_dev_socket_id(vhost_port_id), NULL);
		if (ret < 0) {
			RTE_LOG(ERR, SHARED,
				"Failed to setup TX queue, "
				"port: %d, queue: %d.\n",
				vhost_port_id, q);
			return ret;
		}
	}

	/* Start the Ethernet port. */
	ret = rte_eth_dev_start(vhost_port_id);
	if (ret < 0) {
		RTE_LOG(ERR, SHARED, "Failed to start vhost.\n");
		return ret;
	}

	RTE_LOG(DEBUG, SHARED, "vhost port id %d\n", vhost_port_id);

	return vhost_port_id;
}

/*
 * Open pcap files with given index for rx and tx.
 * Index is given as a argument of 'patch' command.
 * This function returns a port ID if it is succeeded,
 * or negative int if failed.
 */
int
add_pcap_pmd(int index)
{
	struct rte_eth_conf port_conf = {
		.rxmode = { .max_rx_pkt_len = RTE_ETHER_MAX_LEN }
	};

	struct rte_mempool *mp;
	const char *name;
	char devargs[256];
	uint16_t pcap_pmd_port_id;
	uint16_t nr_queues = 1;
	int ret;

	// PCAP file path
	char rx_fpath[128];
	char tx_fpath[128];

	FILE *rx_fp;

	sprintf(rx_fpath, PCAP_IFACE_RX, index);
	sprintf(tx_fpath, PCAP_IFACE_TX, index);

	// create rx pcap file if it does not exist
	rx_fp = fopen(rx_fpath, "r");
	if (rx_fp == NULL) {
		ret = create_pcap_rx(rx_fpath);
		if (ret < 0)
			return ret;
	}

	mp = rte_mempool_lookup(PKTMBUF_POOL_NAME);
	if (mp == NULL)
		rte_exit(EXIT_FAILURE, "Cannon get mempool for mbuf\n");

	name = get_pcap_pmd_name(index);
	sprintf(devargs,
			"%s,rx_pcap=%s,tx_pcap=%s",
			name, rx_fpath, tx_fpath);
	ret = dev_attach_by_devargs(devargs, &pcap_pmd_port_id);

	if (ret < 0)
		return ret;

	ret = rte_eth_dev_configure(
			pcap_pmd_port_id, nr_queues, nr_queues, &port_conf);

	if (ret < 0)
		return ret;

	/* Allocate and set up 1 RX queue per Ethernet port. */
	uint16_t q;
	for (q = 0; q < nr_queues; q++) {
		ret = rte_eth_rx_queue_setup(
				pcap_pmd_port_id, q, NR_DESCS,
				rte_eth_dev_socket_id(pcap_pmd_port_id),
				NULL, mp);
		if (ret < 0)
			return ret;
	}

	/* Allocate and set up 1 TX queue per Ethernet port. */
	for (q = 0; q < nr_queues; q++) {
		ret = rte_eth_tx_queue_setup(
				pcap_pmd_port_id, q, NR_DESCS,
				rte_eth_dev_socket_id(pcap_pmd_port_id),
				NULL);
		if (ret < 0)
			return ret;
	}

	ret = rte_eth_dev_start(pcap_pmd_port_id);

	if (ret < 0)
		return ret;

	RTE_LOG(DEBUG, SHARED, "pcap port id %d\n", pcap_pmd_port_id);

	return pcap_pmd_port_id;
}

int
add_null_pmd(int index)
{
	struct rte_eth_conf port_conf = {
			.rxmode = { .max_rx_pkt_len = RTE_ETHER_MAX_LEN }
	};

	struct rte_mempool *mp;
	const char *name;
	char devargs[64];
	uint16_t null_pmd_port_id;
	uint16_t nr_queues = 1;

	int ret;

	mp = rte_mempool_lookup(PKTMBUF_POOL_NAME);
	if (mp == NULL)
		rte_exit(EXIT_FAILURE, "Cannon get mempool for mbuf\n");

	name = get_null_pmd_name(index);
	sprintf(devargs, "%s", name);
	ret = dev_attach_by_devargs(devargs, &null_pmd_port_id);
	if (ret < 0)
		return ret;

	ret = rte_eth_dev_configure(
			null_pmd_port_id, nr_queues, nr_queues,
			&port_conf);
	if (ret < 0)
		return ret;

	/* Allocate and set up 1 RX queue per Ethernet port. */
	uint16_t q;
	for (q = 0; q < nr_queues; q++) {
		ret = rte_eth_rx_queue_setup(
				null_pmd_port_id, q, NR_DESCS,
				rte_eth_dev_socket_id(
					null_pmd_port_id), NULL, mp);
		if (ret < 0)
			return ret;
	}

	/* Allocate and set up 1 TX queue per Ethernet port. */
	for (q = 0; q < nr_queues; q++) {
		ret = rte_eth_tx_queue_setup(
				null_pmd_port_id, q, NR_DESCS,
				rte_eth_dev_socket_id(
					null_pmd_port_id),
				NULL);
		if (ret < 0)
			return ret;
	}

	ret = rte_eth_dev_start(null_pmd_port_id);
	if (ret < 0)
		return ret;

	RTE_LOG(DEBUG, SHARED, "null port id %d\n", null_pmd_port_id);

	return null_pmd_port_id;
}
