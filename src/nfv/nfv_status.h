/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Nippon Telegraph and Telephone Corporation
 */

#ifndef _NFV_STATUS_H_
#define _NFV_STATUS_H_

/* Get status of spp_nfv or spp_vm as JSON format. */
void get_sec_stats_json(char *str, int client_id,
		const char *running_stat,
		uint8_t lcore_id_used[RTE_MAX_LCORE],
		struct port *ports_fwd_array,
		struct port_map *port_map);

int append_lcore_info_json(char *str,
		uint8_t lcore_id_used[RTE_MAX_LCORE]);

/* Append port info to sec status, called from get_sec_stats_json(). */
int append_port_info_json(char *str,
		struct port *ports_fwd_array,
		struct port_map *port_map);

/* Append patch info to sec status, called from get_sec_stats_json(). */
int append_patch_info_json(char *str,
		struct port *ports_fwd_array,
		struct port_map *port_map);

#endif
