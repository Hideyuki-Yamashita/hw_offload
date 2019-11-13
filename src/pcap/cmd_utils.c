/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Nippon Telegraph and Telephone Corporation
 */

#include <unistd.h>
#include <string.h>

#include <rte_eth_ring.h>
#include <rte_log.h>

#include "cmd_utils.h"
#include "shared/secondary/return_codes.h"

#define RTE_LOGTYPE_PCAP_UTILS RTE_LOGTYPE_USER2

/* Manage data to addoress */
struct mng_data_info {
	struct iface_info *p_iface_info;
	struct spp_pcap_core_mng_info *p_core_info;
	int *p_capture_request;
	int *p_capture_status;
};

/* Declare global variables */
/* Logical core ID for main process */
static struct mng_data_info g_mng_data_addr;

/* generation of the ring port */
int
add_ring_pmd(int ring_id)
{
	struct rte_ring *ring;
	int ring_port_id;
	uint16_t port_id = PORT_RESET;
	char dev_name[RTE_ETH_NAME_MAX_LEN];

	/* Lookup ring of given id */
	ring = rte_ring_lookup(get_rx_queue_name(ring_id));
	if (unlikely(ring == NULL)) {
		RTE_LOG(ERR, PCAP_UTILS,
			"Cannot get RX ring - is server process running?\n");
		return SPPWK_RET_NG;
	}

	/* Create ring pmd */
	snprintf(dev_name, RTE_ETH_NAME_MAX_LEN - 1, "net_ring_%s", ring->name);
	/* check whether a port already exists. */
	ring_port_id = rte_eth_dev_get_port_by_name(dev_name, &port_id);
	if (port_id == PORT_RESET) {
		ring_port_id = rte_eth_from_ring(ring);
		if (ring_port_id < 0) {
			RTE_LOG(ERR, PCAP_UTILS, "Cannot create eth dev with "
						"rte_eth_from_ring()\n");
			return SPPWK_RET_NG;
		}
	} else {
		ring_port_id = port_id;
		rte_eth_dev_start(ring_port_id);
	}
	RTE_LOG(INFO, PCAP_UTILS, "ring port add. (no = %d / port = %d)\n",
			ring_id, ring_port_id);
	return ring_port_id;
}

/* Get status of lcore of given ID from global management info. */
enum sppwk_lcore_status
sppwk_get_lcore_status(unsigned int lcore_id)
{
	return (g_mng_data_addr.p_core_info + lcore_id)->status;
}

/**
 * Check status of all of cores is same as given
 *
 * It returns SPPWK_RET_NG as status mismatch if status is not same.
 * If core is in use, status will be checked.
 */
static int
check_core_status(enum sppwk_lcore_status status)
{
	unsigned int lcore_id = 0;
	RTE_LCORE_FOREACH_SLAVE(lcore_id) {
		if ((g_mng_data_addr.p_core_info + lcore_id)->status !=
								status) {
			/* Status is mismatched */
			return SPPWK_RET_NG;
		}
	}
	return SPPWK_RET_OK;
}

int
check_core_status_wait(enum sppwk_lcore_status status)
{
	int cnt = 0;
	for (cnt = 0; cnt < SPP_CORE_STATUS_CHECK_MAX; cnt++) {
		sleep(1);
		int ret = check_core_status(status);
		if (ret == 0)
			return SPPWK_RET_OK;
	}

	RTE_LOG(ERR, PCAP_UTILS,
			"Status check time out. (status = %d)\n", status);
	return SPPWK_RET_NG;
}

/* Set core status */
void
set_core_status(unsigned int lcore_id,
		enum sppwk_lcore_status status)
{
	(g_mng_data_addr.p_core_info + lcore_id)->status = status;
}

/* Set all core to given status */
void
set_all_core_status(enum sppwk_lcore_status status)
{
	unsigned int lcore_id = 0;
	RTE_LCORE_FOREACH_SLAVE(lcore_id) {
		(g_mng_data_addr.p_core_info + lcore_id)->status = status;
	}
}

/**
 * Set all of component status to SPP_CORE_STOP_REQUEST if received signal
 * is SIGTERM or SIGINT
 */
void
stop_process(int signal)
{
	unsigned int master_lcore;

	if (unlikely(signal != SIGTERM) &&
			unlikely(signal != SIGINT)) {
		return;
	}

	master_lcore = rte_get_master_lcore();
	(g_mng_data_addr.p_core_info + master_lcore)->status =
							SPPWK_LCORE_REQ_STOP;
	set_all_core_status(SPPWK_LCORE_REQ_STOP);
}

/**
 * Return port info of given type and num of interface
 *
 * It returns NULL value if given type is invalid.
 */
struct sppwk_port_info *
get_iface_info(enum port_type iface_type, int iface_no)
{
	struct iface_info *iface_info = g_mng_data_addr.p_iface_info;

	switch (iface_type) {
	case PHY:
		return &iface_info->phy[iface_no];
	case RING:
		return &iface_info->ring[iface_no];
	default:
		return NULL;
	}
}

/**
 * Initialize g_iface_info
 *
 * Clear g_iface_info and set initial value.
 */
static void
init_iface_info(void)
{
	int port_cnt;  /* increment ether ports */
	struct iface_info *p_iface_info = g_mng_data_addr.p_iface_info;
	memset(p_iface_info, 0x00, sizeof(struct iface_info));
	for (port_cnt = 0; port_cnt < RTE_MAX_ETHPORTS; port_cnt++) {
		p_iface_info->phy[port_cnt].iface_type = UNDEF;
		p_iface_info->phy[port_cnt].iface_no   = port_cnt;
		p_iface_info->phy[port_cnt].ethdev_port_id  = -1;
		p_iface_info->phy[port_cnt].cls_attrs.vlantag.vid =
				ETH_VLAN_ID_MAX;
		p_iface_info->ring[port_cnt].iface_type = UNDEF;
		p_iface_info->ring[port_cnt].iface_no   = port_cnt;
		p_iface_info->ring[port_cnt].ethdev_port_id  = -1;
		p_iface_info->ring[port_cnt].cls_attrs.vlantag.vid =
				ETH_VLAN_ID_MAX;
	}
}

/* Initialize g_core_info */
static void
init_core_info(void)
{
	struct spp_pcap_core_mng_info *p_core_info =
		g_mng_data_addr.p_core_info;
	memset(p_core_info, 0x00,
		sizeof(struct spp_pcap_core_mng_info)*RTE_MAX_LCORE);
	set_all_core_status(SPPWK_LCORE_STOPPED);
	*g_mng_data_addr.p_capture_request = SPP_CAPTURE_IDLE;
	*g_mng_data_addr.p_capture_status = SPP_CAPTURE_IDLE;
}

/* Initialize mng data of ports on host */
static int
init_host_port_info(void)
{
	int port_type, port_id;
	int i, ret;
	int nof_phys = 0;
	char dev_name[RTE_DEV_NAME_MAX_LEN] = { 0 };
	struct iface_info *p_iface_info = g_mng_data_addr.p_iface_info;

	for (i = 0; i < RTE_MAX_ETHPORTS; i++) {
		if (!rte_eth_dev_is_valid_port(i))
			continue;

		rte_eth_dev_get_name_by_port(i, dev_name);
		ret = parse_dev_name(dev_name, &port_type, &port_id);
		if (ret < 0)
			RTE_LOG(ERR, PCAP_UTILS, "Failed to parse dev_name.\n");

		if (port_type == PHY) {
			port_id = nof_phys;
			nof_phys++;
		}

		switch (port_type) {
		case PHY:
			p_iface_info->phy[port_id].iface_type = port_type;
			p_iface_info->phy[port_id].ethdev_port_id = port_id;
			break;
		case RING:
			p_iface_info->ring[port_id].iface_type = port_type;
			p_iface_info->ring[port_id].ethdev_port_id = port_id;
			break;
		default:
			RTE_LOG(ERR, PCAP_UTILS, "Unsupported port on host, "
				"type:%d, id:%d.\n",
				port_type, port_id);
		}
	}

	return SPPWK_RET_OK;
}

/* Setup management info for spp_pcap */
int
init_mng_data(void)
{
	/* Initialize interface and core information */
	init_iface_info();
	init_core_info();

	int ret = init_host_port_info();
	if (unlikely(ret != SPPWK_RET_OK))
		return SPPWK_RET_NG;

	return SPPWK_RET_OK;
}

/**
 * Generate a formatted string of combination from interface type and
 * number and assign to given 'port'
 */
int sppwk_port_uid(char *port, enum port_type iface_type, int iface_no)
{
	const char *iface_type_str;

	switch (iface_type) {
	case PHY:
		iface_type_str = SPPWK_PHY_STR;
		break;
	case RING:
		iface_type_str = SPPWK_RING_STR;
		break;
	default:
		return SPPWK_RET_NG;
	}

	sprintf(port, "%s:%d", iface_type_str, iface_no);

	return SPPWK_RET_OK;
}

/* Set mange data address */
int spp_set_mng_data_addr(struct iface_info *iface_p,
			  struct spp_pcap_core_mng_info *core_mng_p,
			  int *capture_request_p,
			  int *capture_status_p)
{
	if (iface_p == NULL || core_mng_p == NULL ||
			capture_request_p == NULL ||
			capture_status_p == NULL)
		return SPPWK_RET_NG;

	g_mng_data_addr.p_iface_info = iface_p;
	g_mng_data_addr.p_core_info = core_mng_p;
	g_mng_data_addr.p_capture_request = capture_request_p;
	g_mng_data_addr.p_capture_status = capture_status_p;

	return SPPWK_RET_OK;
}

/* Get manage data address */
void spp_get_mng_data_addr(struct iface_info **iface_p,
			   struct spp_pcap_core_mng_info **core_mng_p,
			   int **capture_request_p,
			   int **capture_status_p)
{

	if (iface_p != NULL)
		*iface_p = g_mng_data_addr.p_iface_info;
	if (core_mng_p != NULL)
		*core_mng_p = g_mng_data_addr.p_core_info;
	if (capture_request_p != NULL)
		*capture_request_p = g_mng_data_addr.p_capture_request;
	if (capture_status_p != NULL)
		*capture_status_p = g_mng_data_addr.p_capture_status;

}
