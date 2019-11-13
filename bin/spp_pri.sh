# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2019 Nippon Telegraph and Telephone Corporation

# Activate for debugging
#set -x

SPP_PRI_VHOST=""
SPP_PRI_RING=""
SPP_PRI_TAP=""
SPP_PRI_VDEVS=""

function clean_sock_files() {
    # clean /tmp/sock*
    sudo rm -f /tmp/sock*
}

# Add vhost vdevs named as such as `eth_vhost0`.
function setup_vhost_vdevs() {
    if [ ${PRI_VHOST_VDEVS} ]; then
        for id in ${PRI_VHOST_VDEVS[@]}; do
            SPP_SOCK="/tmp/sock${id}"
            SPP_PRI_VHOST="${SPP_PRI_VHOST} --vdev eth_vhost${id},iface=${SPP_SOCK}"
        done
    fi
}

# Add ring vdevs named as such as `net_ring0`.
function setup_ring_vdevs() {
    if [ ${PRI_RING_VDEVS} ]; then
        for id in ${PRI_RING_VDEVS[@]}; do
            SPP_PRI_RING="${SPP_PRI_RING} --vdev net_ring${id}"
        done
    fi
}

# Add tap vdevs named as such as `net_tap0`.
function setup_tap_vdevs() {
    if [ ${PRI_TAP_VDEVS} ]; then
        for id in ${PRI_TAP_VDEVS[@]}; do
            SPP_PRI_TAP="${SPP_PRI_TAP} --vdev net_tap${id},iface=vtap${id}"
        done
    fi
}

# Add any of vdevs.
function setup_vdevs() {
    if [ ${PRI_VDEVS} ]; then
        for vdev in ${PRI_VDEVS[@]}; do
            SPP_PRI_VDEVS="${SPP_PRI_VDEVS} --vdev ${vdev}"
        done
    fi
}

# Launch spp_primary.
function spp_pri() {
    SPP_PRI_BIN=${SPP_DIR}/src/primary/${RTE_TARGET}/spp_primary

    cmd="sudo ${SPP_PRI_BIN} \
        -l ${PRI_CORE_LIST} \
        -n ${PRI_MEMCHAN} \
        --socket-mem ${PRI_MEM} \
        --huge-dir ${SPP_HUGEPAGES} \
        --proc-type primary \
        --base-virtaddr 0x100000000 \
        --log-level ${LOGLEVEL} \
        ${SPP_PRI_VHOST} \
        ${SPP_PRI_RING} \
        ${SPP_PRI_TAP} \
        ${SPP_PRI_VDEVS} \
        -- \
        -p ${PRI_PORTMASK} \
        -n ${NUM_RINGS} \
        -s ${SPP_HOST_IP}:5555"

    if [ ${DRY_RUN} ]; then
        echo ${cmd}
    else
        ${cmd} > ${SPP_DIR}/log/${PRI_LOG} 2>&1 &
    fi
}
