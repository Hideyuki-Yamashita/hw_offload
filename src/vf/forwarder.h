/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017-2018 Nippon Telegraph and Telephone Corporation
 */

#ifndef __SPP_FORWARD_H__
#define __SPP_FORWARD_H__

#include "shared/secondary/spp_worker_th/cmd_utils.h"

/**
 * @file
 * SPP Forwarder and Merger
 *
 * Forwarder
 * This component provides function for packet processing from one port
 * to one port. Incoming packets from port are to be transferred to
 * specific one port. The direction of this transferring is specified
 * by port command.
 * Merger
 * This component provides packet forwarding function from multiple
 * ports to one port. Incoming packets from multiple ports are to be
 * transferred to one specific port. The flow of this merging process
 * is specified by port command.
 */

/* Clear g_forward_info, ref and update indices. */
void init_forwarder(void);

/**
 * Forward packets as forwarder or merger.
 *
 * Behavior of forwarding is defined as core_info->type which is given
 * as an argument of void and typecasted to spp_config_info.
 *
 * @param[in] id Unique component ID.
 * @retval SPPWK_RET_OK If succeeded.
 * @retval SPPWK_RET_NG If failed.
 */
int forward_packets(int id);

/**
 * Get forwarder status.
 *
 * @param lcore_id Lcore ID for forwarder and merger.
 * @param id Unique component ID.
 * @param params Pointer detailed data of forwarder and merger status.
 * @retval SPPWK_RET_OK If succeeded.
 * @retval SPPWK_RET_NG If failed.
 */
/**
 * TODO(yasufum) Consider to move this function to `vf_cmd_runner.c`.
 * This function is called only from `vf_cmd_runner.c`, but
 * must be defined in `spp_forward.c` because it refers g_forward_info defined
 * in this file. It is bad dependency for the global variable.
 */
int
get_forwarder_status(unsigned int lcore_id, int id,
		struct sppwk_lcore_params *params);

#endif /* __SPP_FORWARD_H__ */
