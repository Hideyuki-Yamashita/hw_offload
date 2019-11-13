#!/usr/bin/env python
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2014 Intel Corporation
# Copyright(c) 2017 Cavium, Inc. All rights reserved.
# Copyright(c) 2019 Nippon Telegraph and Telephone Corporation

from __future__ import print_function
import argparse
import json

try:
    xrange  # Python 2
except NameError:
    xrange = range  # Python 3

BASE_PATH = "/sys/devices/system/cpu"


def parse_args():
    parser = argparse.ArgumentParser(
        description="Show CPU layout")
    parser.add_argument(
        "--json", action="store_true",
        help="Output in JSON format")
    return parser.parse_args()


def get_max_cpus():
    fd = open("{}/kernel_max".format(BASE_PATH))
    max_cpus = int(fd.read())
    fd.close()
    return max_cpus


def get_resource_info(max_cpus):
    """Return a set of sockets, cores and core_map as tuple."""
    sockets = []
    cores = []
    core_map = {}

    for cpu in xrange(max_cpus + 1):
        try:
            topo_path = "{}/cpu{}/topology".format(BASE_PATH, cpu)

            # Get physical core ID.
            fd = open("{}/core_id".format(topo_path))
            core = int(fd.read())
            fd.close()
            if core not in cores:
                cores.append(core)

            fd = open("{}/physical_package_id".format(topo_path))
            socket = int(fd.read())
            fd.close()
            if socket not in sockets:
                sockets.append(socket)

            key = (socket, core)
            if key not in core_map:
                core_map[key] = []
            core_map[key].append(cpu)

        except IOError:
            continue

        except Exception as e:
            print(e)
            break

    return sockets, cores, core_map


def print_header(cores, sockets):
    print(format("=" * (47 + len(BASE_PATH))))
    print("Core and Socket Information (as reported by '{}')".format(
        BASE_PATH))
    print("{}\n".format("=" * (47 + len(BASE_PATH))))
    print("cores = ", cores)
    print("sockets = ", sockets)
    print("")


def print_body(cores, sockets, core_map):
    max_processor_len = len(str(len(cores) * len(sockets) * 2 - 1))
    max_thread_count = len(list(core_map.values())[0])
    max_core_map_len = (max_processor_len * max_thread_count) \
        + len(", ") * (max_thread_count - 1) \
        + len('[]') + len('Socket ')
    max_core_id_len = len(str(max(cores)))

    output = " ".ljust(max_core_id_len + len('Core '))
    for s in sockets:
        output += " Socket %s" % str(s).ljust(
            max_core_map_len - len('Socket '))
    print(output)

    output = " ".ljust(max_core_id_len + len('Core '))
    for s in sockets:
        output += " --------".ljust(max_core_map_len)
        output += " "
    print(output)

    for c in cores:
        output = "Core %s" % str(c).ljust(max_core_id_len)
        for s in sockets:
            if (s, c) in core_map:
                output += " " + str(core_map[(s, c)]).ljust(max_core_map_len)
            else:
                output += " " * (max_core_map_len + 1)
        print(output)


def core_map_to_json(core_map):
    cpu_layout = []
    cpu_sockets = {}
    for (s, c), cpus in core_map.items():
        if not s in cpu_sockets:
            cpu_sockets[s] = {}
            cpu_sockets[s]["cores"] = []
        cpu_sockets[s]["cores"].append({"core_id": c, "lcores": cpus})

    for sid, val in cpu_sockets.items():
        cpu_layout.append({"socket_id": sid, "cores": val["cores"]})

    return json.dumps(cpu_layout)


def main():
    args = parse_args()

    max_cpus = get_max_cpus()
    sockets, cores, core_map = get_resource_info(max_cpus)

    if args.json is True:
        print(core_map_to_json(core_map))

    else:
        print_header(cores, sockets)
        print_body(cores, sockets, core_map)


if __name__ == '__main__':
    main()
