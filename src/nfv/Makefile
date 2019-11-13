# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2015-2016 Intel Corporation
# Copyright(c) 2019 Nippon Telegraph and Telephone Corporation

ifeq ($(RTE_SDK),)
$(error "Please define RTE_SDK environment variable")
endif

# Default target, can be overriden by command line or environment
include $(RTE_SDK)/mk/rte.vars.mk

# binary name
APP = spp_nfv

# all source are stored in SRCS-y
SRCS-y := main.c nfv_status.c
SRCS-y += ../shared/common.c ../shared/basic_forwarder.c ../shared/port_manager.c
SRCS-y += ../shared/secondary/common.c
SRCS-y += ../shared/secondary/utils.c ../shared/secondary/add_port.c

CFLAGS += -DALLOW_EXPERIMENTAL_API
CFLAGS += $(WERROR_FLAGS) -O3 -MMD
CFLAGS += -I$(SRCDIR)/../

ifeq ($(CONFIG_RTE_BUILD_SHARED_LIB),y)
LDLIBS += -lrte_pmd_ring
LDLIBS += -lrte_pmd_vhost
endif

include $(RTE_SDK)/mk/rte.extapp.mk
