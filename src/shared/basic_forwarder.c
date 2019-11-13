/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Nippon Telegraph and Telephone Corporation
 */

#include <stdint.h>
#include "shared/common.h"
#include "shared/basic_forwarder.h"

void
forward(void)
{
	uint16_t nb_rx;
	uint16_t nb_tx;
	int in_port;
	int out_port;
	uint16_t buf;
	int i;

	/* Go through every possible port numbers*/
	for (i = 0; i < RTE_MAX_ETHPORTS; i++) {
		struct rte_mbuf *bufs[MAX_PKT_BURST];

		if (ports_fwd_array[i].in_port_id == PORT_RESET)
			continue;

		if (ports_fwd_array[i].out_port_id == PORT_RESET)
			continue;

		/* if status active, i count is in port*/
		in_port = i;
		out_port = ports_fwd_array[i].out_port_id;

		/* Get burst of RX packets, from first port of pair. */
		/*first port rx, second port tx*/
		nb_rx = ports_fwd_array[in_port].rx_func(in_port, 0, bufs,
			MAX_PKT_BURST);
		if (unlikely(nb_rx == 0))
			continue;

		port_map[in_port].stats->rx += nb_rx;

		/* Send burst of TX packets, to second port of pair. */
		nb_tx = ports_fwd_array[out_port].tx_func(out_port, 0, bufs,
			nb_rx);

		port_map[out_port].stats->tx += nb_tx;

		/* Free any unsent packets. */
		if (unlikely(nb_tx < nb_rx)) {
			port_map[out_port].stats->tx_drop += nb_rx - nb_tx;
			for (buf = nb_tx; buf < nb_rx; buf++)
				rte_pktmbuf_free(bufs[buf]);
		}
	}
}
