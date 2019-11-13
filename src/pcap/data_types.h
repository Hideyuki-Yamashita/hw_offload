/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Nippon Telegraph and Telephone Corporation
 */

#ifndef __SPP_PCAP_DATA_TYPES_H__
#define __SPP_PCAP_DATA_TYPES_H__

#include "shared/secondary/spp_worker_th/data_types.h"

/* State on capture */
enum sppwk_capture_status {
	SPP_CAPTURE_IDLE,     /* Idling */
	SPP_CAPTURE_RUNNING   /* Running */
};

/* Manage core status and component information as global variable */
struct spp_pcap_core_mng_info {
	/* Status of cpu core */
	volatile enum sppwk_lcore_status status;
};

#endif  /* __SPP_PCAP_DATA_TYPES_H__ */
