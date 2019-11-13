# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Nippon Telegraph and Telephone Corporation

import eventlet
eventlet.monkey_patch()

import argparse
import errno
import json
import logging
import os
import socket
import subprocess

import spp_proc
import spp_webapi


LOG = logging.getLogger(__name__)

MSG_SIZE = 4096

# relative path of `cpu_layout.py`
CPU_LAYOUT_TOOL = 'tools/helpers/cpu_layout.py'


class Controller(object):

    def __init__(self, host, pri_port, sec_port, api_port):
        self.web_server = spp_webapi.WebServer(self, host, api_port)
        self.procs = {}
        self.ip_addr = host
        self.init_connection(pri_port, sec_port)

    def start(self):
        self.web_server.start()

    def init_connection(self, pri_port, sec_port):
        self.pri_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.pri_sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.pri_sock.bind((self.ip_addr, pri_port))
        self.pri_sock.listen(1)
        self.primary_listen_thread = eventlet.greenthread.spawn(
            self.accept_primary)

        self.sec_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.sec_sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.sec_sock.bind((self.ip_addr, sec_port))
        self.sec_sock.listen(1)
        self.secondary_listen_thread = eventlet.greenthread.spawn(
            self.accept_secondary)

    def accept_primary(self):
        while True:
            conn, _ = self.pri_sock.accept()
            proc = self.procs.get(spp_proc.ID_PRIMARY)
            if proc is not None:
                LOG.warning("spp_primary reconnect !")
                with proc.sem:
                    try:
                        proc.conn.close()
                    except Exception:
                        pass
                    proc.conn = conn
                # NOTE: when spp_primary restart, all secondarys must be
                # restarted. this is out of controle of spp-ctl.
            else:
                LOG.info("primary connected.")
                self.procs[spp_proc.ID_PRIMARY] = spp_proc.PrimaryProc(conn)

    def accept_secondary(self):
        while True:
            conn, _ = self.sec_sock.accept()
            LOG.debug("sec accepted: get process id")
            proc = self._get_proc(conn)
            if proc is None:
                LOG.error("get process id failed")
                conn.close()
                continue
            old_proc = self.procs.get(proc.id)
            if old_proc:
                LOG.warning("%s(%d) reconnect !", old_proc.type, old_proc.id)
                if old_proc.type != proc.type:
                    LOG.warning("type changed ! new type: %s", proc.type)
                with old_proc.sem:
                    try:
                        old_proc.conn.close()
                    except Exception:
                        pass
            else:
                LOG.info("%s(%d) connected.", proc.type, proc.id)
            self.procs[proc.id] = proc

    @staticmethod
    def _continue_recv(conn):
        try:
            # must set non-blocking to recieve remining data not to happen
            # blocking here.
            # NOTE: usually MSG_DONTWAIT flag is used for this purpose but
            # this flag is not supported under eventlet.
            conn.setblocking(False)
            data = b""
            while True:
                try:
                    rcv_data = conn.recv(MSG_SIZE)
                    data += rcv_data
                    if len(rcv_data) < MSG_SIZE:
                        break
                except socket.error as e:
                    if e.args[0] == errno.EAGAIN:
                        # OK, no data remining. this happens when recieve data
                        # length is just multiple of MSG_SIZE.
                        break
                    raise e
            return data
        finally:
            conn.setblocking(True)

    @staticmethod
    def _send_command(conn, command):
        data = None
        try:
            conn.sendall(command.encode())
            data = conn.recv(MSG_SIZE)
            if data and len(data) == MSG_SIZE:
                # could not receive data at once. recieve remining data.
                data += self._continue_recv(conn)
            if data:
                data = data.decode()
        except Exception as e:
            LOG.info("Error: {}".format(e))
        return data

    def _get_proc(self, conn):
        # it is a bit ad hoc. send "_get_clinet_id" command and try to
        # decode reply for each proc type. if success, that is the type.
        data = self._send_command(conn, "_get_client_id")
        for proc in [spp_proc.VfProc, spp_proc.NfvProc, spp_proc.MirrorProc,
                     spp_proc.PcapProc]:
            sec_id = proc._decode_client_id(data)
            if sec_id is not None:
                return proc(sec_id, conn)

    def _update_procs(self):
        """Remove no existing processes from `self.procs`."""

        removed_ids = []
        for idx, proc in self.procs.items():
            if proc.id != spp_proc.ID_PRIMARY:
                try:
                    # Check the process can be accessed. If not, go
                    # to except block.
                    proc.get_status()
                except Exception as e:
                    LOG.error(e)
                    removed_ids.append(idx)
        for idx in removed_ids:
            LOG.info("Remove no existing {}:{}.".format(
                self.procs[idx].type, self.procs[idx].id))
            del self.procs[idx]

    def get_processes(self):
        procs = []
        self._update_procs()
        for proc in self.procs.values():
            p = {"type": proc.type}
            if proc.id != spp_proc.ID_PRIMARY:
                p["client-id"] = proc.id
            procs.append(p)
        return procs

    def get_cpu_usage(self):
        """Get cpu usage from each of status of SPP processes.

        If process returns invalid message or cannot connect, remove
        it from `self.procs` as in _update_procs().
        """

        removed_ids = []
        cpus = []
        for idx, proc in self.procs.items():
            try:
                # Check the process can be accessed. If not, go
                # to except block.
                stat = proc.get_status()
                if proc.id == spp_proc.ID_PRIMARY:
                    cpus.append(
                            {'proc-type': proc.type,
                                'master-lcore': stat['lcores'][0],
                                'lcores': stat['lcores']})
                elif proc.type == 'nfv':
                    cpus.append(
                            {'proc-type': proc.type,
                                'client-id': proc.id,
                                'master-lcore': stat['master-lcore'],
                                'lcores': stat['lcores']})
                elif proc.type in ['vf', 'mirror', 'pcap']:
                    master_lcore = stat['info']['master-lcore']
                    lcores = [stat['info']['master-lcore']]
                    # TODO(yasufum) revise tag name 'core'.
                    for val in stat['info']['core']:
                        lcores.append(val['core'])
                    cpus.append(
                            {'proc-type': proc.type,
                                'client-id': proc.id,
                                'master-lcore': master_lcore,
                                'lcores': lcores})
                else:
                    LOG.debug('No supported proc type: {}'.format(
                        roc.type))

            except Exception as e:
                LOG.error("get_cpu_usage: {}".format(e))
                removed_ids.append(idx)

        for idx in removed_ids:
            LOG.info("Remove no existing {}:{}.".format(
                self.procs[idx].type, self.procs[idx].id))
            del self.procs[idx]

        return cpus

    def get_cpu_layout(self):
        """Get cpu layout with helper tool 'cpu_layout.py'."""

        # This script is 'src/spp-ctl/spp_ctl.py' and it expect to find
        # the tool in tools/helpers/cpu_layout.py'.
        cmd_path = "{}/../../{}".format(
                os.path.dirname(__file__), CPU_LAYOUT_TOOL)

        if os.path.exists(cmd_path):
            # Get cpu layout as bytes of JSON foramtted string
            cmd_res = subprocess.check_output(
                    [cmd_path, '--json'],  # required '--json' option
                    stderr=subprocess.STDOUT)

            # Decode bytes to str
            return json.loads(cmd_res.decode('utf-8'))

        else:
            LOG.error("'{}' cannot be found.".format(CPU_LAYOUT_TOOL))
            return None

    def do_exit(self, proc_type, proc_id):
        removed_id = None  # remove proc info of ID from self.procs
        for proc in self.procs.values():
            if proc.type == proc_type and proc.id == proc_id:
                removed_id = proc.id
                break
        if removed_id is not None:
            del self.procs[removed_id]


def main():
    parser = argparse.ArgumentParser(description="SPP Controller")
    parser.add_argument("-b", '--bind-addr', type=str, default='localhost',
                        help="bind address, default=localhost")
    parser.add_argument("-p", dest='pri_port', type=int, default=5555,
                        action='store', help="primary port, default=5555")
    parser.add_argument("-s", dest='sec_port', type=int, default=6666,
                        action='store', help="secondary port, default=6666")
    parser.add_argument("-a", dest='api_port', type=int, default=7777,
                        action='store', help="web api port, default=7777")
    args = parser.parse_args()

    logging.basicConfig(level=logging.DEBUG)

    controller = Controller(args.bind_addr, args.pri_port, args.sec_port,
                            args.api_port)
    controller.start()
