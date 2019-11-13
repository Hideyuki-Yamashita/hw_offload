/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Nippon Telegraph and Telephone Corporation
 */

#ifndef _SPPWK_CMD_RES_FORMATTER_H_
#define _SPPWK_CMD_RES_FORMATTER_H_

#include "cmd_utils.h"

#define CMD_RES_LEN  32  /* Size of message including null char. */
#define CMD_RES_TAG_LEN  32
#define CMD_ERR_MSG_LEN 128

#define CMD_RES_BUF_INIT_SIZE 2048
#define CMD_TAG_APPEND_SIZE 16

struct cmd_result {
	int code;  /* Response code. */
	char result[CMD_RES_LEN];  /* Response msg in short. */
	char err_msg[CMD_ERR_MSG_LEN];  /* Used only if cmd is failed. */
};

/**
 * Contains command response and operation func for. It is used as an array of
 * this struct.
 */
struct cmd_res_formatter_ops {
	char tag_name[CMD_RES_TAG_LEN];
	int (*func)(const char *name, char **output, void *tmp);
};

int append_interface_array(char **output, const enum port_type type);

int append_process_type_value(const char *name, char **output,
		void *tmp __attribute__ ((unused)));

int append_vlan_value(char **output, const int ope, const int vid,
		const int pcp);

int append_vlan_block(const char *name, char **output,
		const int port_id, const enum sppwk_port_dir dir);

int append_port_block(char **output, const struct sppwk_port_idx *port,
		const enum sppwk_port_dir dir);

int append_port_array(const char *name, char **output, const int num,
		const struct sppwk_port_idx *ports,
		const enum sppwk_port_dir dir);

int append_core_element_value(struct sppwk_lcore_params *params,
		const unsigned int lcore_id,
		const char *name, const char *type,
		const int num_rx, const struct sppwk_port_idx *rx_ports,
		const int num_tx, const struct sppwk_port_idx *tx_ports);

int append_response_list_value(char **output,
		struct cmd_res_formatter_ops *responses, void *tmp);

int append_command_results_value(const char *name, char **output,
		int num, struct cmd_result *results);

int append_info_value(const char *name, char **output);

/**
 * Operation functions start with prefix `add_` defined in `response_info_list`
 * of struct `cmd_res_formatter_ops` which are for making each of parts of
 * command response.
 */
int add_client_id(const char *name, char **output,
		void *tmp __attribute__ ((unused)));

int add_interface(const char *name, char **output,
		void *tmp __attribute__ ((unused)));

int add_master_lcore(const char *name, char **output,
		void *tmp __attribute__ ((unused)));
#endif
