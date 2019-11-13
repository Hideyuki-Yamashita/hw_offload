#!/usr/bin/env python
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Nippon Telegraph and Telephone Corporation


def print_pretty_commands(cmds):
    """Print given command in pretty format"""

    print(' '.join(cmds).replace('\\', '\\\n'))


def container_img_name(base, dist_name, dist_ver):
    """Generate container image name

    Return the name of container image for '-t' of docker command
    such as 'sppc/dpdk-ubuntu:16.04' or 'sppc/spp-ubuntu:18.04'.
    'base' is defined as 'env.CONTAINER_IMG_NAME' in lib module.
    """
    return '%s-%s:%s' % (base, dist_name, dist_ver)


def error_exit(objname):
    """Print error message and exit

    This function is used for notifying an argument for the object
    is not given.
    """

    print('Error: \'%s\' is not defined.' % objname)
    exit()
