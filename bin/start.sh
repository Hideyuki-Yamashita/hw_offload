#!/bin/bash
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2019 Nippon Telegraph and Telephone Corporation

# This script is for launching spp-ctl, spp_primary and SPP CLI.
# You can launch secondaries from SPP CLI by using `pri; launch ...`.

# Activate for debugging
#set -x

WORK_DIR=$(cd $(dirname $0); pwd)
SPP_DIR=${WORK_DIR}/..

DEFAULT_CONFIG=${WORK_DIR}/sample/config.sh
CONFIG=${WORK_DIR}/config.sh
DRY_RUN=

while getopts ":-:" OPT
do
    case ${OPT} in
        "-")
            case ${OPTARG} in
                dry-run)
                    DRY_RUN=true
                    ;;
            esac
            ;;
    esac
done

function start_spp_ctl() {
    cmd="python3 ${SPP_DIR}/src/spp-ctl/spp-ctl -b ${SPP_HOST_IP}"
    if [ ${DRY_RUN} ]; then
        echo ${cmd}
    else
        ${cmd} > ${SPP_DIR}/log/${SPP_CTL_LOG} 2>&1 &
    fi
}

function start_spp_pri() {
    . ${SPP_DIR}/bin/spp_pri.sh
    clean_sock_files  # remove /tmp/sock* as initialization
    setup_vhost_vdevs  # setup vdevs of eth_vhost
    setup_ring_vdevs  # setup vdevs of net_ring
    setup_tap_vdevs  # setup vdevs of net_tap
    setup_vdevs  # setup any of vdevs
    spp_pri  # launch spp_primary
}

if [ ! -f ${CONFIG} ]; then
    echo "Creating config file ..."
    cp ${DEFAULT_CONFIG} ${CONFIG}
    echo "Edit '${CONFIG}' and run this script again!"
    exit
fi

# import vars and functions
. ${CONFIG}

echo "Start spp-ctl"
start_spp_ctl

echo "Start spp_primary"
start_spp_pri

if [ ! ${DRY_RUN} ]; then
    sleep 1  # wait for spp-ctl is ready
    python3 ${SPP_DIR}/src/spp.py -b ${SPP_HOST_IP} --wait-pri
fi
