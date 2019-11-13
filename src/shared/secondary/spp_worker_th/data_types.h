/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Nippon Telegraph and Telephone Corporation
 */

#ifndef __SPPWK_DATA_TYPES_H__
#define __SPPWK_DATA_TYPES_H__

#include "shared/common.h"

#define STR_LEN_SHORT 32  /* Size of short string. */
#define STR_LEN_NAME 128  /* Size of string for names. */

/** Identifier string for each interface */
#define SPPWK_PHY_STR "phy"
#define SPPWK_VHOST_STR "vhost"
#define SPPWK_RING_STR "ring"

/* TODO(yasufum) confirm usage of this value and why it is 4. */
#define PORT_CAPABL_MAX 4  /* Max num of port abilities. */

/* Status of a component on lcore. */
enum sppwk_lcore_status {
	SPPWK_LCORE_UNUSED,
	SPPWK_LCORE_STOPPED,
	SPPWK_LCORE_IDLING,
	SPPWK_LCORE_RUNNING,
	SPPWK_LCORE_REQ_STOP  /**< Request for stopping. */
};

/* Direction of RX or TX on a port. */
enum sppwk_port_dir {
	SPPWK_PORT_DIR_NONE,  /**< None */
	SPPWK_PORT_DIR_RX,    /**< RX port */
	SPPWK_PORT_DIR_TX,    /**< TX port */
	SPPWK_PORT_DIR_BOTH,  /**< Both of RX and TX */
};

/**
 * Port ability operation which indicates vlan tag operation on the port
 * (e.g. add vlan tag or delete vlan tag)
 */
enum sppwk_port_ops {
	SPPWK_PORT_OPS_NONE,
	SPPWK_PORT_OPS_ADD_VLAN,  /* Add vlan tag. */
	SPPWK_PORT_OPS_DEL_VLAN,  /* Delete vlan tag. */
};

/** VLAN tag information */
struct sppwk_vlan_tag {
	int vid; /**< VLAN ID */
	int pcp; /**< Priority Code Point */
	int tci; /**< Tag Control Information */
};

/* Ability for vlantag for a port. */
union sppwk_port_capability {
	/** VLAN tag information */
	struct sppwk_vlan_tag vlantag;
};

/* Port attributes of SPP worker processes. */
struct sppwk_port_attrs {
	enum sppwk_port_ops ops;  /**< Port capability Operations */
	enum sppwk_port_dir dir;  /**< Direction of RX, TX or both */
	union sppwk_port_capability capability;   /**< Port capability */
};

/* Type of SPP worker thread. */
/* TODO(yasufum) it should be separated into each process. */
enum sppwk_worker_type {
	SPPWK_TYPE_NONE,  /**< Not used */
	SPPWK_TYPE_CLS,  /**< Classifier_mac */
	SPPWK_TYPE_MRG,  /**< Merger */
	SPPWK_TYPE_FWD,  /**< Forwarder */
	SPPWK_TYPE_MIR,  /**< Mirror */
};

/* Attributes for classifying. */
struct sppwk_cls_attrs {
	uint64_t mac_addr;  /**< Mac address (binary) */
	char mac_addr_str[STR_LEN_SHORT];  /**< Mac address (text) */
	struct sppwk_vlan_tag vlantag;   /**< VLAN tag information */
};

/**
 * Simply define type and index of resource UID such as phy:0. For detailed
 * attributions, use `sppwk_port_info` which has additional port params.
 */
struct sppwk_port_idx {
	enum port_type iface_type;  /**< phy, vhost or ring. */
	int iface_no;
};

/* Define detailed port params in addition to `sppwk_port_idx`. */
struct sppwk_port_info {
	enum port_type iface_type;  /**< phy, vhost or ring */
	int iface_no;
	int ethdev_port_id;  /**< Consistent ID of ethdev */
	struct sppwk_cls_attrs cls_attrs;
	struct sppwk_port_attrs port_attrs[PORT_CAPABL_MAX];
};

/* Attributes of SPP worker thread named as `component`. */
struct sppwk_comp_info {
	char name[STR_LEN_NAME];  /**< Component name */
	enum sppwk_worker_type wk_type;  /**< Type of worker thread */
	unsigned int lcore_id;
	int comp_id;  /**< Component ID */
	int nof_rx;  /**< The number of rx ports */
	int nof_tx;  /**< The number of tx ports */
	struct sppwk_port_info *rx_ports[RTE_MAX_ETHPORTS]; /**< rx ports */
	struct sppwk_port_info *tx_ports[RTE_MAX_ETHPORTS]; /**< tx ports */
};

/* Manage number of interfaces  and port information as global variable. */
/**
 * TODO(yasufum) confirm why having arrays of types. it seems OK having
 * just one array.
 * TODO(yasufum) spp_pcap does not support vhost currently, consider support
 * or not.
 */
struct iface_info {
	struct sppwk_port_info phy[RTE_MAX_ETHPORTS];
	struct sppwk_port_info vhost[RTE_MAX_ETHPORTS];
	struct sppwk_port_info ring[RTE_MAX_ETHPORTS];
};

struct sppwk_lcore_params;
/**
 * Define func to iterate lcore to list core information for showing status
 * or so, as a member of struct `sppwk_lcore_params`.
 */
typedef int (*sppwk_lcore_proc)(
		struct sppwk_lcore_params *params,
		const unsigned int lcore_id,
		const char *wk_name,  /* Name of worker named as component. */
		const char *wk_type,  /* Type of worker named as component. */
		const int nof_rx,  /* Number of RX ports */
		const struct sppwk_port_idx *rx_ports,
		const int nof_tx,  /* Number of TX ports */
		const struct sppwk_port_idx *tx_ports);

/**
 * iterate core table parameters used to list content of lcore table for.
 * showing status or so.
 */
/* TODO(yasufum) refactor name of func and vars, and comments. */
struct sppwk_lcore_params {
	char *output;  /* Buffer used for output */
	/** The function for creating core information */
	sppwk_lcore_proc lcore_proc;
};

#endif  /* __SPPWK_DATA_TYPES_H__ */
