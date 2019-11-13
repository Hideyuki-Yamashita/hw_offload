/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Nippon Telegraph and Telephone Corporation
 */

#ifndef _SPP_PCAP_CMD_RUNNER_H_
#define _SPP_PCAP_CMD_RUNNER_H_

/**
 * @file
 * SPP Command processing
 *
 * Receive and process the command message, then send back the
 * result JSON formatted data.
 */

#include "cmd_utils.h"

/**
 * initialize command processor.
 *
 * @param controller_ip
 *  The controller's ip address.
 * @param controller_port
 *  The controller's port number.
 *
 * @retval SPPWK_RET_OK succeeded.
 * @retval SPPWK_RET_NG failed.
 */
int
spp_command_proc_init(const char *controller_ip, int controller_port);

/**
 * process command from controller.
 *
 * @retval SPPWK_RET_OK succeeded.
 * @retval SPPWK_RET_NG process termination is required.
 *            (occurred connection failure, or received exit command)
 */
int
sppwk_run_cmd(void);

#endif /* _SPP_PCAP_CMD_RUNNER_H_ */
