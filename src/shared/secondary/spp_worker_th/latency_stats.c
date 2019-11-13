/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017-2018 Nippon Telegraph and Telephone Corporation
 */

#include <unistd.h>
#include <sys/types.h>
#include <stdio.h>
#include <stddef.h>
#include <math.h>

#include <rte_mbuf.h>
#include <rte_log.h>
#include <rte_cycles.h>
#include <rte_malloc.h>
#include <rte_memcpy.h>

#include "latency_stats.h"
#include "cmd_utils.h"
#include "port_capability.h"
#include "../return_codes.h"

#define NS_PER_SEC 1E9

#define RTE_LOGTYPE_SPP_RING_LATENCY_STATS RTE_LOGTYPE_USER1

#ifdef SPP_RINGLATENCYSTATS_ENABLE

/** ring latency statistics information */
struct ring_latency_stats_info {
	uint64_t timer_tsc;  /**< sampling interval */
	uint64_t prev_tsc;   /**< previous time */
	struct ring_latency_stats_t stats;  /**< list of stats */
};

/** sampling interval */
static uint64_t g_samp_intvl;

/** ring latency statistics information instance */
static struct ring_latency_stats_info *g_stats_info;

/** number of ring latency statistics */
static uint16_t g_stats_count;

/* clock cycles per nano second */
static inline uint64_t
cycles_per_ns(void)
{
	return rte_get_timer_hz() / NS_PER_SEC;
}

/**
 * TODO(Hideyuki Yamashita) This function has a fatal bug in rte_zmalloc()
 * for `g_stats_info` and should be fixed. rte_zmalloc() returns NULL for
 * unknow reason.
 */
int
sppwk_init_ring_latency_stats(uint64_t samp_intvl, uint16_t stats_count)
{
	/* allocate memory for ring latency statistics information */
	g_stats_info = rte_zmalloc(
			"global ring_latency_stats_info",
			sizeof(struct ring_latency_stats_info) * stats_count,
			0);
	if (unlikely(g_stats_info == NULL)) {
		RTE_LOG(ERR, SPP_RING_LATENCY_STATS, "Cannot allocate memory "
				"for ring latency stats info\n");
		return SPPWK_RET_NG;
	}

	/* store global information for ring latency statistics */
	g_samp_intvl = samp_intvl * cycles_per_ns();
	g_stats_count = stats_count;

	RTE_LOG(DEBUG, SPP_RING_LATENCY_STATS,
			"g_samp_intvl=%lu, g_stats_count=%hu, "
			"cpns=%lu, NS_PER_SEC=%f\n",
			g_samp_intvl, g_stats_count,
			cycles_per_ns(), NS_PER_SEC);

	return SPPWK_RET_OK;
}

void
sppwk_clean_ring_latency_stats(void)
{
	/* free memory for ring latency statistics information */
	if (likely(g_stats_info != NULL)) {
		rte_free(g_stats_info);
		g_stats_count = 0;
	}
}

void
sppwk_add_ring_latency_time(int ring_id,
		struct rte_mbuf **pkts, uint16_t nb_pkts)
{
	unsigned int i;
	uint64_t diff_tsc, now;
	struct ring_latency_stats_info *stats_info = &g_stats_info[ring_id];

	for (i = 0; i < nb_pkts; i++) {

		/* get tsc now */
		now = rte_rdtsc();

		/* calculate difference from the previous processing time */
		diff_tsc = now - stats_info->prev_tsc;
		stats_info->timer_tsc += diff_tsc;

		/* set tsc to mbuf timestamp if it is over sampling interval. */
		if (unlikely(stats_info->timer_tsc >= g_samp_intvl)) {
			RTE_LOG(DEBUG, SPP_RING_LATENCY_STATS,
					"Set timestamp. ring_id=%d, "
					"pkts_index=%u, timestamp=%lu\n",
					ring_id, i, now);
			pkts[i]->timestamp = now;
			stats_info->timer_tsc = 0;
		}

		/* update previous tsc */
		stats_info->prev_tsc = now;
	}
}

void
sppwk_calc_ring_latency(int ring_id,
		struct rte_mbuf **pkts, uint16_t nb_pkts)
{
	unsigned int i;
	uint64_t now;
	int64_t latency;
	struct ring_latency_stats_info *stats_info = &g_stats_info[ring_id];

	now = rte_rdtsc();
	for (i = 0; i < nb_pkts; i++) {
		if (likely(pkts[i]->timestamp == 0))
			continue;

		/* calc latency if mbuf `timestamp` is non-zero. */
		latency = (uint64_t)floor((now - pkts[i]->timestamp) /
				cycles_per_ns());
		if (likely(latency < TOTAL_LATENCY_ENT-1))
			stats_info->stats.distr[latency]++;
		else
			stats_info->stats.distr[TOTAL_LATENCY_ENT-1]++;
	}
}

int
sppwk_get_ring_latency_stats_count(void)
{
	return g_stats_count;
}

void
sppwk_get_ring_latency_stats(int ring_id,
		struct ring_latency_stats_t *stats)
{
	struct ring_latency_stats_info *stats_info = &g_stats_info[ring_id];

	rte_memcpy(stats, &stats_info->stats,
			sizeof(struct ring_latency_stats_t));
}

/* Print statistics of time for packet processing in ring interface */
void
print_ring_latency_stats(struct iface_info *if_info)
{
	/* Clear screen and move cursor to top left */
	const char topLeft[] = { 27, '[', '1', ';', '1', 'H', '\0' };
	const char clr[] = { 27, '[', '2', 'J', '\0' };
	printf("%s%s", clr, topLeft);

	int ring_cnt, stats_cnt;
	struct ring_latency_stats_t stats[RTE_MAX_ETHPORTS];
	memset(&stats, 0x00, sizeof(stats));

	printf("RING Latency\n");
	printf(" RING");
	for (ring_cnt = 0; ring_cnt < RTE_MAX_ETHPORTS; ring_cnt++) {
		if (if_info->ring[ring_cnt].iface_type == UNDEF)
			continue;

		sppwk_get_ring_latency_stats(ring_cnt, &stats[ring_cnt]);
		printf(", %-18d", ring_cnt);
	}
	printf("\n");

	for (stats_cnt = 0; stats_cnt < TOTAL_LATENCY_ENT;
			stats_cnt++) {
		printf("%3dns", stats_cnt);
		for (ring_cnt = 0; ring_cnt < RTE_MAX_ETHPORTS; ring_cnt++) {
			if (if_info->ring[ring_cnt].iface_type == UNDEF)
				continue;

			printf(", 0x%-16lx", stats[ring_cnt].distr[stats_cnt]);
		}
		printf("\n");
	}
}

/* Wrapper function for rte_eth_rx_burst() with calc ring latency. */
uint16_t
sppwk_eth_ring_stats_rx_burst(uint16_t port_id,
		enum port_type iface_type,
		int iface_no,
		uint16_t queue_id  __attribute__ ((unused)),
		struct rte_mbuf **rx_pkts, const uint16_t nb_pkts)
{
	uint16_t nb_rx;

	nb_rx = rte_eth_rx_burst(port_id, 0, rx_pkts, nb_pkts);

	/* TODO(yasufum) confirm why it returns SPPWK_RET_OK. */
	if (unlikely(nb_rx == 0))
		return SPPWK_RET_OK;

	if (iface_type == RING)
		sppwk_calc_ring_latency(iface_no, rx_pkts, nb_pkts);
	return nb_rx;
}

/* Wrapper function for rte_eth_tx_burst() with calc ring latency. */
uint16_t
sppwk_eth_ring_stats_tx_burst(uint16_t port_id,
		enum port_type iface_type,
		int iface_no,
		uint16_t queue_id __attribute__ ((unused)),
		struct rte_mbuf **tx_pkts, uint16_t nb_pkts)
{
	uint16_t nb_tx;

	nb_tx = rte_eth_tx_burst(port_id, 0, tx_pkts, nb_pkts);

	if (iface_type == RING)
		sppwk_add_ring_latency_time(iface_no, tx_pkts, nb_pkts);
	return nb_tx;
}

#endif /* SPP_RINGLATENCYSTATS_ENABLE */
