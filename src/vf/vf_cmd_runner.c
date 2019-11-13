/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Nippon Telegraph and Telephone Corporation
 */

#include "classifier.h"
#include "forwarder.h"
#include "shared/secondary/return_codes.h"
#include "shared/secondary/json_helper.h"
#include "shared/secondary/spp_worker_th/port_capability.h"
#include "shared/secondary/spp_worker_th/cmd_parser.h"
#include "shared/secondary/spp_worker_th/cmd_runner.h"
#include "shared/secondary/spp_worker_th/cmd_res_formatter.h"
#include "shared/secondary/spp_worker_th/vf_deps.h"

#define RTE_LOGTYPE_VF_CMD_RUNNER RTE_LOGTYPE_USER1

/**
 * List of classifier type. The order of items should be same as the order of
 * enum `spp_classifier_type` defined in cmd_utils.h.
 */
/* TODO(yasufum) fix similar var in cmd_parser.c */
const char *CLS_TYPE_A_LIST[] = {
	"none",
	"mac",
	"vlan",
	"",  /* termination */
};

/* Update classifier table with given action, add or del. */
static int
update_cls_table(enum sppwk_action wk_action,
		enum sppwk_cls_type cls_type __attribute__ ((unused)),
		int vid, const char *mac_str,
		const struct sppwk_port_idx *port)
{
	/**
	 * Use two types of mac addr in int64_t and uint64_t because first
	 * one is checked if converted value from string  is negative for error.
	 * If it is invalid, convert it to uint64_t.
	 */
	int64_t mac_int64;
	uint64_t mac_uint64;
	struct sppwk_port_info *port_info;

	RTE_LOG(DEBUG, VF_CMD_RUNNER, "Called __func__ with "
			"type `mac`, mac_addr `%s`, and port `%d:%d`.\n",
			mac_str, port->iface_type, port->iface_no);

	mac_int64 = sppwk_convert_mac_str_to_int64(mac_str);
	if (unlikely(mac_int64 == -1)) {
		RTE_LOG(ERR, VF_CMD_RUNNER, "Invalid MAC address `%s`.\n",
				mac_str);
		return SPPWK_RET_NG;
	}
	mac_uint64 = (uint64_t)mac_int64;

	port_info = get_sppwk_port(port->iface_type, port->iface_no);
	if (unlikely(port_info == NULL)) {
		RTE_LOG(ERR, VF_CMD_RUNNER, "Failed to get port %d:%d.\n",
				port->iface_type, port->iface_no);
		return SPPWK_RET_NG;
	}
	if (unlikely(port_info->iface_type == UNDEF)) {
		RTE_LOG(ERR, VF_CMD_RUNNER, "Port %d:%d doesn't exist.\n",
				port->iface_type, port->iface_no);
		return SPPWK_RET_NG;
	}

	if (wk_action == SPPWK_ACT_DEL) {
		if ((port_info->cls_attrs.vlantag.vid != 0) &&
				port_info->cls_attrs.vlantag.vid != vid) {
			RTE_LOG(ERR, VF_CMD_RUNNER,
					"Unexpected VLAN ID `%d`.\n", vid);
			return SPPWK_RET_NG;
		}
		if ((port_info->cls_attrs.mac_addr != 0) &&
				port_info->cls_attrs.mac_addr != mac_uint64) {
			RTE_LOG(ERR, VF_CMD_RUNNER, "Unexpected MAC %s.\n",
					mac_str);
			return SPPWK_RET_NG;
		}

		/* Initialize deleted attributes again. */
		port_info->cls_attrs.vlantag.vid = ETH_VLAN_ID_MAX;
		port_info->cls_attrs.mac_addr = 0;
		memset(port_info->cls_attrs.mac_addr_str, 0x00, STR_LEN_SHORT);
	} else if (wk_action == SPPWK_ACT_ADD) {
		if (unlikely(port_info->cls_attrs.vlantag.vid !=
				ETH_VLAN_ID_MAX)) {
			/* TODO(yasufum) why two vids are required in msg ? */
			RTE_LOG(ERR, VF_CMD_RUNNER, "Used port %d:%d, vid %d != %d.\n",
					port->iface_type, port->iface_no,
					port_info->cls_attrs.vlantag.vid, vid);
			return SPPWK_RET_NG;
		}
		if (unlikely(port_info->cls_attrs.mac_addr != 0)) {
			/* TODO(yasufum) why two macs are required in msg ? */
			RTE_LOG(ERR, VF_CMD_RUNNER, "Used port %d:%d, mac %s != %s.\n",
					port->iface_type, port->iface_no,
					port_info->cls_attrs.mac_addr_str,
					mac_str);
			return SPPWK_RET_NG;
		}

		/* Update attrs with validated params. */
		port_info->cls_attrs.vlantag.vid = vid;
		port_info->cls_attrs.mac_addr = mac_uint64;
		strcpy(port_info->cls_attrs.mac_addr_str, mac_str);
	}

	set_component_change_port(port_info, SPPWK_PORT_DIR_TX);
	return SPPWK_RET_OK;
}

/* Assign worker thread or remove on specified lcore. */
/* TODO(yasufum) revise func name for removing term `component` or `comp`. */
static int
update_comp(enum sppwk_action wk_action, const char *name,
		unsigned int lcore_id, enum sppwk_worker_type wk_type)
{
	int ret;
	int ret_del;
	int comp_lcore_id = 0;
	unsigned int tmp_lcore_id = 0;
	struct sppwk_comp_info *comp_info = NULL;
	/* TODO(yasufum) revise `core` to be more specific. */
	struct core_info *core = NULL;
	struct core_mng_info *info = NULL;
	struct sppwk_comp_info *comp_info_base = NULL;
	/* TODO(yasufum) revise `core_info` which is same as struct name. */
	struct core_mng_info *core_info = NULL;
	int *change_core = NULL;
	int *change_component = NULL;

	sppwk_get_mng_data(NULL, &comp_info_base, &core_info, &change_core,
			&change_component, NULL);

	switch (wk_action) {
	case SPPWK_ACT_START:
		info = (core_info + lcore_id);
		if (info->status == SPPWK_LCORE_UNUSED) {
			RTE_LOG(ERR, VF_CMD_RUNNER,
					"Not available lcore %d for %s.\n",
					lcore_id, "SPPWK_LCORE_UNUSED");
			return SPPWK_RET_NG;
		}

		comp_lcore_id = sppwk_get_lcore_id(name);
		if (comp_lcore_id >= 0) {
			RTE_LOG(ERR, VF_CMD_RUNNER, "Component name '%s' is already "
				"used.\n", name);
			return SPPWK_RET_NG;
		}

		comp_lcore_id = get_free_lcore_id();
		if (comp_lcore_id < 0) {
			RTE_LOG(ERR, VF_CMD_RUNNER, "Cannot assign component over the "
				"maximum number.\n");
			return SPPWK_RET_NG;
		}

		core = &info->core[info->upd_index];

		comp_info = (comp_info_base + comp_lcore_id);
		memset(comp_info, 0x00, sizeof(struct sppwk_comp_info));
		strcpy(comp_info->name, name);
		comp_info->wk_type = wk_type;
		comp_info->lcore_id = lcore_id;
		comp_info->comp_id = comp_lcore_id;

		core->id[core->num] = comp_lcore_id;
		core->num++;
		ret = SPPWK_RET_OK;
		tmp_lcore_id = lcore_id;
		*(change_component + comp_lcore_id) = 1;
		break;

	case SPPWK_ACT_STOP:
		comp_lcore_id = sppwk_get_lcore_id(name);
		if (comp_lcore_id < 0)
			return SPPWK_RET_OK;

		comp_info = (comp_info_base + comp_lcore_id);
		tmp_lcore_id = comp_info->lcore_id;
		memset(comp_info, 0x00, sizeof(struct sppwk_comp_info));

		info = (core_info + tmp_lcore_id);
		core = &info->core[info->upd_index];

		/* initialize classifier information */
		if (comp_info->wk_type == SPPWK_TYPE_CLS)
			init_classifier_info(comp_lcore_id);

		/* The latest lcore is released if worker thread is stopped. */
		ret_del = del_comp_info(comp_lcore_id, core->num, core->id);
		if (ret_del >= 0)
			core->num--;

		ret = SPPWK_RET_OK;
		*(change_component + comp_lcore_id) = 0;
		break;

	default:  /* Unexpected case. */
		ret = SPPWK_RET_NG;
		break;
	}

	*(change_core + tmp_lcore_id) = 1;
	return ret;
}

/* Check if over the maximum num of rx and tx ports of component. */
static int
check_vf_port_count(int component_type, enum sppwk_port_dir dir,
		int nof_rx, int nof_tx)
{
	RTE_LOG(INFO, VF_CMD_RUNNER, "port count, port_type=%d,"
				" rx=%d, tx=%d\n", dir, nof_rx, nof_tx);
	if (dir == SPPWK_PORT_DIR_RX)
		nof_rx++;
	else
		nof_tx++;
	/* Add rx or tx port appointed in port_type. */
	RTE_LOG(INFO, VF_CMD_RUNNER, "Num of ports after count up,"
				" port_type=%d, rx=%d, tx=%d\n",
				dir, nof_rx, nof_tx);
	switch (component_type) {
	case SPPWK_TYPE_FWD:
		if (nof_rx > 1 || nof_tx > 1)
			return SPPWK_RET_NG;
		break;

	case SPPWK_TYPE_MRG:
		if (nof_tx > 1)
			return SPPWK_RET_NG;
		break;

	case SPPWK_TYPE_CLS:
		if (nof_rx > 1)
			return SPPWK_RET_NG;
		break;

	default:
		/* Illegal component type. */
		return SPPWK_RET_NG;
	}

	return SPPWK_RET_OK;
}

/* Port add or del to execute it */
static int
update_port(enum sppwk_action wk_action,
		const struct sppwk_port_idx *port,
		enum sppwk_port_dir dir,
		const char *name,
		const struct sppwk_port_attrs *port_attrs)
{
	int ret = SPPWK_RET_NG;
	int port_idx;
	int ret_del = -1;
	int comp_lcore_id = 0;
	int cnt = 0;
	struct sppwk_comp_info *comp_info = NULL;
	struct sppwk_port_info *port_info = NULL;
	int *nof_ports = NULL;
	struct sppwk_port_info **ports = NULL;
	struct sppwk_comp_info *comp_info_base = NULL;
	int *change_component = NULL;

	comp_lcore_id = sppwk_get_lcore_id(name);
	if (comp_lcore_id < 0) {
		RTE_LOG(ERR, VF_CMD_RUNNER, "Unknown component by port command. "
				"(component = %s)\n", name);
		return SPPWK_RET_NG;
	}
	sppwk_get_mng_data(NULL, &comp_info_base, NULL, NULL,
			&change_component, NULL);
	comp_info = (comp_info_base + comp_lcore_id);
	port_info = get_sppwk_port(port->iface_type, port->iface_no);
	if (dir == SPPWK_PORT_DIR_RX) {
		nof_ports = &comp_info->nof_rx;
		ports = comp_info->rx_ports;
	} else {
		nof_ports = &comp_info->nof_tx;
		ports = comp_info->tx_ports;
	}

	switch (wk_action) {
	case SPPWK_ACT_ADD:
		/* Check if over the maximum num of ports of component. */
		if (check_vf_port_count(comp_info->wk_type, dir,
				comp_info->nof_rx,
				comp_info->nof_tx) != SPPWK_RET_OK)
			return SPPWK_RET_NG;

		/* Check if the port_info is included in array `ports`. */
		port_idx = get_idx_port_info(port_info, *nof_ports, ports);
		if (port_idx >= SPPWK_RET_OK) {
			/* registered */
			if (port_attrs->ops == SPPWK_PORT_OPS_ADD_VLAN) {
				while ((cnt < PORT_CAPABL_MAX) &&
					    (port_info->port_attrs[cnt].ops !=
					    SPPWK_PORT_OPS_ADD_VLAN))
					cnt++;
				if (cnt >= PORT_CAPABL_MAX) {
					RTE_LOG(ERR, VF_CMD_RUNNER, "update VLAN tag "
						"Non-registratio\n");
					return SPPWK_RET_NG;
				}
				memcpy(&port_info->port_attrs[cnt], port_attrs,
					sizeof(struct sppwk_port_attrs));

				ret = SPPWK_RET_OK;
				break;
			}
			return SPPWK_RET_OK;
		}

		if (*nof_ports >= RTE_MAX_ETHPORTS) {
			RTE_LOG(ERR, VF_CMD_RUNNER, "Cannot assign port over the "
				"maximum number.\n");
			return SPPWK_RET_NG;
		}

		if (port_attrs->ops != SPPWK_PORT_OPS_NONE) {
			while ((cnt < PORT_CAPABL_MAX) &&
					(port_info->port_attrs[cnt].ops !=
					SPPWK_PORT_OPS_NONE)) {
				cnt++;
			}
			if (cnt >= PORT_CAPABL_MAX) {
				RTE_LOG(ERR, VF_CMD_RUNNER,
						"No space of port ability.\n");
				return SPPWK_RET_NG;
			}
			memcpy(&port_info->port_attrs[cnt], port_attrs,
					sizeof(struct sppwk_port_attrs));
		}

		port_info->iface_type = port->iface_type;
		ports[*nof_ports] = port_info;
		(*nof_ports)++;

		ret = SPPWK_RET_OK;
		break;

	case SPPWK_ACT_DEL:
		for (cnt = 0; cnt < PORT_CAPABL_MAX; cnt++) {
			if (port_info->port_attrs[cnt].ops ==
					SPPWK_PORT_OPS_NONE)
				continue;

			if (port_info->port_attrs[cnt].dir == dir)
				memset(&port_info->port_attrs[cnt], 0x00,
					sizeof(struct sppwk_port_attrs));
		}

		ret_del = delete_port_info(port_info, *nof_ports, ports);
		if (ret_del == 0)
			(*nof_ports)--; /* If deleted, decrement number. */

		ret = SPPWK_RET_OK;
		break;

	default:  /* This case cannot be happend without invlid wk_action. */
		return SPPWK_RET_NG;
	}

	*(change_component + comp_lcore_id) = 1;
	return ret;
}

/* Execute one command. */
int
exec_one_cmd(const struct sppwk_cmd_attrs *cmd)
{
	int ret;

	RTE_LOG(INFO, VF_CMD_RUNNER, "Exec `%s` cmd.\n",
			sppwk_cmd_type_str(cmd->type));

	switch (cmd->type) {
	case SPPWK_CMDTYPE_CLS_MAC:
	case SPPWK_CMDTYPE_CLS_VLAN:
		ret = update_cls_table(cmd->spec.cls_table.wk_action,
				cmd->spec.cls_table.cls_type,
				cmd->spec.cls_table.vid,
				cmd->spec.cls_table.mac,
				&cmd->spec.cls_table.port);
		if (ret == 0) {
			RTE_LOG(INFO, VF_CMD_RUNNER, "Exec flush.\n");
			ret = flush_cmd();
		}
		break;

	case SPPWK_CMDTYPE_WORKER:
		ret = update_comp(
				cmd->spec.comp.wk_action,
				cmd->spec.comp.name,
				cmd->spec.comp.core,
				cmd->spec.comp.wk_type);
		if (ret == 0) {
			RTE_LOG(INFO, VF_CMD_RUNNER, "Exec flush.\n");
			ret = flush_cmd();
		}
		break;

	case SPPWK_CMDTYPE_PORT:
		RTE_LOG(INFO, VF_CMD_RUNNER, "with action `%s`.\n",
				sppwk_action_str(cmd->spec.port.wk_action));
		ret = update_port(cmd->spec.port.wk_action,
				&cmd->spec.port.port, cmd->spec.port.dir,
				cmd->spec.port.name,
				&cmd->spec.port.port_attrs);
		if (ret == 0) {
			RTE_LOG(INFO, VF_CMD_RUNNER, "Exec flush.\n");
			ret = flush_cmd();
		}
		break;

	default:
		/* Do nothing. */
		ret = SPPWK_RET_OK;
		break;
	}

	return ret;
}

/* Iterate core info to create response of spp_vf status */
static int
iterate_lcore_info(struct sppwk_lcore_params *params)
{
	int ret;
	int lcore_id, cnt;
	struct core_info *core = NULL;
	struct sppwk_comp_info *comp_info_base = NULL;
	struct sppwk_comp_info *comp_info = NULL;

	RTE_LCORE_FOREACH_SLAVE(lcore_id) {
		if (sppwk_get_lcore_status(lcore_id) == SPPWK_LCORE_UNUSED)
			continue;

		core = get_core_info(lcore_id);
		if (core->num == 0) {
			ret = (*params->lcore_proc)(params, lcore_id, "",
				SPPWK_TYPE_NONE_STR, 0, NULL, 0, NULL);
			if (unlikely(ret != 0)) {
				RTE_LOG(ERR, VF_CMD_RUNNER,
						"Failed to proc on lcore %d\n",
						lcore_id);
				return SPPWK_RET_NG;
			}
			continue;
		}

		for (cnt = 0; cnt < core->num; cnt++) {
			sppwk_get_mng_data(NULL, &comp_info_base, NULL, NULL,
					NULL, NULL);
			comp_info = (comp_info_base + core->id[cnt]);

			if (comp_info->wk_type == SPPWK_TYPE_CLS) {
				ret = get_classifier_status(lcore_id,
						core->id[cnt], params);
			} else {
				ret = get_forwarder_status(lcore_id,
						core->id[cnt], params);
			}

			if (unlikely(ret != 0)) {
				RTE_LOG(ERR, VF_CMD_RUNNER,
						"Failed to get on lcore %d ,"
						"type %d\n",
						lcore_id, comp_info->wk_type);
				return SPPWK_RET_NG;
			}
		}
	}

	return SPPWK_RET_OK;
}

/* Add entry of core info of worker to a response in JSON such as "core:0". */
int
add_core(const char *name, char **output,
		void *tmp __attribute__ ((unused)))
{
	int ret = SPPWK_RET_NG;
	struct sppwk_lcore_params lcore_params;
	char *tmp_buff = spp_strbuf_allocate(CMD_RES_BUF_INIT_SIZE);
	if (unlikely(tmp_buff == NULL)) {
		RTE_LOG(ERR, VF_CMD_RUNNER, "Failed to alloc buff.\n");
		return SPPWK_RET_NG;
	}

	lcore_params.output = tmp_buff;
	lcore_params.lcore_proc = append_core_element_value;

	ret = iterate_lcore_info(&lcore_params);
	if (unlikely(ret != SPPWK_RET_OK)) {
		spp_strbuf_free(lcore_params.output);
		return SPPWK_RET_NG;
	}

	ret = append_json_array_brackets(output, name, lcore_params.output);
	spp_strbuf_free(lcore_params.output);
	return ret;
}

/* Activate temporarily stored component info while flushing. */
int
update_comp_info(struct sppwk_comp_info *p_comp_info, int *p_change_comp)
{
	int ret = 0;
	int cnt = 0;
	struct sppwk_comp_info *comp_info = NULL;

	for (cnt = 0; cnt < RTE_MAX_LCORE; cnt++) {
		if (*(p_change_comp + cnt) == 0)
			continue;

		comp_info = (p_comp_info + cnt);
		sppwk_update_port_dir(comp_info);

		if (comp_info->wk_type == SPPWK_TYPE_CLS) {
			ret = update_classifier(comp_info);
			RTE_LOG(DEBUG, VF_CMD_RUNNER, "Update classifier.\n");
		} else {
			ret = update_forwarder(comp_info);
			RTE_LOG(DEBUG, VF_CMD_RUNNER, "Update forwarder.\n");
		}

		if (unlikely(ret < 0)) {
			RTE_LOG(ERR, VF_CMD_RUNNER, "Flush error. "
					"( component = %s, type = %d)\n",
					comp_info->name,
					comp_info->wk_type);
			return SPPWK_RET_NG;
		}
	}
	return SPPWK_RET_OK;
}

/**
 * Operation function called in iterator for getting each of entries of
 * classifier table named as iterate_adding_mac_entry().
 */
int
append_classifier_element_value(
		struct classifier_table_params *params,
		enum sppwk_cls_type cls_type,
		int vid, const char *mac,
		const struct sppwk_port_idx *port)
{
	int ret = SPPWK_RET_NG;
	char *buff, *tmp_buff;
	char port_str[CMD_TAG_APPEND_SIZE];
	char value_str[STR_LEN_SHORT];
	buff = params->output;
	tmp_buff = spp_strbuf_allocate(CMD_RES_BUF_INIT_SIZE);
	if (unlikely(tmp_buff == NULL)) {
		RTE_LOG(ERR, VF_CMD_RUNNER,
				"Failed to allocate buffer.\n");
		return ret;
	}

	sppwk_port_uid(port_str, port->iface_type, port->iface_no);

	ret = append_json_str_value(&tmp_buff, "type",
			CLS_TYPE_A_LIST[cls_type]);
	if (unlikely(ret < SPPWK_RET_OK))
		return ret;

	memset(value_str, 0x00, STR_LEN_SHORT);
	switch (cls_type) {
	case SPPWK_CLS_TYPE_MAC:
		sprintf(value_str, "%s", mac);
		break;
	case SPPWK_CLS_TYPE_VLAN:
		sprintf(value_str, "%d/%s", vid, mac);
		break;
	default:
		/* not used */
		break;
	}

	ret = append_json_str_value(&tmp_buff, "value", value_str);
	if (unlikely(ret < 0))
		return ret;

	ret = append_json_str_value(&tmp_buff, "port", port_str);
	if (unlikely(ret < SPPWK_RET_OK))
		return ret;

	ret = append_json_block_brackets(&buff, "", tmp_buff);
	spp_strbuf_free(tmp_buff);
	params->output = buff;
	return ret;
}

/* Get component type from string of its name. */
/* TODO(yasufum) consider to create and move to vf_cmd_parser.c */
enum sppwk_worker_type
get_comp_type_from_str(const char *type_str)
{
	RTE_LOG(DEBUG, VF_CMD_RUNNER, "type_str is %s\n", type_str);

	if (strncmp(type_str, CORE_TYPE_CLASSIFIER_MAC_STR,
			strlen(CORE_TYPE_CLASSIFIER_MAC_STR)+1) == 0) {
		return SPPWK_TYPE_CLS;
	} else if (strncmp(type_str, CORE_TYPE_MERGE_STR,
			strlen(CORE_TYPE_MERGE_STR)+1) == 0) {
		return SPPWK_TYPE_MRG;
	} else if (strncmp(type_str, CORE_TYPE_FORWARD_STR,
			strlen(CORE_TYPE_FORWARD_STR)+1) == 0) {
		return SPPWK_TYPE_FWD;
	}

	return SPPWK_TYPE_NONE;
}

/**
 * List of combination of tag and operation function. It is used to assemble
 * a result of command in JSON like as following.
 *
 *     {
 *         "client-id": 1,
 *         "ports": ["phy:0", "phy:1", "vhost:0", "ring:0"],
 *         "components": [
 *             {
 *                 "core": 2,
 *                 ...
 */
/* TODO(yasufum) consider to create and move to vf_cmd_formatter.c */
int get_status_ops(struct cmd_res_formatter_ops *ops_list)
{
	/* Num of entries should be the same as NOF_STAT_OPS in vf_deps.h. */
	struct cmd_res_formatter_ops tmp_ops_list[] = {
		{ "client-id", add_client_id },
		{ "phy", add_interface },
		{ "vhost", add_interface },
		{ "ring", add_interface },
		{ "master-lcore", add_master_lcore},
		{ "core", add_core},
		{ "classifier_table", add_classifier_table},
		{ "", NULL }
	};
	memcpy(ops_list, tmp_ops_list, sizeof(tmp_ops_list));
	if (unlikely(ops_list == NULL)) {
		RTE_LOG(ERR, VF_CMD_RUNNER, "Failed to setup ops_list.\n");
		return -1;
	}

	return 0;
}
