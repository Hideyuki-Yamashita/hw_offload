/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Nippon Telegraph and Telephone Corporation
 */

#include <stdint.h>
#include "shared/common.h"
#include "shared/secondary/utils.h"

#define RTE_LOGTYPE_SHARED RTE_LOGTYPE_USER1

int client_id;
int vhost_cli;

int set_client_id(int cid)
{
	client_id = cid;
	return 0;
}

int get_client_id(void)
{
	if (client_id < 0) {
		RTE_LOG(ERR, SHARED, "Client ID is not initialized.\n");
		return -1;
	}
	return client_id;
}

int set_vhost_cli_mode(int mode)
{
	if (mode == 0 || mode == 1)
		vhost_cli = mode;
	else {
		RTE_LOG(ERR, SHARED, "Invalid value of vhost client\n");
		return -1;
	}
	return 0;
}

int get_vhost_cli_mode(void)
{
	if (vhost_cli < 0) {
		RTE_LOG(ERR, SHARED, "Vhost client is not initialized.\n");
		return -1;
	}
	return vhost_cli;
}

/* Parse client ID from given value of string. */
int
parse_client_id(int *cli_id, const char *cli_id_str)
{
	int id = 0;
	char *endptr = NULL;

	id = strtol(cli_id_str, &endptr, 0);
	if (unlikely(cli_id_str == endptr) || unlikely(*endptr != '\0'))
		return -1;

	if (id >= RTE_MAX_LCORE)
		return -1;

	*cli_id = id;
	RTE_LOG(DEBUG, SHARED, "Parse client ID %d.\n", *cli_id);
	return 0;
}

/**
 * Retieve port type and ID from resource UID. For example, resource UID
 * 'ring:0' is  parsed to retrieve port tyep 'ring' and ID '0'.
 */
int
parse_resource_uid(char *str, char **port_type, int *port_id)
{
	char *token;
	char delim[] = ":";
	char *endp;

	RTE_LOG(DEBUG, SHARED, "Parsing resource UID: '%s'\n", str);
	if (strstr(str, delim) == NULL) {
		RTE_LOG(ERR, SHARED, "Invalid resource UID: '%s'\n", str);
		return -1;
	}
	RTE_LOG(DEBUG, SHARED, "Delimiter %s is included\n", delim);

	*port_type = strtok(str, delim);

	token = strtok(NULL, delim);
	*port_id = strtol(token, &endp, 10);

	if (*endp) {
		RTE_LOG(ERR, SHARED, "Bad integer value: %s\n", str);
		return -1;
	}

	return 0;
}

int
spp_atoi(const char *str, int *val)
{
	char *end;
	*val = strtol(str, &end, 10);
	if (*end) {
		RTE_LOG(ERR, SHARED, "Bad integer value: %s\n", str);
		return -1;
	}
	return 0;
}

/* attach the new device, then store port_id of the device */
int
dev_attach_by_devargs(const char *devargs, uint16_t *port_id)
{
	int ret = -1;
	struct rte_devargs da;

	memset(&da, 0, sizeof(da));

	/* parse devargs */
	if (rte_devargs_parse(&da, devargs))
		return -1;

	ret = rte_eal_hotplug_add(da.bus->name, da.name, da.args);
	if (ret < 0) {
		free(da.args);
		return ret;
	}

	ret = rte_eth_dev_get_port_by_name(da.name, port_id);

	free(da.args);

	return ret;
}

/* detach the device, then store the name of the device */
int
dev_detach_by_port_id(uint16_t port_id)
{
	struct rte_device *dev;
	struct rte_bus *bus;
	uint32_t dev_flags;
	int ret = -1;

	if (rte_eth_devices[port_id].data == NULL) {
		RTE_LOG(INFO, SHARED,
			"rte_eth_devices[%"PRIu16"].data is  NULL\n", port_id);
		return 0;
	}
	dev_flags = rte_eth_devices[port_id].data->dev_flags;
	if (dev_flags & RTE_ETH_DEV_BONDED_SLAVE) {
		RTE_LOG(ERR, SHARED,
			"Port %"PRIu16" is bonded, cannot detach\n", port_id);
		return -ENOTSUP;
	}

	dev = rte_eth_devices[port_id].device;
	if (dev == NULL)
		return -EINVAL;

	bus = rte_bus_find_by_device(dev);
	if (bus == NULL)
		return -ENOENT;

	ret = rte_eal_hotplug_remove(bus->name, dev->name);
	if (ret < 0)
		return ret;

	rte_eth_dev_release_port(&rte_eth_devices[port_id]);

	return 0;
}
