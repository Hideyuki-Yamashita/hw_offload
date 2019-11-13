/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Nippon Telegraph and Telephone Corporation
 */

#ifndef __SPP_WORKER_TH_MIRROR_DEPS_H__
#define __SPP_WORKER_TH_MIRROR_DEPS_H__

#include "cmd_utils.h"
#include "cmd_parser.h"
#include "cmd_res_formatter.h"

#define SPPWK_PROC_TYPE "mirror"

/* Num of entries of ops_list in mir_cmd_runner.c. */
#define NOF_STAT_OPS 7

int exec_one_cmd(const struct sppwk_cmd_attrs *cmd);

int add_core(const char *name, char **output,
		void *tmp __attribute__ ((unused)));

/**
 * Update mirror info.
 *
 * @param wk_comp_info Pointer to internal data of mirror.
 * @retval SPPWK_RET_OK If succeeded.
 * @retval SPPWK_RET_NG If failed.
 */
int update_mirror(struct sppwk_comp_info *wk_comp_info);

/**
 * Activate temporarily stored component info while flushing.
 *
 * @param[in] p_comp_info Info of component.
 * @param[in] p_change_comp Pointer to a set of Flags for udpated component.
 * @retval SPPWK_RET_OK If succeeded.
 * @retval SPPWK_RET_NG If failed.
 */
int update_comp_info(struct sppwk_comp_info *p_comp_info, int *p_change_comp);

enum sppwk_worker_type get_comp_type_from_str(const char *type_str);

int get_status_ops(struct cmd_res_formatter_ops *ops_list);

int get_client_id(void);

#endif  /* __SPP_WORKER_TH_MIRROR_DEPS_H__ */
