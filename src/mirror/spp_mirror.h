/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Nippon Telegraph and Telephone Corporation
 */

#ifndef __SPP_MIRROR_H__
#define __SPP_MIRROR_H__

#include "shared/secondary/spp_worker_th/cmd_utils.h"

/**
 * Get mirror status.
 *
 * @param lcore_id Lcore ID for forwarder and merger.
 * @param id Unique component ID.
 * @param params Pointer to detailed data of mirror status.
 * @retval SPPWK_RET_OK If succeeded.
 * @retval SPPWK_RET_NG If failed.
 */
/**
 * TODO(yasufum) Consider to move this function to `mir_cmd_runner.c`.
 * This function is called only from `mir_cmd_runner.c`, but
 * must be defined in `spp_mirror.c` because it refers g_mirror_info defined
 * in this file. It is bad dependency for the global variable.
 */
int get_mirror_status(unsigned int lcore_id, int id,
		struct sppwk_lcore_params *params);

#endif /* __SPP_MIRROR_H__ */
