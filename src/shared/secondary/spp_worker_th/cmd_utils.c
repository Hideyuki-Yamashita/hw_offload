/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018-2019 Nippon Telegraph and Telephone Corporation
 */

#include <string.h>
#include <unistd.h>

#include <rte_eth_ring.h>
#include <rte_eth_vhost.h>
#include <rte_cycles.h>
#include <rte_log.h>
#include <rte_branch_prediction.h>

#include "cmd_utils.h"
#include "shared/secondary/add_port.h"
#include "shared/secondary/return_codes.h"

#ifdef SPP_VF_MODULE
#include "vf_deps.h"
#endif

#ifdef SPP_MIRROR_MODULE
#include "mirror_deps.h"
#endif

#include "shared/secondary/add_port.h"
#include "shared/secondary/utils.h"


/* TODO(yasufum) change log label after filename is revised. */
#define RTE_LOGTYPE_WK_CMD_UTILS RTE_LOGTYPE_USER1

/* A set of pointers of management data */
/* TODO(yasufum) change names start with `p_change` because it wrong meanig. */
struct mng_data_info {
	struct iface_info *p_iface_info;
	struct sppwk_comp_info *p_component_info;
	struct core_mng_info *p_core_info;
	int *p_change_core;
	int *p_change_component;  /* Set of flags for udpated components */
	struct cancel_backup_info *p_backup_info;
};

/* Logical core ID for main process */
static struct mng_data_info g_mng_data;

/* Hexdump `addr` for logging, used for core_info or component info. */
void
log_hexdumped(const char *obj_name, const void *obj_addr, const size_t size)
{
	size_t cnt;
	size_t max_cnt = (size / sizeof(unsigned int)) +
			((size % sizeof(unsigned int)) != 0);
	const uint32_t *buf = obj_addr;

	if ((obj_name != NULL) && (obj_name[0] != '\0'))
		RTE_LOG(DEBUG, WK_CMD_UTILS, "Name of dumped buf: %s.\n",
				obj_name);

	for (cnt = 0; cnt < max_cnt; cnt += 16) {
		RTE_LOG(DEBUG, WK_CMD_UTILS, "[%p]"
			" %08x %08x %08x %08x %08x %08x %08x %08x"
			" %08x %08x %08x %08x %08x %08x %08x %08x",
			&buf[cnt],
			buf[cnt+0], buf[cnt+1], buf[cnt+2], buf[cnt+3],
			buf[cnt+4], buf[cnt+5], buf[cnt+6], buf[cnt+7],
			buf[cnt+8], buf[cnt+9], buf[cnt+10], buf[cnt+11],
			buf[cnt+12], buf[cnt+13], buf[cnt+14], buf[cnt+15]);
	}
}

/* Get status of lcore of given ID. */
enum sppwk_lcore_status
sppwk_get_lcore_status(unsigned int lcore_id)
{
	return (g_mng_data.p_core_info + lcore_id)->status;
}

/**
 * Check status of all of cores is same as given
 *
 * It returns SPPWK_RET_NG as status mismatch if status is not same.
 * If core is in use, status will be checked.
 */
static int
check_core_status(enum sppwk_lcore_status status)
{
	unsigned int lcore_id = 0;
	RTE_LCORE_FOREACH_SLAVE(lcore_id) {
		if ((g_mng_data.p_core_info + lcore_id)->status !=
								status) {
			/* Status is mismatched */
			return SPPWK_RET_NG;
		}
	}
	return SPPWK_RET_OK;
}

int
check_core_status_wait(enum sppwk_lcore_status status)
{
	int cnt = 0;
	for (cnt = 0; cnt < SPP_CORE_STATUS_CHECK_MAX; cnt++) {
		sleep(1);
		int ret = check_core_status(status);
		if (ret == 0)
			return SPPWK_RET_OK;
	}

	RTE_LOG(ERR, WK_CMD_UTILS, "Status check time out. (status = %d)\n",
			status);
	return SPPWK_RET_NG;
}

/* Set core status */
void
set_core_status(unsigned int lcore_id,
		enum sppwk_lcore_status status)
{
	(g_mng_data.p_core_info + lcore_id)->status = status;
}

/* Set all core to given status */
void
set_all_core_status(enum sppwk_lcore_status status)
{
	unsigned int lcore_id = 0;
	RTE_LCORE_FOREACH_SLAVE(lcore_id) {
		(g_mng_data.p_core_info + lcore_id)->status = status;
	}
}

/**
 * Set all of component status to SPPWK_LCORE_REQ_STOP if received signal
 * is SIGTERM or SIGINT
 */
void
stop_process(int signal)
{
	unsigned int master_lcore;
	if (unlikely(signal != SIGTERM) &&
			unlikely(signal != SIGINT)) {
		return;
	}

	master_lcore = rte_get_master_lcore();
	(g_mng_data.p_core_info + master_lcore)->status =
		SPPWK_LCORE_REQ_STOP;
	set_all_core_status(SPPWK_LCORE_REQ_STOP);
}

/**
 * Return sppwk_port_info of given type and num of interface. It returns NULL
 * if given type is invalid.
 */
struct sppwk_port_info *
get_sppwk_port(enum port_type iface_type, int iface_no)
{
	struct iface_info *iface_info = g_mng_data.p_iface_info;

	switch (iface_type) {
	case PHY:
		return &iface_info->phy[iface_no];
	case VHOST:
		return &iface_info->vhost[iface_no];
	case RING:
		return &iface_info->ring[iface_no];
	default:
		return NULL;
	}
}

/* Dump of core information */
void
log_core_info(const struct core_mng_info *core_info)
{
	char str[STR_LEN_NAME];
	const struct core_mng_info *info = NULL;
	unsigned int lcore_id = 0;
	RTE_LCORE_FOREACH_SLAVE(lcore_id) {
		info = &core_info[lcore_id];
		RTE_LOG(DEBUG, WK_CMD_UTILS,
				"core[%d] status=%d, ref=%d, upd=%d\n",
				lcore_id, info->status,
				info->ref_index, info->upd_index);

		memset(str, 0x00, STR_LEN_NAME);
		log_hexdumped(str, info->core[0].id,
				sizeof(int)*info->core[0].num);

		sprintf(str, "core[%d]-1 num=%d", lcore_id, info->core[1].num);
		log_hexdumped(str, info->core[1].id,
				sizeof(int)*info->core[1].num);
	}
}

/* Dump of component information */
void
log_component_info(const struct sppwk_comp_info *comp_info)
{
	char str[STR_LEN_NAME];
	const struct sppwk_comp_info *tmp_ci = NULL;
	int cnt = 0;
	for (cnt = 0; cnt < RTE_MAX_LCORE; cnt++) {
		tmp_ci = &comp_info[cnt];
		if (tmp_ci->wk_type == SPPWK_TYPE_NONE)
			continue;

		RTE_LOG(DEBUG, WK_CMD_UTILS, "component[%d] name=%s, type=%d, "
				"core=%u, index=%d\n",
				cnt, tmp_ci->name, tmp_ci->wk_type,
				tmp_ci->lcore_id, tmp_ci->comp_id);

		sprintf(str, "component[%d] rx=%d", cnt,
				tmp_ci->nof_rx);
		log_hexdumped(str, tmp_ci->rx_ports,
			sizeof(struct sppwk_port_info *)*tmp_ci->nof_rx);

		sprintf(str, "component[%d] tx=%d", cnt,
				tmp_ci->nof_tx);
		log_hexdumped(str, tmp_ci->tx_ports,
			sizeof(struct sppwk_port_info *)*tmp_ci->nof_tx);
	}
}

/* Dump of interface information */
void
log_interface_info(const struct iface_info *iface_info)
{
	const struct sppwk_port_info *port = NULL;
	int cnt = 0;
	for (cnt = 0; cnt < RTE_MAX_ETHPORTS; cnt++) {
		port = &iface_info->phy[cnt];
		if (port->iface_type == UNDEF)
			continue;

		RTE_LOG(DEBUG, WK_CMD_UTILS,
				"phy  [%d] type=%d, no=%d, port=%d, "
				"vid = %u, mac=%08lx(%s)\n",
				cnt, port->iface_type, port->iface_no,
				port->ethdev_port_id,
				port->cls_attrs.vlantag.vid,
				port->cls_attrs.mac_addr,
				port->cls_attrs.mac_addr_str);
	}
	for (cnt = 0; cnt < RTE_MAX_ETHPORTS; cnt++) {
		port = &iface_info->vhost[cnt];
		if (port->iface_type == UNDEF)
			continue;

		RTE_LOG(DEBUG, WK_CMD_UTILS,
				"vhost[%d] type=%d, no=%d, port=%d, "
				"vid = %u, mac=%08lx(%s)\n",
				cnt, port->iface_type, port->iface_no,
				port->ethdev_port_id,
				port->cls_attrs.vlantag.vid,
				port->cls_attrs.mac_addr,
				port->cls_attrs.mac_addr_str);
	}
	for (cnt = 0; cnt < RTE_MAX_ETHPORTS; cnt++) {
		port = &iface_info->ring[cnt];
		if (port->iface_type == UNDEF)
			continue;

		RTE_LOG(DEBUG, WK_CMD_UTILS,
				"ring [%d] type=%d, no=%d, port=%d, "
				"vid = %u, mac=%08lx(%s)\n",
				cnt, port->iface_type, port->iface_no,
				port->ethdev_port_id,
				port->cls_attrs.vlantag.vid,
				port->cls_attrs.mac_addr,
				port->cls_attrs.mac_addr_str);
	}
}

/* Dump of all management information */
static void
log_all_mng_info(
		const struct core_mng_info *core,
		const struct sppwk_comp_info *component,
		const struct iface_info *interface)
{
	if (rte_log_get_global_level() < RTE_LOG_DEBUG)
		return;

	log_core_info(core);
	log_component_info(component);
	log_interface_info(interface);
}

/* Copy management information */
void
copy_mng_info(
		struct core_mng_info *dst_core,
		struct sppwk_comp_info *dst_component,
		struct iface_info *dst_interface,
		const struct core_mng_info *src_core,
		const struct sppwk_comp_info *src_component,
		const struct iface_info *src_interface,
		enum copy_mng_flg flg)
{
	int upd_index = 0;
	unsigned int lcore_id = 0;

	switch (flg) {
	case COPY_MNG_FLG_UPDCOPY:
		RTE_LCORE_FOREACH_SLAVE(lcore_id) {
			upd_index = src_core[lcore_id].upd_index;
			memcpy(&dst_core[lcore_id].core[upd_index],
					&src_core[lcore_id].core[upd_index],
					sizeof(struct core_info));
		}
		break;
	default:
		/*
		 * Even if the flag is set to None,
		 * copying of core is necessary,
		 * so we will treat all copies as default.
		 */
		memcpy(dst_core, src_core,
				sizeof(struct core_mng_info)*RTE_MAX_LCORE);
		break;
	}

	memcpy(dst_component, src_component,
			sizeof(struct sppwk_comp_info)*RTE_MAX_LCORE);
	memcpy(dst_interface, src_interface,
			sizeof(struct iface_info));
}

/* Backup the management information */
void
backup_mng_info(struct cancel_backup_info *backup)
{
	log_all_mng_info(g_mng_data.p_core_info,
			g_mng_data.p_component_info,
			g_mng_data.p_iface_info);
	copy_mng_info(backup->core, backup->component, &backup->interface,
			g_mng_data.p_core_info,
			g_mng_data.p_component_info,
			g_mng_data.p_iface_info,
			COPY_MNG_FLG_ALLCOPY);
	log_all_mng_info(backup->core, backup->component, &backup->interface);
	memset(g_mng_data.p_change_core, 0x00,
				sizeof(int)*RTE_MAX_LCORE);
	memset(g_mng_data.p_change_component, 0x00,
				sizeof(int)*RTE_MAX_LCORE);
}

/**
 * Initialize g_iface_info
 *
 * Clear g_iface_info and set initial value.
 */
static void
init_iface_info(void)
{
	int port_cnt;  /* increment ether ports */
	struct iface_info *p_iface_info = g_mng_data.p_iface_info;
	memset(p_iface_info, 0x00, sizeof(struct iface_info));
	for (port_cnt = 0; port_cnt < RTE_MAX_ETHPORTS; port_cnt++) {
		p_iface_info->phy[port_cnt].iface_type = UNDEF;
		p_iface_info->phy[port_cnt].iface_no = port_cnt;
		p_iface_info->phy[port_cnt].ethdev_port_id = -1;
		p_iface_info->phy[port_cnt].cls_attrs.vlantag.vid =
			ETH_VLAN_ID_MAX;
		p_iface_info->vhost[port_cnt].iface_type = UNDEF;
		p_iface_info->vhost[port_cnt].iface_no = port_cnt;
		p_iface_info->vhost[port_cnt].ethdev_port_id = -1;
		p_iface_info->vhost[port_cnt].cls_attrs.vlantag.vid =
			ETH_VLAN_ID_MAX;
		p_iface_info->ring[port_cnt].iface_type = UNDEF;
		p_iface_info->ring[port_cnt].iface_no = port_cnt;
		p_iface_info->ring[port_cnt].ethdev_port_id = -1;
		p_iface_info->ring[port_cnt].cls_attrs.vlantag.vid =
			ETH_VLAN_ID_MAX;
	}
}

/* Initialize g_component_info */
static void
init_component_info(void)
{
	int cnt;
	memset(g_mng_data.p_component_info, 0x00,
			sizeof(struct sppwk_comp_info)*RTE_MAX_LCORE);
	for (cnt = 0; cnt < RTE_MAX_LCORE; cnt++)
		(g_mng_data.p_component_info + cnt)->comp_id = cnt;
	memset(g_mng_data.p_change_component, 0x00,
			sizeof(int)*RTE_MAX_LCORE);
}

/* Initialize g_core_info */
static void
init_core_info(void)
{
	int cnt = 0;
	struct core_mng_info *p_core_info = g_mng_data.p_core_info;
	memset(p_core_info, 0x00,
			sizeof(struct core_mng_info)*RTE_MAX_LCORE);
	set_all_core_status(SPPWK_LCORE_STOPPED);
	for (cnt = 0; cnt < RTE_MAX_LCORE; cnt++) {
		(p_core_info + cnt)->ref_index = 0;
		(p_core_info + cnt)->upd_index = 1;
	}
	memset(g_mng_data.p_change_core, 0x00, sizeof(int)*RTE_MAX_LCORE);
}

/* Initialize mng data of ports on host */
static int
init_host_port_info(void)
{
	int port_type, port_id;
	int i, ret;
	int nof_phys = 0;
	char dev_name[RTE_DEV_NAME_MAX_LEN] = { 0 };
	struct iface_info *p_iface_info = g_mng_data.p_iface_info;

	for (i = 0; i < RTE_MAX_ETHPORTS; i++) {
		if (!rte_eth_dev_is_valid_port(i))
			continue;

		rte_eth_dev_get_name_by_port(i, dev_name);
		ret = parse_dev_name(dev_name, &port_type, &port_id);
		if (ret < 0)
			RTE_LOG(ERR, WK_CMD_UTILS,
					"Failed to parse dev_name.\n");

		if (port_type == PHY) {
			port_id = nof_phys;
			nof_phys++;
		}

		switch (port_type) {
		case PHY:
			p_iface_info->phy[port_id].iface_type = port_type;
			p_iface_info->phy[port_id].ethdev_port_id = port_id;
			break;
		case VHOST:
			p_iface_info->vhost[port_id].iface_type = port_type;
			p_iface_info->vhost[port_id].ethdev_port_id = port_id;
			break;
		case RING:
			p_iface_info->ring[port_id].iface_type = port_type;
			p_iface_info->ring[port_id].ethdev_port_id = port_id;
			break;
		default:
			RTE_LOG(ERR, WK_CMD_UTILS,
					"Unsupported port on host, "
					"type:%d, id:%d.\n",
					port_type, port_id);
		}
	}

	return SPPWK_RET_OK;
}

/* Setup management info for spp_vf */
int
init_mng_data(void)
{
	/* Initialize interface and core information */
	init_iface_info();
	init_core_info();
	init_component_info();

	int ret = init_host_port_info();
	if (unlikely(ret != SPPWK_RET_OK))
		return SPPWK_RET_NG;
	return SPPWK_RET_OK;
}

/* Remove sock file if spp is not running */
void
del_vhost_sockfile(struct sppwk_port_info *vhost)
{
	int cnt;

	/* Do not remove for if it is running in vhost-client mode. */
	if (get_vhost_cli_mode() != 0)
		return;

	for (cnt = 0; cnt < RTE_MAX_ETHPORTS; cnt++) {
		if (likely(vhost[cnt].iface_type == UNDEF)) {
			/* Skip removing if it is not using vhost */
			continue;
		}

		remove(get_vhost_iface_name(cnt));
	}
}

/* Get component type of target component_info */
enum sppwk_worker_type
sppwk_get_comp_type(int id)
{
	struct sppwk_comp_info *ci = (g_mng_data.p_component_info + id);
	return ci->wk_type;
}

/* Get core information which is in use */
struct core_info *
get_core_info(unsigned int lcore_id)
{
	struct core_mng_info *info = (g_mng_data.p_core_info + lcore_id);
	return &(info->core[info->ref_index]);
}

/* Check lcore info of given ID is updated */
int
sppwk_is_lcore_updated(unsigned int lcore_id)
{
	struct core_mng_info *info = (g_mng_data.p_core_info + lcore_id);
	if (info->ref_index == info->upd_index)
		return 1;
	else
		return 0;
}

/* Check if component is using port. */
int
sppwk_check_used_port(
		enum port_type iface_type,
		int iface_no,
		enum sppwk_port_dir dir)
{
	int cnt, port_cnt, max = 0;
	struct sppwk_comp_info *component = NULL;
	struct sppwk_port_info **port_array = NULL;
	struct sppwk_port_info *port = get_sppwk_port(iface_type, iface_no);
	struct sppwk_comp_info *component_info =
					g_mng_data.p_component_info;

	if (port == NULL)
		return SPPWK_RET_NG;

	for (cnt = 0; cnt < RTE_MAX_LCORE; cnt++) {
		component = (component_info + cnt);
		if (component->wk_type == SPPWK_TYPE_NONE)
			continue;

		if (dir == SPPWK_PORT_DIR_RX) {
			max = component->nof_rx;
			port_array = component->rx_ports;
		} else if (dir == SPPWK_PORT_DIR_TX) {
			max = component->nof_tx;
			port_array = component->tx_ports;
		}
		for (port_cnt = 0; port_cnt < max; port_cnt++) {
			if (unlikely(port_array[port_cnt] == port))
				return cnt;
		}
	}

	return SPPWK_RET_NG;
}

/* Set component update flag for given port */
void
set_component_change_port(struct sppwk_port_info *port,
		enum sppwk_port_dir dir)
{
	int ret = 0;
	if ((dir == SPPWK_PORT_DIR_RX) || (dir == SPPWK_PORT_DIR_BOTH)) {
		ret = sppwk_check_used_port(port->iface_type, port->iface_no,
				SPPWK_PORT_DIR_RX);
		if (ret >= 0)
			*(g_mng_data.p_change_component + ret) = 1;
	}

	if ((dir == SPPWK_PORT_DIR_TX) || (dir == SPPWK_PORT_DIR_BOTH)) {
		ret = sppwk_check_used_port(port->iface_type, port->iface_no,
				SPPWK_PORT_DIR_TX);
		if (ret >= 0)
			*(g_mng_data.p_change_component + ret) = 1;
	}
}

/* Get ID of unused lcore. */
int
get_free_lcore_id(void)
{
	struct sppwk_comp_info *comp_info = g_mng_data.p_component_info;

	int cnt = 0;
	for (cnt = 0; cnt < RTE_MAX_LCORE; cnt++) {
		if ((comp_info + cnt)->wk_type == SPPWK_TYPE_NONE)
			return cnt;
	}
	return SPPWK_RET_NG;
}

/* Get lcore ID as user-defined component name. */
int
sppwk_get_lcore_id(const char *comp_name)
{
	struct sppwk_comp_info *comp_info = g_mng_data.p_component_info;

	int cnt = 0;
	if (comp_name[0] == '\0')
		return SPPWK_RET_NG;

	for (cnt = 0; cnt < RTE_MAX_LCORE; cnt++) {
		if (strcmp(comp_name, (comp_info + cnt)->name) == 0)
			return cnt;
	}
	return SPPWK_RET_NG;
}

/**
 * Get index of given entry in given port info array. It returns the index,
 * or NG code if the entry is not found.
 */
int
get_idx_port_info(struct sppwk_port_info *p_info, int nof_ports,
		struct sppwk_port_info *p_info_ary[])
{
	int cnt = 0;
	int ret = SPPWK_RET_NG;
	for (cnt = 0; cnt < nof_ports; cnt++) {
		if (p_info == p_info_ary[cnt])
			ret = cnt;
	}
	return ret;
}

/* Delete given port info from the port info array. */
int
delete_port_info(struct sppwk_port_info *p_info, int nof_ports,
		struct sppwk_port_info *p_info_ary[])
{
	int target_idx;  /* The index of deleted port */
	int cnt;

	/* Find index of target port to be deleted. */
	target_idx = get_idx_port_info(p_info, nof_ports, p_info_ary);
	if (target_idx < 0)
		return SPPWK_RET_NG;

	/**
	 * Overwrite the deleted port by the next one, and shift all of
	 * remained ports.
	 */
	nof_ports--;
	for (cnt = target_idx; cnt < nof_ports; cnt++)
		p_info_ary[cnt] = p_info_ary[cnt+1];
	p_info_ary[cnt] = NULL;  /* Remove old last port. */
	return SPPWK_RET_OK;
}

/* Activate temporarily stored port info while flushing. */
int
update_port_info(void)
{
	int ret = 0;
	int cnt = 0;
	struct sppwk_port_info *port = NULL;
	struct iface_info *p_iface_info = g_mng_data.p_iface_info;

	/* Initialize added vhost. */
	for (cnt = 0; cnt < RTE_MAX_ETHPORTS; cnt++) {
		port = &p_iface_info->vhost[cnt];
		if ((port->iface_type != UNDEF) && (port->ethdev_port_id < 0)) {
			ret = add_vhost_pmd(port->iface_no);
			if (ret < 0)
				return SPPWK_RET_NG;
			port->ethdev_port_id = ret;
		}
	}

	/* Initialize added ring. */
	for (cnt = 0; cnt < RTE_MAX_ETHPORTS; cnt++) {
		port = &p_iface_info->ring[cnt];
		if ((port->iface_type != UNDEF) && (port->ethdev_port_id < 0)) {
			ret = add_ring_pmd(port->iface_no);
			if (ret < 0)
				return SPPWK_RET_NG;
			port->ethdev_port_id = ret;
		}
	}
	return SPPWK_RET_OK;
}

/* Activate temporarily stored lcore info while flushing. */
void
update_lcore_info(void)
{
	int cnt = 0;
	struct core_mng_info *info = NULL;
	struct core_mng_info *p_core_info = g_mng_data.p_core_info;
	int *p_change_core = g_mng_data.p_change_core;

	/* Changed core has changed index. */
	for (cnt = 0; cnt < RTE_MAX_LCORE; cnt++) {
		if (*(p_change_core + cnt) != 0) {
			info = (p_core_info + cnt);
			info->upd_index = info->ref_index;
		}
	}

	/* Waiting for changed core change. */
	for (cnt = 0; cnt < RTE_MAX_LCORE; cnt++) {
		if (*(p_change_core + cnt) != 0) {
			info = (p_core_info + cnt);
			while (likely(info->ref_index == info->upd_index))
				rte_delay_us_block(SPPWK_UPDATE_INTERVAL);

			memcpy(&info->core[info->upd_index],
					&info->core[info->ref_index],
					sizeof(struct core_info));
		}
	}
}

/* Return port uid such as `phy:0`, `ring:1` or so. */
int sppwk_port_uid(char *port_uid, enum port_type p_type, int iface_no)
{
	const char *p_type_str;

	switch (p_type) {
	case PHY:
		p_type_str = SPPWK_PHY_STR;
		break;
	case RING:
		p_type_str = SPPWK_RING_STR;
		break;
	case VHOST:
		p_type_str = SPPWK_VHOST_STR;
		break;
	default:
		return SPPWK_RET_NG;
	}

	sprintf(port_uid, "%s:%d", p_type_str, iface_no);

	return SPPWK_RET_OK;
}

/* Convert MAC address of 'aa:bb:cc:dd:ee:ff' to value of int64_t. */
int64_t
sppwk_convert_mac_str_to_int64(const char *macaddr)
{
	int64_t ret_mac = 0;
	int64_t token_val = 0;
	int token_cnt = 0;
	char tmp_mac[STR_LEN_SHORT];
	char *str = tmp_mac;
	char *saveptr = NULL;
	char *endptr = NULL;

	RTE_LOG(DEBUG, WK_CMD_UTILS, "Try to convert MAC addr `%s`.\n",
			macaddr);

	strcpy(tmp_mac, macaddr);
	while (1) {
		/* Split by colon ':'. */
		char *ret_tok = strtok_r(str, ":", &saveptr);
		if (unlikely(ret_tok == NULL))
			break;

		/* Check for mal-formatted address */
		if (unlikely(token_cnt >= RTE_ETHER_ADDR_LEN)) {
			RTE_LOG(ERR, WK_CMD_UTILS,
					"Invalid MAC address `%s`.\n",
					macaddr);
			return SPPWK_RET_NG;
		}

		/* Convert string to hex value */
		int ret_tol = strtol(ret_tok, &endptr, 16);
		if (unlikely(ret_tok == endptr) || unlikely(*endptr != '\0'))
			break;

		/* Append separated value to the result */
		token_val = (int64_t)ret_tol;
		ret_mac |= token_val << (token_cnt * 8);
		token_cnt++;
		str = NULL;
	}

	RTE_LOG(DEBUG, WK_CMD_UTILS,
			"Succeeded to convert MAC addr `%s` to `0x%08lx`.\n",
			macaddr, ret_mac);
	return ret_mac;
}

/* Set management data of global var for given non-NULL args. */
int sppwk_set_mng_data(
		struct iface_info *iface_p,
		struct sppwk_comp_info *component_p,
		struct core_mng_info *core_mng_p,
		int *change_core_p,
		int *change_component_p,
		struct cancel_backup_info *backup_info_p)
{
	/**
	 * TODO(yasufum) confirm why the last `0xffffffff` is same as NULL,
	 * although it is reserved for meaning as invalid.
	 */
	if (iface_p == NULL || component_p == NULL || core_mng_p == NULL ||
			change_core_p == NULL || change_component_p == NULL ||
			backup_info_p == NULL)
		return SPPWK_RET_NG;

	g_mng_data.p_iface_info = iface_p;
	g_mng_data.p_component_info = component_p;
	g_mng_data.p_core_info = core_mng_p;
	g_mng_data.p_change_core = change_core_p;
	g_mng_data.p_change_component = change_component_p;
	g_mng_data.p_backup_info = backup_info_p;

	return SPPWK_RET_OK;
}

/* Get management data from global var for given non-NULL args. */
void sppwk_get_mng_data(
		struct iface_info **iface_p,
		struct sppwk_comp_info **component_p,
		struct core_mng_info **core_mng_p,
		int **change_core_p,
		int **change_component_p,
		struct cancel_backup_info **backup_info_p)
{

	if (iface_p != NULL)
		*iface_p = g_mng_data.p_iface_info;
	if (component_p != NULL)
		*component_p = g_mng_data.p_component_info;
	if (core_mng_p != NULL)
		*core_mng_p = g_mng_data.p_core_info;
	if (change_core_p != NULL)
		*change_core_p = g_mng_data.p_change_core;
	if (change_component_p != NULL)
		*change_component_p = g_mng_data.p_change_component;
	if (backup_info_p != NULL)
		*backup_info_p = g_mng_data.p_backup_info;

}
