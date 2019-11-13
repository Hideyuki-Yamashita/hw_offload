/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015-2016 Intel Corporation
 * Copyright(c) 2019 Nippon Telegraph and Telephone Corporation
 */

#ifndef _PRIMARY_ARGS_H_
#define _PRIMARY_ARGS_H_

#include <stdint.h>
#include "shared/common.h"

extern uint16_t num_rings;
extern char *server_ip;
extern int server_port;

/**
 * Set flg from given argument.
 *
 * @params[in] flg Enabled if 1, or disabled if 0.
 * @return 0 if succeeded, or -1 if failed.
 */
int set_forwarding_flg(int flg);

/**
 * Get forwarding flag.
 *
 * @return 1 if doing forwarding, or 0 if disabled.
 */
int get_forwarding_flg(void);

int parse_portmask(struct port_info *ports, uint16_t max_ports,
		const char *portmask);
int parse_app_args(uint16_t max_ports, int argc, char *argv[]);

#endif /* _PRIMARY_ARGS_H_ */
