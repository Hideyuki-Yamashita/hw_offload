/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Nippon Telegraph and Telephone Corporation
 */

#define RTE_LOGTYPE_SHARED RTE_LOGTYPE_USER1

#include <arpa/inet.h>
#include "shared/common.h"
#include "nfv_status.h"

/*
 * Get status of spp_nfv as JSON format. It consists of running
 * status and patch info of ports.
 *
 * Here is an example of well-formatted JSON status to better understand.
 * Actual status has no spaces and new lines inserted.
 *
 *   {
 *     "status": "running",
 *     "lcores": [1, 2],
 *     "ports": ["phy:0", "phy:1", "ring:0", "vhost:0"],
 *     "patches": [
 *       {"src":"phy:0","dst": "ring:0"},
 *       {"src":"ring:0","dst": "vhost:0"}
 *     ]
 *   }
 */
void
get_sec_stats_json(char *str, int cli_id,
		const char *running_stat,
		uint8_t lcore_id_used[RTE_MAX_LCORE],
		struct port *ports_fwd_array,
		struct port_map *port_map)
{
	sprintf(str, "{\"client-id\":%d,", cli_id);

	sprintf(str + strlen(str), "\"status\":");
	sprintf(str + strlen(str), "\"%s\",", running_stat);

	append_lcore_info_json(str, lcore_id_used);
	sprintf(str + strlen(str), ",");

	append_port_info_json(str, ports_fwd_array, port_map);
	sprintf(str + strlen(str), ",");

	append_patch_info_json(str, ports_fwd_array, port_map);
	sprintf(str + strlen(str), "}");

	/* Make sure to be terminated with null character. */
	sprintf(str + strlen(str), "%c", '\0');
}

int
append_lcore_info_json(char *str,
		uint8_t lcore_id_used[RTE_MAX_LCORE])
{
	int i;
	sprintf(str + strlen(str), "\"master-lcore\":%d,",
			rte_get_master_lcore());
	sprintf(str + strlen(str), "\"lcores\":[");
	for (i = 0; i < RTE_MAX_LCORE; i++) {
		if (lcore_id_used[i] == 1)
			sprintf(str + strlen(str), "%d,", i);
	}

	/* Remove last ','. */
	sprintf(str + strlen(str) - 1, "%s", "]");
	return 0;
}


/*
 * Append patch info to sec status. It is called from get_sec_stats_json()
 * to add a JSON formatted patch info to given 'str'. Here is an example.
 *
 *     "ports": ["phy:0", "phy:1", "ring:0", "vhost:0"]
 */
int
append_port_info_json(char *str,
		struct port *ports_fwd_array,
		struct port_map *port_map)
{
	unsigned int i;
	unsigned int has_port = 0;  // for checking having port at last

	sprintf(str + strlen(str), "\"ports\":[");
	for (i = 0; i < RTE_MAX_ETHPORTS; i++) {

		if (ports_fwd_array[i].in_port_id == PORT_RESET)
			continue;

		has_port = 1;
		switch (port_map[i].port_type) {
		case PHY:
			sprintf(str + strlen(str), "\"phy:%u\",",
					port_map[i].id);
			break;
		case RING:
			sprintf(str + strlen(str), "\"ring:%u\",",
				port_map[i].id);
			break;
		case VHOST:
			sprintf(str + strlen(str), "\"vhost:%u\",",
				port_map[i].id);
			break;
		case PCAP:
			sprintf(str + strlen(str), "\"pcap:%u\",",
					port_map[i].id);
			break;
		case NULLPMD:
			sprintf(str + strlen(str), "\"nullpmd:%u\",",
					port_map[i].id);
			break;
		case TAP:
			sprintf(str + strlen(str), "\"tap:%u\",",
					port_map[i].id);
			break;
		case UNDEF:
			/* TODO(yasufum) Need to remove print for undefined ? */
			sprintf(str + strlen(str), "\"udf\",");
			break;
		}
	}

	/* Check if it has at least one port to remove ",". */
	if (has_port == 0) {
		sprintf(str + strlen(str), "]");
	} else {  /* Remove last ',' .*/
		sprintf(str + strlen(str) - 1, "]");
	}

	return 0;
}

/*
 * Append patch info to sec status. It is called from get_sec_stats_json()
 * to add a JSON formatted patch info to given 'str'. Here is an example.
 *
 *     "patches": [
 *       {"src":"phy:0","dst": "ring:0"},
 *       {"src":"ring:0","dst": "vhost:0"}
 *      ]
 */
int
append_patch_info_json(char *str,
		struct port *ports_fwd_array,
		struct port_map *port_map)
{
	unsigned int i;
	unsigned int has_patch = 0;  // for checking having patch at last

	char patch_str[128];
	sprintf(str + strlen(str), "\"patches\":[");
	for (i = 0; i < RTE_MAX_ETHPORTS; i++) {

		if (ports_fwd_array[i].in_port_id == PORT_RESET)
			continue;

		RTE_LOG(INFO, SHARED, "Port ID %d\n", i);
		RTE_LOG(INFO, SHARED, "Status %d\n",
			ports_fwd_array[i].in_port_id);

		memset(patch_str, '\0', sizeof(patch_str));

		sprintf(patch_str, "{\"src\":");

		switch (port_map[i].port_type) {
		case PHY:
			RTE_LOG(INFO, SHARED, "Type: PHY\n");
			sprintf(patch_str + strlen(patch_str),
					"\"phy:%u\",",
					port_map[i].id);
			break;
		case RING:
			RTE_LOG(INFO, SHARED, "Type: RING\n");
			sprintf(patch_str + strlen(patch_str),
					"\"ring:%u\",",
					port_map[i].id);
			break;
		case VHOST:
			RTE_LOG(INFO, SHARED, "Type: VHOST\n");
			sprintf(patch_str + strlen(patch_str),
					"\"vhost:%u\",",
					port_map[i].id);
			break;
		case PCAP:
			RTE_LOG(INFO, SHARED, "Type: PCAP\n");
			sprintf(patch_str + strlen(patch_str),
					"\"pcap:%u\",",
					port_map[i].id);
			break;
		case NULLPMD:
			RTE_LOG(INFO, SHARED, "Type: NULLPMD\n");
			sprintf(patch_str + strlen(patch_str),
					"\"nullpmd:%u\",",
					port_map[i].id);
			break;
		case TAP:
			RTE_LOG(INFO, SHARED, "Type: TAP\n");
			sprintf(patch_str + strlen(patch_str),
					"\"tap:%u\",",
					port_map[i].id);
			break;
		case UNDEF:
			RTE_LOG(INFO, SHARED, "Type: UDF\n");
			/* TODO(yasufum) Need to remove print for undefined ? */
			sprintf(patch_str + strlen(patch_str),
					"\"udf\",");
			break;
		}

		sprintf(patch_str + strlen(patch_str), "\"dst\":");

		RTE_LOG(INFO, SHARED, "Out Port ID %d\n",
				ports_fwd_array[i].out_port_id);

		if (ports_fwd_array[i].out_port_id == PORT_RESET) {
			//sprintf(patch_str + strlen(patch_str), "%s", "\"\"");
			continue;
		} else {
			has_patch = 1;
			unsigned int j = ports_fwd_array[i].out_port_id;
			switch (port_map[j].port_type) {
			case PHY:
				RTE_LOG(INFO, SHARED, "Type: PHY\n");
				sprintf(patch_str + strlen(patch_str),
						"\"phy:%u\"",
						port_map[j].id);
				break;
			case RING:
				RTE_LOG(INFO, SHARED, "Type: RING\n");
				sprintf(patch_str + strlen(patch_str),
						"\"ring:%u\"",
						port_map[j].id);
				break;
			case VHOST:
				RTE_LOG(INFO, SHARED, "Type: VHOST\n");
				sprintf(patch_str + strlen(patch_str),
						"\"vhost:%u\"",
						port_map[j].id);
				break;
			case PCAP:
				RTE_LOG(INFO, SHARED, "Type: PCAP\n");
				sprintf(patch_str + strlen(patch_str),
						"\"pcap:%u\"",
						port_map[j].id);
				break;
			case NULLPMD:
				RTE_LOG(INFO, SHARED, "Type: NULLPMD\n");
				sprintf(patch_str + strlen(patch_str),
						"\"nullpmd:%u\"",
						port_map[j].id);
				break;
			case TAP:
				RTE_LOG(INFO, SHARED, "Type: TAP\n");
				sprintf(patch_str + strlen(patch_str),
						"\"tap:%u\"",
						port_map[j].id);
				break;
			case UNDEF:
				RTE_LOG(INFO, SHARED, "Type: UDF\n");
				/*
				 * TODO(yasufum) Need to remove print for
				 * undefined ?
				 */
				sprintf(patch_str + strlen(patch_str),
						"\"udf\"");
				break;
			}
		}

		sprintf(patch_str + strlen(patch_str), "},");

		if (has_patch != 0)
			sprintf(str + strlen(str), "%s", patch_str);
	}


	/* Check if it has at least one patch to remove ",". */
	if (has_patch == 0) {
		sprintf(str + strlen(str), "]");
	} else {  /* Remove last ','. */
		sprintf(str + strlen(str) - 1, "]");
	}

	return 0;
}
