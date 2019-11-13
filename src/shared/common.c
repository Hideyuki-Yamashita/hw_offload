/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015-2016 Intel Corporation
 * Copyright(c) 2019 Nippon Telegraph and Telephone Corporation
 */

#include <rte_cycles.h>
#include "common.h"

#define RTE_LOGTYPE_SHARED RTE_LOGTYPE_USER1

/**
 * Set log level of type RTE_LOGTYPE_USER* to given level, for instance,
 * RTE_LOG_INFO or RTE_LOG_DEBUG.
 *
 * This function is typically used to output debug log as following.
 *
 *   #define RTE_LOGTYPE_APP RTE_LOGTYPE_USER1
 *   ...
 *   set_user_log_level(1, RTE_LOG_DEBUG);
 *   ...
 *   RTE_LOG(DEBUG, APP, "Your debug log...");
 */
int
set_user_log_level(int num_user_log, uint32_t log_level)
{
	char userlog[8];

	if (num_user_log < 1 || num_user_log > 8)
		return -1;

	memset(userlog, '\0', sizeof(userlog));
	sprintf(userlog, "user%d", num_user_log);

	rte_log_set_level(rte_log_register(userlog), log_level);
	return 0;
}

/* Set log level of type RTE_LOGTYPE_USER* to RTE_LOG_DEBUG. */
int
set_user_log_debug(int num_user_log)
{
	set_user_log_level(num_user_log, RTE_LOG_DEBUG);
	return 0;
}

int
parse_server(char **server_ip, int *server_port, char *server_addr)
{
	const char delim[2] = ":";
	char *token;

	if (server_addr == NULL || *server_addr == '\0')
		return -1;

	*server_ip = strtok(server_addr, delim);
	RTE_LOG(DEBUG, SHARED, "server ip %s\n", *server_ip);

	token = strtok(NULL, delim);
	RTE_LOG(DEBUG, SHARED, "token %s\n", token);
	if (token == NULL || *token == '\0')
		return -1;

	RTE_LOG(DEBUG, SHARED, "token %s\n", token);
	*server_port = atoi(token);
	return 0;
}

/**
 * Get port type and port ID from ethdev name, such as `eth_vhost1` which
 * can be retrieved with rte_eth_dev_get_name_by_port().
 * In this case of `eth_vhost1`, port type is `VHOST` and port ID is `1`.
 */
int parse_dev_name(char *dev_name, int *port_type, int *port_id)
{
	char pid_str[12] = { 0 };
	int pid_len;
	int dev_name_len = strlen(dev_name);
	int dev_str_len;

	if (strncmp(dev_name, VDEV_ETH_RING,
				strlen(VDEV_ETH_RING)) == 0 ||
			strncmp(dev_name, VDEV_NET_RING,
				strlen(VDEV_NET_RING)) == 0) {
		dev_str_len = strlen(VDEV_NET_RING);
		pid_len = dev_name_len - dev_str_len;
		strncpy(pid_str, dev_name + strlen(VDEV_NET_RING),
				pid_len);
		*port_id = (int)strtol(pid_str, NULL, 10);
		*port_type = RING;

	} else if (strncmp(dev_name, VDEV_ETH_VHOST,
				strlen(VDEV_ETH_VHOST)) == 0 ||
			strncmp(dev_name, VDEV_NET_VHOST,
				strlen(VDEV_NET_VHOST)) == 0) {
		dev_str_len = strlen(VDEV_NET_VHOST);
		pid_len = dev_name_len - dev_str_len;
		strncpy(pid_str, dev_name + strlen(VDEV_NET_VHOST),
				pid_len);
		*port_id = (int)strtol(pid_str, NULL, 10);
		*port_type = VHOST;

	} else if (strncmp(dev_name, VDEV_PCAP,
			strlen(VDEV_PCAP)) == 0) {
		dev_str_len = strlen(VDEV_PCAP);
		pid_len = dev_name_len - dev_str_len;
		strncpy(pid_str, dev_name + strlen(VDEV_PCAP),
				pid_len);
		*port_id = (int)strtol(pid_str, NULL, 10);
		*port_type = PCAP;

	} else if (strncmp(dev_name, VDEV_ETH_TAP,
				strlen(VDEV_ETH_TAP)) == 0 ||
			strncmp(dev_name, VDEV_NET_TAP,
				strlen(VDEV_NET_TAP)) == 0) {
		dev_str_len = strlen(VDEV_NET_TAP);
		pid_len = dev_name_len - dev_str_len;
		strncpy(pid_str, dev_name + strlen(VDEV_NET_TAP),
				pid_len);
		*port_id = (int)strtol(pid_str, NULL, 10);
		*port_type = TAP;

	} else if (strncmp(dev_name, VDEV_NULL,
			strlen(VDEV_NULL)) == 0) {
		dev_str_len = strlen(VDEV_NULL);
		pid_len = dev_name_len - dev_str_len;
		strncpy(pid_str, dev_name + strlen(VDEV_NULL),
				pid_len);
		*port_id = (int)strtol(pid_str, NULL, 10);
		*port_type = PCAP;

	/* TODO(yasufum) add checking invalid port type and return -1 */
	} else {
		*port_id = 0;
		*port_type = PHY;
	}

	return 0;
}
