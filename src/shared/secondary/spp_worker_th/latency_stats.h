/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017-2018 Nippon Telegraph and Telephone Corporation
 */

#ifndef _RINGLATENCYSTATS_H_
#define _RINGLATENCYSTATS_H_

/**
 * @file
 * SPP RING latency statistics
 *
 * Util functions for measuring latency of ring-PMD.
 */

#include <rte_mbuf.h>
#include "cmd_utils.h"

/**
 * Statistics of latency of ring is counted with histgram like data structure.
 * To count frequency of each of time in nano sec, this data is implemented as
 * an array in which frequency counts of 1-100[ns] are contained. If the
 * latency is larger than 100[ns], it is added to the last entry. It means
 * this array has 101 entries, 100 entries for 1-100[ns] and 1 entry for over
 * 100[ns].
 */
#define TOTAL_LATENCY_ENT 101

/** statistics of latency of ring */
struct ring_latency_stats_t {
	uint64_t distr[TOTAL_LATENCY_ENT]; /* distribution of time */
};


#ifdef SPP_RINGLATENCYSTATS_ENABLE
/**
 * initialize ring latency statistics.
 *
 * @param samp_intvl
 *  The interval timer(ns) to refer the counter.
 * @param stats_count
 *  The number of ring to be measured.
 *
 * @retval SPPWK_RET_OK: succeeded.
 * @retval SPPWK_RET_NG: failed.
 */
int sppwk_init_ring_latency_stats(uint64_t samp_intvl, uint16_t stats_count);

void sppwk_clean_ring_latency_stats(void);

/**
 * add time-stamp to mbuf's member.
 *
 * @note call at enqueue.
 *
 * @param ring_id Ring id.
 * @param pkts Pointer to nb_pkts rte_mbuf containing packets.
 * @param nb_pkts Maximum number of packets to be measured.
 */
void sppwk_add_ring_latency_time(int ring_id,
		struct rte_mbuf **pkts, uint16_t nb_pkts);

/**
 * calculate latency of ring.
 *
 * @note call at dequeue.
 *
 * @param ring_id ring id.
 * @param pkts Pointer to nb_pkts to containing packets.
 * @param nb_pkts Max number of packets to be measured.
 */
void sppwk_calc_ring_latency(int ring_id,
		struct rte_mbuf **pkts, uint16_t nb_pkts);

/**
 * get number of ring latency statistics.
 *
 * @return sppwk_init_ring_latency_stats's parameter "stats_count"
 */
int sppwk_get_ring_latency_stats_count(void);

/**
 *get specific ring latency statistics.
 *
 * @param ring_id
 *  The ring id.
 * @param stats
 *  The statistics values.
 */
void sppwk_get_ring_latency_stats(int ring_id,
		struct ring_latency_stats_t *stats);

/* Print statistics of time for packet processing in ring interface */
void print_ring_latency_stats(struct iface_info *if_info);

/**
 * Wrapper function for rte_eth_rx_burst() with ring latency feature.
 *
 * @param[in] port_id Etherdev ID.
 * @param[in] queue_id RX queue ID, but fixed value 0 in SPP.
 * @param[in] rx_pkts Pointers to mbuf should be enough to store nb_pkts.
 * @param nb_pkts Maximum number of RX packets.
 * @return Number of RX packets as number of pointers to mbuf.
 */
uint16_t sppwk_eth_ring_stats_rx_burst(uint16_t port_id,
		enum port_type iface_type,
		int iface_no, uint16_t queue_id,
		struct rte_mbuf **rx_pkts, const uint16_t nb_pkts);

/**
 * Wrapper function for rte_eth_tx_burst() with ring latency feature.
 *
 * @param port_id Etherdev ID.
 * @param[in] queue_id TX queue ID, but fixed value 0 in SPP.
 * @param[in] tx_pkts Pointers to mbuf should be enough to store nb_pkts.
 * @param nb_pkts Maximum number of TX packets.
 * @return Number of TX packets as number of pointers to mbuf.
 */
uint16_t sppwk_eth_ring_stats_tx_burst(uint16_t port_id,
		enum port_type iface_type,
		int iface_no, uint16_t queue_id,
		struct rte_mbuf **tx_pkts, uint16_t nb_pkts);

/**
 * Wrapper function for rte_eth_rx_burst() with VLAN and ring latency feature.
 *
 * @param[in] port_id Etherdev ID.
 * @param[in] queue_id RX queue ID, but fixed value 0 in SPP.
 * @param[in] rx_pkts Pointers to mbuf should be enough to store nb_pkts.
 * @param nb_pkts Maximum number of RX packets.
 * @return Number of RX packets as number of pointers to mbuf.
 */
uint16_t sppwk_eth_vlan_ring_stats_rx_burst(uint16_t port_id,
		enum port_type iface_type,
		int iface_no, uint16_t queue_id,
		struct rte_mbuf **rx_pkts, const uint16_t nb_pkts);

/**
 * Wrapper function for rte_eth_tx_burst() with VLAN and ring latency feature.
 *
 * @param port_id Etherdev ID.
 * @param[in] queue_id TX queue ID, but fixed value 0 in SPP.
 * @param[in] tx_pkts Pointers to mbuf should be enough to store nb_pkts.
 * @param nb_pkts Maximum number of TX packets.
 * @return Number of TX packets as number of pointers to mbuf.
 */
uint16_t sppwk_eth_vlan_ring_stats_tx_burst(uint16_t port_id,
		enum port_type iface_type,
		int iface_no, uint16_t queue_id,
		struct rte_mbuf **tx_pkts, uint16_t nb_pkts);

#else

#define sppwk_init_ring_latency_stats(arg1, arg2) 0
#define sppwk_clean_ring_latency_stats()
#define sppwk_add_ring_latency_time(arg1, arg2, arg3)
#define sppwk_calc_ring_latency(arg1, arg2, arg3)
#define sppwk_get_ring_latency_stats_count() 0
#define sppwk_get_ring_latency_stats(arg1, arg2)
#define print_ringlatencystats_stats(arg)
#define sppwk_eth_ring_stats_rx_burst(arg1, arg2, arg3, arg4, arg5, arg6)
#define sppwk_eth_ring_stats_tx_burst(arg1, arg2, arg3, arg4, arg5, arg6)
#define sppwk_eth_vlan_ring_stats_rx_burst(arg1, arg2, arg3, arg4, arg5, arg6)
#define sppwk_eth_vlan_ring_stats_tx_burst(arg1, arg2, arg3, arg4, arg5, arg6)

#endif /* SPP_RINGLATENCYSTATS_ENABLE */

#endif /* _RINGLATENCYSTATS_H_ */
