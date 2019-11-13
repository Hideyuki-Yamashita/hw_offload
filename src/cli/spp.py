#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2015-2016 Intel Corporation

import argparse
import os
import re
from .shell import Shell
from .shell_lib import common
from . import spp_ctl_client
import sys


def main(argv):

    # Default
    api_ipaddr = '127.0.0.1'
    api_port = 7777

    parser = argparse.ArgumentParser(description="SPP Controller")
    parser.add_argument('-b', '--bind-addr', action='append',
                        default=['%s:%s' % (api_ipaddr, api_port)],
                        help='bind address, default=127.0.0.1:7777')
    parser.add_argument('--wait-pri', action='store_true',
                        help='Wait for spp_primary is launched')
    parser.add_argument('--config', type=str,
                        help='Config file path')
    args = parser.parse_args()

    if len(args.bind_addr) > 1:
        args.bind_addr.pop(0)

    spp_cli_objs = []
    for addr in args.bind_addr:
        if ':' in addr:
            api_ipaddr, api_port = addr.split(':')
            if common.is_valid_port(api_port) is False:
                print('Error: Invalid port in "{}"'.format(addr))
                exit()
        else:
            api_ipaddr = addr

        if common.is_valid_ipv4_addr(api_ipaddr) is False:
            print('Error: Invalid address "{}"'.format(addr))
            exit()

        spp_ctl_cli = spp_ctl_client.SppCtlClient(api_ipaddr, int(api_port))

        if spp_ctl_cli.is_server_running() is False:
            print('Is not spp-ctl running on %s, nor correct IP address?' %
                  api_ipaddr)
            exit()

        spp_cli_objs.append(spp_ctl_cli)

    shell = Shell(spp_cli_objs, args.config, args.wait_pri)
    shell.cmdloop()
    shell = None


if __name__ == "__main__":

    main(sys.argv[1:])
