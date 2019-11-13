/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Nippon Telegraph and Telephone Corporation
 */

#include "shared/port_manager.h"

struct porttype_map portmap[] = {
	{ .port_name = "phy",   .port_type = PHY, },
	{ .port_name = "ring",  .port_type = RING, },
	{ .port_name = "vhost", .port_type = VHOST, },
	{ .port_name = "pcap", .port_type = PCAP, },
	{ .port_name = "nullpmd", .port_type = NULLPMD, },
	{ .port_name = "tap", .port_type = TAP, },
	{ .port_name = NULL,    .port_type = UNDEF, },
};

void
forward_array_init_one(unsigned int i)
{
	ports_fwd_array[i].in_port_id = PORT_RESET;
	ports_fwd_array[i].out_port_id = PORT_RESET;
}

/* initialize forward array with default value */
void
forward_array_init(void)
{
	unsigned int i;

	/* initialize port forward array*/
	for (i = 0; i < RTE_MAX_ETHPORTS; i++)
		forward_array_init_one(i);
}

void
forward_array_reset(void)
{
	unsigned int i;

	/* initialize port forward array*/
	for (i = 0; i < RTE_MAX_ETHPORTS; i++) {
		if (ports_fwd_array[i].in_port_id != PORT_RESET) {
			ports_fwd_array[i].out_port_id = PORT_RESET;
			RTE_LOG(INFO, SHARED, "Port ID %d\n", i);
			RTE_LOG(INFO, SHARED, "out_port_id %d\n",
				ports_fwd_array[i].out_port_id);
		}
	}
}

void
port_map_init_one(unsigned int i)
{
	port_map[i].id = PORT_RESET;
	port_map[i].port_type = UNDEF;
	port_map[i].stats = &port_map[i].default_stats;
}

void
port_map_init(void)
{
	unsigned int i;

	for (i = 0; i < RTE_MAX_ETHPORTS; i++)
		port_map_init_one(i);
}

/* Return -1 as an error if given patch is invalid */
int
add_patch(uint16_t in_port, uint16_t out_port)
{
	if (!is_valid_port(in_port) || !is_valid_port(out_port))
		return -1;

	/* Populate in port data */
	ports_fwd_array[in_port].in_port_id = in_port;
	ports_fwd_array[in_port].rx_func = &rte_eth_rx_burst;
	ports_fwd_array[in_port].tx_func = &rte_eth_tx_burst;
	ports_fwd_array[in_port].out_port_id = out_port;

	/* Populate out port data */
	ports_fwd_array[out_port].in_port_id = out_port;
	ports_fwd_array[out_port].rx_func = &rte_eth_rx_burst;
	ports_fwd_array[out_port].tx_func = &rte_eth_tx_burst;

	RTE_LOG(DEBUG, SHARED, "STATUS: in port %d in_port_id %d\n", in_port,
		ports_fwd_array[in_port].in_port_id);
	RTE_LOG(DEBUG, SHARED, "STATUS: in port %d patch out port id %d\n",
		in_port, ports_fwd_array[in_port].out_port_id);
	RTE_LOG(DEBUG, SHARED, "STATUS: outport %d in_port_id %d\n", out_port,
		ports_fwd_array[out_port].in_port_id);

	return 0;
}

/*
 * Return actual port ID which is assigned by system internally, or PORT_RESET
 * if port is not found.
 */
uint16_t
find_port_id(int id, enum port_type type)
{
	uint16_t port_id = PORT_RESET;
	uint16_t i;

	for (i = 0; i < RTE_MAX_ETHPORTS; i++) {
		if (port_map[i].port_type != type)
			continue;

		if (port_map[i].id == id) {
			port_id = i;
			break;
		}
	}

	return port_id;
}

/* Return 0 if invalid */
int
is_valid_port(uint16_t port_id)
{
	if (port_id > RTE_MAX_ETHPORTS)
		return 0;

	return port_map[port_id].id != PORT_RESET;
}

void
forward_array_remove(int port_id)
{
	unsigned int i;

	/* Update ports_fwd_array */
	forward_array_init_one(port_id);

	for (i = 0; i < RTE_MAX_ETHPORTS; i++) {
		if (ports_fwd_array[i].in_port_id == PORT_RESET)
			continue;

		if (ports_fwd_array[i].out_port_id == port_id) {
			ports_fwd_array[i].out_port_id = PORT_RESET;
			break;
		}
	}
}

/* Return a type of port as a enum member of porttype_map structure. */
enum port_type get_port_type(char *portname)
{
	int i;
	for (i = 0; portmap[i].port_name != NULL; i++) {
		const char *port_name = portmap[i].port_name;
		if (strncmp(portname, port_name, strlen(port_name)) == 0)
			return portmap[i].port_type;
	}
	return UNDEF;
}
