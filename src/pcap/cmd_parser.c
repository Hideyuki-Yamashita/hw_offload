/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Nippon Telegraph and Telephone Corporation
 */

#include <unistd.h>
#include <string.h>

#include <rte_ether.h>
#include <rte_log.h>
#include <rte_branch_prediction.h>

#include "cmd_parser.h"
#include "shared/secondary/return_codes.h"

#define RTE_LOGTYPE_PCAP_PARSER RTE_LOGTYPE_USER2

/* Format error message object and return error code for an error case. */
/* TODO(yasufum) merge it to the same definition in shared/.../cmd_parser.c */
static inline int
set_parse_error(struct sppwk_parse_err_msg *wk_err_msg,
		const int err_code, const char *err_msg)
{
	wk_err_msg->code = err_code;

	if (likely(err_msg != NULL))
		strcpy(wk_err_msg->msg, err_msg);

	return wk_err_msg->code;
}

/* set parse error */
static inline int
set_string_value_parse_error(struct sppwk_parse_err_msg *wk_err_msg,
		const char *err_details, const char *err_msg)
{
	strcpy(wk_err_msg->details, err_details);
	return set_parse_error(wk_err_msg, SPPWK_PARSE_INVALID_VALUE, err_msg);
}

/* Split tokens of pcap command with delimiter. */
/* TODO(yasufum) consider this func can be moved to shared. */
static int
split_cmd_tokens(char *tokens_str, int max_tokens, int *nof_tokens,
		char *tokens[])
{
	const char *delim = " ";
	char *token = NULL;
	char *t_ptr = NULL;
	int cnt = 0;

	token = strtok_r(tokens_str, delim, &t_ptr);
	while (token != NULL) {
		if (cnt >= max_tokens) {
			RTE_LOG(ERR, PCAP_PARSER,
					"Invalid num of tokens in %s."
					"It should be less than %d.\n",
					tokens_str, max_tokens);
			return SPPWK_RET_NG;
		}
		tokens[cnt] = token;
		cnt++;
		token = strtok_r(NULL, delim, &t_ptr);
	}
	*nof_tokens = cnt;

	return SPPWK_RET_OK;
}

/**
 * A set of attributes of commands for parsing. The fourth member of function
 * pointer is the operator function for the command.
 */
struct pcap_cmd_parse_attr {
	const char *cmd_name;
	int nof_params_min;
	int nof_params_max;
	int (*func)(struct spp_command_request *request, int nof_tokens,
			char *tokens[], struct sppwk_parse_err_msg *error,
			int nof_max_tokens);
				/* Pointer to command handling function */
	enum pcap_cmd_type type;
				/* Command type */
};

/**
 * List of command attributes defines the name of command, number of params
 * and operator functions.
 */
static struct pcap_cmd_parse_attr pcap_cmd_attrs[] = {
	{ "_get_client_id", 1, 1, NULL, PCAP_CMDTYPE_CLIENT_ID },
	{ "status", 1, 1, NULL, PCAP_CMDTYPE_STATUS },
	{ "exit",  1, 1, NULL, PCAP_CMDTYPE_EXIT },
	{ "start", 1, 1, NULL, PCAP_CMDTYPE_START },
	{ "stop",  1, 1, NULL, PCAP_CMDTYPE_STOP },
	{ "", 0, 0, NULL, 0 }  /* termination */
};

/* Parse command requested from spp-ctl. */
static int
parse_pcap_cmd(struct spp_command_request *request,
		const char *cmd_str,
		struct sppwk_parse_err_msg *wk_err_msg)
{
	int is_invalid_cmd = 0;
	struct pcap_cmd_parse_attr *cmd_attr = NULL;
	int ret;
	int i;
	char *tokens[SPPWK_MAX_PARAMS];
	int nof_tokens = 0;
	char tmp_str[SPPWK_MAX_PARAMS * SPPWK_VAL_BUFSZ];
	memset(tokens, 0x00, sizeof(tokens));
	memset(tmp_str, 0x00, sizeof(tmp_str));

	strcpy(tmp_str, cmd_str);
	ret = split_cmd_tokens(tmp_str, SPPWK_MAX_PARAMS,
			&nof_tokens, tokens);
	if (ret < SPPWK_RET_OK) {
		RTE_LOG(ERR, PCAP_PARSER, "Invalid cmd '%s', "
				"num of tokens is over SPPWK_MAX_PARAMS.\n",
				cmd_str);
		return set_parse_error(wk_err_msg,
				SPPWK_PARSE_WRONG_FORMAT, NULL);
	}
	RTE_LOG(DEBUG, PCAP_PARSER, "Parsed cmd '%s', nof token is %d\n",
			cmd_str, nof_tokens);

	for (i = 0; pcap_cmd_attrs[i].cmd_name[0] != '\0'; i++) {
		cmd_attr = &pcap_cmd_attrs[i];
		if (strcmp(tokens[0], cmd_attr->cmd_name) != 0)
			continue;

		if (unlikely(nof_tokens < cmd_attr->nof_params_min) ||
			unlikely(cmd_attr->nof_params_max < nof_tokens)) {
			is_invalid_cmd = 1;
			continue;
		}

		request->cmd_attrs[0].type = pcap_cmd_attrs[i].type;
		if (cmd_attr->func != NULL)
			return (*cmd_attr->func)(
					request, nof_tokens, tokens, wk_err_msg,
					cmd_attr->nof_params_max);

		return SPPWK_RET_OK;
	}

	if (is_invalid_cmd != 0) {
		RTE_LOG(ERR, PCAP_PARSER, "Invalid cmd '%s', "
				"num of params is out of range.\n", cmd_str);
		return set_parse_error(wk_err_msg,
				SPPWK_PARSE_WRONG_FORMAT, NULL);
	}

	RTE_LOG(ERR, PCAP_PARSER,
			"Unknown cmd '%s' in '%s'.\n", tokens[0], cmd_str);
	return set_string_value_parse_error(wk_err_msg, tokens[0], "command");
}

/* Parse request of non null terminated string. */
int
sppwk_parse_req(
		struct spp_command_request *request,
		const char *request_str, size_t request_str_len,
		struct sppwk_parse_err_msg *wk_err_msg)
{
	int ret = SPPWK_RET_NG;
	int i;

	/* parse request */
	request->num_command = 1;
	ret = parse_pcap_cmd(request, request_str, wk_err_msg);
	if (unlikely(ret != SPPWK_RET_OK)) {
		RTE_LOG(ERR, PCAP_PARSER,
				"Cannot parse command request. "
				"ret=%d, request_str=%.*s\n",
				ret, (int)request_str_len, request_str);
		return ret;
	}
	request->num_valid_command = 1;

	/* check getter command */
	for (i = 0; i < request->num_valid_command; ++i) {
		switch (request->cmd_attrs[i].type) {
		case PCAP_CMDTYPE_CLIENT_ID:
			request->is_requested_client_id = 1;
			break;
		case PCAP_CMDTYPE_STATUS:
			request->is_requested_status = 1;
			break;
		case PCAP_CMDTYPE_EXIT:
			request->is_requested_exit = 1;
			break;
		case PCAP_CMDTYPE_START:
			request->is_requested_start = 1;
			break;
		case PCAP_CMDTYPE_STOP:
			request->is_requested_stop = 1;
			break;
		default:
			/* nothing to do */
			break;
		}
	}

	return ret;
}
