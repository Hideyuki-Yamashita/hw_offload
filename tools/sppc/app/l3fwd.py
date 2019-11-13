#!/usr/bin/env python
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Nippon Telegraph and Telephone Corporation

import argparse
import os
import re
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
        description="Launcher for l3fwd application container")

    parser = app_helper.add_eal_args(parser)
    parser = app_helper.add_appc_args(parser)

    # Application specific args
    parser.add_argument(
        '-p', '--port-mask',
        type=str,
        help='(Mandatory) Port mask')
    parser.add_argument(
        '--config',
        type=str,
        help='(Mandatory) Define set of port, queue, lcore for ports')
    parser.add_argument(
        '-P', '--promiscous',
        action='store_true',
        help='Set all ports to promiscous mode (default is None)')
    parser.add_argument(
        '-E', '--exact-match',
        action='store_true',
        help='Enable exact match (default is None)')
    parser.add_argument(
        '-L', '--longest-prefix-match',
        action='store_true',
        help='Enable longest prefix match (default is None)')
    parser.add_argument(
        '-dst', '--eth-dest',
        nargs='*',
        action='append',
        type=str,
        help='Ethernet dest for port X (X,MM:MM:MM:MM:MM:MM)')
    parser.add_argument(
        '--enable-jumbo',
        action='store_true',
        help='Enable jumbo frames, [--enable-jumbo [--max-pkt-len PKTLEN]]')
    parser.add_argument(
        '--max-pkt-len',
        type=int,
        help='Max packet length (64-9600) if jumbo is enabled.')
    parser.add_argument(
        '--no-numa',
        action='store_true',
        help='Disable NUMA awareness (default is None)')
    parser.add_argument(
        '--hash-entry-num',
        action='store_true',
        help='Specify the hash entry number in hexadecimal (default is None)')
    parser.add_argument(
        '--ipv6',
        action='store_true',
        help='Specify the hash entry number in hexadecimal (default is None)')
    parser.add_argument(
        '--parse-ptype',
        type=str,
        default='ipv4',
        help='Set analyze packet type, ipv4 or ipv6 (default is ipv4)')

    parser = app_helper.add_sppc_args(parser)

    return parser.parse_args()


def check_config_format(config_opt, nof_queues):
    """Check if config format is valid

    Config options is for Determining which queues from which ports
    are mapped to which cores.

      --config (port,queue,lcore)[,(port,queue,lcore)]

    Queue IDs should be started from 0 and sequential for each of
    ports. For example, '(0,1,0),(0,0,1)' is invlid because queue IDs
    of port 0 are '1, 0' and it does started from 1 and not sequential.

    The number of tx queues should be equal or less than the number of
    queues of virtio device defined with '-nq' or '--nof-queues' option.
    """

    config_opt = re.sub(r'\s+', '', config_opt)

    config_opt = config_opt.lstrip('(').rstrip(')')
    nof_tx_queues = len(config_opt.split('),('))

    ptn = re.compile(r'(\d+)\,(\d+)\,(\d+)')
    port_conf = {}
    for c in config_opt.split('),('):
        m = re.match(ptn, c)
        port_idx = int(m.group(1))
        if port_conf.get(port_idx) is None:
            port_conf[port_idx] = [[int(m.group(2)), int(m.group(3))]]
        else:
            port_conf[port_idx].append([int(m.group(2)), int(m.group(3))])

    for attrs in port_conf.values():
        i = 0
        for attr in attrs:
            if attr[0] != i:
                print('Error: queue ids should be started from 0 ' +
                      'and sequential!')
                return False
            i = i + 1

    if nof_tx_queues > nof_queues:
        print('Error: %s=%d should be equal or less than %s=%d!' %
              ('tx_queues', nof_tx_queues, 'nof_queues', nof_queues))
        print("\tnof_queues is defiend with '-nq' or '--nof-queues' option")
        return False

    return True


def check_jumbo_opt(enable_jumbo, max_pkt_len):
    """Check if jumbo frame option is valid

    Jumbo frame is enabled with '--enable-jumbo' and max packet size is
    defined with '--max-pkt-len'.
    '--max-pkt-len' cannot be used without '--enable-jumbo'.
    """

    if (enable_jumbo is None) and (max_pkt_len is not None):
        print('Error: --enable-jumbo is required')
        return False

    if max_pkt_len is not None:
        if (max_pkt_len < 64) or (max_pkt_len > 9600):
            print('Error: --max-pkt-len %s should be %d-%d' % (
                max_pkt_len, 64, 9600))
            return False

    return True


def check_eth_dest(eth_dests):
    """Check if --eth-dest option is valied

    Format of --eth-dest for port X should be 'X,MM:MM:MM:MM:MM:MM'.
    This options can be given for several ports and 'eth_dests' arg
    is passed as a double array as below.
        [['0,ca:a4:45:e7:6a:ee'], ['1,ca:a4:45:e7:6a:ee'], ...]
    """

    xx = '[0-9A-Fa-f][0-9A-Fa-f]'
    ptn = re.compile(
        r'(\d+),(%s:%s:%s:%s:%s:%s)' % (xx, xx, xx, xx, xx, xx))
    for eth_dest in eth_dests:
        m = re.match(ptn, eth_dest[0])
        if m is None:
            return False

    return True


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

    # Parse vhost device IDs and Check the number of devices is
    # sufficient for port mask.
    if app_helper.is_sufficient_dev_ids(
            args.dev_ids, args.port_mask) is not True:
        print("Error: Cannot reserve ports '%s (= 0b%s)' on '%s'." % (
            args.port_mask,
            format(int(args.port_mask, 16), 'b'),
            args.dev_ids))
        exit()

    # Setup l3fwd command runs on container.
    cmd_path = '%s/examples/l3fwd/%s/l3fwd' % (
        env.RTE_SDK, env.RTE_TARGET)

    l3fwd_cmd = [cmd_path, '\\']

    # Setup EAL options.
    file_prefix = 'spp-l3fwd-container%d' % dev_ids_list[0]
    eal_opts = app_helper.setup_eal_opts(args, file_prefix)

    # Setup l3fwd options.
    l3fwd_opts = ['-p', args.port_mask, '\\']

    if args.config is None:
        common.error_exit('--config')
    elif check_config_format(args.config, args.nof_queues) is not True:
        print('Invalid config: %s' % args.config)
        exit()
    else:
        l3fwd_opts += ['--config', '"%s"' % args.config, '\\']

    # '--parse-ptype' is optional on host, but not on container.
    if args.parse_ptype == 'ipv4' or args.parse_ptype == 'ipv6':
        ptype_valid = True
    else:
        ptype_valid = False
    if ptype_valid is False:
        print('Error: invalid --parse-ptype %s' % args.parse_ptype)
        exit()
    else:
        l3fwd_opts += ['--parse-ptype', args.parse_ptype, '\\']

    # Check for optional opitons of l3fwd.
    if (args.exact_match is True) and (args.longest_prefix_match) is True:
        print('Error: -L and -E are mutually exclusive, select only one')
        exit()

    jumbo_opt_valid = False
    if args.enable_jumbo is True:
        jumbo_opt_valid = check_jumbo_opt(
            args.enable_jumbo, args.max_pkt_len)
        if jumbo_opt_valid is False:
            print('Error: invalid jumbo frame option(s)')
            exit()

    eth_dest_opt_valid = False
    if args.eth_dest is not None:
        eth_dest_opt_valid = check_eth_dest(args.eth_dest)
        if eth_dest_opt_valid is False:
            print('Error: invalid --eth-dest option')
            exit()

    if args.promiscous is True:
        l3fwd_opts += ['-P', '\\']
    if args.exact_match is True:
        l3fwd_opts += ['-E', '\\']
    if args.longest_prefix_match is True:
        l3fwd_opts += ['-L', '\\']
    if (args.eth_dest is not None) and (eth_dest_opt_valid is True):
        for eth_dest in args.eth_dest:  # args.eth_dest is a double array
            l3fwd_opts += ['--eth-dest %s' % eth_dest[0], '\\']
    if (args.enable_jumbo is not None) and (jumbo_opt_valid is True):
        l3fwd_opts += ['--enable-jumbo', '\\']
        if args.max_pkt_len is not None:
            l3fwd_opts += ['--max-pkt-len %d' % args.max_pkt_len, '\\']
    if args.no_numa is True:
        l3fwd_opts += ['--no-numa', '\\']
    if args.hash_entry_num is True:
        l3fwd_opts += ['--hash-entry-num', '\\']
    if args.ipv6 is True:
        l3fwd_opts += ['--ipv6', '\\']

    cmds = docker_cmd + docker_opts + l3fwd_cmd + eal_opts + l3fwd_opts
    if cmds[-1] == '\\':
        cmds.pop()
    common.print_pretty_commands(cmds)

    if args.dry_run is True:
        exit()

    # Remove delimiters for print_pretty_commands().
    while '\\' in cmds:
        cmds.remove('\\')
    # Call with shell=True because parsing '--eth_dest' option is failed
    # without it.
    subprocess.call(' '.join(cmds), shell=True)


if __name__ == '__main__':
    main()
