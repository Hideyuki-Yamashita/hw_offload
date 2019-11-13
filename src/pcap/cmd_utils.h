/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Nippon Telegraph and Telephone Corporation
 */

/**
 * TODO(yasufum) change this define tag because it is the same as
 * shared/.../cmd_utils.h. However, it should be the same to avoid both of
 * this and shared headers are included which are incompabtible and causes
 * an compile error. After fixing the incompatibility, change the tag name.
 */
#ifndef _SPPWK_CMD_UTILS_H_
#define _SPPWK_CMD_UTILS_H_

#include "data_types.h"

/**
 * @file cmd_utils.h
 *
 * Command utility functions for SPP worker thread.
 */

#include <netinet/in.h>
#include "shared/common.h"

/* Max number of core status check */
#define SPP_CORE_STATUS_CHECK_MAX 5

/**
 * Add ring pmd for owned proccess or thread.
 *
 * @param[in] ring_id added ring id.
 * @return ring port ID, or -1 if failed.
 */
/* TODO(yasufum) consider to merge to shared. */
int add_ring_pmd(int ring_id);

/**
 * Get status of lcore of given ID from global management info.
 *
 * @param[in] lcore_id Logical core ID.
 * @return Status of specified logical core.
 */
enum sppwk_lcore_status sppwk_get_lcore_status(unsigned int lcore_id);

/**
 * Run check_core_status() for SPP_CORE_STATUS_CHECK_MAX times with
 * interval time (1sec)
 *
 * @param status Wait check status.
 * @retval 0  If succeeded.
 * @retval -1 If failed.
 */
/* TODO(yasufum) remove it and use in shared because it is same. */
int check_core_status_wait(enum sppwk_lcore_status status);

/**
 * Set core status
 *
 * @param lcore_id Logical core ID.
 * @param status Set status.
 */
void set_core_status(unsigned int lcore_id, enum sppwk_lcore_status status);

/**
 * Set all core status to given
 *
 * @param status
 *  set status.
 *
 */
void set_all_core_status(enum sppwk_lcore_status status);

/**
 * Set all of component status to SPP_CORE_STOP_REQUEST if received signal
 * is SIGTERM or SIGINT
 *
 * @param signl
 *  received signal.
 *
 */
void stop_process(int signal);

/**
 * Return port info of given type and num of interface
 *
 * @param iface_type
 *  Interface type to be validated.
 * @param iface_no
 *  Interface number to be validated.
 *
 * @retval !NULL  sppwk_port_info.
 * @retval NULL   failed.
 */
struct sppwk_port_info *
get_iface_info(enum port_type iface_type, int iface_no);

/**
 * Setup management info for spp_vf
 */
int init_mng_data(void);

/* Get core information which is in use */
struct core_info *get_core_info(unsigned int lcore_id);

/**
 * Port type to string
 *
 * @param port String of port type to be converted.
 * @param iface_type Interface type.
 * @param iface_no Interface number.
 * @retval SPPWK_RET_OK If succeeded.
 * @retval SPPWK_RET_NG If failed.
 */
/* TODO(yasufum) consider to merge to shared. */
int
sppwk_port_uid(char *port, enum port_type iface_type, int iface_no);

/**
 * Set mange data address
 *
 * @param iface_p Pointer to g_iface_info address.
 * @param core_mng_p Pointer to g_core_info address.
 * @param capture_status_p Pointer to status of pcap.
 * @param capture_request_p Pointer to req of pcap.
 * @retval SPPWK_RET_OK If succeeded.
 * @retval SPPWK_RET_NG If failed.
 */
int spp_set_mng_data_addr(struct iface_info *iface_p,
			  struct spp_pcap_core_mng_info *core_mng_p,
			  int *capture_request_p,
			  int *capture_status_p);

/**
 * Get mange data address
 *
 * @param iface_p Pointer to g_iface_info.
 * @param core_mng_p Pointer to g_core_mng_info.
 * @param capture_request_p Pointer to status of pcap.
 * @param capture_status_p Pointer to req of pcap.
 */
void spp_get_mng_data_addr(struct iface_info **iface_p,
			   struct spp_pcap_core_mng_info **core_mng_p,
			   int **capture_request_p,
			   int **capture_status_p);

#endif
