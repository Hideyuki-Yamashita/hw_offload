/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015-2016 Intel Corporation
 * Copyright(c) 2019 Nippon Telegraph and Telephone Corporation
 */

#ifndef _NFV_COMMANDS_H_
#define _NFV_COMMANDS_H_

#include "shared/secondary/common.h"
#include "shared/secondary/add_port.h"
#include "shared/secondary/utils.h"

#define RTE_LOGTYPE_SPP_NFV RTE_LOGTYPE_USER1

/* TODO(yasufum): consider to rename find_port_id() to find_ethdev_id()
 * defined in shared/port_manager.c
 */

static int
do_del(char *p_type, int p_id)
{
	uint16_t port_id = PORT_RESET;

	if (!strcmp(p_type, "vhost")) {
		port_id = find_port_id(p_id, VHOST);
		if (port_id == PORT_RESET)
			return -1;
		dev_detach_by_port_id(port_id);

	} else if (!strcmp(p_type, "ring")) {
		port_id = find_port_id(p_id, RING);
		if (port_id == PORT_RESET)
			return -1;
		rte_eth_dev_stop(port_id);
		rte_eth_dev_close(port_id);

	} else if (!strcmp(p_type, "pcap")) {
		port_id = find_port_id(p_id, PCAP);
		if (port_id == PORT_RESET)
			return -1;
		dev_detach_by_port_id(port_id);

	} else if (!strcmp(p_type, "nullpmd")) {
		port_id = find_port_id(p_id, NULLPMD);
		if (port_id == PORT_RESET)
			return -1;
		dev_detach_by_port_id(port_id);

	}

	forward_array_remove(port_id);
	port_map_init_one(port_id);

	return 0;
}

/**
 * Add a port to this process. Port is described with resource UID which is a
 * combination of port type and ID like as 'ring:0'.
 */
static int
do_add(char *p_type, int p_id)
{
	enum port_type type = UNDEF;
	uint16_t port_id = PORT_RESET;
	int res = 0;

	if (!strcmp(p_type, "vhost")) {
		type = VHOST;
		res = add_vhost_pmd(p_id);

	} else if (!strcmp(p_type, "ring")) {
		type = RING;
		res = add_ring_pmd(p_id);

	} else if (!strcmp(p_type, "pcap")) {
		type = PCAP;
		res = add_pcap_pmd(p_id);

	} else if (!strcmp(p_type, "nullpmd")) {
		type = NULLPMD;
		res = add_null_pmd(p_id);
	}

	if (res < 0)
		return -1;

	port_id = (uint16_t) res;
	port_map[port_id].id = p_id;
	port_map[port_id].port_type = type;
	port_map[port_id].stats = &ports->client_stats[p_id];

	/* Update ports_fwd_array with port id */
	ports_fwd_array[port_id].in_port_id = port_id;

	return 0;
}

static int
do_connection(int *connected, int *sock)
{
	static struct sockaddr_in servaddr;
	int ret = 0;
	char ctl_ip[IPADDR_LEN] = { 0 };  /* spp_ctl's IP addr. */

	if (*connected == 0) {
		if (*sock < 0) {
			RTE_LOG(INFO, SPP_NFV, "Creating socket...\n");
			*sock = socket(AF_INET, SOCK_STREAM, 0);
			if (*sock < 0)
				rte_exit(EXIT_FAILURE, "socket error\n");

			/*Create of the tcp socket*/
			memset(&servaddr, 0, sizeof(servaddr));
			servaddr.sin_family = AF_INET;
			get_spp_ctl_ip(ctl_ip);
			servaddr.sin_addr.s_addr = inet_addr(ctl_ip);
			servaddr.sin_port = htons(get_spp_ctl_port());
		}

		RTE_LOG(INFO,
			SPP_NFV, "Trying to connect ... socket %d\n", *sock);
		ret = connect(*sock, (struct sockaddr *) &servaddr,
				sizeof(servaddr));
		if (ret < 0) {
			RTE_LOG(ERR, SPP_NFV, "Connection Error");
			return ret;
		}

		RTE_LOG(INFO, SPP_NFV, "Connected\n");
		*connected = 1;
	}

	return ret;
}

/* Return -1 if exit command is called to terminate the process */
static int
parse_command(char *str)
{
	uint16_t dev_id;
	char dev_name[RTE_DEV_NAME_MAX_LEN] = { 0 };
	char *token_list[MAX_PARAMETER] = { 0 };
	int cli_id;
	int max_token = 0;
	int ret = 0;
	char result[16] = { 0 };  /* succeeded or failed. */
	char port_set[128] = { 0 };
	char *p_type;
	int p_id;

	if (!str)
		return 0;

	/* tokenize user command from controller */
	token_list[max_token] = strtok(str, " ");
	while (token_list[max_token] != NULL) {
		RTE_LOG(DEBUG, SPP_NFV, "token %d = %s\n", max_token,
			token_list[max_token]);
		max_token++;
		token_list[max_token] = strtok(NULL, " ");
	}

	if (max_token == 0)
		return 0;

	if (!strcmp(token_list[0], "status")) {
		RTE_LOG(DEBUG, SPP_NFV, "status\n");
		memset(str, '\0', MSG_SIZE);
		if (cmd == FORWARD)
			get_sec_stats_json(str, get_client_id(), "running",
					lcore_id_used,
					ports_fwd_array, port_map);
		else
			get_sec_stats_json(str, get_client_id(), "idling",
					lcore_id_used,
					ports_fwd_array, port_map);

		RTE_ETH_FOREACH_DEV(dev_id) {
			rte_eth_dev_get_name_by_port(dev_id, dev_name);
			if (strlen(dev_name) > 0)
				RTE_LOG(DEBUG, SPP_NFV, "ethdevs:%2d %s\n",
						dev_id, dev_name);
		}

	} else if (!strcmp(token_list[0], "_get_client_id")) {
		memset(str, '\0', MSG_SIZE);
		/**
		 * TODO(yasufum) revise result msg. 1) need both of `results`
		 * and `result`. 2) change `success` to `succeeded`.
		 */
		sprintf(str, "{%s:%s,%s:%d,%s:%s}",
				"\"results\"", "[{\"result\":\"success\"}]",
				"\"client_id\"", get_client_id(),
				"\"process_type\"", "\"nfv\"");

	} else if (!strcmp(token_list[0], "_set_client_id")) {
		if (spp_atoi(token_list[1], &cli_id) >= 0) {
			set_client_id(cli_id);
			sprintf(result, "%s", "\"succeeded\"");
		} else
			sprintf(result, "%s", "\"failed\"");
		sprintf(str, "{%s:%s,%s:%s}",
				"\"result\"", result,
				"\"command\"", "\"_set_client_id\"");

	} else if (!strcmp(token_list[0], "exit")) {
		RTE_LOG(DEBUG, SPP_NFV, "exit\n");
		cmd = STOP;
		ret = -1;
		sprintf(str, "{%s:%s,%s:%s}",
				"\"result\"", "\"succeeded\"",
				"\"command\"", "\"exit\"");

	} else if (!strcmp(token_list[0], "stop")) {
		RTE_LOG(DEBUG, SPP_NFV, "stop\n");
		cmd = STOP;
		sprintf(str, "{%s:%s,%s:%s}",
				"\"result\"", "\"succeeded\"",
				"\"command\"", "\"stop\"");

	} else if (!strcmp(token_list[0], "forward")) {
		RTE_LOG(DEBUG, SPP_NFV, "forward\n");
		cmd = FORWARD;
		sprintf(str, "{%s:%s,%s:%s}",
				"\"result\"", "\"succeeded\"",
				"\"command\"", "\"forward\"");

	} else if (!strcmp(token_list[0], "add")) {
		RTE_LOG(DEBUG, SPP_NFV, "Received add command\n");

		ret = parse_resource_uid(token_list[1], &p_type, &p_id);
		if (ret < 0)
			return ret;

		if (do_add(p_type, p_id) < 0) {
			RTE_LOG(ERR, SPP_NFV, "Failed to do_add()\n");
			sprintf(result, "%s", "\"failed\"");
		} else
			sprintf(result, "%s", "\"succeeded\"");

		sprintf(port_set, "\"%s:%d\"", p_type, p_id);
		memset(str, '\0', MSG_SIZE);
		sprintf(str, "{%s:%s,%s:%s,%s:%s}",
				"\"result\"", result,
				"\"command\"", "\"add\"",
				"\"port\"", port_set);

	} else if (!strcmp(token_list[0], "patch")) {
		RTE_LOG(DEBUG, SPP_NFV, "patch\n");

		if (max_token <= 1)
			return 0;

		if (strncmp(token_list[1], "reset", 5) == 0) {
			/* reset forward array*/
			forward_array_reset();
		} else {
			uint16_t in_port;
			uint16_t out_port;

			if (max_token <= 2)
				return 0;

			char *in_p_type;
			char *out_p_type;
			int in_p_id;
			int out_p_id;

			parse_resource_uid(token_list[1], &in_p_type, &in_p_id);
			in_port = find_port_id(in_p_id,
					get_port_type(in_p_type));

			parse_resource_uid(token_list[2],
					&out_p_type, &out_p_id);
			out_port = find_port_id(out_p_id,
					get_port_type(out_p_type));

			if (in_port == PORT_RESET && out_port == PORT_RESET) {
				char err_msg[128];
				memset(err_msg, '\0', sizeof(err_msg));
				sprintf(err_msg, "%s '%s:%d' and '%s:%d'",
					"Patch not found, both of",
					in_p_type, in_p_id,
					out_p_type, out_p_id);
				RTE_LOG(ERR, SPP_NFV, "%s\n", err_msg);
			} else if (in_port == PORT_RESET) {
				char err_msg[128];
				memset(err_msg, '\0', sizeof(err_msg));
				sprintf(err_msg, "%s '%s:%d'",
					"Patch not found, in_port",
					in_p_type, in_p_id);
				RTE_LOG(ERR, SPP_NFV, "%s\n", err_msg);
			} else if (out_port == PORT_RESET) {
				char err_msg[128];
				memset(err_msg, '\0', sizeof(err_msg));
				sprintf(err_msg, "%s '%s:%d'",
					"Patch not found, out_port",
					out_p_type, out_p_id);
				RTE_LOG(ERR, SPP_NFV, "%s\n", err_msg);
			}

			if (add_patch(in_port, out_port) == 0) {
				RTE_LOG(INFO, SPP_NFV,
					"Patched '%s:%d' and '%s:%d'\n",
					in_p_type, in_p_id,
					out_p_type, out_p_id);
				sprintf(result, "%s", "\"succeeded\"");
			} else {
				RTE_LOG(ERR, SPP_NFV, "Failed to patch\n");
				sprintf(result, "%s", "\"failed\"");
			}

			sprintf(port_set,
				"{\"src\":\"%s:%d\",\"dst\":\"%s:%d\"}",
				in_p_type, in_p_id, out_p_type, out_p_id);

			memset(str, '\0', MSG_SIZE);
			sprintf(str, "{%s:%s,%s:%s,%s:%s}",
					"\"result\"", result,
					"\"command\"", "\"patch\"",
					"\"ports\"", port_set);

			ret = 0;
		}

	} else if (!strcmp(token_list[0], "del")) {
		RTE_LOG(DEBUG, SPP_NFV, "Received del command\n");

		cmd = STOP;

		ret = parse_resource_uid(token_list[1], &p_type, &p_id);
		if (ret < 0)
			return ret;

		if (do_del(p_type, p_id) < 0) {
			RTE_LOG(ERR, SPP_NFV, "Failed to do_del()\n");
			sprintf(result, "%s", "\"failed\"");
		} else
			sprintf(result, "%s", "\"succeeded\"");

		sprintf(port_set, "\"%s:%d\"", p_type, p_id);
		memset(str, '\0', MSG_SIZE);
		sprintf(str, "{%s:%s,%s:%s,%s:%s}",
				"\"result\"", result,
				"\"command\"", "\"del\"",
				"\"port\"", port_set);
	}

	return ret;
}

static int
do_receive(int *connected, int *sock, char *str)
{
	int ret;

	memset(str, '\0', MSG_SIZE);

	ret = recv(*sock, str, MSG_SIZE, 0);
	if (ret <= 0) {
		RTE_LOG(DEBUG, SPP_NFV, "Receive count: %d\n", ret);
		if (ret < 0)
			RTE_LOG(ERR, SPP_NFV, "Receive Fail");
		else
			RTE_LOG(INFO, SPP_NFV, "Receive 0\n");

		RTE_LOG(INFO, SPP_NFV, "Assume Server closed connection\n");
		close(*sock);
		*sock = SOCK_RESET;
		*connected = 0;
		return -1;
	}

	return 0;
}

static int
do_send(int *connected, int *sock, char *str)
{
	int ret;

	ret = send(*sock, str, MSG_SIZE, 0);
	if (ret == -1) {
		RTE_LOG(ERR, SPP_NFV, "send failed");
		*connected = 0;
		return -1;
	}

	RTE_LOG(INFO, SPP_NFV, "To Server: %s\n", str);

	return 0;
}

#endif // _NFV_COMMANDS_H_
