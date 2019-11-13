/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018-2019 Nippon Telegraph and Telephone Corporation
 */

#ifndef __SPPWK_CMD_UTILS_H__
#define __SPPWK_CMD_UTILS_H__

/**
 * @file cmd_utils.h
 *
 * Command utility functions for SPP worker thread.
 */

#include <netinet/in.h>
#include "data_types.h"
#include "shared/common.h"

/**
 * TODO(Yamashita) change type names.
 *  "merge" -> "merger", "forward" -> "forwarder".
 */
/** Identifier string for each component (status command) */
#define SPPWK_TYPE_CLS_STR "classifier"
#define SPPWK_TYPE_MRG_STR "merge"
#define SPPWK_TYPE_FWD_STR "forward"
#define SPPWK_TYPE_MIR_STR "mirror"
#define SPPWK_TYPE_PCAP_STR "pcap"
#define SPPWK_TYPE_NONE_STR "unuse"

/** Waiting time for checking update (not used for spp_pcap). */
#define SPPWK_UPDATE_INTERVAL 10  /* micro sec */

/**
 * Used for index of arrary of management data which has two sides. It is not
 * used for spp_pcap.
 */
#define TWO_SIDES 2

/** Maximum VLAN PCP, used only for spp_vf. */
#define SPP_VLAN_PCP_MAX 7

/* Max number of core status check */
#define SPP_CORE_STATUS_CHECK_MAX 5

/** Character sting for default port of classifier */
#define SPPWK_TERM_DEFAULT "default"

/**
 * Character sting for default MAC address of classifier.
 * It is used only for spp_vf.
 */
#define CLS_DUMMY_ADDR_STR "00:00:00:00:00:01"

/* Sampling interval timer for latency evaluation */
#define SPP_RING_LATENCY_STATS_SAMPLING_INTERVAL 1000000

/**
 * TODO(Yamashita) change type names.
 *  "merge" -> "merger", "forward" -> "forwarder".
 */
/* Name string for each component */
#define CORE_TYPE_CLASSIFIER_MAC_STR "classifier"
#define CORE_TYPE_MERGE_STR	     "merge"
#define CORE_TYPE_FORWARD_STR	     "forward"
#define CORE_TYPE_MIRROR_STR	     "mirror"

/* Classifier Type */
enum sppwk_cls_type {
	SPPWK_CLS_TYPE_NONE,
	SPPWK_CLS_TYPE_MAC,
	SPPWK_CLS_TYPE_VLAN
};

/* Flag of processing type to copy management information */
/* TODO(yasufum) add comments for each of members. */
enum copy_mng_flg {
	COPY_MNG_FLG_NONE,
	COPY_MNG_FLG_UPDCOPY,
	COPY_MNG_FLG_ALLCOPY,
};

/* Manage component running in core as global variable. */
struct core_info {
	int num;  /* Number of IDs below */
	int id[RTE_MAX_LCORE];  /* IDs of components run on the lcore. */
};

/**
 * Manage core status and comp info as global variable,
 * used for spp_vf and spp_mirror.
 */
struct core_mng_info {
	volatile enum sppwk_lcore_status status;
	volatile int ref_index;  /* index for reference */
	volatile int upd_index;  /* index for update */
	struct core_info core[TWO_SIDES];  /* info of each core */
};

/* Manage data used for backup. */
struct cancel_backup_info {
	struct core_mng_info core[RTE_MAX_LCORE];
	struct sppwk_comp_info component[RTE_MAX_LCORE];
	struct iface_info interface;
};

/**
 * Hexdump `addr` for logging, used for core_info or component info.
 *
 * @param name Name of object to be dumped.
 * @param addr Address of dumped value.
 * @param size Size of dumped value.
 */
void log_hexdumped(const char *obj_name, const void *obj_addr,
		const size_t size);

/**
 * Add ring pmd for owned proccess or thread.
 *
 * @param[in] index Vohst port id.
 * @param[in] client Client id.
 * @return Vhost port ID, or -1 if failed.
 */
int spp_vf_add_vhost_pmd(int index, int client);

/**
 * Get lcore status.
 *
 * @param[in] lcore_id Logical core ID.
 * @return Status of specified logical core.
 */
enum sppwk_lcore_status sppwk_get_lcore_status(unsigned int lcore_id);

/**
 * Get component type of target component_info
 *
 * @param id Component ID.
 * @return Type of component executed
 */
enum sppwk_worker_type sppwk_get_comp_type(int id);

/* TODO(yasufum) revise the name of func. */
/**
 * Run check_core_status() several times with interval, up to
 * SPP_CORE_STATUS_CHECK_MAX times.
 *
 * @param[in] status Status for checking.
 * @retval  0 If succeeded.
 * @retval -1 If failed.
 */
int check_core_status_wait(enum sppwk_lcore_status status);

/**
 * Set core status
 *
 * @param lcore_id Lcore ID.
 * @param status Status to be set.
 *
 */
void set_core_status(unsigned int lcore_id, enum sppwk_lcore_status status);

/**
 * Set all core status to given one.
 *
 * @param status status to be set.
 */
void set_all_core_status(enum sppwk_lcore_status status);

/**
 * Set all comp status to SPPWK_LCORE_REQ_STOP if received SIGTERM or SIGINT.
 *
 * @param[in] signal Received signal.
 */
void stop_process(int signal);

/* Return sppwk_port_info of given type and num of interface. */
struct sppwk_port_info *
get_sppwk_port(enum port_type iface_type, int iface_no);

/* Output log message for core information */
void log_core_info(const struct core_mng_info *core_info);

/* Output log message for component information */
void log_component_info(const struct sppwk_comp_info *component_info);

/* Output log message for interface information */
void log_interface_info(const struct iface_info *iface_info);

/* Copy management information */
void copy_mng_info(
		struct core_mng_info *dst_core,
		struct sppwk_comp_info *dst_component,
		struct iface_info *dst_interface,
		const struct core_mng_info *src_core,
		const struct sppwk_comp_info *src_component,
		const struct iface_info *src_interface,
		enum copy_mng_flg flg);

/* Backup the management information */
void backup_mng_info(struct cancel_backup_info *backup);

/* Setup management info for spp_vf */
int init_mng_data(void);

/* Remove sock file if spp is not running */
void del_vhost_sockfile(struct sppwk_port_info *vhost);

/* Get core information which is in use */
struct core_info *get_core_info(unsigned int lcore_id);

/**
 * Check lcore info of given ID is updated.
 *
 * @param lcore_id Lcore ID.
 * @retval 1 If it is updated.
 * @retval 0 If it is not updated.
 */
int sppwk_is_lcore_updated(unsigned int lcore_id);

/**
 * Check if component is using port.
 *
 * @param iface_type Interface type to be validated.
 * @param iface_no Interface number to be validated.
 * @param rxtx Value of spp_port_rxtx to be validated.
 * @retval 0~127      If match component ID
 * @retval SPPWK_RET_NG If failed.
 */
int sppwk_check_used_port(
		enum port_type iface_type,
		int iface_no,
		enum sppwk_port_dir dir);

/**
 * Set component update flag for given port.
 *
 * @param port Pointer of sppwk_port_info.
 * @param rxtx Enum spp_port_rxtx.
 */
void
set_component_change_port(struct sppwk_port_info *port,
		enum sppwk_port_dir dir);

/**
 * Get ID of unused lcore.
 *
 * @retval 0~127 Component ID.
 * @retval -1    failed.
 */
int get_free_lcore_id(void);

/**
 * Get component ID from given name.
 *
 * @param[in] name Component name.
 * @retval 0~127 Component ID.
 * @retval SPPWK_RET_NG if failed.
 */
int sppwk_get_lcore_id(const char *comp_name);

/**
 * Get index of given entry in given port info array. It returns the index,
 * or NG code if the entry is not found.
 *
 * @param[in] p_info Target port_info for getting index.
 * @param[in] nof_ports Num of ports for iterating given array.
 * @param[in] p_info_ary The array of port_info.
 * @return Index of given array, or NG code if not found.
 */
int get_idx_port_info(struct sppwk_port_info *p_info, int nof_ports,
		struct sppwk_port_info *p_info_ary[]);

/**
 *  search matched port_info from array and delete it.
 *
 * @param[in] p_info Target port to be deleted.
 * @param[in] nof_ports Number of ports of given p_info_ary.
 * @param[in] array[] Array of p_info.
 *
 * @retval 0  succeeded.
 * @retval -1 failed.
 */
int delete_port_info(struct sppwk_port_info *p_info, int nof_ports,
		struct sppwk_port_info *p_info_ary[]);

/**
 * Activate temporarily stored port info while flushing.
 *
 * @retval SPPWK_RET_OK if succeeded.
 * @retval SPPWK_RET_NG if failed.
 */
int update_port_info(void);

/* Activate temporarily stored lcore info while flushing. */
void update_lcore_info(void);

/**
 * Return port uid such as `phy:0`, `ring:1` or so.
 *
 * @param[in,out] port_uid String of port type to be converted.
 * @param[in] iface_type Interface type such as PHY or so.
 * @param[in] iface_no Interface number.
 * @return SPPWK_RET_OK If succeeded, or SPPWK_RET_NG if failed.
 */
int
sppwk_port_uid(char *port_uid, enum port_type iface_type, int iface_no);

/**
 * Change string of MAC address to int64.
 *
 * @param macaddr String of MAC address to be converted.
 * @retval 0~N MAC address in int64 format.
 * @retval SPPWK_RET_NG if invalid.
 */
int64_t sppwk_convert_mac_str_to_int64(const char *macaddr);

/**
 * Set mange data address.
 *
 * @param iface_p Pointer to g_iface_info address.
 * @param component_p Pointer to g_component_info address.
 * @param core_mng_p Pointer to g_core_info address.
 * @param change_core_p Pointer to g_change_core address.
 * @param change_component_p Pointer to g_change_component address.
 * @param backup_info_p Pointer to g_backup_info address.
 * @retval SPPWK_RET_OK If succeeded.
 * @retval SPPWK_RET_NG If failed.
 */
int sppwk_set_mng_data(struct iface_info *iface_p,
		struct sppwk_comp_info *component_p,
		struct core_mng_info *core_mng_p,
		int *change_core_p,
		int *change_component_p,
		struct cancel_backup_info *backup_info_p);

/**
 * Get mange data address.
 *
 * @param iface_p Pointer to g_iface_info.
 * @param component_p Pointer to g_component_info.
 * @param core_mng_p Pointer to g_core_mng_info.
 * @param change_core_p Pointer to change_core_addr.
 * @param change_component_p Pointer to g_change_component.
 * @param backup_info_p Pointer to g_backup_info.
 */
void sppwk_get_mng_data(struct iface_info **iface_p,
		struct sppwk_comp_info **component_p,
		struct core_mng_info **core_mng_p,
		int **change_core_p,
		int **change_component_p,
		struct cancel_backup_info **backup_info_p);

#endif /* __SPPWK_CMD_UTILS_H__ */
