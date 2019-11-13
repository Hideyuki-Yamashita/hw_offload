#!/usr/bin/env python
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Nippon Telegraph and Telephone Corporation

from __future__ import absolute_import
import argparse
import os
import subprocess
import sys

work_dir = os.path.dirname(__file__)
sys.path.append(work_dir + '/..')
from lib import common
from conf import env


def parse_args():
    parser = argparse.ArgumentParser(
        description="Docker image builder for DPDK applications")
    parser.add_argument(
        '-t', '--target',
        type=str,
        help="Build target ('dpdk', 'pktgen', 'spp' or 'suricata')")
    parser.add_argument(
        '-ci', '--container-image',
        type=str,
        help="Name of container image")
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
        '--dpdk-repo',
        type=str,
        default="http://dpdk.org/git/dpdk",
        help="Git URL of DPDK")
    parser.add_argument(
        '--dpdk-branch',
        type=str,
        help="Specific version or branch of DPDK")
    parser.add_argument(
        '--pktgen-repo',
        type=str,
        default="http://dpdk.org/git/apps/pktgen-dpdk",
        help="Git URL of pktgen-dpdk")
    parser.add_argument(
        '--pktgen-branch',
        type=str,
        help="Specific version or branch of pktgen-dpdk")
    parser.add_argument(
        '--spp-repo',
        type=str,
        default="http://dpdk.org/git/apps/spp",
        help="Git URL of SPP")
    parser.add_argument(
        '--spp-branch',
        type=str,
        help="Specific version or branch of SPP")

    # Supporting suricata is experimental
    parser.add_argument(
        '--suricata-repo',
        type=str,
        default="https://github.com/vipinpv85/DPDK_SURICATA-4_1_1.git",
        help="Git URL of DPDK-Suricata")
    parser.add_argument(
        '--suricata-branch',
        type=str,
        help="Specific version or branch of DPDK-Suricata")

    parser.add_argument(
        '--only-envsh',
        action='store_true',
        help="Create config 'env.sh' and exit without docker build")
    parser.add_argument(
        '--dry-run',
        action='store_true',
        help="Print matrix for checking and exit without docker build")
    return parser.parse_args()


def create_env_sh(dst_dir, rte_sdk, target, target_dir):
    """Create config file for DPDK environment variables

    Create 'env.sh' which defines $RTE_SDK and $RTE_TARGET inside a
    container to be referredd from 'run.sh' and Dockerfile.
    """
    contents = "export RTE_SDK=%s\n" % rte_sdk
    contents += "export RTE_TARGET=%s\n" % env.RTE_TARGET
    if target == 'pktgen':
        contents += "export PKTGEN_DIR=%s" % target_dir
    elif target == 'spp':
        contents += "export SPP_DIR=%s" % target_dir
    elif target == 'suricata':
        contents += "export SURICATA_DIR=%s" % target_dir

    try:
        f = open('%s/env.sh' % dst_dir, 'w')
        f.write(contents)
        f.close()
    except IOError:
        print('Error: Failed to create env.sh.')


def main():
    args = parse_args()

    if args.target is not None:
        dockerfile_dir = '%s/%s/%s' % (  # Dockerfile is contained here
            work_dir, args.dist_name, args.target)
        # Container image name, for exp 'sppc/dpdk-ubuntu:18.04'
        container_image = common.container_img_name(
            env.CONTAINER_IMG_NAME[args.target],
            args.dist_name,
            args.dist_ver)
    else:
        print("Error: Target, such as '-t dpdk' or '-t spp' is required!")
        exit()

    # Decide which of Dockerfile with given base image version
    dockerfile = '%s/Dockerfile.%s' % (dockerfile_dir, args.dist_ver)

    # Overwrite container's name if it is given.
    if args.container_image is not None:
        container_image = args.container_image

    # Setup branches if user specifies.
    if args.dpdk_branch is not None:
        dpdk_branch = "-b %s" % args.dpdk_branch
    else:
        dpdk_branch = ''

    if args.pktgen_branch is not None:
        pktgen_branch = "-b %s" % args.pktgen_branch
    else:
        pktgen_branch = ''

    if args.spp_branch is not None:
        spp_branch = "-b %s" % args.spp_branch
    else:
        spp_branch = ''

    if args.suricata_branch is not None:
        suricata_branch = "-b %s" % args.suricata_branch
    else:
        suricata_branch = ''

    # Setup project directory by extracting from any of git URL.
    # If DPDK is hosted on 'https://github.com/user/custom-dpdk.git',
    # the directory is 'custom-dpdk'.
    dpdk_dir = args.dpdk_repo.split('/')[-1].split('.')[0]
    pktgen_dir = args.pktgen_repo.split('/')[-1].split('.')[0]
    spp_dir = args.spp_repo.split('/')[-1].split('.')[0]

    # NOTE: Suricata has sub-directory as project root.
    suricata_ver = '4.1.4'
    suricata_dir = '{}/suricata-{}'.format(
            args.suricata_repo.split('/')[-1].split('.')[0], suricata_ver)

    # RTE_SDK is decided with DPDK's dir.
    rte_sdk = '%s/%s' % (env.HOMEDIR, dpdk_dir)

    # Check for just creating env.sh, or run docker build.
    if args.only_envsh is True:
        if args.dry_run is False:
            if args.target == 'pktgen':
                create_env_sh(dockerfile_dir, rte_sdk, args.target, pktgen_dir)
            elif args.target == 'spp':
                create_env_sh(dockerfile_dir, rte_sdk, args.target, spp_dir)
            elif args.target == 'suricata':
                create_env_sh(dockerfile_dir, rte_sdk, args.target, suricata_dir)
            elif args.target == 'dpdk':
                create_env_sh(dockerfile_dir, rte_sdk, args.target, dpdk_dir)
            print("Info: '%s/env.sh' created." % dockerfile_dir)
            exit()
        else:
            print("Info: Nothin done because you gave %s with %s." % (
                '--only-envsh', '--dry-run'))
            exit()
    else:
        if args.target == 'pktgen':
            create_env_sh(dockerfile_dir, rte_sdk, args.target, pktgen_dir)
        elif args.target == 'spp':
            create_env_sh(dockerfile_dir, rte_sdk, args.target, spp_dir)
        elif args.target == 'suricata':
            create_env_sh(dockerfile_dir, rte_sdk, args.target, suricata_dir)
        elif args.target == 'dpdk':
            create_env_sh(dockerfile_dir, rte_sdk, args.target, dpdk_dir)

    # Setup environment variables on host to pass 'docker build'.
    env_opts = [
        'http_proxy',
        'https_proxy',
        'no_proxy'
    ]

    docker_cmd = ['sudo', 'docker', 'build', '\\']

    for opt in env_opts:
        if opt in os.environ.keys():
            docker_cmd += [
                '--build-arg', '%s=%s' % (opt, os.environ[opt]), '\\']

    docker_cmd += [
        '--build-arg', 'home_dir=%s' % env.HOMEDIR, '\\',
        '--build-arg', 'rte_sdk=%s' % rte_sdk, '\\',
        '--build-arg', 'rte_target=%s' % env.RTE_TARGET, '\\',
        '--build-arg', 'dpdk_repo=%s' % args.dpdk_repo, '\\',
        '--build-arg', 'dpdk_branch=%s' % dpdk_branch, '\\']

    if args.target == 'pktgen':
        docker_cmd += [
            '--build-arg', 'pktgen_repo=%s' % args.pktgen_repo, '\\',
            '--build-arg', 'pktgen_branch=%s' % pktgen_branch, '\\',
            '--build-arg', 'pktgen_dir=%s' % pktgen_dir, '\\']
    elif args.target == 'spp':
        docker_cmd += [
            '--build-arg', 'spp_repo=%s' % args.spp_repo, '\\',
            '--build-arg', 'spp_branch=%s' % spp_branch, '\\',
            '--build-arg', 'spp_dir=%s' % spp_dir, '\\']
    elif args.target == 'suricata':
        docker_cmd += [
            '--build-arg', 'suricata_repo=%s' % args.suricata_repo, '\\',
            '--build-arg', 'suricata_branch=%s' % suricata_branch, '\\',
            '--build-arg', 'suricata_dir=%s' % suricata_dir, '\\']

    docker_cmd += [
        '-f', '%s' % dockerfile, '\\',
        '-t', container_image, '\\',
        dockerfile_dir]

    common.print_pretty_commands(docker_cmd)

    if args.dry_run is True:
        exit()

    # Remove delimiters for print_pretty_commands().
    while '\\' in docker_cmd:
        docker_cmd.remove('\\')
    subprocess.call(docker_cmd)


if __name__ == '__main__':
    main()
