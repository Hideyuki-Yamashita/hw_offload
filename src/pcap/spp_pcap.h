/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Nippon Telegraph and Telephone Corporation
 */

#ifndef __SPP_PCAP_H__
#define __SPP_PCAP_H__

#include "cmd_utils.h"

/**
 * Pcap get core status
 *
 * @param lcore_id Lcore ID.
 * @param params Pointer to struct sppwk_lcore_params.
 *
 * @retval SPPWK_RET_OK succeeded.
 * @retval SPPWK_RET_NG failed.
 */
/* TODO(yasufum) consider to move spp_pcap.c. */
int spp_pcap_get_core_status(
		unsigned int lcore_id,
		struct sppwk_lcore_params *params);

#endif /* __SPP_PCAP_H__ */
