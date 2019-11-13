# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Nippon Telegraph and Telephone Corporation

ifeq ($(RTE_SDK),)
$(error "Please define RTE_SDK environment variable")
endif

# Default target, can be overridden by command line or environment
include $(RTE_SDK)/mk/rte.vars.mk

# binary name
APP = spp_mirror

SPP_SEC_DIR = ../shared/secondary
SPP_WKT_DIR = ../shared/secondary/spp_worker_th

# all source are stored in SRCS-y
SRCS-y := spp_mirror.c mir_cmd_runner.c
SRCS-y += ../shared/common.c
SRCS-y += $(SPP_SEC_DIR)/utils.c $(SPP_SEC_DIR)/add_port.c
SRCS-y += $(SPP_SEC_DIR)/json_helper.c
SRCS-y += $(SPP_SEC_DIR)/string_buffer.c
SRCS-y += $(SPP_SEC_DIR)/common.c
SRCS-y += $(SPP_WKT_DIR)/cmd_utils.c
SRCS-y += $(SPP_WKT_DIR)/port_capability.c
SRCS-y += $(SPP_WKT_DIR)/conn_spp_ctl.c
SRCS-y += $(SPP_WKT_DIR)/cmd_parser.c
SRCS-y += $(SPP_WKT_DIR)/cmd_runner.c
SRCS-y += $(SPP_WKT_DIR)/latency_stats.c
SRCS-y += $(SPP_WKT_DIR)/cmd_res_formatter.c

CFLAGS += -DALLOW_EXPERIMENTAL_API
CFLAGS += $(WERROR_FLAGS) -O3 -MMD
CFLAGS += -I$(SRCDIR)/../
CFLAGS += -DSPP_MIRROR_MODULE

# There are two kinds of copy mode, deep copy and shallow copy. If this
# `DSPP_MIRROR_SHALLOWCOPY` is commented out, spp_mirror runs as in
# deep copy mode.
# Default mode is shallow copy.
CFLAGS += -DSPP_MIRROR_SHALLOWCOPY

# Optional Settings
#CFLAGS += -DSPP_DEMONIZE

# TODO(Hideyuki Yamashita) ringlatencystats cannot run because a fatal
# bug in initialization of this feature. So you should not use it until
# this fatal bug is fixed.
#CFLAGS += -DSPP_RINGLATENCYSTATS_ENABLE

ifeq ($(CONFIG_RTE_BUILD_SHARED_LIB),y)
LDLIBS += -lrte_pmd_ring
LDLIBS += -lrte_pmd_vhost
endif

include $(RTE_SDK)/mk/rte.extapp.mk
