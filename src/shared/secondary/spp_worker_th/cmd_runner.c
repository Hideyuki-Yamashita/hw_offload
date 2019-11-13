/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017-2019 Nippon Telegraph and Telephone Corporation
 */

#include <unistd.h>
#include <string.h>

#include <rte_log.h>
#include <rte_branch_prediction.h>

#include "cmd_runner.h"
#include "cmd_res_formatter.h"
#include "conn_spp_ctl.h"
#include "cmd_parser.h"
#include "shared/secondary/string_buffer.h"
#include "shared/secondary/json_helper.h"

#ifdef SPP_VF_MODULE
#include "vf_deps.h"
#endif

#ifdef SPP_MIRROR_MODULE
#include "mirror_deps.h"
#endif

#define RTE_LOGTYPE_WK_CMD_RUNNER RTE_LOGTYPE_USER1

/* request message initial size */
#define CMD_REQ_BUF_INIT_SIZE 2048

enum cmd_res_code {
	CMD_SUCCESS = 0,
	CMD_FAILED,
	CMD_INVALID,
};

/* Activate temporarily stored command. */
int
flush_cmd(void)
{
	int ret;
	int *p_change_comp;
	struct sppwk_comp_info *p_comp_info;
	struct cancel_backup_info *backup_info;

	sppwk_get_mng_data(NULL, &p_comp_info, NULL, NULL, &p_change_comp,
			&backup_info);

	ret = update_port_info();
	if (ret < SPPWK_RET_OK)
		return ret;

	/* TODO(yasufum) confirm why no returned value. */
	update_lcore_info();

	/* TODO(yasufum) confirm why no checking for returned value. */
	ret = update_comp_info(p_comp_info, p_change_comp);

	backup_mng_info(backup_info);
	return ret;
}

/* Get error message of parsing from given wk_err_msg object. */
static const char *
get_parse_err_msg(
		const struct sppwk_parse_err_msg *wk_err_msg,
		char *message)
{
	switch (wk_err_msg->code) {
	case SPPWK_PARSE_WRONG_FORMAT:
		sprintf(message, "Wrong message format");
		break;

	case SPPWK_PARSE_UNKNOWN_CMD:
		/* TODO(yasufum) Fix compile err if space exists before "(" */
		sprintf(message, "Unknown command(%s)", wk_err_msg->details);
		break;

	case SPPWK_PARSE_NO_PARAM:
		sprintf(message, "No or insufficient number of params (%s)",
				wk_err_msg->msg);
		break;

	case SPPWK_PARSE_INVALID_TYPE:
		sprintf(message, "Invalid value type (%s)",
				wk_err_msg->msg);
		break;

	case SPPWK_PARSE_INVALID_VALUE:
		sprintf(message, "Invalid value (%s)", wk_err_msg->msg);
		break;

	default:
		sprintf(message, "Failed to parse with unexpected reason");
		break;
	}

	return message;
}

/* Setup cmd_result with given code and message. */
static inline void
set_cmd_result(struct cmd_result *cmd_res,
		int code, const char *error_messege)
{
	cmd_res->code = code;
	/**
	 * TODO(yasufum) confirm these string "success", "error" or "invalid"
	 * should be fixed or not because this no meaning short message is
	 * obvious from code and nouse actually.
	 */
	switch (code) {
	case CMD_SUCCESS:
		strcpy(cmd_res->result, "success");
		memset(cmd_res->err_msg, 0x00, CMD_ERR_MSG_LEN);
		break;
	case CMD_FAILED:
		strcpy(cmd_res->result, "error");
		strcpy(cmd_res->err_msg, error_messege);
		break;
	case CMD_INVALID:
	default:
		strcpy(cmd_res->result, "invalid");
		memset(cmd_res->err_msg, 0x00, CMD_ERR_MSG_LEN);
		break;
	}
}

/* Setup error message of parsing for requested command. */
static void
prepare_parse_err_msg(struct cmd_result *results,
		const struct sppwk_cmd_req *request,
		const struct sppwk_parse_err_msg *wk_err_msg)
{
	int i;
	const char *tmp_buff;
	char error_messege[CMD_ERR_MSG_LEN];

	for (i = 0; i < request->nof_cmds; i++) {
		if (wk_err_msg->code == 0)
			set_cmd_result(&results[i], CMD_SUCCESS, "");
		else
			set_cmd_result(&results[i], CMD_INVALID, "");
	}

	if (wk_err_msg->code != 0) {
		tmp_buff = get_parse_err_msg(wk_err_msg, error_messege);
		set_cmd_result(&results[request->nof_valid_cmds],
				CMD_FAILED, tmp_buff);
	}
}

/* send response for decode error */
static void
send_decode_error_response(int *sock,
		const struct sppwk_cmd_req *request,
		struct cmd_result *cmd_results)
{
	int ret = SPPWK_RET_NG;
	char *msg, *tmp_buff;
	tmp_buff = spp_strbuf_allocate(CMD_RES_BUF_INIT_SIZE);
	if (unlikely(tmp_buff == NULL)) {
		/* TODO(yasufum) refactor no meaning err msg */
		RTE_LOG(ERR, WK_CMD_RUNNER, "allocate error. "
				"(name = decode_error_response)\n");
		return;
	}

	/* create & append result array */
	ret = append_command_results_value("results", &tmp_buff,
			request->nof_cmds, cmd_results);
	if (unlikely(ret < SPPWK_RET_OK)) {
		spp_strbuf_free(tmp_buff);
		RTE_LOG(ERR, WK_CMD_RUNNER,
				"Failed to make command result response.\n");
		return;
	}

	msg = spp_strbuf_allocate(CMD_RES_BUF_INIT_SIZE);
	if (unlikely(msg == NULL)) {
		spp_strbuf_free(tmp_buff);
		/* TODO(yasufum) refactor no meaning err msg */
		RTE_LOG(ERR, WK_CMD_RUNNER, "allocate error. "
				"(name = decode_error_response)\n");
		return;
	}
	ret = append_json_block_brackets(&msg, "", tmp_buff);
	spp_strbuf_free(tmp_buff);
	if (unlikely(ret < SPPWK_RET_OK)) {
		spp_strbuf_free(msg);
		RTE_LOG(ERR, WK_CMD_RUNNER,
				/* TODO(yasufum) refactor no meaning err msg */
				"allocate error. (name = result_response)\n");
		return;
	}

	RTE_LOG(DEBUG, WK_CMD_RUNNER,
			"Make command response (decode error). "
			"response_str=\n%s\n", msg);

	/* send response to requester */
	ret = send_ctl_msg(sock, msg, strlen(msg));
	if (unlikely(ret != SPPWK_RET_OK)) {
		RTE_LOG(ERR, WK_CMD_RUNNER,
				"Failed to send decode error response.\n");
		/* not return */
	}

	spp_strbuf_free(msg);
}

/* Send the result of command to spp-ctl. */
static void
send_result_spp_ctl(int *sock,
		const struct sppwk_cmd_req *request,
		struct cmd_result *cmd_results)
{
	int ret = SPPWK_RET_NG;
	char *msg, *tmp_buff;
	tmp_buff = spp_strbuf_allocate(CMD_RES_BUF_INIT_SIZE);
	if (unlikely(tmp_buff == NULL)) {
		RTE_LOG(ERR, WK_CMD_RUNNER,
				/* TODO(yasufum) refactor no meaning err msg */
				"allocate error. (name = result_response)\n");
		return;
	}

	/* create & append result array */
	ret = append_command_results_value("results", &tmp_buff,
			request->nof_cmds, cmd_results);
	if (unlikely(ret < SPPWK_RET_OK)) {
		spp_strbuf_free(tmp_buff);
		RTE_LOG(ERR, WK_CMD_RUNNER,
				"Failed to make command result response.\n");
		return;
	}

	/* append client id information value */
	if (request->is_requested_client_id) {
		ret = add_client_id("client_id", &tmp_buff, NULL);
		if (unlikely(ret < SPPWK_RET_OK)) {
			spp_strbuf_free(tmp_buff);
			RTE_LOG(ERR, WK_CMD_RUNNER, "Failed to make "
					"client id response.\n");
			return;
		}
		ret = append_process_type_value("process_type",
							&tmp_buff, NULL);
	}

	/* append info value */
	if (request->is_requested_status) {
		ret = append_info_value("info", &tmp_buff);
		if (unlikely(ret < SPPWK_RET_OK)) {
			spp_strbuf_free(tmp_buff);
			RTE_LOG(ERR, WK_CMD_RUNNER,
					"Failed to make status response.\n");
			return;
		}
	}

	msg = spp_strbuf_allocate(CMD_RES_BUF_INIT_SIZE);
	if (unlikely(msg == NULL)) {
		spp_strbuf_free(tmp_buff);
		RTE_LOG(ERR, WK_CMD_RUNNER,
				/* TODO(yasufum) refactor no meaning err msg */
				"allocate error. (name = result_response)\n");
		return;
	}
	ret = append_json_block_brackets(&msg, "", tmp_buff);
	spp_strbuf_free(tmp_buff);
	if (unlikely(ret < SPPWK_RET_OK)) {
		spp_strbuf_free(msg);
		RTE_LOG(ERR, WK_CMD_RUNNER,
				/* TODO(yasufum) refactor no meaning err msg */
				"allocate error. (name = result_response)\n");
		return;
	}

	RTE_LOG(DEBUG, WK_CMD_RUNNER,
			"Make command response (command result). "
			"response_str=\n%s\n", msg);

	/* send response to requester */
	ret = send_ctl_msg(sock, msg, strlen(msg));
	if (unlikely(ret != SPPWK_RET_OK)) {
		RTE_LOG(ERR, WK_CMD_RUNNER,
			"Failed to send command result response.\n");
		/* not return */
	}

	spp_strbuf_free(msg);
}

/* Execute series of commands. */
static int
exec_cmds(int *sock, const char *req_str, size_t req_str_len)
{
	int ret = SPPWK_RET_NG;
	int i;

	struct sppwk_cmd_req cmd_req;
	struct sppwk_parse_err_msg wk_err_msg;
	struct cmd_result cmd_results[SPPWK_MAX_CMDS];

	memset(&cmd_req, 0, sizeof(struct sppwk_cmd_req));
	memset(&wk_err_msg, 0, sizeof(struct sppwk_parse_err_msg));
	memset(cmd_results, 0, sizeof(cmd_results));

	/* Parse request message. */
	RTE_LOG(DEBUG, WK_CMD_RUNNER, "Parse cmds, %.*s\n",
			(int)req_str_len, req_str);
	ret = sppwk_parse_req(&cmd_req, req_str, req_str_len, &wk_err_msg);

	if (unlikely(ret != SPPWK_RET_OK)) {
		/* Setup and send error response. */
		prepare_parse_err_msg(cmd_results, &cmd_req, &wk_err_msg);
		send_decode_error_response(sock, &cmd_req, cmd_results);
		RTE_LOG(DEBUG, WK_CMD_RUNNER, "Failed to parse cmds.\n");
		return SPPWK_RET_OK;
	}

	RTE_LOG(DEBUG, WK_CMD_RUNNER,
			"Num of cmds is %d, and valid cmds is %d\n",
			cmd_req.nof_cmds, cmd_req.nof_valid_cmds);

	/* execute commands */
	for (i = 0; i < cmd_req.nof_cmds; ++i) {
		ret = exec_one_cmd(cmd_req.commands + i);
		if (unlikely(ret != SPPWK_RET_OK)) {
			set_cmd_result(&cmd_results[i], CMD_FAILED,
					"error occur");
			/* Does not execute remaining commands */
			for (++i; i < cmd_req.nof_cmds; ++i)
				set_cmd_result(&cmd_results[i],
					CMD_INVALID, "");
			break;
		}

		set_cmd_result(&cmd_results[i], CMD_SUCCESS, "");
	}

	/* Exec exit command. */
	if (cmd_req.is_requested_exit) {
		set_cmd_result(&cmd_results[0], CMD_SUCCESS, "");
		send_result_spp_ctl(sock, &cmd_req, cmd_results);
		RTE_LOG(INFO, WK_CMD_RUNNER,
				"Process is terminated with exit cmd.\n");
		return SPPWK_RET_NG;
	}

	/* Send response to spp-ctl. */
	send_result_spp_ctl(sock, &cmd_req, cmd_results);

	RTE_LOG(DEBUG, WK_CMD_RUNNER, "End command request processing.\n");

	return SPPWK_RET_OK;
}

/* Setup connection for accepting commands from spp-ctl. */
int
sppwk_cmd_runner_conn(const char *ctl_ipaddr, int ctl_port)
{
	return conn_spp_ctl_init(ctl_ipaddr, ctl_port);
}

/* Run command sent from spp-ctl. */
int
sppwk_run_cmd(void)
{
	int ret;
	int msg_ret;

	static int sock = -1;
	static char *msgbuf;

	if (unlikely(msgbuf == NULL)) {
		msgbuf = spp_strbuf_allocate(CMD_REQ_BUF_INIT_SIZE);
		if (unlikely(msgbuf == NULL)) {
			RTE_LOG(ERR, WK_CMD_RUNNER,
					"Cannot allocate memory "
					"for receive data(init).\n");
			return SPPWK_RET_NG;
		}
	}

	ret = conn_spp_ctl(&sock);

	if (unlikely(ret != SPPWK_RET_OK))
		return SPPWK_RET_OK;

	msg_ret = recv_ctl_msg(&sock, &msgbuf);
	if (unlikely(msg_ret <= 0)) {
		if (likely(msg_ret == 0))
			return SPPWK_RET_OK;
		else if (unlikely(msg_ret == SPP_CONNERR_TEMPORARY))
			return SPPWK_RET_OK;
		else
			return SPPWK_RET_NG;
	}

	ret = exec_cmds(&sock, msgbuf, msg_ret);
	spp_strbuf_remove_front(msgbuf, msg_ret);

	return ret;
}

/* Delete component information */
int
del_comp_info(int lcore_id, int nof_comps, int *comp_ary)
{
	int idx = 0;  /* The index of comp_ary to be deleted. */
	int cnt;

	/* Find the index. */
	for (cnt = 0; cnt < nof_comps; cnt++) {
		if (lcore_id == comp_ary[cnt])
			idx = cnt;
	}
	if (idx < 0)
		return SPPWK_RET_NG;

	/* Overwrite the deleted entry, and shift the remained. */
	nof_comps--;
	for (cnt = idx; cnt < nof_comps; cnt++)
		comp_ary[cnt] = comp_ary[cnt + 1];

	/* Clean the unused last entry. */
	comp_ary[cnt] = 0;

	return SPPWK_RET_OK;
}
