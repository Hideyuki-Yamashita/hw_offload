/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Nippon Telegraph and Telephone Corporation
 */

#ifndef __SHARED_SECONDARY_COMMON_H__
#define __SHARED_SECONDARY_COMMON_H__

#define IPADDR_LEN 16  /* Length of IP address in string. */

/**
 * Get IP address of spp_ctl as string.
 *
 * @param[in,out] s_ip IP address of spp_ctl.
 * @return 0 if succeeded, or -1 if failed.
 */
int get_spp_ctl_ip(char *s_ip);

/**
 * Set IP address of spp_ctl.
 *
 * @param[in] s_ip IP address of spp_ctl.
 * @return 0 if succeeded, or -1 if failed.
 */
int set_spp_ctl_ip(const char *s_ip);

/**
 * Get port number for connecting to spp_ctl as string.
 *
 * @return Port number, or -1 if failed.
 */
int get_spp_ctl_port(void);

/**
 * Set port number for connecting to spp_ctl.
 *
 * @param[in] s_port Port number for spp_ctl.
 * @return 0 if succeeded, or -1 if failed.
 */
int set_spp_ctl_port(int s_port);

#endif
