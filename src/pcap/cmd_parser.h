/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Nippon Telegraph and Telephone Corporation
 */

#ifndef _SPP_PCAP_CMD_PARSER_H_
#define _SPP_PCAP_CMD_PARSER_H_

/**
 * @file
 * SPP pcap command parse
 *
 * Decode and validate the command message string.
 */

#include "cmd_utils.h"

/** max number of command per request */
#define SPPWK_MAX_CMDS 32

/** maximum number of parameters per command */
#define SPPWK_MAX_PARAMS 8

/** command name string buffer size (include null char) */
#define SPPWK_NAME_BUFSZ  32

/** command value string buffer size (include null char) */
#define SPPWK_VAL_BUFSZ 111

/** parse error code */
enum sppwk_parse_error_code {
	/* not use 0, in general 0 is OK */
	SPPWK_PARSE_WRONG_FORMAT = 1,  /**< Wrong format */
	SPPWK_PARSE_UNKNOWN_CMD,  /**< Unknown command */
	SPPWK_PARSE_NO_PARAM,  /**< No parameters */
	SPPWK_PARSE_INVALID_TYPE,  /**< Wrong data type */
	SPPWK_PARSE_INVALID_VALUE,  /**< Wrong value */
};

/**
 * spp command type.
 *
 * @attention This enumerated type must have the same order of command_list
 *            defined in command_dec_pcap.c
 */
/* TODO(yasufum) consider to remove restriction above. */
enum pcap_cmd_type {
	PCAP_CMDTYPE_CLIENT_ID,  /**< get_client_id */
	PCAP_CMDTYPE_STATUS,  /**< status */
	PCAP_CMDTYPE_EXIT,  /**< exit */
	PCAP_CMDTYPE_START,  /**< worker thread */
	PCAP_CMDTYPE_STOP,  /**< port */
};

struct pcap_cmd_attr {
	enum pcap_cmd_type type;
};

/** request parameters */
struct spp_command_request {
	int num_command;                /**< Number of accepted commands */
	int num_valid_command;          /**< Number of executed commands */
	struct pcap_cmd_attr cmd_attrs[SPPWK_MAX_CMDS];
					/**<Information of executed commands */

	int is_requested_client_id;     /**< Id for get_client_id command */
	int is_requested_status;        /**< Id for status command */
	int is_requested_exit;          /**< Id for exit command */
	int is_requested_start;         /**< Id for start command */
	int is_requested_stop;          /**< Id for stop command */
};

/* Error message if parse failed. */
struct sppwk_parse_err_msg {
	int code;  /**< Code in enu sppwk_parse_error_code */
	char msg[SPPWK_NAME_BUFSZ];   /**< Message in short */
	char details[SPPWK_VAL_BUFSZ];  /**< Detailed message */
};

/**
 * parse request from no-null-terminated string
 *
 * @param request
 *  The pointer to struct spp_command_request.@n
 *  The result value of decoding the command message.
 * @param request_str
 *  The pointer to requested command message.
 * @param request_str_len
 *  The length of requested command message.
 * @param error
 *  The pointer to struct sppwk_parse_err_msg.@n
 *  Detailed error information will be stored.
 *
 * @retval SPPWK_RET_OK succeeded.
 * @retval !0 failed.
 */
int sppwk_parse_req(struct spp_command_request *request,
		const char *request_str, size_t request_str_len,
		struct sppwk_parse_err_msg *error);

#endif /* _SPP_PCAP_CMD_PARSER_H_ */
