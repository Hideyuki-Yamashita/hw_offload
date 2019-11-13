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
        description="Launcher for l2fwd application container")

    parser = app_helper.add_eal_args(parser)
    parser = app_helper.add_appc_args(parser)

    # Application specific args
    parser.add_argument(
        '-p', '--port-mask',
        type=str,
        help="Port mask")

    parser = app_helper.add_sppc_args(parser)
    return parser.parse_args()


def main():
    args = parse_args()

    # Check for other mandatory opitons.
    if args.port_mask is None:
        common.error_exit('--port-mask')
    if args.dev_ids is None:
        common.error_exit('--dev-ids')

    # Setup for vhost devices with given device IDs.
    dev_ids_list = app_helper.dev_ids_to_list(args.dev_ids)
    sock_files = app_helper.sock_files(dev_ids_list)

    # Setup docker command.
    docker_cmd = ['sudo', 'docker', 'run', '\\']
    docker_opts = app_helper.setup_docker_opts(
        args, target_name, sock_files)

    # Check if the number of ports is even for l2fwd.
    nof_ports = app_helper.count_ports(args.port_mask)
    if (nof_ports % 2) != 0:
        print("Error: Number of ports must be an even number!")
        exit()

    # Setup l2fwd command run on container.
    cmd_path = '%s/examples/l2fwd/%s/l2fwd' % (
        env.RTE_SDK, env.RTE_TARGET)

    l2fwd_cmd = [cmd_path, '\\']

    file_prefix = 'spp-l2fwd-container%d' % dev_ids_list[0]
    eal_opts = app_helper.setup_eal_opts(args, file_prefix)

    l2fwd_opts = ['-p', args.port_mask, '\\']

    # Parse vhost device IDs and Check the number of devices is
    # sufficient for port mask.
    if app_helper.is_sufficient_dev_ids(
            args.dev_ids, args.port_mask) is not True:
        print("Error: Cannot reserve ports '%s (= 0b%s)' on '%s'." % (
            args.port_mask,
            format(int(args.port_mask, 16), 'b'),
            args.dev_ids))
        exit()

    cmds = docker_cmd + docker_opts + l2fwd_cmd + eal_opts + l2fwd_opts
    if cmds[-1] == '\\':
        cmds.pop()
    common.print_pretty_commands(cmds)

    if args.dry_run is True:
        exit()

    # Remove delimiters for print_pretty_commands().
    while '\\' in cmds:
        cmds.remove('\\')
    subprocess.call(cmds)


if __name__ == '__main__':
    main()
