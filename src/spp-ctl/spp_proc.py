# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Nippon Telegraph and Telephone Corporation

import bottle
import eventlet
import json
import logging

import spp_ctl


LOG = logging.getLogger(__name__)

# TODO(yasufum): Move following definitions to another file.
ID_PRIMARY = 0
TYPE_PRIMARY = "primary"
TYPE_VF = "vf"
TYPE_NFV = "nfv"
TYPE_MIRROR = "mirror"
TYPE_PCAP = "pcap"

EAL_OPTS = [
        # Lcore-related options
        '-c',  # core mask
        '-l',  # core list
        '--lcores',  # core map
        '--master-lcore',  # is used as master
        '-s',  # Hex bitmask of cores used as service cores
        # Device-related options
        '-b', '--pci-blacklist',  # Blacklist of PCI devs
        '-w', '--pci-whitelist',  # Blacklist of PCI devs
        '--vdev',  # Add a virtual device
        '-d',  # Load external drivers
        '--no-pci',  # Disable PCI bus.
        # Multiprocessing-related options
        '--proc-type',  # primamry, secondary or auto
        # Memory-related options
        '-n',  # number of memory channels
        '-r',  # number of memory ranks
        '-m',  # Amount of memory to preallocate at startup
        '--in-memory',  # not create shared data and run entirely in memory
        '--iova-mode',  # Force IOVA mode to a specific value
        # Debugging options
        '--no-shconf',  # No shared files created (no sec)
        '--no-huge',  # Use anonymous memory instead of hugepages (no sec)
        '--log-level',   # Specify log level, e.g. '--log-level eal:8'
        '--file-prefix',  # Use different shared file prefix for a DPDK process
        # Linux-specific EAL parameters
        '--create-uio-dev',  # Create /dev/uioX bound to igb_uio
        '--vmware-tsc-map',  # Use VMware TSC map instead of native RDTSC
        '--no-hpet',  # Do not use the HPET timer
        '--vfio-intr',  # Use specified interrupt mode for devs bound to VFIO
        '--base-virtaddr',  # use different addr for all memmaps of primary
        '--legacy-mem',  # Use legacy DPDK memory allocation mode
        '--socket-mem',  # Preallocate memory per socket
        '--socket-limit',  # Place a per-socket upper limit on memory
        '--single-file-segments',  # Create fewer files in hugetlbfs
        '--huge-dir',  # Use specified hugetlbfs instead of autodetected
        '--huge-unlink',  # Unlink hugepage files after creating
        '--match-allocations',  # Free hugepages back as original
        '--syslog'  # syslog facility
        ]

APP_OPTS = {
        'spp_nfv':
        [
            '-n',  # sec ID
            '-s',  # address and port
            '--vhost-client'  # enable client mode
            ],
        'spp_vf':
        [
            '--client-id',  # sec ID
            '-s',  # address nd port
            '--vhost-client'  # enable client mode
            ],
        'spp_mirror':
        [
            '--client-id',  # sec ID
            '-s',  # address nd port
            '--vhost-client'  # enable client mode
            ],
        'spp_pcap':
        [
            '--client-id',  # sec ID
            '-s',  # address and port
            '-c',  # captured port
            '--out-dir',  # captured file dir
            '--fsize'  # max size of captured file
            ]}


def exec_command(func):
    """Decorator for Sending command and receiving reply.

    Define the common function for sending command and receiving reply
    as a decorator. Each of methods for executing command has only to
    return command string.

    exp)
    @exec_command
    def some_command(self, ...):
        return "command string of some_command"
    """
    def wrapper(self, *args, **kwargs):
        with self.sem:
            command = func(self, *args, **kwargs)
            LOG.info("%s(%d) command executed: %s", self.type, self.id,
                     command)
            data = spp_ctl.Controller._send_command(self.conn, command)
            if data is None:
                raise RuntimeError("%s(%d): %s: no-data returned" %
                                   (self.type, self.id, command))
            LOG.debug("reply: %s", data.replace('\0', ''))
            return self._decode_reply(data)
    return wrapper


class SppProc(object):
    def __init__(self, proc_type, id, conn):
        self.id = id
        self.type = proc_type
        # NOTE: executing command is serialized by using a semaphore
        # for each process.
        self.sem = eventlet.semaphore.Semaphore(value=1)
        self.conn = conn

    @staticmethod
    def _decode_reply(data):
        # Remove '\0' in msg from secondary process to avoid error.
        try:
            data = json.loads(data.replace('\0', ''))

            if "results" in data.keys():  # msg ffrom spp_vf
                result = data["results"][0]
                if result["result"] == "error":
                    msg = result["error_details"]["message"]
                    raise bottle.HTTPError(400, "command error: %s" % msg)

            return data

        except json.JSONDecodeError as e:
            LOG.error("'{}' in JSON decoding.".format(e))

        LOG.debug("Reply msg is not JSON format '{data}'.".format(**locals()))
        return data

    @staticmethod
    def _decode_client_id_common(data, proc_type):
        try:
            data = SppProc._decode_reply(data)
            if data["process_type"] == proc_type:
                return data["client_id"]
        except Exception as e:
            LOG.error(e)
            return None


class VfCommon(SppProc):

    def __init__(self, proc_type, id, conn):
        super(VfCommon, self).__init__(proc_type, id, conn)

    @exec_command
    def get_status(self):
        return "status"

    @exec_command
    def start_component(self, comp_name, core_id, comp_type):
        return ("component start {comp_name} {core_id} {comp_type}"
                .format(**locals()))

    @exec_command
    def stop_component(self, comp_name):
        return "component stop {comp_name}".format(**locals())

    @exec_command
    def port_del(self, port, direction, comp_name):
        return "port del {port} {direction} {comp_name}".format(**locals())

    @exec_command
    def do_exit(self):
        return "exit"


class VfProc(VfCommon):

    def __init__(self, id, conn):
        super(VfProc, self).__init__(TYPE_VF, id, conn)

    @staticmethod
    def _decode_client_id(data):
        return SppProc._decode_client_id_common(data, TYPE_VF)

    @exec_command
    def port_add(self, port, direction, comp_name, op, vlan_id, pcp):
        command = "port add {port} {direction} {comp_name}".format(**locals())
        if op != "none":
            command += " %s" % op
            if op == "add_vlantag":
                command += " %d %d" % (vlan_id, pcp)
        return command

    @exec_command
    def set_classifier_table(self, mac_address, port):
        return ("classifier_table add mac {mac_address} {port}"
                .format(**locals()))

    @exec_command
    def clear_classifier_table(self, mac_address, port):
        return ("classifier_table del mac {mac_address} {port}"
                .format(**locals()))

    @exec_command
    def set_classifier_table_with_vlan(self, mac_address, port,
                                       vlan_id):
        return ("classifier_table add vlan {vlan_id} {mac_address} {port}"
                .format(**locals()))

    @exec_command
    def clear_classifier_table_with_vlan(self, mac_address, port,
                                         vlan_id):
        return ("classifier_table del vlan {vlan_id} {mac_address} {port}"
                .format(**locals()))


class MirrorProc(VfCommon):

    def __init__(self, id, conn):
        super(MirrorProc, self).__init__(TYPE_MIRROR, id, conn)

    @staticmethod
    def _decode_client_id(data):
        return SppProc._decode_client_id_common(data, TYPE_MIRROR)

    @exec_command
    def port_add(self, port, direction, comp_name):
        return "port add {port} {direction} {comp_name}".format(**locals())


class NfvProc(SppProc):

    def __init__(self, id, conn):
        super(NfvProc, self).__init__(TYPE_NFV, id, conn)

    @staticmethod
    def _decode_client_id(data):
        return SppProc._decode_client_id_common(data, TYPE_NFV)

    @exec_command
    def get_status(self):
        return "status"

    @exec_command
    def port_add(self, port):
        return "add {port}".format(**locals())

    @exec_command
    def port_del(self, port):
        return "del {port}".format(**locals())

    @exec_command
    def patch_add(self, src_port, dst_port):
        return "patch {src_port} {dst_port}".format(**locals())

    @exec_command
    def patch_reset(self):
        return "patch reset"

    @exec_command
    def forward(self):
        return "forward"

    @exec_command
    def stop(self):
        return "stop"

    @exec_command
    def do_exit(self):
        return "exit"


class PrimaryProc(SppProc):

    def __init__(self, conn):
        super(PrimaryProc, self).__init__(TYPE_PRIMARY, ID_PRIMARY, conn)

    @exec_command
    def get_status(self):
        return "status"

    @exec_command
    def clear(self):
        return "clear"

    @exec_command
    def port_add(self, port):
        return "add {port}".format(**locals())

    @exec_command
    def port_del(self, port):
        return "del {port}".format(**locals())

    @exec_command
    def patch_add(self, src_port, dst_port):
        return "patch {src_port} {dst_port}".format(**locals())

    @exec_command
    def patch_reset(self):
        return "patch reset"

    @exec_command
    def forward(self):
        return "forward"

    @exec_command
    def stop(self):
        return "stop"

    @exec_command
    def do_launch_sec_proc(self, args):
        proc_name = args['proc_name']
        sec_id = args['client_id']

        eal_opts = []
        app_opts = []

        # EAL options
        # Check mandatory options
        mandatory = False
        for key in ['-c', '-l', '--lcores']:
            if key in args['eal'].keys():
                mandatory = True
                break
        if mandatory is False:
            return None

        if '--proc-type' not in args['eal'].keys():
            return None

        for opt in EAL_OPTS:
            if opt in args['eal'].keys():
                eal_opts.append(opt)
                val = args['eal'][opt]
                if (val is not None) and (val != ''):
                    eal_opts.append(str(val))

        if proc_name in APP_OPTS.keys():
            for opt in APP_OPTS[proc_name]:
                if opt in args['app'].keys():
                    app_opts.append(opt)
                    val = args['app'][opt]
                    if (val is not None) and (val != ''):
                        app_opts.append(str(val))

        query = "launch {sid} {pname} {eal} -- {app}".format(
                sid=sec_id, pname=proc_name, eal=' '.join(eal_opts),
                app=' '.join(app_opts))

        LOG.info("Query: {}".format(query))

        return query

    @exec_command
    def do_exit(self):
        return "exit"


class PcapProc(SppProc):

    def __init__(self, id, conn):
        super(PcapProc, self).__init__(TYPE_PCAP, id, conn)

    @staticmethod
    def _decode_client_id(data):
        return SppProc._decode_client_id_common(data, TYPE_PCAP)

    @exec_command
    def get_status(self):
        return "status"

    @exec_command
    def start(self):
        return "start"

    @exec_command
    def stop(self):
        return "stop"

    @exec_command
    def do_exit(self):
        return "exit"
