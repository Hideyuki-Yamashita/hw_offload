/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Nippon Telegraph and Telephone Corporation
 */

#ifndef __PORT_CAPABILITY_H__
#define __PORT_CAPABILITY_H__

/**
 * @file
 * SPP Port ability
 *
 * Provide about the ability per port.
 */

#include "cmd_utils.h"

/** Calculate TCI of VLAN tag. */
#define SPP_VLANTAG_CALC_TCI(id, pcp) (((pcp & 0x07) << 13) | (id & 0x0fff))

/** Type for swaping sides . */
enum sppwk_swap_type {
	SPPWK_SWAP_REF,  /** Swap to reference area. */
	SPPWK_SWAP_UPD,  /** Swap to update area. */
};

/**
 * Initialize global variable g_port_mng_info, and set ref side to 0 and
 * update side to 1.
 */
void sppwk_port_capability_init(void);

/**
 * Get port attributes of given ID and direction from global g_port_mng_info.
 *
 * @param[in,out] p_attrs Port attributes.
 * @param[in] port_id Etherdev ID.
 * @param[in] dir Direction of teh port of sppwk_port_dir.
 */
void sppwk_get_port_attrs(
		struct sppwk_port_attrs **p_attrs,
		int port_id, enum sppwk_port_dir dir);

/**
 * Swap ref side and update side.
 *
 * @param port_id Etherdev ID.
 * @param rxtx RX/TX ID of port_id.
 * @param type Type for changing index.
 */
void sppwk_swap_two_sides(
		enum sppwk_swap_type swap_type,
		int port_id, enum sppwk_port_dir dir);

/**
 * Update port direction of given component.
 *
 * @param comp Pointer to sppwk_comp_info.
 */
void sppwk_update_port_dir(const struct sppwk_comp_info *comp);

/**
 * Wrapper function for rte_eth_rx_burst() with VLAN feature.
 *
 * @param[in] port_id Etherdev ID.
 * @param[in] queue_id RX queue ID, but fixed value 0 in SPP.
 * @param[in] rx_pkts Pointers to mbuf should be enough to store nb_pkts.
 * @param nb_pkts Maximum number of RX packets.
 * @return Number of RX packets as number of pointers to mbuf.
 */
uint16_t sppwk_eth_vlan_rx_burst(uint16_t port_id, uint16_t queue_id,
		struct rte_mbuf **rx_pkts, const uint16_t nb_pkts);

/**
 * Wrapper function for rte_eth_tx_burst() with VLAN feature.
 *
 * @param port_id Etherdev ID.
 * @param[in] queue_id TX queue ID, but fixed value 0 in SPP.
 * @param[in] tx_pkts Pointers to mbuf should be enough to store nb_pkts.
 * @param nb_pkts Maximum number of TX packets.
 * @return Number of TX packets as number of pointers to mbuf.
 */
uint16_t sppwk_eth_vlan_tx_burst(uint16_t port_id, uint16_t queue_id,
		struct rte_mbuf **tx_pkts, uint16_t nb_pkts);

#endif /*  __PORT_CAPABILITY_H__ */
