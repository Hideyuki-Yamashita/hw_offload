#!/usr/bin/env python
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Nippon Telegraph and Telephone Corporation

import argparse
import subprocess
from time import sleep


def parse_args():
    parser = argparse.ArgumentParser(
        description="Launcher for SPP")

    parser.add_argument(
        '-pl', '--pri-lcore',
        type=int,
        default='0',
        help="SPP primary lcore ID")
    parser.add_argument(
        '-dt', '--dev-tap-ids',
        type=str,
        default='1',
        help="Set of IDs of TAP device such as '1,2'")
    parser.add_argument(
        '-p', '--portmask',
        type=str,
        default='0x01',
        help="Portmask")
    parser.add_argument(
        '-sid', '--nfv-master-lcore-id',
        type=int,
        default=1,
        help="ID of spp_nfv master lcore")
    parser.add_argument(
        '-n', '--nof-spp-nfv',
        type=int,
        default=1,
        help="Number of spp_nfv")
    parser.add_argument(
        '--shared',
        action='store_true',
        help="Set master lcore is shared among spp_nfv(s)")
    parser.add_argument(
        '--dist-ver',
        type=str,
        default='latest',
        help="Version of Linux distribution")
    parser.add_argument(
        '-wt', '--wait-time',
        type=int,
        default=1,
        help="Sleep time for waiting launching spp_nfv(s)")

    return parser.parse_args()


def main():
    args = parse_args()

    cmd = ['app/spp-primary.py',
           '-l', str(args.pri_lcore),
           '-dt', args.dev_tap_ids,
           '-p', args.portmask,
           '--dist-ver', args.dist_ver]
    subprocess.call(cmd)

    sleep(args.wait_time)

    master_id = args.nfv_master_lcore_id
    for i in range(1, args.nof_spp_nfv + 1):
        cmd = ['sudo', 'rm', '-rf', '/tmp/sock%d' % i]
        subprocess.call(cmd)

        if args.shared is True:
            slave_id = master_id + i
            l_opt = '%d,%d' % (master_id, slave_id)
        else:
            slave_id = master_id + 1
            l_opt = '%d,%d' % (master_id, slave_id)
            # update master lcore ID to not be shared
            master_id = slave_id + 1

        if (args.pri_lcore == master_id) or (args.pri_lcore == slave_id):
            print("Error: cannot assign primary and secondary " +
                  "on the same lcore")
            exit()

        cmd = ['app/spp-nfv.py',
               '-i', str(i),
               '-l', l_opt,
               '--dist-ver', args.dist_ver]
        subprocess.call(cmd)


if __name__ == '__main__':
    main()
