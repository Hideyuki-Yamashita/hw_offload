/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017-2018 Nippon Telegraph and Telephone Corporation
 */

#include <rte_cycles.h>

#include "forwarder.h"
#include "shared/secondary/return_codes.h"
#include "shared/secondary/spp_worker_th/vf_deps.h"
#include "shared/secondary/spp_worker_th/port_capability.h"

#ifdef SPP_RINGLATENCYSTATS_ENABLE
#include "shared/secondary/spp_worker_th/latency_stats.h"
#endif

#define RTE_LOGTYPE_FORWARD RTE_LOGTYPE_USER1

/* A set of port info of rx and tx */
struct forward_rxtx {
	struct sppwk_port_info rx; /* rx port */
	struct sppwk_port_info tx; /* tx port */
};

/* Information on the path used for forward. */
struct forward_path {
	char name[STR_LEN_NAME];  /* Component name */
	volatile enum sppwk_worker_type wk_type;
	int nof_rx;  /* Number of RX ports */
	int nof_tx;  /* Number of TX ports */
	struct forward_rxtx ports[RTE_MAX_ETHPORTS];  /* Set of RX and TX */
};

/* Information for forward. */
struct forward_info {
	volatile int ref_index; /* index to reference area */
	volatile int upd_index; /* index to update area    */
	struct forward_path path[TWO_SIDES];
				/* Information of data path */
};

struct forward_info g_forward_info[RTE_MAX_LCORE];

/* Clear g_forward_info, ref and update indices. */
void
init_forwarder(void)
{
	int cnt = 0;
	memset(&g_forward_info, 0x00, sizeof(g_forward_info));
	for (cnt = 0; cnt < RTE_MAX_LCORE; cnt++) {
		g_forward_info[cnt].ref_index = 0;
		g_forward_info[cnt].upd_index = 1;
	}
}

/* Get forwarder status. */
int
get_forwarder_status(unsigned int lcore_id, int id,
		struct sppwk_lcore_params *params)
{
	int ret = SPPWK_RET_NG;
	int cnt;
	const char *component_type = NULL;
	struct forward_info *fwd_info = &g_forward_info[id];
	struct forward_path *fwd_path = &fwd_info->path[fwd_info->ref_index];
	struct sppwk_port_idx rx_ports[RTE_MAX_ETHPORTS];
	struct sppwk_port_idx tx_ports[RTE_MAX_ETHPORTS];

	if (unlikely(fwd_path->wk_type == SPPWK_TYPE_NONE)) {
		RTE_LOG(ERR, FORWARD,
				"Forwarder is not used. "
				"(id=%d, lcore=%d, type=%d).\n",
				id, lcore_id, fwd_path->wk_type);
		return SPPWK_RET_NG;
	}

	if (fwd_path->wk_type == SPPWK_TYPE_MRG)
		component_type = SPPWK_TYPE_MRG_STR;
	else
		component_type = SPPWK_TYPE_FWD_STR;

	memset(rx_ports, 0x00, sizeof(rx_ports));
	for (cnt = 0; cnt < fwd_path->nof_rx; cnt++) {
		rx_ports[cnt].iface_type = fwd_path->ports[cnt].rx.iface_type;
		rx_ports[cnt].iface_no = fwd_path->ports[cnt].rx.iface_no;
	}

	memset(tx_ports, 0x00, sizeof(tx_ports));
	for (cnt = 0; cnt < fwd_path->nof_tx; cnt++) {
		tx_ports[cnt].iface_type = fwd_path->ports[cnt].tx.iface_type;
		tx_ports[cnt].iface_no = fwd_path->ports[cnt].tx.iface_no;
	}

	/* Set the information with the function specified by the command. */
	ret = (*params->lcore_proc)(params, lcore_id, fwd_path->name,
			component_type, fwd_path->nof_rx, rx_ports,
			fwd_path->nof_tx, tx_ports);
	if (unlikely(ret != SPPWK_RET_OK))
		return SPPWK_RET_NG;

	return SPPWK_RET_OK;
}

/* Update forward info */
int
update_forwarder(struct sppwk_comp_info *comp_info)
{
	int cnt = 0;
	int nof_rx = comp_info->nof_rx;
	int nof_tx = comp_info->nof_tx;
	int max = (nof_rx > nof_tx)?nof_rx*nof_tx:nof_tx;
	struct forward_info *fwd_info = &g_forward_info[comp_info->comp_id];
	/* TODO(yasufum) rename `path` of struct forward_path. */
	struct forward_path *fwd_path = &fwd_info->path[fwd_info->upd_index];

	/**
	 * Check num of RX and TX ports because forwarder has just a set of
	 * RX and TX.
	 */
	if ((comp_info->wk_type == SPPWK_TYPE_FWD) &&
			unlikely(nof_rx > 1)) {
		RTE_LOG(ERR, FORWARD,
			"Invalid forwarder type or num of RX ports "
			"(id=%d, type=%d, nof_rx=%d).\n",
			comp_info->comp_id, comp_info->wk_type, nof_rx);
		return SPPWK_RET_NG;
	}
	if (unlikely(nof_tx != 0) && unlikely(nof_tx != 1)) {
		RTE_LOG(ERR, FORWARD,
			"Invalid forwarder type or num of TX ports "
			"(id=%d, type=%d, nof_tx=%d).\n",
			comp_info->comp_id, comp_info->wk_type, nof_tx);
		return SPPWK_RET_NG;
	}

	memset(fwd_path, 0x00, sizeof(struct forward_path));

	RTE_LOG(INFO, FORWARD,
			"Start updating forwarder (id=%d, name=%s, type=%d)\n",
			comp_info->comp_id, comp_info->name,
			comp_info->wk_type);

	memcpy(&fwd_path->name, comp_info->name, STR_LEN_NAME);
	fwd_path->wk_type = comp_info->wk_type;
	fwd_path->nof_rx = comp_info->nof_rx;
	fwd_path->nof_tx = comp_info->nof_tx;
	for (cnt = 0; cnt < nof_rx; cnt++)
		memcpy(&fwd_path->ports[cnt].rx, comp_info->rx_ports[cnt],
				sizeof(struct sppwk_port_info));

	/* TX port is set according with larger nof_rx / nof_tx. */
	for (cnt = 0; cnt < max; cnt++)
		memcpy(&fwd_path->ports[cnt].tx, comp_info->tx_ports[0],
				sizeof(struct sppwk_port_info));

	fwd_info->upd_index = fwd_info->ref_index;
	while (likely(fwd_info->ref_index == fwd_info->upd_index))
		rte_delay_us_block(SPPWK_UPDATE_INTERVAL);

	RTE_LOG(INFO, FORWARD,
			"Done update forwarder. (id=%d, name=%s, type=%d)\n",
			comp_info->comp_id, comp_info->name,
			comp_info->wk_type);

	return SPPWK_RET_OK;
}

/* Change index of forward info */
static inline void
change_forward_index(int id)
{
	struct forward_info *info = &g_forward_info[id];
	if (info->ref_index == info->upd_index) {
		/* Change reference index of port ability. */
		sppwk_swap_two_sides(SPPWK_SWAP_REF, 0, 0);

		info->ref_index = (info->upd_index+1) % TWO_SIDES;
	}
}
/**
 * Forward packets as forwarder or merger.
 *
 * Behavior of forwarding is defined as core_info->type which is given
 * as an argument of void and typecasted to spp_config_info.
 */
int
forward_packets(int id)
{
	int cnt, buf;
	int nb_rx = 0;
	int nb_tx = 0;
	struct forward_info *info = &g_forward_info[id];
	struct forward_path *path = NULL;
	struct sppwk_port_info *rx;
	struct sppwk_port_info *tx;
	struct rte_mbuf *bufs[MAX_PKT_BURST];

	change_forward_index(id);
	path = &info->path[info->ref_index];

	/* Practice condition check */
	if (path->wk_type == SPPWK_TYPE_MRG) {
		/* merger */
		if (!(path->nof_tx == 1 && path->nof_rx >= 1))
			return SPPWK_RET_OK;
	} else {
		/* forwarder */
		if (!(path->nof_tx == 1 && path->nof_rx == 1))
			return SPPWK_RET_OK;
	}

	for (cnt = 0; cnt < path->nof_rx; cnt++) {
		rx = &path->ports[cnt].rx;
		tx = &path->ports[cnt].tx;

#ifdef SPP_RINGLATENCYSTATS_ENABLE
		nb_rx = sppwk_eth_vlan_ring_stats_rx_burst(rx->ethdev_port_id,
				rx->iface_type, rx->iface_no, 0,
				bufs, MAX_PKT_BURST);
#else
		nb_rx = sppwk_eth_vlan_rx_burst(rx->ethdev_port_id, 0,
				bufs, MAX_PKT_BURST);
#endif
		if (unlikely(nb_rx == 0))
			continue;

		/* Send packets */
		if (tx->ethdev_port_id >= 0)
#ifdef SPP_RINGLATENCYSTATS_ENABLE
			nb_tx = sppwk_eth_vlan_ring_stats_tx_burst(
					tx->ethdev_port_id, tx->iface_type,
					tx->iface_no, 0, bufs, nb_rx);
#else
			nb_tx = sppwk_eth_vlan_tx_burst(tx->ethdev_port_id,
					0, bufs, nb_rx);
#endif

		/* Discard remained packets to release mbuf */
		if (unlikely(nb_tx < nb_rx)) {
			for (buf = nb_tx; buf < nb_rx; buf++)
				rte_pktmbuf_free(bufs[buf]);
		}
	}
	return SPPWK_RET_OK;
}
