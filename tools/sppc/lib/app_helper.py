#!/usr/bin/env python
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Nippon Telegraph and Telephone Corporation

import common
import os
import sys

work_dir = os.path.dirname(__file__)
sys.path.append(work_dir + '/..')
from conf import env


def add_eal_args(parser, mem_size=1024, mem_channel=4):
    parser.add_argument(
        '-l', '--core-list',
        type=str,
        help='Core list')
    parser.add_argument(
        '-c', '--core-mask',
        type=str,
        help='Core mask')
    parser.add_argument(
        '-m', '--mem',
        type=int,
        default=mem_size,
        help='Memory size (default is %s)' % mem_size)
    parser.add_argument(
        '--socket-mem',
        type=str,
        help='Memory size')
    parser.add_argument(
        '-b', '--pci-blacklist',
        nargs='*', type=str,
        help='PCI blacklist for excluding devices')
    parser.add_argument(
        '-w', '--pci-whitelist',
        nargs='*', type=str,
        help='PCI whitelist for including devices')
    parser.add_argument(
        '--single-file-segments',
        action='store_true',
        help='Create fewer files in hugetlbfs (non-legacy mode only).')
    parser.add_argument(
        '--nof-memchan',
        type=int,
        default=mem_channel,
        help='Number of memory channels (default is %s)' % mem_channel)
    return parser


def get_core_opt(args):
    # Check core_mask or core_list is defined.
    if args.core_mask is not None:
        core_opt = {'attr': '-c', 'val': args.core_mask}
    elif args.core_list is not None:
        core_opt = {'attr': '-l', 'val': args.core_list}
    else:
        common.error_exit('core_mask or core_list')
    return core_opt


def get_mem_opt(args):
    # Check memory option is defined.
    if args.socket_mem is not None:
        mem_opt = {'attr': '--socket-mem', 'val': args.socket_mem}
    else:
        mem_opt = {'attr': '-m', 'val': str(args.mem)}
    return mem_opt


def setup_eal_opts(args, file_prefix, proc_type='auto', hugedir=None):
    core_opt = get_core_opt(args)
    mem_opt = get_mem_opt(args)

    eal_opts = [
        core_opt['attr'], core_opt['val'], '\\',
        '-n', str(args.nof_memchan), '\\',
        mem_opt['attr'], mem_opt['val'], '\\',
        '--proc-type', proc_type, '\\']

    if args.dev_ids is None:
        common.error_exit('--dev-ids')
    else:
        dev_ids = dev_ids_to_list(args.dev_ids)

    socks = []
    for dev_id in dev_ids:
        socks.append({
            'host': '/tmp/sock%d' % dev_id,
            'guest': '/var/run/usvhost%d' % dev_id})

    for i in range(len(dev_ids)):
        eal_opts += [
            '--vdev', 'virtio_user%d,queues=%d,path=%s' % (
                dev_ids[i], args.nof_queues, socks[i]['guest']), '\\']

    if (args.pci_blacklist is not None) and (args.pci_whitelist is not None):
        common.error_exit("Cannot use both of '-b' and '-w' at once")
    elif args.pci_blacklist is not None:
        for bd in args.pci_blacklist:
            eal_opts += ['-b', bd, '\\']
    elif args.pci_whitelist is not None:
        for wd in args.pci_whitelist:
            eal_opts += ['-w', wd, '\\']

    if args.single_file_segments is True:
        eal_opts += ['--single-file-segments', '\\']

    eal_opts += [
        '--file-prefix', file_prefix, '\\',
        '--', '\\']

    return eal_opts


def add_sppc_args(parser):
    parser.add_argument(
        '--dist-name',
        type=str,
        default='ubuntu',
        help="Name of Linux distribution")
    parser.add_argument(
        '--dist-ver',
        type=str,
        default='latest',
        help="Version of Linux distribution")
    parser.add_argument(
        '--workdir',
        type=str,
        help="Path of directory in which the command is launched")
    parser.add_argument(
        '-ci', '--container-image',
        type=str,
        help="Name of container image")
    parser.add_argument(
        '-fg', '--foreground',
        action='store_true',
        help="Run container as foreground mode")
    parser.add_argument(
        '--dry-run',
        action='store_true',
        help="Only print matrix, do not run, and exit")
    return parser


def setup_docker_opts(args, target_name, sock_files, workdir=None):
    docker_opts = []

    if args.foreground is True:
        docker_opts = ['-it', '\\']
    else:
        docker_opts = ['-d', '\\']

    if workdir is not None:
        docker_opts += ['--workdir', workdir, '\\']

    if args.no_privileged is not True:
        docker_opts += ['--privileged', '\\']

    for sock in sock_files:
        docker_opts += [
            '-v', '%s:%s' % (sock['host'], sock['guest']), '\\']

    if args.container_image is not None:
        container_image = args.container_image
    else:
        # Container image name, for exp 'sppc/dpdk-ubuntu:18.04'
        container_image = common.container_img_name(
            env.CONTAINER_IMG_NAME[target_name],
            args.dist_name,
            args.dist_ver)

    docker_opts += [
        '-v', '/dev/hugepages:/dev/hugepages', '\\',
        container_image, '\\']

    return docker_opts


def add_appc_args(parser):
    parser.add_argument(
        '-d', '--dev-ids',
        type=str,
        help='two or more even vhost device IDs')
    parser.add_argument(
        '-nq', '--nof-queues',
        type=int,
        default=1,
        help="Number of queues of virtio (default is 1)")
    parser.add_argument(
        '--no-privileged',
        action='store_true',
        help="Disable docker's privileged mode if it's needed")
    return parser


def uniq(dup_list):
    """Remove duplicated elements in a list and return a unique list

    Example: [1,1,2,2,3,3] #=> [1,2,3]
    """

    return list(set(dup_list))


def dev_ids_to_list(dev_ids):
    """Parse vhost device IDs and return as a list.

    Example:
    '1,3-5' #=> [1,3,4,5]
    """

    res = []
    for dev_id_part in dev_ids.split(','):
        if '-' in dev_id_part:
            cl = dev_id_part.split('-')
            res = res + range(int(cl[0]), int(cl[1])+1)
        else:
            res.append(int(dev_id_part))
    return res


def is_sufficient_dev_ids(dev_ids, port_mask):
    """Check if ports can be reserved for dev_ids

    Return true if the number of dev IDs equals or more than given ports.
    'dev_ids' is a value of '-d' or '--dev-ids' such as '1,2'.
    """

    dev_ids_list = dev_ids_to_list(dev_ids)
    if not ('0x' in port_mask):  # invalid port mask
        return False

    ports_in_binary = format(int(port_mask, 16), 'b')
    if len(dev_ids_list) >= len(ports_in_binary):
        return True
    else:
        return False


def sock_files(dev_ids_list):
    socks = []
    for dev_id in dev_ids_list:
        socks.append({
            'host': '/tmp/sock%d' % dev_id,
            'guest': '/var/run/usvhost%d' % dev_id})
    return socks


def count_ports(port_mask):
    """Return the number of ports of given portmask"""

    ports_in_binary = format(int(port_mask, 16), 'b')
    nof_ports = ports_in_binary.count('1')
    return nof_ports


def cores_to_list(core_opt):
    """Expand DPDK core option to ranged list.

    Core option must be a hash of attritute and its value.
    Attribute is -c(core mask) or -l(core list).
    For example, '-c 0x03' is described as:
      core_opt = {'attr': '-c', 'val': '0x03'}
    or '-l 0-1' is as
      core_opt = {'attr': '-l', 'val': '0-1'}

    Returned value is a list, such as:
      '0x17' is converted to [1,2,3,5].
    or
      '-l 1-3,5' is converted to [1,2,3,5],
    """

    res = []
    if core_opt['attr'] == '-c':
        bin_list = list(
            format(
                int(core_opt['val'], 16), 'b'))
        cnt = 1
        bin_list.reverse()
        for i in bin_list:
            if i == '1':
                res.append(cnt)
            cnt += 1
    elif core_opt['attr'] == '-l':
        for core_part in core_opt['val'].split(','):
            if '-' in core_part:
                cl = core_part.split('-')
                res = res + range(int(cl[0]), int(cl[1])+1)
            else:
                res.append(int(core_part))
    else:
        pass
    res = uniq(res)
    res.sort()
    return res
