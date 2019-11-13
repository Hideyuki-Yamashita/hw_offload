#!/usr/bin/env python
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Nippon Telegraph and Telephone Corporation

import argparse
import os
import subprocess
import sys

work_dir = os.path.dirname(__file__)
sys.path.append(work_dir + '/..')
from conf import env
from lib import app_helper
from lib import common

target_name = 'dpdk'


def parse_args():
    parser = argparse.ArgumentParser(
        description="Launcher for load-balancer application container")

    parser = app_helper.add_eal_args(parser)
    parser = app_helper.add_appc_args(parser)

    # Application specific args
    parser.add_argument(
        '-rx', '--rx-ports',
        type=str,
        help="List of rx ports and queues handled by the I/O rx lcores")
    parser.add_argument(
        '-tx', '--tx-ports',
        type=str,
        help="List of tx ports and queues handled by the I/O tx lcores")
    parser.add_argument(
        '-w', '--worker-lcores',
        type=str,
        help="List of worker lcores")
    parser.add_argument(
        '-rsz', '--ring-sizes',
        type=str,
        help="Ring sizes of 'rx_read,rx_send,w_send,tx_written'")
    parser.add_argument(
        '-bsz', '--burst-sizes',
        type=str,
        help="Burst sizes of rx, worker or tx")
    parser.add_argument(
        '--lpm',
        type=str,
        help="List of LPM rules")
    parser.add_argument(
        '--pos-lb',
        type=int,
        help="Position of the 1-byte field used for identify worker")

    parser = app_helper.add_sppc_args(parser)
    return parser.parse_args()


def main():
    args = parse_args()

    # Check for other mandatory opitons.
    if args.dev_ids is None:
        common.error_exit('--dev-ids')

    # Setup for vhost devices with given device IDs.
    dev_ids_list = app_helper.dev_ids_to_list(args.dev_ids)
    sock_files = app_helper.sock_files(dev_ids_list)

    # Setup docker command.
    docker_cmd = ['sudo', 'docker', 'run', '\\']
    docker_opts = app_helper.setup_docker_opts(
        args, target_name, sock_files)

    app_name = 'load_balancer'
    cmd_path = '%s/examples/%s/%s/%s' % (
        env.RTE_SDK, app_name, env.RTE_TARGET, app_name)

    # Setup testpmd command.
    lb_cmd = [cmd_path, '\\']

    file_prefix = 'spp-lb-container%d' % dev_ids_list[0]
    eal_opts = app_helper.setup_eal_opts(args, file_prefix)

    lb_opts = []

    # Check for other mandatory opitons.
    if args.rx_ports is None:
        common.error_exit('--rx-ports')
    else:
        rx_ports = '"%s"' % args.rx_ports
        lb_opts += ['--rx', rx_ports, '\\']

    if args.tx_ports is None:
        common.error_exit('--tx-ports')
    else:
        tx_ports = '"%s"' % args.tx_ports
        lb_opts += ['--tx', tx_ports, '\\']

    if args.worker_lcores is None:
        common.error_exit('--worker-lcores')
    else:
        worker_lcores = '%s' % args.worker_lcores
        lb_opts += ['--w', worker_lcores, '\\']

    if args.lpm is None:
        common.error_exit('--lpm')
    else:
        lpm = '"%s"' % args.lpm
        lb_opts += ['--lpm', lpm, '\\']

    # Check optional opitons.
    if args.ring_sizes is not None:
        lb_opts += [
            '--ring-sizes', args.ring_sizes, '\\']
    if args.burst_sizes is not None:
        lb_opts += [
            '--burst-sizes', args.burst_sizes, '\\']
    if args.pos_lb is not None:
        lb_opts += [
            '--pos-lb', str(args.pos_lb)]

    cmds = docker_cmd + docker_opts + lb_cmd + eal_opts + lb_opts
    if cmds[-1] == '\\':
        cmds.pop()
    common.print_pretty_commands(cmds)

    if args.dry_run is True:
        exit()

    # Remove delimiters for print_pretty_commands().
    while '\\' in cmds:
        cmds.remove('\\')
    # Call with shell=True because parsing '--w' option is failed
    # without it.
    subprocess.call(' '.join(cmds), shell=True)


if __name__ == '__main__':
    main()
