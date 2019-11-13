/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017-2018 Nippon Telegraph and Telephone Corporation
 */

#ifndef _COMMAND_CONN_H_
#define _COMMAND_CONN_H_

/**
 * @file SPP Connection
 *
 * Command connection management.
 */

#include "cmd_utils.h"

/** result code - temporary error. please retry */
#define SPP_CONNERR_TEMPORARY -1
/** result code - fatal error occurred. should terminate process. */
#define SPP_CONNERR_FATAL     -2

/**
 * Initialize connection to spp-ctl.
 *
 * @param[in] ctl_ipaddr IP address of spp-ctl.
 * @param[in] ctl_port Port num of spp-ctl.
 * @retval SPPWK_RET_OK If succeeded.
 * @retval SPPWK_RET_NG If failed.
 */
int conn_spp_ctl_init(const char *ctl_ipaddr, int ctl_port);

/**
 * Connect to spp-ctl.
 *
 * @note bocking.
 * @param sock Socket number for connecting to controller.
 * @retval SPPWK_RET_OK If succeeded.
 * @retval SPP_CONNERR_TEMPORARY Temporary error for retry.
 */
int conn_spp_ctl(int *sock);

/**
 * Receive message from spp-ctl.
 *
 * This function returns the num of received  msg in bytes, or SPPWK_RET_OK
 * if empty message. Given socket is closed if spp-ctl has terminated the
 * session.
 *
 * @note non-blocking.
 * @param[in,out] sock Socket.
 * @param[in,out] msgbuf The pointer to command message buffer.
 * @retval NOB_BYTES Num of bytes of received msg if succeeded.
 * @retval SPPWK_RET_OK No receive message.
 * @retval SPP_CONNERR_TEMPORARY Temporary error for retry.
 * @retval SPP_CONNERR_FATAL Fatal error for terminating the process.
 */
int recv_ctl_msg(int *sock, char **msgbuf);

/**
 * Send message to spp-ctl.
 *
 * @note non-blocking.
 * @param[in,out] sock Socket.
 * @param[in] msg Message sent to spp-ctl.
 * @param[in] msg_len Length of given message.
 * @retval SPPWK_RET_OK If succeeded.
 * @retval SPP_CONNERR_TEMPORARY Temporary error for retry.
 */
int send_ctl_msg(int *sock, const char *msg, size_t msg_len);

#endif /* _COMMAND_CONN_H_ */
