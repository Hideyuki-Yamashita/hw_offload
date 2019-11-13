/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Nippon Telegraph and Telephone Corporation
 */

#ifndef _SHARED_SECONDARY_UTILS_H_
#define _SHARED_SECONDARY_UTILS_H_

int parse_resource_uid(char *str, char **port_type, int *port_id);

int spp_atoi(const char *str, int *val);

/**
 * Set client ID from given command argment.
 *
 * @params[in] cid Client ID.
 * @return 0 if succeeded, or -1 if failed.
 */
int set_client_id(int cid);

/**
 * Get client ID.
 *
 * @return int value of client ID if succeeded, or -1 if failed.
 */
int get_client_id(void);

/**
 * Set vhost client mode from given command argument.
 *
 * @params[in] vhost_cli Enabled if 1, or disabled if 0.
 * @return 0 if succeeded, or -1 if failed.
 */
int set_vhost_cli_mode(int vhost_cli);

/**
 * Get vhost client mode.
 *
 * @return 1 if vhost client is enabled, or 0 if disabled.
 */
int get_vhost_cli_mode(void);

/**
 * Parse client ID from given value of string.
 *
 * @params[in] cli_id_str String value of client ID.
 * @params[in,out] cli_id client ID of int value.
 * @return 0 if succeeded, or -1 if failed.
 */
int parse_client_id(int *cli_id, const char *cli_id_str);

/**
 * Attach a new Ethernet device specified by arguments.
 *
 * @param devargs
 *  A pointer to a strings array describing the new device
 *  to be attached. The strings should be a pci address like
 *  '0000:01:00.0' or virtual device name like 'net_pcap0'.
 * @param port_id
 *  A pointer to a port identifier actually attached.
 * @return
 *  0 on success and port_id is filled, negative on error
 */
int
dev_attach_by_devargs(const char *devargs, uint16_t *port_id);

/**
 * Detach a Ethernet device specified by port identifier.
 * This function must be called when the device is in the
 * closed state.
 *
 * @param port_id
 *   The port identifier of the device to detach.
 * @return
 *  0 on success and devname is filled, negative on error
 */
int dev_detach_by_port_id(uint16_t port_id);

#endif
