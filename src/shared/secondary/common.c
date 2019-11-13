/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Nippon Telegraph and Telephone Corporation
 */

#include <string.h>
#include <rte_log.h>
#include "common.h"

#define RTE_LOGTYPE_SEC_COMMON RTE_LOGTYPE_USER1

char spp_ctl_ip[IPADDR_LEN] = { 0 };  /* IP address of spp_ctl. */
int spp_ctl_port = -1;  /* Port num to connect spp_ctl. */

/* Get IP address of spp_ctl as string. */
int get_spp_ctl_ip(char *s_ip)
{
	if (spp_ctl_ip == NULL) {
		RTE_LOG(ERR, SEC_COMMON,
				"IP addr of spp_ctl not initialized.\n");
		return -1;
	}
	sprintf(s_ip, "%s", spp_ctl_ip);
	return 0;
}

/* Set IP address of spp_ctl. */
int set_spp_ctl_ip(const char *s_ip)
{
	memset(spp_ctl_ip, 0x00, sizeof(spp_ctl_ip));
	sprintf(spp_ctl_ip, "%s", s_ip);
	if (spp_ctl_ip == NULL) {
		RTE_LOG(ERR, SEC_COMMON, "Failed to set IP of spp_ctl.\n");
		return -1;
	}
	return 0;
}

/* Get port number for connecting to spp_ctl as string. */
int get_spp_ctl_port(void)
{
	if (spp_ctl_port < 0) {
		RTE_LOG(ERR, SEC_COMMON, "Server port is not initialized.\n");
		return -1;
	}
	return spp_ctl_port;
}

/* Set port number for connecting to spp_ctl. */
int set_spp_ctl_port(int s_port)
{
	if (s_port < 0) {
		RTE_LOG(ERR, SEC_COMMON, "Given invalid port number '%d'.\n",
				s_port);
		return -1;
	}
	spp_ctl_port = s_port;
	return 0;
}
