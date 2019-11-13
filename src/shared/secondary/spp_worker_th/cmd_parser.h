/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017-2018 Nippon Telegraph and Telephone Corporation
 */

#ifndef _SPPWK_CMD_PARSER_H_
#define _SPPWK_CMD_PARSER_H_

/**
 * @file cmd_parser.h
 * @brief Define a set of vars and functions for parsing SPP worker commands.
 */

#include "cmd_utils.h"

/* Maximum number of commands per request. */
#define SPPWK_MAX_CMDS 32

/* Maximum number of parameters per command. */
#define SPPWK_MAX_PARAMS 8

/* Size of string buffer of message including null char. */
#define SPPWK_NAME_BUFSZ  32

/* Size of string buffer of detailed message including null char. */
#define SPPWK_VAL_BUFSZ 111

/**
 * Error code for diagnosis and notifying the reason. It starts from 1 because
 * 0 is used for succeeded and not appropriate for error in general.
 */
enum sppwk_parse_error_code {
	SPPWK_PARSE_WRONG_FORMAT = 1,  /**< Wrong format */
	SPPWK_PARSE_UNKNOWN_CMD,  /**< Unknown command */
	SPPWK_PARSE_NO_PARAM,  /**< No parameters */
	SPPWK_PARSE_INVALID_TYPE,  /**< Invalid data type */
	SPPWK_PARSE_INVALID_VALUE,  /**< Invalid value */
};

/**
 * Define actions of SPP worker threads. Each of targeting objects and actions
 * is defined as following.
 *   - compomnent      : start, stop
 *   - port            : add, del
 *   - classifier_table: add, del
 */
enum sppwk_action {
	SPPWK_ACT_NONE,  /**< none */
	SPPWK_ACT_START, /**< start */
	SPPWK_ACT_STOP,  /**< stop */
	SPPWK_ACT_ADD,   /**< add */
	SPPWK_ACT_DEL,   /**< delete */
};

const char *sppwk_action_str(enum sppwk_action wk_action);

/**
 * SPP command type.
 *
 * @attention This enumerated type must have the same order of command_list
 *            defined in cmd_parser.c
 */
/* TODO(yasufum) consider to remove restriction above. */
/**
 * TODO(yasufum) consider to divide because each of target of scope is
 * different and not so understandable for usage. For example, worker is
 * including classifier or it status.
 */
enum sppwk_cmd_type {
	SPPWK_CMDTYPE_CLS_MAC,
	SPPWK_CMDTYPE_CLS_VLAN,
	SPPWK_CMDTYPE_CLIENT_ID,  /**< get_client_id */
	SPPWK_CMDTYPE_STATUS,  /**< status */
	SPPWK_CMDTYPE_EXIT,  /**< exit */
	SPPWK_CMDTYPE_WORKER,  /**< worker thread */
	SPPWK_CMDTYPE_PORT,  /**< port */
};

const char *sppwk_cmd_type_str(enum sppwk_cmd_type ctype);

/* `classifier_table` command specific parameters. */
struct sppwk_cls_cmd_attrs {
	enum sppwk_action wk_action;  /**< add or del */
	enum sppwk_cls_type cls_type;  /**< currently only for MAC. */
	int vid;  /**< VLAN ID  */
	char mac[SPPWK_VAL_BUFSZ];  /**< MAC address  */
	struct sppwk_port_idx port;/**< Destination port type and number */
};

/* `flush` command specific parameters. */
struct sppwk_cmd_flush {
	/* Take no params. */
};

/* `component` command parameters. */
struct sppwk_cmd_comp {
	enum sppwk_action wk_action;  /**< start or stop */
	char name[SPPWK_NAME_BUFSZ];  /**< component name */
	unsigned int core;  /**< logical core number */
	enum sppwk_worker_type wk_type;  /**< worker thread type */
};

/* `port` command parameters. */
struct sppwk_cmd_port {
	enum sppwk_action wk_action;  /**< add or del */
	struct sppwk_port_idx port;   /**< port type and number */
	enum sppwk_port_dir dir;  /**< Direction of RX, TX or both. */
	char name[SPPWK_NAME_BUFSZ];  /**<  component name */
	struct sppwk_port_attrs port_attrs;  /**< port attrs for spp_vf. */
};

/* TODO(yasufum) Add usage and desc for members. What's command descriptors? */
struct sppwk_cmd_attrs {
	enum sppwk_cmd_type type; /**< command type */

	union {  /**< command descriptors */
		struct sppwk_cls_cmd_attrs cls_table;
		struct sppwk_cmd_flush flush;
		struct sppwk_cmd_comp comp;
		struct sppwk_cmd_port port;
	} spec;  /* TODO(yasufum) rename no reasonable name */
};

/* Request parameters. */
struct sppwk_cmd_req {
	int nof_cmds;  /**< Number of accepted commands */
	int nof_valid_cmds;  /**< Number of executed commands */
	struct sppwk_cmd_attrs commands[SPPWK_MAX_CMDS];  /**< list of cmds */

	int is_requested_client_id;
	int is_requested_status;
	int is_requested_exit;
};

/* Error message if parse failed. */
struct sppwk_parse_err_msg {
	int code;  /**< Code in enu sppwk_parse_error_code */
	char msg[SPPWK_NAME_BUFSZ];   /**< Message in short */
	char details[SPPWK_VAL_BUFSZ];  /**< Detailed message */
};

/**
 * Parse request of non null terminated string.
 *
 * @param request
 *  The pointer to struct sppwk_cmd_req.@n
 *  The result value of decoding the command message.
 * @param request_str
 *  The pointer to requested command message.
 * @param request_str_len
 *  The length of requested command message.
 * @param wk_err_msg
 *  The pointer to struct sppwk_parse_err_msg.@n
 *  Detailed error information will be stored.
 *
 * @retval SPPWK_RET_OK succeeded.
 * @retval !0 failed.
 */
int sppwk_parse_req(struct sppwk_cmd_req *request,
		const char *request_str, size_t request_str_len,
		struct sppwk_parse_err_msg *wk_err_msg);

#endif /* _SPPWK_CMD_PARSER_H_ */
