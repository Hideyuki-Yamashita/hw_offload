/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Nippon Telegraph and Telephone Corporation
 */

#include "cmd_res_formatter.h"
#include "port_capability.h"
#include "cmd_utils.h"
#include "shared/secondary/json_helper.h"

#ifdef SPP_VF_MODULE
#include "vf_deps.h"
#endif

#ifdef SPP_MIRROR_MODULE
#include "mirror_deps.h"
#endif

#define RTE_LOGTYPE_WK_CMD_RES_FMT RTE_LOGTYPE_USER1

/* Proto type declaration for a list of operation functions. */
static int append_result_value(const char *name, char **output, void *tmp);
static int append_error_details_value(const char *name, char **output,
		void *tmp);

/**
 * List of port abilities. The order of items should be same as the order of
 * enum `sppwk_port_abl_ops` in spp_vf.h.
 */
const char *PORT_ABILITY_STAT_LIST[] = {
	"none",
	"add",
	"del",
	"",  /* termination */
};

/* command response result string list */
struct cmd_res_formatter_ops response_result_list[] = {
	{ "result", append_result_value },
	{ "error_details", append_error_details_value },
	{ "", NULL }
};

/* Append a command result in JSON format. */
static int
append_result_value(const char *name, char **output, void *tmp)
{
	const struct cmd_result *result = tmp;
	return append_json_str_value(output, name, result->result);
}

/* Append error details in JSON format. */
static int
append_error_details_value(const char *name, char **output, void *tmp)
{
	int ret = SPPWK_RET_NG;
	const struct cmd_result *result = tmp;
	char *tmp_buff;
	/* string is empty, except for errors */
	if (result->err_msg[0] == '\0')
		return SPPWK_RET_OK;

	tmp_buff = spp_strbuf_allocate(CMD_RES_BUF_INIT_SIZE);
	if (unlikely(tmp_buff == NULL)) {
		RTE_LOG(ERR, WK_CMD_RES_FMT,
				"Fail to alloc buf for `%s`.\n", name);
		return SPPWK_RET_NG;
	}

	ret = append_json_str_value(&tmp_buff, "message", result->err_msg);
	if (unlikely(ret < 0)) {
		spp_strbuf_free(tmp_buff);
		return SPPWK_RET_NG;
	}

	ret = append_json_block_brackets(output, name, tmp_buff);
	spp_strbuf_free(tmp_buff);
	return ret;
}

/* Check if port is already flushed. */
static int
is_port_flushed(enum port_type iface_type, int iface_no)
{
	struct sppwk_port_info *port = get_sppwk_port(iface_type, iface_no);
	return port->ethdev_port_id >= 0;
}

/* Append index number as comma separated format such as `0, 1, ...`. */
int
append_interface_array(char **output, const enum port_type type)
{
	int i, port_cnt = 0;
	char tmp_str[CMD_TAG_APPEND_SIZE];

	for (i = 0; i < RTE_MAX_ETHPORTS; i++) {
		if (!is_port_flushed(type, i))
			continue;

		sprintf(tmp_str, "%s%d", JSON_APPEND_COMMA(port_cnt), i);

		*output = spp_strbuf_append(*output, tmp_str, strlen(tmp_str));
		if (unlikely(*output == NULL)) {
			RTE_LOG(ERR, WK_CMD_RES_FMT,
				/* TODO(yasufum) replace %d to string. */
				"Failed to add index for type `%d`.\n", type);
			return SPPWK_RET_NG;
		}
		port_cnt++;
	}
	return SPPWK_RET_OK;
}

/* append a secondary process type for JSON format */
int
append_process_type_value(const char *name, char **output,
		void *tmp __attribute__ ((unused)))
{
	return append_json_str_value(output, name, SPPWK_PROC_TYPE);
}

/* append a value of vlan for JSON format */
int
append_vlan_value(char **output, const int ope, const int vid, const int pcp)
{
	int ret = SPPWK_RET_OK;
	ret = append_json_str_value(output, "operation",
			PORT_ABILITY_STAT_LIST[ope]);
	if (unlikely(ret < SPPWK_RET_OK))
		return SPPWK_RET_NG;

	ret = append_json_int_value(output, "id", vid);
	if (unlikely(ret < 0))
		return SPPWK_RET_NG;

	ret = append_json_int_value(output, "pcp", pcp);
	if (unlikely(ret < 0))
		return SPPWK_RET_NG;

	return SPPWK_RET_OK;
}

/* append a block of vlan for JSON format */
int
append_vlan_block(const char *name, char **output,
		const int port_id, const enum sppwk_port_dir dir)
{
	int ret = SPPWK_RET_NG;
	int i = 0;
	struct sppwk_port_attrs *port_attrs = NULL;
	char *tmp_buff = spp_strbuf_allocate(CMD_RES_BUF_INIT_SIZE);
	if (unlikely(tmp_buff == NULL)) {
		RTE_LOG(ERR, WK_CMD_RES_FMT,
				"Failed to allocate buffer (name = %s).\n",
				name);
		return SPPWK_RET_NG;
	}

	sppwk_get_port_attrs(&port_attrs, port_id, dir);
	for (i = 0; i < PORT_CAPABL_MAX; i++) {
		switch (port_attrs[i].ops) {
		case SPPWK_PORT_OPS_ADD_VLAN:
		case SPPWK_PORT_OPS_DEL_VLAN:
			ret = append_vlan_value(&tmp_buff, port_attrs[i].ops,
					port_attrs[i].capability.vlantag.vid,
					port_attrs[i].capability.vlantag.pcp);
			if (unlikely(ret < SPPWK_RET_OK))
				return SPPWK_RET_NG;

			/*
			 * Change counter to "maximum+1" for exit the loop.
			 * An if statement after loop termination is false
			 * by "maximum+1 ".
			 */
			i = PORT_CAPABL_MAX + 1;
			break;
		default:
			/* not used */
			break;
		}
	}
	if (i == PORT_CAPABL_MAX) {
		ret = append_vlan_value(&tmp_buff, SPPWK_PORT_OPS_NONE,
				0, 0);
		if (unlikely(ret < SPPWK_RET_OK))
			return SPPWK_RET_NG;
	}

	ret = append_json_block_brackets(output, name, tmp_buff);
	spp_strbuf_free(tmp_buff);
	return ret;
}

/**
 * Get consistent port ID of rte ethdev from resource UID such as `phy:0`.
 * It returns a port ID, or error code if it's failed to.
 */
static int
get_ethdev_port_id(enum port_type iface_type, int iface_no)
{
	struct iface_info *iface_info = NULL;

	sppwk_get_mng_data(&iface_info, NULL, NULL, NULL, NULL, NULL);
	switch (iface_type) {
	case PHY:
		return iface_info->phy[iface_no].ethdev_port_id;
	case RING:
		return iface_info->ring[iface_no].ethdev_port_id;
	case VHOST:
		return iface_info->vhost[iface_no].ethdev_port_id;
	default:
		return SPPWK_RET_NG;
	}
}

/* append a block of port numbers for JSON format */
int
append_port_block(char **output, const struct sppwk_port_idx *port,
		const enum sppwk_port_dir dir)
{
	int ret = SPPWK_RET_NG;
	char port_str[CMD_TAG_APPEND_SIZE];
	char *tmp_buff = spp_strbuf_allocate(CMD_RES_BUF_INIT_SIZE);
	if (unlikely(tmp_buff == NULL)) {
		RTE_LOG(ERR, WK_CMD_RES_FMT,
				/* TODO(yasufum) refactor no meaning err msg */
				"allocate error. (name = port_block)\n");
		return SPPWK_RET_NG;
	}

	sppwk_port_uid(port_str, port->iface_type, port->iface_no);
	ret = append_json_str_value(&tmp_buff, "port", port_str);
	if (unlikely(ret < SPPWK_RET_OK))
		return SPPWK_RET_NG;

	ret = append_vlan_block("vlan", &tmp_buff,
			get_ethdev_port_id(
				port->iface_type, port->iface_no),
			dir);
	if (unlikely(ret < SPPWK_RET_OK))
		return SPPWK_RET_NG;

	ret = append_json_block_brackets(output, "", tmp_buff);
	spp_strbuf_free(tmp_buff);
	return ret;
}

/* append a list of port numbers for JSON format */
int
append_port_array(const char *name, char **output, const int num,
		const struct sppwk_port_idx *ports,
		const enum sppwk_port_dir dir)
{
	int ret = SPPWK_RET_NG;
	int i = 0;
	char *tmp_buff = spp_strbuf_allocate(CMD_RES_BUF_INIT_SIZE);
	if (unlikely(tmp_buff == NULL)) {
		RTE_LOG(ERR, WK_CMD_RES_FMT,
				/* TODO(yasufum) refactor no meaning err msg */
				"allocate error. (name = %s)\n",
				name);
		return SPPWK_RET_NG;
	}

	for (i = 0; i < num; i++) {
		ret = append_port_block(&tmp_buff, &ports[i], dir);
		if (unlikely(ret < SPPWK_RET_OK))
			return SPPWK_RET_NG;
	}

	ret = append_json_array_brackets(output, name, tmp_buff);
	spp_strbuf_free(tmp_buff);
	return ret;
}

/**
 * TODO(yasufum) add usages called from `add_core` or refactor
 * confusing function names.
 */
/* append one element of core information for JSON format */
int
append_core_element_value(struct sppwk_lcore_params *params,
		const unsigned int lcore_id,
		const char *name, const char *type,
		const int num_rx, const struct sppwk_port_idx *rx_ports,
		const int num_tx, const struct sppwk_port_idx *tx_ports)
{
	int ret = SPPWK_RET_NG;
	int unuse_flg = 0;
	char *buff, *tmp_buff;
	buff = params->output;
	tmp_buff = spp_strbuf_allocate(CMD_RES_BUF_INIT_SIZE);
	if (unlikely(tmp_buff == NULL)) {
		/* TODO(yasufum) refactor no meaning err msg */
		RTE_LOG(ERR, WK_CMD_RES_FMT,
				"allocate error. (name = %s)\n",
				name);
		return ret;
	}

	/* there is unnecessary data when "unuse" by type */
	unuse_flg = strcmp(type, SPPWK_TYPE_NONE_STR);

	/**
	 * TODO(yasufum) change ambiguous "core" to more specific one such as
	 * "worker-lcores" or "slave-lcores".
	 */
	ret = append_json_uint_value(&tmp_buff, "core", lcore_id);
	if (unlikely(ret < SPPWK_RET_OK))
		return ret;

	if (unuse_flg) {
		ret = append_json_str_value(&tmp_buff, "name", name);
		if (unlikely(ret < 0))
			return ret;
	}

	ret = append_json_str_value(&tmp_buff, "type", type);
	if (unlikely(ret < SPPWK_RET_OK))
		return ret;

	if (unuse_flg) {
		ret = append_port_array("rx_port", &tmp_buff,
				num_rx, rx_ports, SPPWK_PORT_DIR_RX);
		if (unlikely(ret < 0))
			return ret;

		ret = append_port_array("tx_port", &tmp_buff,
				num_tx, tx_ports, SPPWK_PORT_DIR_TX);
		if (unlikely(ret < SPPWK_RET_OK))
			return ret;
	}

	ret = append_json_block_brackets(&buff, "", tmp_buff);
	spp_strbuf_free(tmp_buff);
	params->output = buff;
	return ret;
}

/* append string of command response list for JSON format */
int
append_response_list_value(char **output,
		struct cmd_res_formatter_ops *responses, void *tmp)
{
	int ret = SPPWK_RET_NG;
	int i;
	char *tmp_buff;
	tmp_buff = spp_strbuf_allocate(CMD_RES_BUF_INIT_SIZE);
	if (unlikely(tmp_buff == NULL)) {
		RTE_LOG(ERR, WK_CMD_RES_FMT,
				/* TODO(yasufum) refactor no meaning err msg */
				"allocate error. (name = response_list)\n");
		return SPPWK_RET_NG;
	}

	for (i = 0; responses[i].tag_name[0] != '\0'; i++) {
		tmp_buff[0] = '\0';
		ret = responses[i].func(responses[i].tag_name, &tmp_buff, tmp);
		if (unlikely(ret < SPPWK_RET_OK)) {
			spp_strbuf_free(tmp_buff);
			RTE_LOG(ERR, WK_CMD_RES_FMT,
					"Failed to get reply string. "
					"(tag = %s)\n", responses[i].tag_name);
			return SPPWK_RET_NG;
		}

		if (tmp_buff[0] == '\0')
			continue;

		if ((*output)[0] != '\0') {
			ret = append_json_comma(output);
			if (unlikely(ret < SPPWK_RET_OK)) {
				spp_strbuf_free(tmp_buff);
				RTE_LOG(ERR, WK_CMD_RES_FMT,
						"Failed to add commas. "
						"(tag = %s)\n",
						responses[i].tag_name);
				return SPPWK_RET_NG;
			}
		}

		*output = spp_strbuf_append(*output, tmp_buff,
				strlen(tmp_buff));
		if (unlikely(*output == NULL)) {
			spp_strbuf_free(tmp_buff);
			RTE_LOG(ERR, WK_CMD_RES_FMT,
					"Failed to add reply string. "
					"(tag = %s)\n",
					responses[i].tag_name);
			return SPPWK_RET_NG;
		}
	}

	spp_strbuf_free(tmp_buff);
	return SPPWK_RET_OK;
}

/**
 * Setup `results` section in JSON msg. This is an example.
 *   "results": [ { "result": "success" } ]
 */
int
append_command_results_value(const char *name, char **output,
		int num, struct cmd_result *results)
{
	int ret = SPPWK_RET_NG;
	int i;
	char *tmp_buff1, *tmp_buff2;

	/* Setup result statement step by step with two buffers. */
	tmp_buff1 = spp_strbuf_allocate(CMD_RES_BUF_INIT_SIZE);
	if (unlikely(tmp_buff1 == NULL)) {
		RTE_LOG(ERR, WK_CMD_RES_FMT,
				"Faield to alloc 1st buf for `%s`.\n", name);
		return SPPWK_RET_NG;
	}
	tmp_buff2 = spp_strbuf_allocate(CMD_RES_BUF_INIT_SIZE);
	if (unlikely(tmp_buff2 == NULL)) {
		spp_strbuf_free(tmp_buff1);
		RTE_LOG(ERR, WK_CMD_RES_FMT,
				"Faield to alloc 2nd buf for `%s`.\n", name);
		return SPPWK_RET_NG;
	}

	for (i = 0; i < num; i++) {
		tmp_buff1[0] = '\0';

		/* Setup key-val pair such as `"result": "success"` */
		ret = append_response_list_value(&tmp_buff1,
				response_result_list, &results[i]);
		if (unlikely(ret < 0)) {
			spp_strbuf_free(tmp_buff1);
			spp_strbuf_free(tmp_buff2);
			return SPPWK_RET_NG;
		}

		/* Surround key-val pair such as `{ "result": "success" }`. */
		ret = append_json_block_brackets(&tmp_buff2, "", tmp_buff1);
		if (unlikely(ret < 0)) {
			spp_strbuf_free(tmp_buff1);
			spp_strbuf_free(tmp_buff2);
			return SPPWK_RET_NG;
		}
	}

	/**
	 * Setup result statement such as
	 * `"results": [ { "result": "success" } ]`.
	 */
	ret = append_json_array_brackets(output, name, tmp_buff2);

	spp_strbuf_free(tmp_buff1);
	spp_strbuf_free(tmp_buff2);
	return ret;
}

/**
 * Setup response of `status` command.
 *
 * This is an example of the response.
 *   "results": [ { "result": "success" } ],
 *   "info": {
 *       "client-id": 2,
 *       "phy": [ 0, 1 ], "vhost": [  ], "ring": [  ],
 *       "master-lcore": 1,
 *       "core": [
 *           {"core": 2, "type": "unuse"}, {"core": 3, "type": "unuse"}, ...
 *       ],
 *       "classifier_table": [  ]
 *   }
 */
int
append_info_value(const char *name, char **output)
{
	int ret = SPPWK_RET_NG;
	char *tmp_buff = spp_strbuf_allocate(CMD_RES_BUF_INIT_SIZE);
	struct cmd_res_formatter_ops ops_list[NOF_STAT_OPS];

	if (unlikely(tmp_buff == NULL)) {
		RTE_LOG(ERR, WK_CMD_RES_FMT,
				"Failed to get empty buf for append `%s`.\n",
				name);
		return SPPWK_RET_NG;
	}

	memset(ops_list, 0x00,
			sizeof(struct cmd_res_formatter_ops) * NOF_STAT_OPS);

	int is_got_ops = get_status_ops(ops_list);
	if (unlikely(is_got_ops < 0)) {
		RTE_LOG(ERR, WK_CMD_RES_FMT,
				"Failed to get ops_list.\n");
		return SPPWK_RET_NG;
	}

	/* Setup JSON msg in value of `info` key. */
	ret = append_response_list_value(&tmp_buff, ops_list, NULL);
	if (unlikely(ret < SPPWK_RET_OK)) {
		spp_strbuf_free(tmp_buff);
		return SPPWK_RET_NG;
	}

	/* Setup response of JSON msg. */
	ret = append_json_block_brackets(output, name, tmp_buff);
	spp_strbuf_free(tmp_buff);
	return ret;
}

/**
 * Operation functions start with prefix `add_` defined in get_status_ops()
 * of struct `cmd_res_formatter_ops` which are for making each of parts of
 * command response.
 */

/* Add entry of client ID such as `"client-id": 1` to a response in JSON. */
int
add_client_id(const char *name, char **output,
		void *tmp __attribute__ ((unused)))
{
	return append_json_int_value(output, name, get_client_id());
}

/* Add entry of port to a response in JSON such as "phy:0". */
int
add_interface(const char *name, char **output,
		void *tmp __attribute__ ((unused)))
{
	int ret = SPPWK_RET_NG;
	char *tmp_buff = spp_strbuf_allocate(CMD_RES_BUF_INIT_SIZE);
	if (unlikely(tmp_buff == NULL)) {
		RTE_LOG(ERR, WK_CMD_RES_FMT,
				/* TODO(yasufum) refactor no meaning err msg */
				"allocate error. (name = %s)\n",
				name);
		return SPPWK_RET_NG;
	}

	if (strcmp(name, SPPWK_PHY_STR) == 0)
		ret = append_interface_array(&tmp_buff, PHY);

	else if (strcmp(name, SPPWK_VHOST_STR) == 0)
		ret = append_interface_array(&tmp_buff, VHOST);

	else if (strcmp(name, SPPWK_RING_STR) == 0)
		ret = append_interface_array(&tmp_buff, RING);

	if (unlikely(ret < SPPWK_RET_OK)) {
		spp_strbuf_free(tmp_buff);
		return SPPWK_RET_NG;
	}

	ret = append_json_array_brackets(output, name, tmp_buff);
	spp_strbuf_free(tmp_buff);
	return ret;
}

/* Add entry of master lcore to a response in JSON. */
int
add_master_lcore(const char *name, char **output,
		void *tmp __attribute__ ((unused)))
{
	int ret = SPPWK_RET_NG;
	ret = append_json_int_value(output, name, rte_get_master_lcore());
	return ret;
}
