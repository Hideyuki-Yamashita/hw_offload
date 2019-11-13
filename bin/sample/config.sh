# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2019 Nippon Telegraph and Telephone Corporation

SPP_HOST_IP=127.0.0.1
SPP_HUGEPAGES=/dev/hugepages

# spp_primary options
LOGLEVEL=7  # change to 8 if you refer debug messages.
PRI_CORE_LIST=0,1  # required one lcore usually.
PRI_MEM=1024
PRI_MEMCHAN=4  # change for your memory channels.
NUM_RINGS=8
PRI_PORTMASK=0x03  # total num of ports of spp_primary.

# Vdevs of spp_primary
#PRI_VHOST_VDEVS=(11 12)  # IDs of `eth_vhost`
#PRI_RING_VDEVS=(1 2)  # IDs of `net_ring`
#PRI_TAP_VDEVS=(1 2)  # IDs of `net_tap`
# You can give whole of vdev options here.
#PRI_VDEVS=(
#eth_vhost11,iface=/tmp/sock13,queues=1
#eth_vhost12,iface=/tmp/sock14,queues=1
#)

# You do not need to change usually.
# Log files are created in 'spp/log/'.
SPP_CTL_LOG=spp_ctl.log
PRI_LOG=spp_primary.log
