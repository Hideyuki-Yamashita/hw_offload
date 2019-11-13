/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Nippon Telegraph and Telephone Corporation
 */

#include <rte_ether.h>
#include <rte_ip.h>
#include <rte_udp.h>
#include <rte_tcp.h>
#include <rte_net_crc.h>

#include "port_capability.h"
#include "shared/secondary/return_codes.h"

#ifdef SPP_RINGLATENCYSTATS_ENABLE
#include "latency_stats.h"
#endif

/**
 * TODO(yasufum) This `port capability` is intended to be used mainly for VLAN
 * features. However, other features, such as two sides structure of
 * management info or port direction, are also included this capability.
 * For the reason, SPP worker processes other spp_vf should include the
 * capability even if it is not using VLAN. It is a bad design because of
 * tightly coupled for dependency and it is so confusing.
 *
 * This problem should be fixed in a future update.
 */

/* Port capability management information used as a member of port_mng_info. */
struct port_capabl_mng_info {
	/* TODO(yasufum) rename ref_index and upd_index because flag. */
	/* TODO(yasufum) consider to not use two flags for (0,1) and (1,0). */
	volatile int ref_index; /* Flag to indicate using reference side. */
	volatile int upd_index; /* Flag to indicate using update side. */

	/* A set of attrs including sppwk_port_capability. */
	/* TODO(yasufum) confirm why using PORT_CAPABL_MAX. */
	struct sppwk_port_attrs port_attrs[TWO_SIDES][PORT_CAPABL_MAX];
};

/* Port ability port information */
struct port_mng_info {
	enum port_type iface_type;  /* Interface type (phy, vhost or so). */
	int iface_no;  /* Interface number. */
	struct port_capabl_mng_info rx;  /* Mng data of capability for RX. */
	struct port_capabl_mng_info tx;  /* Mng data of capability for Tx. */
};

/* Information for VLAN tag management. */
struct port_mng_info g_port_mng_info[RTE_MAX_ETHPORTS];

/* TPID of VLAN. */
static uint16_t g_vlan_tpid;

/* Initialize g_port_mng_info, and set ref side to 0 and update side to 1. */
void
sppwk_port_capability_init(void)
{
	int cnt = 0;
	g_vlan_tpid = rte_cpu_to_be_16(RTE_ETHER_TYPE_VLAN);
	memset(g_port_mng_info, 0x00, sizeof(g_port_mng_info));
	for (cnt = 0; cnt < RTE_MAX_ETHPORTS; cnt++) {
		g_port_mng_info[cnt].rx.ref_index = 0;
		g_port_mng_info[cnt].rx.upd_index = 1;
		g_port_mng_info[cnt].tx.ref_index = 0;
		g_port_mng_info[cnt].tx.upd_index = 1;
	}
}

/* Get port attributes of given ID and direction from g_port_mng_info. */
void
sppwk_get_port_attrs(struct sppwk_port_attrs **p_attrs,
		int port_id, enum sppwk_port_dir dir)
{
	struct port_capabl_mng_info *mng = NULL;

	switch (dir) {
	case SPPWK_PORT_DIR_RX:
		mng = &g_port_mng_info[port_id].rx;
		break;
	case SPPWK_PORT_DIR_TX:
		mng = &g_port_mng_info[port_id].tx;
		break;
	default:
		/* Not used. */
		break;
	}

	*p_attrs = mng->port_attrs[mng->ref_index];
}

/* Calculation and Setting of FCS. */
static inline void
set_fcs_packet(struct rte_mbuf *pkt)
{
	uint32_t *fcs = NULL;
	fcs = rte_pktmbuf_mtod_offset(pkt, uint32_t *, pkt->data_len);
	*fcs = rte_net_crc_calc(rte_pktmbuf_mtod(pkt, void *),
			pkt->data_len, RTE_NET_CRC32_ETH);
}

/* Add VLAN tag to a packet. It is called from add_vlan_tag_all(). */
static inline int
add_vlan_tag_one(
		struct rte_mbuf *pkt,
		const union sppwk_port_capability *capability)
{
	struct rte_ether_hdr *old_ether = NULL;
	struct rte_ether_hdr *new_ether = NULL;
	struct rte_vlan_hdr  *vlan      = NULL;
	const struct sppwk_vlan_tag *vlantag = &capability->vlantag;

	old_ether = rte_pktmbuf_mtod(pkt, struct rte_ether_hdr *);
	if (old_ether->ether_type == g_vlan_tpid) {
		/* For packets with VLAN tags, only VLAN ID is updated */
		new_ether = old_ether;
		vlan = (struct rte_vlan_hdr *)&new_ether[1];
	} else {
		/* For packets without VLAN tag, add VLAN tag. */
		new_ether = (struct rte_ether_hdr *)rte_pktmbuf_prepend(pkt,
				sizeof(struct rte_vlan_hdr));
		if (unlikely(new_ether == NULL)) {
			RTE_LOG(ERR, PORT, "Failed to "
					"get additional header area.\n");
			return SPPWK_RET_NG;
		}

		rte_memcpy(new_ether, old_ether, sizeof(struct rte_ether_hdr));
		vlan = (struct rte_vlan_hdr *)&new_ether[1];
		vlan->eth_proto = new_ether->ether_type;
		new_ether->ether_type = g_vlan_tpid;
	}

	vlan->vlan_tci = vlantag->tci;
	set_fcs_packet(pkt);
	return SPPWK_RET_OK;
}

/* Add VLAN tag to all packets. */
static inline int
add_vlan_tag_all(
		struct rte_mbuf **pkts, int nb_pkts,
		const union sppwk_port_capability *capability)
{
	int ret = SPPWK_RET_OK;
	int cnt = 0;
	for (cnt = 0; cnt < nb_pkts; cnt++) {
		ret = add_vlan_tag_one(pkts[cnt], capability);
		if (unlikely(ret < 0)) {
			RTE_LOG(ERR, PORT,
					"Failed to add VLAN tag."
					"(pkts %d/%d)\n", cnt, nb_pkts);
			break;
		}
	}
	return cnt;
}

/* Delete VLAN tag from a packet. It is called from del_vlan_tag_all(). */
static inline int
del_vlan_tag_one(
		struct rte_mbuf *pkt,
		const union sppwk_port_capability *cbl __attribute__ ((unused)))
{
	struct rte_ether_hdr *old_ether = NULL;
	struct rte_ether_hdr *new_ether = NULL;
	uint32_t *old, *new;

	old_ether = rte_pktmbuf_mtod(pkt, struct rte_ether_hdr *);
	if (old_ether->ether_type == g_vlan_tpid) {
		/* For packets without VLAN tag, delete VLAN tag. */
		new_ether = (struct rte_ether_hdr *)rte_pktmbuf_adj(pkt,
				sizeof(struct rte_vlan_hdr));
		if (unlikely(new_ether == NULL)) {
			RTE_LOG(ERR, PORT, "Failed to "
					"delete unnecessary header area.\n");
			return SPPWK_RET_NG;
		}

		old = (uint32_t *)old_ether;
		new = (uint32_t *)new_ether;
		new[2] = old[2];
		new[1] = old[1];
		new[0] = old[0];
		old[0] = 0;
		set_fcs_packet(pkt);
	}
	return SPPWK_RET_OK;
}

/* Delete VLAN tag from all packets. */
static inline int
del_vlan_tag_all(
		struct rte_mbuf **pkts, int nb_pkts,
		const union sppwk_port_capability *capability)
{
	int ret = SPPWK_RET_OK;
	int cnt = 0;
	for (cnt = 0; cnt < nb_pkts; cnt++) {
		ret = del_vlan_tag_one(pkts[cnt], capability);
		if (unlikely(ret < 0)) {
			RTE_LOG(ERR, PORT,
					"Failed to del VLAN tag."
					"(pkts %d/%d)\n", cnt, nb_pkts);
			break;
		}
	}
	return cnt;
}

/* Swap ref side and update side. */
/* TODO(yasufum) add desc for this function. */
void
sppwk_swap_two_sides(
		enum sppwk_swap_type swap_type,
		int port_id, enum sppwk_port_dir dir)
{
	int cnt;
	static int num_rx;
	static int num_tx;
	static int rx_list[RTE_MAX_ETHPORTS];
	static int tx_list[RTE_MAX_ETHPORTS];
	struct port_capabl_mng_info *mng = NULL;

	if (swap_type == SPPWK_SWAP_UPD) {
		switch (dir) {
		case SPPWK_PORT_DIR_RX:
			mng = &g_port_mng_info[port_id].rx;
			mng->upd_index = mng->ref_index;
			rx_list[num_rx++] = port_id;
			break;
		case SPPWK_PORT_DIR_TX:
			mng = &g_port_mng_info[port_id].tx;
			mng->upd_index = mng->ref_index;
			tx_list[num_tx++] = port_id;
			break;
		default:
			/* Not used. */
			break;
		}
		return;
	}

	for (cnt = 0; cnt < num_rx; cnt++) {
		mng = &g_port_mng_info[rx_list[cnt]].rx;
		mng->ref_index = (mng->upd_index+1) % TWO_SIDES;
		rx_list[cnt] = 0;
	}

	for (cnt = 0; cnt < num_tx; cnt++) {
		mng = &g_port_mng_info[tx_list[cnt]].tx;
		mng->ref_index = (mng->upd_index+1) % TWO_SIDES;
		tx_list[cnt] = 0;
	}
}

/* Update port attributes of given direction. */
static void
update_port_attrs(struct sppwk_port_info *port,
		enum sppwk_port_dir dir)
{
	int in_cnt, out_cnt = 0;
	int port_id = port->ethdev_port_id;
	struct port_mng_info *port_mng = &g_port_mng_info[port_id];
	struct port_capabl_mng_info *mng = NULL;
	struct sppwk_port_attrs *port_attrs_in = port->port_attrs;
	struct sppwk_port_attrs *port_attrs_out = NULL;
	struct sppwk_vlan_tag *tag = NULL;

	port_mng->iface_type = port->iface_type;
	port_mng->iface_no   = port->iface_no;

	switch (dir) {
	case SPPWK_PORT_DIR_RX:
		mng = &port_mng->rx;
		break;
	case SPPWK_PORT_DIR_TX:
		mng = &port_mng->tx;
		break;
	default:
		/* Not used. */
		break;
	}

	port_attrs_out = mng->port_attrs[mng->upd_index];
	memset(port_attrs_out, 0x00, sizeof(struct sppwk_port_attrs)
			* PORT_CAPABL_MAX);
	for (in_cnt = 0; in_cnt < PORT_CAPABL_MAX; in_cnt++) {
		if (port_attrs_in[in_cnt].dir != dir)
			continue;

		memcpy(&port_attrs_out[out_cnt], &port_attrs_in[in_cnt],
				sizeof(struct sppwk_port_attrs));

		switch (port_attrs_out[out_cnt].ops) {
		case SPPWK_PORT_OPS_ADD_VLAN:
			tag = &port_attrs_out[out_cnt].capability.vlantag;
			tag->tci = rte_cpu_to_be_16(SPP_VLANTAG_CALC_TCI(
					tag->vid, tag->pcp));
			break;
		case SPPWK_PORT_OPS_DEL_VLAN:
		default:
			/* Nothing to do. */
			break;
		}

		out_cnt++;
	}

	sppwk_swap_two_sides(SPPWK_SWAP_UPD, port_id, dir);
}

/* Update port direction of given component. */
void
sppwk_update_port_dir(const struct sppwk_comp_info *comp)
{
	int cnt;
	struct sppwk_port_info *port_info = NULL;

	for (cnt = 0; cnt < comp->nof_rx; cnt++) {
		port_info = comp->rx_ports[cnt];
		update_port_attrs(port_info, SPPWK_PORT_DIR_RX);
	}

	for (cnt = 0; cnt < comp->nof_tx; cnt++) {
		port_info = comp->tx_ports[cnt];
		update_port_attrs(port_info, SPPWK_PORT_DIR_TX);
	}
}

/**
 * Define list of VLAN opeartion functions. It is only used in
 * vlan_operation().
 */
typedef int (*vlan_f)(
		struct rte_mbuf **pkts, int nb_pkts,
		const union sppwk_port_capability *capability);

vlan_f vlan_ops[] = {
	NULL,              /* None */
	add_vlan_tag_all,  /* Add VLAN tag */
	del_vlan_tag_all,  /* Del VLAN tag */
	NULL               /* Termination */
};

/* Add or delete VLAN tag. */
static inline int
vlan_operation(uint16_t port_id, struct rte_mbuf **pkts, const uint16_t nb_pkts,
		enum sppwk_port_dir dir)
{
	int cnt, buf;
	int ok_pkts = nb_pkts;
	struct sppwk_port_attrs *port_attrs = NULL;

	sppwk_get_port_attrs(&port_attrs, port_id, dir);
	if (unlikely(port_attrs[0].ops == SPPWK_PORT_OPS_NONE))
		return nb_pkts;

	for (cnt = 0; cnt < PORT_CAPABL_MAX; cnt++) {
		/* Do nothing if the port is assigned no VLAN feature. */
		if (port_attrs[cnt].ops == SPPWK_PORT_OPS_NONE)
			break;

		/* Add or delete VLAN tag with operation function. */
		ok_pkts = vlan_ops[port_attrs[cnt].ops](
				pkts, ok_pkts, &port_attrs->capability);
	}

	/* Discard remained packets to release mbuf. */
	if (unlikely(ok_pkts < nb_pkts)) {
		for (buf = ok_pkts; buf < nb_pkts; buf++)
			rte_pktmbuf_free(pkts[buf]);
	}

	return ok_pkts;
}

/* Wrapper function for rte_eth_rx_burst() with VLAN feature. */
uint16_t
sppwk_eth_vlan_rx_burst(uint16_t port_id,
		uint16_t queue_id __attribute__ ((unused)),
		struct rte_mbuf **rx_pkts, const uint16_t nb_pkts)
{
	uint16_t nb_rx;
	nb_rx = rte_eth_rx_burst(port_id, 0, rx_pkts, nb_pkts);
	if (unlikely(nb_rx == 0))
		return SPPWK_RET_OK;

	/* Add or delete VLAN tag. */
	return vlan_operation(port_id, rx_pkts, nb_rx, SPPWK_PORT_DIR_RX);
}

/* Wrapper function for rte_eth_tx_burst() with VLAN feature. */
uint16_t
sppwk_eth_vlan_tx_burst(uint16_t port_id,
		uint16_t queue_id __attribute__ ((unused)),
		struct rte_mbuf **tx_pkts, uint16_t nb_pkts)
{
	uint16_t nb_tx;

	/* Add or delete VLAN tag. */
	nb_tx = vlan_operation(port_id, tx_pkts, nb_pkts, SPPWK_PORT_DIR_TX);

	if (unlikely(nb_tx == 0))
		return SPPWK_RET_OK;

	return rte_eth_tx_burst(port_id, 0, tx_pkts, nb_tx);
}

#ifdef SPP_RINGLATENCYSTATS_ENABLE

/* Wrapper function for rte_eth_rx_burst() with VLAN feature. */
uint16_t
sppwk_eth_vlan_ring_stats_rx_burst(uint16_t port_id,
		enum port_type iface_type, int iface_no,
		uint16_t queue_id __attribute__ ((unused)),
		struct rte_mbuf **rx_pkts, const uint16_t nb_pkts)
{
	uint16_t nb_rx;
	nb_rx = rte_eth_rx_burst(port_id, 0, rx_pkts, nb_pkts);
	if (unlikely(nb_rx == 0))
		return SPPWK_RET_OK;

	if (iface_type == RING)
		sppwk_calc_ring_latency(iface_no, rx_pkts, nb_pkts);

	/* Add or delete VLAN tag. */
	return vlan_operation(port_id, rx_pkts, nb_rx, SPPWK_PORT_DIR_RX);
}

/* Wrapper function for rte_eth_tx_burst() with VLAN feature. */
uint16_t
sppwk_eth_vlan_ring_stats_tx_burst(uint16_t port_id,
		enum port_type iface_type, int iface_no,
		uint16_t queue_id __attribute__ ((unused)),
		struct rte_mbuf **tx_pkts, uint16_t nb_pkts)
{
	uint16_t nb_tx;

	/* Add or delete VLAN tag. */
	nb_tx = vlan_operation(port_id, tx_pkts, nb_pkts, SPPWK_PORT_DIR_TX);

	if (unlikely(nb_tx == 0))
		return SPPWK_RET_OK;

	if (iface_type == RING) {
		sppwk_add_ring_latency_time(iface_no, tx_pkts, nb_pkts);
	}

	return rte_eth_tx_burst(port_id, 0, tx_pkts, nb_tx);
}

#endif /* SPP_RINGLATENCYSTATS_ENABLE */
