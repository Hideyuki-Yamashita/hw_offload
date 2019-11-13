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

target_name = 'pktgen'


def parse_args():
    parser = argparse.ArgumentParser(
        description="Launcher for pktgen-dpdk application container")

    parser = app_helper.add_eal_args(parser)
    parser = app_helper.add_appc_args(parser)

    parser.add_argument(
        '-s', '--pcap-file',
        type=str,
        help="PCAP packet flow file of port, defined as 'N:filename'")
    parser.add_argument(
        '-f', '--script-file',
        type=str,
        help="Pktgen script (.pkt) to or a Lua script (.lua)")
    parser.add_argument(
        '-lf', '--log-file',
        type=str,
        help="Filename to write a log, as '-l' of pktgen")
    parser.add_argument(
        '-P', '--promiscuous',
        action='store_true',
        help="Enable PROMISCUOUS mode on all ports")
    parser.add_argument(
        '-G', '--sock-default',
        action='store_true',
        help="Enable socket support using default server values of " +
        "localhost:0x5606")
    parser.add_argument(
        '-g', '--sock-address',
        type=str,
        help="Same as -G but with an optional IP address and port number")
    parser.add_argument(
        '-T', '--term-color',
        action='store_true',
        help="Enable color terminal output in VT100")
    parser.add_argument(
        '-N', '--numa',
        action='store_true',
        help="Enable NUMA support")
    parser.add_argument(
        '--matrix',
        type=str,
        help="Matrix of cores and port as '-m' of pktgen, " +
        "such as [1:2].0 or 1.0")

    parser = app_helper.add_sppc_args(parser)
    return parser.parse_args()


def main():
    args = parse_args()

    # Setup for vhost devices with given device IDs.
    dev_ids_list = app_helper.dev_ids_to_list(args.dev_ids)
    sock_files = app_helper.sock_files(dev_ids_list)

    # Setup docker command.
    if args.workdir is not None:
        wd = args.workdir
    else:
        wd = '/root/pktgen-dpdk'
    docker_cmd = ['sudo', 'docker', 'run', '\\']
    docker_opts = app_helper.setup_docker_opts(
            args, target_name, sock_files, wd)

    # Setup pktgen command
    pktgen_cmd = ['pktgen', '\\']

    file_prefix = 'spp-pktgen-container%d' % dev_ids_list[0]
    eal_opts = app_helper.setup_eal_opts(args, file_prefix)

    # Setup matrix for assignment of cores and ports.
    if args.matrix is not None:
        matrix = args.matrix
    else:
        # Get core_list such as [0,1,2] from '-c 0x07' or '-l 0-2'
        core_opt = app_helper.get_core_opt(args)
        core_list = app_helper.cores_to_list(core_opt)
        if len(core_list) < 2:
            print("Error: Two or more cores required!")
            exit()

        slave_core_list = core_list[1:]
        nof_slave_cores = len(slave_core_list)
        nof_ports = len(dev_ids_list)
        nof_cores_each_port = nof_slave_cores / nof_ports
        if nof_cores_each_port < 1:
            print("Error: Too few cores for given port(s)!")
            print("%d cores required for %d port(s)" % (
                (nof_slave_cores + 1), nof_ports))
            exit()

        matrix_list = []
        if nof_cores_each_port == 1:
            for i in range(0, nof_ports):
                matrix_list.append('%d.%d' % (slave_core_list[i], i))
        elif nof_cores_each_port == 2:
            for i in range(0, nof_ports):
                matrix_list.append('[%d:%d].%d' % (
                    slave_core_list[2*i], slave_core_list[2*i + 1], i))
        elif nof_cores_each_port == 3:  # Two cores for rx, one for tx
            for i in range(0, nof_ports):
                matrix_list.append('[%d-%d:%d].%d' % (
                    slave_core_list[3*i],
                    slave_core_list[3*i + 1],
                    slave_core_list[3*i + 2], i))
        elif nof_cores_each_port == 4:
            for i in range(0, nof_ports):
                matrix_list.append('[%d-%d:%d-%d].%d' % (
                    slave_core_list[4*i],
                    slave_core_list[4*i + 1],
                    slave_core_list[4*i + 2],
                    slave_core_list[4*i + 3], i))
        # Do not support more than five because it is rare case and
        # calculation is complex.
        else:
            print("Error: Too many cores for calculation for ports!")
            print("Consider to use '--matrix' for assigning directly")
            exit()
        matrix = ','.join(matrix_list)

    pktgen_opts = ['-m', matrix, '\\']

    if args.pcap_file is not None:
        pktgen_opts += ['-s', args.pcap_file, '\\']

    if args.script_file is not None:
        pktgen_opts += ['-f', args.script_file, '\\']

    if args.log_file is not None:
        pktgen_opts += ['-l', args.log_file, '\\']

    if args.promiscuous is True:
        pktgen_opts += ['-P', '\\']

    if args.sock_default is True:
        pktgen_opts += ['-G', '\\']

    if args.sock_address is not None:
        pktgen_opts += ['-g', args.sock_address, '\\']

    if args.term_color is True:
        pktgen_opts += ['-T', '\\']

    if args.numa is True:
        pktgen_opts += ['-N', '\\']

    cmds = docker_cmd + docker_opts + pktgen_cmd + eal_opts + pktgen_opts
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
