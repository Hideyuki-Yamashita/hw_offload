#!/usr/bin/env python
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Nippon Telegraph and Telephone Corporation


HOMEDIR = '/root'
RTE_SDK = '/root/dpdk'
RTE_TARGET = 'x86_64-native-linuxapp-gcc'

CONTAINER_IMG_NAME = {
    'dpdk': 'sppc/dpdk',
    'pktgen': 'sppc/pktgen',
    'spp': 'sppc/spp',
    'suricata': 'sppc/suricata',
    }
