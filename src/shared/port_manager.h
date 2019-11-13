/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015-2016 Intel Corporation
 * Copyright(c) 2019 Nippon Telegraph and Telephone Corporation
 */

#ifndef __SHARED_PORT_MANAGER_H__
#define __SHARED_PORT_MANAGER_H__

#define RTE_LOGTYPE_SHARED RTE_LOGTYPE_USER1

#include "shared/basic_forwarder.h"

/* It is used to convert port name from string type to enum */
struct porttype_map {
	const char     *port_name;
	enum port_type port_type;
};

/* initialize forward array with default value */
void forward_array_init_one(unsigned int i);
void forward_array_init(void);
void forward_array_reset(void);
void forward_array_remove(int port_id);

void port_map_init_one(unsigned int i);
void port_map_init(void);

enum port_type get_port_type(char *portname);

int add_patch(uint16_t in_port, uint16_t out_port);

uint16_t find_port_id(int id, enum port_type type);

int is_valid_port(uint16_t port_id);

#endif  // __SHARED_PORT_MANAGER_H__
