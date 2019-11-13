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
        description="Launcher for testpmd application container")

    parser = app_helper.add_eal_args(parser)
    parser = app_helper.add_appc_args(parser)

    # Application specific args
    parser.add_argument(
        '--pci',
        action='store_true',
        help="Enable PCI (default is None)")
    parser.add_argument(
        '-i', '--interactive',
        action='store_true',
        help="Run in interactive mode (default is None)")
    parser.add_argument(
        '-a', '--auto-start',
        action='store_true',
        help="Start forwarding on initialization (default is None)")
    parser.add_argument(
        '--tx-first',
        action='store_true',
        help="Start forwarding, after sending a burst of packets first")
    parser.add_argument(
        '--stats-period',
        type=int,
        help="Period of displaying stats, if interactive is disabled")
    parser.add_argument(
        '--nb-cores',
        type=int,
        help="Number of forwarding cores")
    parser.add_argument(
        '--coremask',
        type=str,
        help="Hexadecimal bitmask of the cores, " +
        "do not include master lcore")
    parser.add_argument(
        '--portmask',
        type=str,
        help="Hexadecimal bitmask of the ports")
    parser.add_argument(
        '--no-numa',
        action='store_true',
        help="Disable NUMA-aware allocation of RX/TX rings and RX mbuf")
    parser.add_argument(
        '--port-numa-config',
        type=str,
        help="Specify port allocation as (port,socket)[,(port,socket)]")
    parser.add_argument(
        '--ring-numa-config',
        type=str,
        help="Specify ring allocation as " +
        "(port,flag,socket)[,(port,flag,socket)]")
    parser.add_argument(
        '--socket-num',
        type=int,
        help="Socket from which all memory is allocated in NUMA mode")
    parser.add_argument(
        '--mbuf-size',
        type=int,
        help="Size of mbufs used to N (< 65536) bytes (default is 2048)")
    parser.add_argument(
        '--total-num-mbufs',
        type=int,
        help="Number of mbufs allocated in mbuf pools, N > 1024.")
    parser.add_argument(
        '--max-pkt-len',
        type=int,
        help="Maximum packet size to N (>= 64) bytes (default is 1518)")
    parser.add_argument(
        '--eth-peers-configfile',
        type=str,
        help="Config file of Ether addrs of the peer ports")
    parser.add_argument(
        '--eth-peer',
        type=str,
        help="Set MAC addr of port N as 'N,XX:XX:XX:XX:XX:XX'")
    parser.add_argument(
        '--pkt-filter-mode',
        type=str,
        help="Flow Director mode, " +
        "'none'(default), 'signature' or 'perfect'")
    parser.add_argument(
        '--pkt-filter-report-hash',
        type=str,
        help="Flow Director hash match mode, " +
        "'none', 'match'(default) or 'always'")
    parser.add_argument(
        '--pkt-filter-size',
        type=str,
        help="Flow Director memory size ('64K', '128K', '256K'). " +
        "The default is 64K.")
    parser.add_argument(
        '--pkt-filter-flexbytes-offset',
        type=int,
        help="Flexbytes offset (0-32, default is 0x6) defined in " +
        "words counted from the first byte of the dest MAC address")
    parser.add_argument(
        '--pkt-filter-drop-queue',
        type=int,
        help="Set the drop-queue (default is 127)")
    parser.add_argument(
        '--disable-crc-strip',
        action='store_true',
        help="Disable hardware CRC stripping")
    parser.add_argument(
        '--enable-lro',
        action='store_true',
        help="Enable large receive offload")
    parser.add_argument(
        '--enable-rx-cksum',
        action='store_true',
        help="Enable hardware RX checksum offload")
    parser.add_argument(
        '--enable-scatter',
        action='store_true',
        help="Enable scatter (multi-segment) RX")
    parser.add_argument(
        '--enable-hw-vlan',
        action='store_true',
        help="Enable hardware vlan (default is None)")
    parser.add_argument(
        '--enable-hw-vlan-filter',
        action='store_true',
        help="Enable hardware VLAN filter")
    parser.add_argument(
        '--enable-hw-vlan-strip',
        action='store_true',
        help="Enable hardware VLAN strip")
    parser.add_argument(
        '--enable-hw-vlan-extend',
        action='store_true',
        help="Enable hardware VLAN extend")
    parser.add_argument(
        '--enable-drop-en',
        action='store_true',
        help="Enable per-queue packet drop if no descriptors")
    parser.add_argument(
        '--disable-rss',
        action='store_true',
        help="Disable RSS (Receive Side Scaling")
    parser.add_argument(
        '--port-topology',
        type=str,
        help="Port topology, 'paired' (the default) or 'chained'")
    parser.add_argument(
        '--forward-mode',
        type=str,
        help="Forwarding mode, " +
        "'io' (default), 'mac', 'mac_swap', 'flowgen', 'rxonly', " +
        "'txonly', 'csum', 'icmpecho', 'ieee1588', 'tm'")
    parser.add_argument(
        '--rss-ip',
        action='store_true',
        help="Set RSS functions for IPv4/IPv6 only")
    parser.add_argument(
        '--rss-udp',
        action='store_true',
        help="Set RSS functions for IPv4/IPv6 and UDP")
    parser.add_argument(
        '--rxq',
        type=int,
        help="Number of RX queues per port, 1-65535 (default is 1)")
    parser.add_argument(
        '--rxd',
        type=int,
        help="Number of descriptors in the RX rings (default is 128)")
    parser.add_argument(
        '--txq',
        type=int,
        help="Number of TX queues per port, 1-65535 (default is 1)")
    parser.add_argument(
        '--txd',
        type=int,
        help="Number of descriptors in the TX rings (default is 512)")
    parser.add_argument(
        '--burst',
        type=int,
        help="Number of packets per burst, 1-512 (default is 32)")
    parser.add_argument(
        '--mbcache',
        type=int,
        help="Cache of mbuf memory pools, 0-512 (default is 16)")
    parser.add_argument(
        '--rxpt',
        type=int,
        help="Prefetch threshold register of RX rings (default is 8)")
    parser.add_argument(
        '--rxht',
        type=int,
        help="Host threshold register of RX rings (default is 8)")
    parser.add_argument(
        '--rxfreet',
        type=int,
        help="Free threshold of RX descriptors,0-'rxd' (default is 0)")
    parser.add_argument(
        '--rxwt',
        type=int,
        help="Write-back threshold register of RX rings (default is 4)")
    parser.add_argument(
        '--txpt',
        type=int,
        help="Prefetch threshold register of TX rings (default is 36)")
    parser.add_argument(
        '--txht',
        type=int,
        help="Host threshold register of TX rings (default is 0)")
    parser.add_argument(
        '--txwt',
        type=int,
        help="Write-back threshold register of TX rings (default is 0)")
    parser.add_argument(
        '--txfreet',
        type=int,
        help="Free threshold of RX descriptors, 0-'txd' (default is 0)")
    parser.add_argument(
        '--txrst',
        type=int,
        help="Transmit RS bit threshold of TX rings, 0-'txd' (default is 0)")
    parser.add_argument(
        '--rx-queue-stats-mapping',
        type=str,
        help="RX queues statistics counters mapping 0-15 as " +
        "'(port,queue,mapping)[,(port,queue,mapping)]'")
    parser.add_argument(
        '--tx-queue-stats-mapping',
        type=str,
        help="TX queues statistics counters mapping 0-15 as " +
        "'(port,queue,mapping)[,(port,queue,mapping)]'")
    parser.add_argument(
        '--no-flush-rx',
        action='store_true',
        help="Don't flush the RX streams before starting forwarding, " +
        "Used mainly with the PCAP PMD")
    parser.add_argument(
        '--txpkts',
        type=str,
        help="TX segment sizes or total packet length, " +
        "Valid for tx-only and flowgen")
    parser.add_argument(
        '--disable-link-check',
        action='store_true',
        help="Disable check on link status when starting/stopping ports")
    parser.add_argument(
        '--no-lsc-interrupt',
        action='store_true',
        help="Disable LSC interrupts for all ports")
    parser.add_argument(
        '--no-rmv-interrupt',
        action='store_true',
        help="Disable RMV interrupts for all ports")
    parser.add_argument(
        '--bitrate-stats',
        nargs='*',
        action='append',
        type=int,
        help="Logical core N to perform bitrate calculation")
    parser.add_argument(
        '--print-event',
        type=str,
        help="Enable printing the occurrence of the designated event, " +
        "<unknown|intr_lsc|queue_state|intr_reset|vf_mbox|macsec|" +
        "intr_rmv|dev_probed|dev_released|all>")
    parser.add_argument(
        '--mask-event',
        type=str,
        help="Disable printing the occurrence of the designated event, " +
        "<unknown|intr_lsc|queue_state|intr_reset|vf_mbox|macsec|" +
        "intr_rmv|dev_probed|dev_released|all>")
    parser.add_argument(
        '--flow-isolate-all',
        action='store_true',
        help="Providing this parameter requests flow API isolated mode " +
        "on all ports at initialization time")
    parser.add_argument(
        '--tx-offloads',
        type=str,
        help="Hexadecimal bitmask of TX queue offloads (default is 0)")
    parser.add_argument(
        '--hot-plug',
        action='store_true',
        help="Enable device event monitor machenism for hotplug")
    parser.add_argument(
        '--vxlan-gpe-port',
        type=int,
        help="UDP port number of tunnel VXLAN-GPE (default is 4790)")
    parser.add_argument(
        '--mlockall',
        action='store_true',
        help="Enable locking all memory")
    parser.add_argument(
        '--no-mlockall',
        action='store_true',
        help="Disable locking all memory")

    parser = app_helper.add_sppc_args(parser)

    return parser.parse_args()


def check_eth_peer(eth_peer):
    """Check if --eth-peer option is valied

    Format of --eth-peer for port X should be 'N,XX:XX:XX:XX:XX:XX'.
    """

    xx = '[0-9A-Fa-f][0-9A-Fa-f]'
    ptn = re.compile(
        r'(\d+),(%s:%s:%s:%s:%s:%s)' % (xx, xx, xx, xx, xx, xx))
    m = re.match(ptn, eth_peer)
    if m is None:
            return False
    return True


def check_pkt_filter_mode(mode):
    """Check if Flow Director mode is valid

    There are three modes for Flow Director.
      * none (default)
      * signature
      * perfect
    """

    if mode in ['none', 'signature', 'perfect']:
        return True
    else:
        return False


def check_pkt_filter_report_hash(mode):
    """Check if Flow Director hash match reporting mode is valid

    There are three modes for the reporting mode.
      * none
      * match (default)
      * always
    """

    if mode in ['none', 'match', 'always']:
        return True
    else:
        return False


def check_pkt_filter_size(pkt_size):
    """Check if Flow Director size is valid

    Packet size should be 64K, 128K or 256K
    """

    if pkt_size in ['64K', '128K', '256K']:
        return True
    else:
        return False


def check_txpkts(txpkts):
    """Check if txpkts is valid.

    Txpkts is a TX segment sizes or total packet length. For example,
    'txpkts=64,4,4,4,4,4'.
    This option is valid for 'tx-only' or 'flowgen' forwarding modes.
    """

    for i in txpkts.split(','):
        if not re.match(r'\d+', i):
            return False
    return True


def check_event(event):
    events = ['unknown', 'intr_lsc', 'queue_state', 'intr_reset',
              'vf_mbox', 'macsec', 'intr_rmv', 'dev_probed',
              'dev_released', 'all']
    if event in events:
        return True
    else:
        return False


def check_port_topology(mode):
    if mode in ['paired', 'chained', 'loop']:
        return True
    else:
        return False


def check_forward_mode(mode):
    modes = ['io', 'mac', 'macswap', 'flowgen', 'rxonly', 'txonly', 'csum',
            'icmpecho', 'ieee1588', 'tm', 'noisy']
    if mode in modes:
        return True
    else:
        return False


def check_port_numa_config(pnconf):
    """Check if --port-numa-config is valid.

    '--port-numa-config' is a tuples of port and socket such as
    --port-numa-config=(port,socket),(port,socket),...
    """

    pnconf = pnconf.replace('),(', ')|(')
    for s in pnconf.split('|'):
        if not re.match(r'\(\w+,\w+\)', s):
            return False
    return True


def check_ring_numa_config(rnconf):
    """Check if --ring-numa-config is valid.

    '--port-numa-config' is a tuples of port, flag and socket such as
    --port-numa-config=(port,flag,socket),(port,flag,socket),...
    """

    rnconf = rnconf.replace('),(', ')|(')
    for s in rnconf.split('|'):
        if not re.match(r'\(\w+,\w+,\w+\)', s):
            return False
    return True


def error_exit(opt):
    print("Error: invalid '{}' option".format(opt))
    exit()


def not_supported_exit(opt):
    print("Error: '{}' is not supported yet".format(opt))
    exit()


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

    cmd_path = 'testpmd'

    # Setup testpmd command.
    testpmd_cmd = [cmd_path, '\\']

    # Setup EAL options
    file_prefix = 'spp-testpmd-container%d' % dev_ids_list[0]
    eal_opts = app_helper.setup_eal_opts(args, file_prefix)

    # Setup testpmd options
    testpmd_opts = []

    if args.interactive is True:
        testpmd_opts += ['--interactive', '\\']

    if args.auto_start is True:
        testpmd_opts += ['--auto-start', '\\']

    if args.tx_first is True:
        if args.interactive is not True:
            testpmd_opts += ['--tx-first', '\\']
        else:
            print("Error: '%s' cannot be used in interactive mode" % (
                '--tx-first'))
            exit()

    if args.stats_period is not None:
        testpmd_opts += ['--stats-period', str(args.stats_period), '\\']

    if args.nb_cores is not None:
        testpmd_opts += ['--nb-cores=%d' % args.nb_cores, '\\']

    if args.coremask is not None:
        testpmd_opts += ['--coremask=%s' % args.coremask, '\\']

    if args.portmask is not None:
        testpmd_opts += ['--portmask=%s' % args.portmask, '\\']

    if args.no_numa is True:
        testpmd_opts += ['--no-numa', '\\']

    if args.port_numa_config is not None:
        if check_port_numa_config(args.port_numa_config) is True:
            testpmd_opts += [
                    '--port-numa-config={}'.format(
                        args.port_numa_config),
                    '\\']

    if args.ring_numa_config is not None:
        if check_ring_numa_config(args.ring_numa_config) is True:
            testpmd_opts += [
                    '--ring-numa-config={}'.format(
                        args.ring_numa_config),
                    '\\']

    if args.socket_num is not None:
        testpmd_opts += ['%s=%d' % (
            '--socket-num', args.socket_num), '\\']

    if args.mbuf_size is not None:
        mbuf_limit = 65536
        if args.mbuf_size > mbuf_limit:
            print("Error: '%s' should be less than %d" % (
                '--mbuf-size', mbuf_limit))
            exit()
        else:
            testpmd_opts += ['%s=%d' % (
                '--mbuf-size', args.mbuf_size), '\\']

    if args.total_num_mbufs is not None:
        nof_mbuf_limit = 1024
        if args.total_num_mbufs <= nof_mbuf_limit:
            print("Error: '%s' should be more than %d" % (
                '--total-num-mbufs', nof_mbuf_limit))
            exit()
        else:
            testpmd_opts += ['%s=%d' % (
                '--total-num-mbufs', args.total_num_mbufs), '\\']

    if args.max_pkt_len is not None:
        pkt_len_limit = 64
        if args.max_pkt_len < pkt_len_limit:
            print("Error: '%s' should be equal or more than %d" % (
                '--max-pkt-len', pkt_len_limit))
            exit()
        else:
            testpmd_opts += ['%s=%d' % (
                '--max-pkt-len', args.max_pkt_len), '\\']

    if args.eth_peers_configfile is not None:
        testpmd_opts += ['%s=%s' % (
            '--eth-peers-configfile',
            args.eth_peers_configfile), '\\']

    if args.eth_peer is not None:
        if check_eth_peer(args.eth_peer) is True:
            testpmd_opts += ['%s=%s' % (
                '--eth-peer', args.eth_peer), '\\']
        else:
            error_exit('--eth-peer')

    if args.pkt_filter_mode is not None:
        if check_pkt_filter_mode(args.pkt_filter_mode) is True:
            testpmd_opts += ['%s=%s' % (
                '--pkt-filter-mode', args.pkt_filter_mode), '\\']
        else:
            print("Error: '--pkt-filter-mode' should be " +
                  "'none', 'signature' or 'perfect'")
            exit()

    if args.pkt_filter_report_hash is not None:
        if check_pkt_filter_report_hash(args.pkt_filter_report_hash) is True:
            testpmd_opts += ['%s=%s' % (
                '--pkt-filter-report-hash',
                args.pkt_filter_report_hash), '\\']
        else:
            print("Error: '--pkt-filter-report-hash' should be " +
                  "'none', 'match' or 'always'")
            exit()

    if args.pkt_filter_size is not None:
        if check_pkt_filter_size(args.pkt_filter_size) is True:
            testpmd_opts += ['%s=%s' % (
                '--pkt-filter-size', args.pkt_filter_size), '\\']
        else:
            print("Error: '--pkt-filter-size' should be " +
                  "'64K', '128K' or '256K'")
            exit()

    # TODO(yasufum) Confirm this option is supported in dpdk 18.02
    if args.pkt_filter_flexbytes_offset is not None:
        not_supported_exit('--pkt-filter-flexbytes-offset')

    # It causes 'unrecognized option' error.
    # if args.pkt_filter_flexbytes_offset is not None:
    #     f_offset = args.pkt_filter_flexbytes_offset
    #     f_offset_min = 0
    #     f_offset_max = 32
    #     if (f_offset < f_offset_min) or (f_offset > f_offset_max):
    #         print("Error: '%s' should be %d-%d" % (
    #             '--pkt-filter-flexbytes-offset',
    #             f_offset_min, f_offset_max))
    #         exit()
    #     else:
    #         testpmd_opts += ['%s=%d' % (
    #             '--pkt-filter-flexbytes-offset', f_offset), '\\']

    if args.pkt_filter_drop_queue is not None:
        testpmd_opts += ['%s=%d' % (
            '--pkt-filter-drop-queue', args.pkt_filter_drop_queue), '\\']

    if args.disable_crc_strip is True:
        testpmd_opts += ['--disable-crc-strip', '\\']

    if args.enable_lro is True:
        testpmd_opts += ['--enable-lro', '\\']

    if args.enable_rx_cksum is True:
        testpmd_opts += ['--enable-rx-cksum', '\\']

    if args.enable_scatter is True:
        testpmd_opts += ['--enable-scatter', '\\']

    if args.enable_hw_vlan is True:
        testpmd_opts += ['--enable-hw-vlan', '\\']

    if args.enable_hw_vlan_filter is True:
        testpmd_opts += ['--enable-hw-vlan-filter', '\\']

    if args.enable_hw_vlan_strip is True:
        testpmd_opts += ['--enable-hw-vlan-strip', '\\']

    if args.enable_hw_vlan_extend is True:
        testpmd_opts += ['--enable-hw-vlan-extend', '\\']

    if args.enable_drop_en is True:
        testpmd_opts += ['--enable-drop-en', '\\']

    if args.disable_rss is True:
        testpmd_opts += ['--disable-rss', '\\']

    if args.port_topology is not None:
        if check_port_topology(args.port_topology) is True:
            testpmd_opts += [
                    '--port-topology={}'.format(args.port_topology), '\\']
        else:
            error_exit('--port-topology')

    if args.forward_mode is not None:
        if check_forward_mode(args.forward_mode) is True:
            testpmd_opts += [
                    '--forward-mode={}'.format(args.forward_mode), '\\']
        else:
            error_exit('--forward-mode')

    if args.rss_ip is True:
        testpmd_opts += ['--rss-ip', '\\']

    if args.rss_udp is True:
        testpmd_opts += ['--rss-udp', '\\']

    if args.rxq is not None:
        nof_q_min = 1
        nof_q_max = 65535
        if (args.rxq < nof_q_min) or (nof_q_max < args.rxq):
            print("Error: '%s' should be %d-%d" % (
                '--rxq', nof_q_min, nof_q_max))
            exit()
        else:
            testpmd_opts += ['%s=%d' % ('--rxq', args.rxq), '\\']

    if args.rxd is not None:
        nof_d_min = 1
        if (args.rxd < nof_d_min):
            print("Error: '%s' should be equal or more than %d" % (
                '--rxd', nof_d_min))
            exit()
        else:
            testpmd_opts += ['%s=%d' % ('--rxd', args.rxd), '\\']

    if args.txq is not None:
        nof_q_min = 1
        nof_q_max = 65535
        if (args.txq < nof_q_min) or (nof_q_max < args.txq):
            print("Error: '%s' should be %d-%d" % (
                '--txq', nof_q_min, nof_q_max))
            exit()
        else:
            testpmd_opts += ['%s=%d' % ('--txq', args.txq), '\\']

    if args.txd is not None:
        nof_d_min = 1
        if (args.txd < nof_d_min):
            print("Error: '%s' should be equal or more than %d" % (
                '--txd', nof_d_min))
            exit()
        else:
            testpmd_opts += ['%s=%d' % ('--txd', args.txd), '\\']

    if args.burst is not None:
        b_min = 1
        b_max = 512
        if (args.burst < b_min) or (b_max < args.burst):
            print("Error: '%s' should be %d-%d" % (
                '--burst', b_min, b_max))
            exit()
        else:
            testpmd_opts += ['%s=%d' % ('--burst', args.burst), '\\']

    if args.mbcache is not None:
        mb_min = 0
        mb_max = 512
        if (args.mbcache < mb_min) or (mb_max < args.mbcache):
            print("Error: '%s' should be %d-%d" % (
                '--mbcache', mb_min, mb_max))
            exit()
        else:
            testpmd_opts += ['%s=%d' % ('--mbcache', args.mbcache), '\\']

    if args.rxpt is not None:
        nof_p_min = 0
        if (args.rxpt < nof_p_min):
            print("Error: '%s' should be equal or more than %d" % (
                '--rxpt', nof_p_min))
            exit()
        else:
            testpmd_opts += ['%s=%d' % ('--rxpt', args.rxpt), '\\']

    if args.rxht is not None:
        nof_h_min = 0
        if (args.rxht < nof_h_min):
            print("Error: '%s' should be equal or more than %d" % (
                '--rxht', nof_h_min))
            exit()
        else:
            testpmd_opts += ['%s=%d' % ('--rxht', args.rxht), '\\']

    if args.rxfreet is not None:
        nof_f_min = 0
        if args.rxd is not None:
            nof_f_max = args.rxd - 1
        else:
            nof_f_max = 128 - 1  # as default of rxd - 1
        if (args.rxfreet < nof_f_min) or (nof_f_max < args.rxfreet):
            print("Error: '%s' should be %d-%d" % (
                '--rxfreet', nof_f_min, nof_f_max))
            exit()
        else:
            testpmd_opts += ['%s=%d' % ('--rxfreet', args.rxfreet), '\\']

    if args.rxwt is not None:
        nof_w_min = 0
        if (args.rxwt < nof_w_min):
            print("Error: '%s' should be equal or more than %d" % (
                '--rxwt', nof_w_min))
            exit()
        else:
            testpmd_opts += ['%s=%d' % ('--rxwt', args.rxwt), '\\']

    if args.txpt is not None:
        nof_p_min = 0
        if (args.txpt < nof_p_min):
            print("Error: '%s' should be equal or more than %d" % (
                '--txpt', nof_p_min))
            exit()
        else:
            testpmd_opts += ['%s=%d' % ('--txpt', args.txpt), '\\']

    if args.txht is not None:
        nof_h_min = 0
        if (args.txht < nof_h_min):
            print("Error: '%s' should be equal or more than %d" % (
                '--txht', nof_h_min))
            exit()
        else:
            testpmd_opts += ['%s=%d' % ('--txht', args.txht), '\\']

    if args.txwt is not None:
        nof_w_min = 0
        if (args.txwt < nof_w_min):
            print("Error: '%s' should be equal or more than %d" % (
                '--txwt', nof_w_min))
            exit()
        else:
            testpmd_opts += ['%s=%d' % ('--txwt', args.txwt), '\\']

    if args.txfreet is not None:
        nof_f_min = 0
        if args.txd is not None:
            nof_f_max = args.txd
        else:
            nof_f_max = 512  # as default of txd
        if (args.txfreet < nof_f_min) or (nof_f_max < args.txfreet):
            print("Error: '%s' should be %d-%d" % (
                '--txfreet', nof_f_min, nof_f_max))
            exit()
        else:
            testpmd_opts += ['%s=%d' % ('--txfreet', args.txfreet), '\\']

    if args.txrst is not None:
        nof_r_min = 0
        if args.txd is not None:
            nof_r_max = args.txd
        else:
            nof_r_max = 512  # as default of txd
        if (args.txrst < nof_r_min) or (nof_r_max < args.txrst):
            print("Error: '%s' should be %d-%d" % (
                '--txrst', nof_r_min, nof_r_max))
            exit()
        else:
            testpmd_opts += ['%s=%d' % ('--txrst', args.txrst), '\\']

    if args.rx_queue_stats_mapping is not None:
        testpmd_opts += [
                '--rx-queue-stats-mapping={}'.format(
                    args.rx_queue_stats_mapping),
                '\\']

    if args.tx_queue_stats_mapping is not None:
        testpmd_opts += [
                '--tx-queue-stats-mapping={}'.format(
                    args.tx_queue_stats_mapping),
                '\\']

    if args.no_flush_rx is True:
        testpmd_opts += ['--no-flush-rx', '\\']

    if args.txpkts is not None:
        if check_txpkts(args.txpkts) is True:
            testpmd_opts += ['%s=%s' % (
                '--txpkts', args.txpkts), '\\']
        else:
            error_exit('--txpkts')

    if args.disable_link_check is True:
        testpmd_opts += ['--disable-link-check', '\\']

    if args.no_lsc_interrupt is True:
        testpmd_opts += ['--no-lsc-interrupt', '\\']

    if args.no_rmv_interrupt is True:
        testpmd_opts += ['--no-rmv-interrupt', '\\']

    if args.bitrate_stats is not None:
        # --bitrate-stats can be several
        for stat in args.bitrate_stats:
            if stat[0] >= 0:
                testpmd_opts += ['%s=%d' % (
                    '--bitrate-stats', stat[0]), '\\']
            else:
                print("Error: '--bitrate-stats' should be <= 0")
                exit()

    if args.print_event is not None:
        if check_event(args.print_event) is True:
            testpmd_opts += ['%s=%s' % (
                '--print-event', args.print_event), '\\']
        else:
            error_exit('--print-event')

    if args.mask_event is not None:
        if check_event(args.mask_event) is True:
            testpmd_opts += ['%s=%s' % (
                '--mask-event', args.mask_event), '\\']
        else:
            error_exit('--mask-event')

    if args.flow_isolate_all is True:
        testpmd_opts += ['--flow-isolate-all', '\\']

    if args.tx_offloads is not None:
        ptn = r'^0x[0-9aA-Fa-f]+$'  # should be hexadecimal
        if re.match(ptn, args.tx_offloads) is True:
            testpmd_opts += ['%s=%s' % (
                '--tx-offloads', args.tx_offloads), '\\']
        else:
            error_exit('--tx-offloads')

    if args.hot_plug is True:
        testpmd_opts += ['--hot-plug', '\\']

    if args.vxlan_gpe_port is not None:
        nof_p_min = 0
        if (args.vxlan_gpe_port < nof_p_min):
            print("Error: '%s' should be <= %d" % (
                '--vxlan-gpe-port', nof_p_min))
            exit()
        else:
            testpmd_opts += ['%s=%d' % (
                '--vxlan-gpe-port', args.vxlan_gpe_port), '\\']

    if args.mlockall is True:
        testpmd_opts += ['--mlockall', '\\']

    if args.no_mlockall is True:
        testpmd_opts += ['--no-mlockall', '\\']

    cmds = docker_cmd + docker_opts + testpmd_cmd + eal_opts + testpmd_opts
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
