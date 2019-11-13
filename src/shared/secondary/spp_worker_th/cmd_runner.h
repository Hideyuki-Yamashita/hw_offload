/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017-2018 Nippon Telegraph and Telephone Corporation
 */

#ifndef _SPPWK_CMD_RUNNER_H_
#define _SPPWK_CMD_RUNNER_H_

/**
 * @file cmd_runner.h
 *
 * Run command for SPP worker thread.
 * Receive command message from SPP controller and run. The result is returned
 * to SPP controller as a JSON formatted message.
 */

#include "cmd_utils.h"

/**
 */
int flush_cmd(void);

/**
 * Setup connection for accepting commands from spp-ctl.
 *
 * @param ctl_ipaddr
 * IP address of spp-ctl.
 * @param ctl_port
 * Port number of spp-ctl.
 *
 * @retval SPPWK_RET_OK if succeeded.
 * @retval SPPWK_RET_NG if failed.
 */
int
sppwk_cmd_runner_conn(const char *ctl_ipaddr, int ctl_port);

/**
 * Run command sent from spp-ctl.
 *
 * @retval SPPWK_RET_OK if succeeded.
 * TODO(yasufum) change exclude case of exit cmd because it is not NG.
 * @retval SPPWK_RET_NG if connection failure or received exit command.
 */
int
sppwk_run_cmd(void);

/**
 *  Delete component information.
 *
 * @param[in] lcore_id The lcore ID of deleted comp.
 * @param[in] nof_comps The num of elements in comp_ary.
 * @param[in] *comp_ary Set of comps from which an comp is deleted.
 *
 * @retval SPPWK_RET_OK If succeeded.
 * @retval SPPWK_RET_NG If failed.
 */
int
del_comp_info(int lcore_id, int nof_comps, int *comp_ary);

#endif  /* _SPPWK_CMD_RUNNER_H_ */
