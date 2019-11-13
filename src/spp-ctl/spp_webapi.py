# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Nippon Telegraph and Telephone Corporation

import bottle
import errno
import json
import logging
import netaddr
import re
import socket
import subprocess
import sys

import spp_proc

PORT_TYPES = ["phy", "vhost", "ring", "pcap", "nullpmd", "tap"]
VF_PORT_TYPES = ["phy", "vhost", "ring"]
# TODO(yasufum) consider PCAP_PORT_TYPES is required.

LOG = logging.getLogger(__name__)


class KeyRequired(bottle.HTTPError):

    def __init__(self, key):
        msg = "key(%s) required." % key
        super(KeyRequired, self).__init__(400, msg)


class KeyInvalid(bottle.HTTPError):

    def __init__(self, key, value):
        msg = "invalid key(%s): %s." % (key, value)
        super(KeyRequired, self).__init__(400, msg)


class BaseHandler(bottle.Bottle):
    """Define common methods for each handler."""

    def __init__(self, controller):
        super(BaseHandler, self).__init__()
        self.ctrl = controller

        self.default_error_handler = self._error_handler
        bottle.response.default_status = 404

    def _error_handler(self, res):
        # use "text/plain" as content_type rather than bottle's default
        # "html".
        res.content_type = "text/plain"
        return res.body

    def _validate_port(self, port):
        try:
            if_type, if_num = port.split(":")
            if if_type not in PORT_TYPES:
                raise
            int(if_num)
        except Exception:
            raise KeyInvalid('port', port)

    def log_url(self):
        LOG.info("%s %s called", bottle.request.method, bottle.request.path)

    def log_response(self):
        LOG.info("response: %s", bottle.response.status)

    # following three decorators do common works for each API.
    # each handler 'install' appropriate decorators.
    #
    def get_body(self, func):
        """Get body and set it to method argument.
        content-type is OK whether application/json or plain text.
        """
        def wrapper(*args, **kwargs):
            req = bottle.request
            if req.method in ["POST", "PUT"]:
                if req.get_header('Content-Type') == "application/json":
                    body = req.json
                else:
                    body = json.loads(req.body.read().decode())
                kwargs['body'] = body
                LOG.info("body: %s", body)
            return func(*args, **kwargs)
        return wrapper

    def check_sec_id(self, func):
        """Get and check proc and set it to method argument."""
        def wrapper(*args, **kwargs):
            sec_id = kwargs.pop('sec_id', None)
            if sec_id is not None:
                proc = self.ctrl.procs.get(sec_id)
                if proc is None or proc.type != self.type:
                    raise bottle.HTTPError(404,
                                           "sec_id %d not found." % sec_id)
                kwargs['proc'] = proc
            return func(*args, **kwargs)
        return wrapper

    def make_response(self, func):
        """Convert plain response to bottle.HTTPResponse."""
        def wrapper(*args, **kwargs):
            ret = func(*args, **kwargs)
            if ret is None:
                return bottle.HTTPResponse(status=204)
            else:
                r = bottle.HTTPResponse(status=200, body=json.dumps(ret))
                r.content_type = "application/json"
                return r
        return wrapper


class WebServer(BaseHandler):
    """Top level handler.

    handlers are hierarchized using 'mount' as follows:
    /          WebServer
    /v1          V1Handler
       /vfs        V1VFHandler
       /mirrors    V1MirrorHandler
       /nfvs       V1NFVHandler
       /primary    V1PrimaryHandler
       /pcaps      V1PcapHandler
    """

    def __init__(self, controller, host, api_port):
        super(WebServer, self).__init__(controller)
        self.host = host
        self.api_port = api_port

        self.mount("/v1", V1Handler(controller))

        # request and response logging.
        self.add_hook("before_request", self.log_url)
        self.add_hook("after_request", self.log_response)

    def start(self):
        self.run(server='eventlet', host=self.host, port=self.api_port,
                 quiet=True)


class V1Handler(BaseHandler):
    def __init__(self, controller):
        super(V1Handler, self).__init__(controller)

        self.set_route()

        self.mount("/vfs", V1VFHandler(controller))
        self.mount("/mirrors", V1MirrorHandler(controller))
        self.mount("/nfvs", V1NFVHandler(controller))
        self.mount("/primary", V1PrimaryHandler(controller))
        self.mount("/pcaps", V1PcapHandler(controller))

        self.install(self.make_response)

    def set_route(self):
        self.route('/processes', 'GET', callback=self.get_processes)
        self.route('/cpu_usage', 'GET', callback=self.get_cpu_usage)
        self.route('/cpu_layout', 'GET', callback=self.get_cpu_layout)

    def get_processes(self):
        LOG.info("get processes called.")
        return self.ctrl.get_processes()

    def get_cpu_usage(self):
        LOG.info("get cpu usage called.")
        return self.ctrl.get_cpu_usage()

    def get_cpu_layout(self):
        LOG.info("get cpu layout called.")
        return self.ctrl.get_cpu_layout()


class V1VFCommon(object):
    """Define common methods for vf and mirror handler."""

    def convert_info(self, data):
        info = data["info"]
        vf = {}
        vf["client-id"] = info["client-id"]
        vf["ports"] = []
        for key in VF_PORT_TYPES:
            for idx in info[key]:
                vf["ports"].append(key + ":" + str(idx))
        vf["master-lcore"] = info["master-lcore"]
        vf["components"] = info["core"]
        if "classifier_table" in info:
            vf["classifier_table"] = info["classifier_table"]

        return vf

    def validate_comp_start(self, body, types):
        for key in ['name', 'core', 'type']:
            if key not in body:
                raise KeyRequired(key)
        if not isinstance(body['name'], str):
            raise KeyInvalid('name', body['name'])
        if not isinstance(body['core'], int):
            raise KeyInvalid('core', body['core'])
        if body['type'] not in types:
            raise KeyInvalid('type', body['type'])

    def validate_comp_port(self, body):
        for key in ['action', 'port', 'dir']:
            if key not in body:
                raise KeyRequired(key)
        if body['action'] not in ["attach", "detach"]:
            raise KeyInvalid('action', body['action'])
        if body['dir'] not in ["rx", "tx"]:
            raise KeyInvalid('dir', body['dir'])
        self._validate_port(body['port'])

    def vf_exit(self, proc):
        self.ctrl.do_exit(proc.type, proc.id)
        proc.do_exit()


class V1VFHandler(BaseHandler, V1VFCommon):

    def __init__(self, controller):
        super(V1VFHandler, self).__init__(controller)
        self.type = spp_proc.TYPE_VF

        self.set_route()

        self.install(self.check_sec_id)
        self.install(self.get_body)
        self.install(self.make_response)

    def set_route(self):
        self.route('/<sec_id:int>', 'GET', callback=self.vf_get)
        self.route('/<sec_id:int>', 'DELETE', callback=self.vf_exit)
        self.route('/<sec_id:int>/components', 'POST',
                   callback=self.vf_comp_start)
        self.route('/<sec_id:int>/components/<name>', 'DELETE',
                   callback=self.vf_comp_stop)
        self.route('/<sec_id:int>/components/<name>/ports', 'PUT',
                   callback=self.vf_comp_port)
        self.route('/<sec_id:int>/classifier_table', 'PUT',
                   callback=self.vf_classifier)

    def vf_get(self, proc):
        return self.convert_info(proc.get_status())

    def vf_comp_start(self, proc, body):
        self.validate_comp_start(body, ["forward", "merge", "classifier"])
        proc.start_component(body['name'], body['core'], body['type'])

    def vf_comp_stop(self, proc, name):
        proc.stop_component(name)

    def _validate_vf_comp_port(self, body):
        self.validate_comp_port(body)
        if body['action'] == "attach":
            vlan = body.get('vlan')
            if vlan:
                try:
                    if vlan['operation'] not in ["none", "add", "del"]:
                        raise
                    if vlan['operation'] == "add":
                        int(vlan['id'])
                        int(vlan['pcp'])
                except Exception:
                    raise KeyInvalid('vlan', vlan)

    def vf_comp_port(self, proc, name, body):
        self._validate_vf_comp_port(body)

        if body['action'] == "attach":
            op = "none"
            vlan_id = 0
            pcp = 0
            vlan = body.get('vlan')
            if vlan:
                if vlan['operation'] == "add":
                    op = "add_vlantag"
                    vlan_id = vlan['id']
                    pcp = vlan['pcp']
                elif vlan['operation'] == "del":
                    op = "del_vlantag"
            proc.port_add(body['port'], body['dir'],
                          name, op, vlan_id, pcp)
        else:
            proc.port_del(body['port'], body['dir'], name)

    def _validate_mac(self, mac_address):
        try:
            netaddr.EUI(mac_address)
        except Exception:
            raise KeyInvalid('mac_address', mac_address)

    def _validate_vf_classifier(self, body):
        for key in ['action', 'type', 'port', 'mac_address']:
            if key not in body:
                raise KeyRequired(key)
        if body['action'] not in ["add", "del"]:
            raise KeyInvalid('action', body['action'])
        if body['type'] not in ["mac", "vlan"]:
            raise KeyInvalid('type', body['type'])
        self._validate_port(body['port'])

        if not body['mac_address'] == 'default':
            self._validate_mac(body['mac_address'])

        if body['type'] == "vlan":
            try:
                int(body['vlan'])
            except Exception:
                raise KeyInvalid('vlan', body.get('vlan'))

    def vf_classifier(self, proc, body):
        self._validate_vf_classifier(body)

        port = body['port']
        mac_address = body['mac_address']

        if body['action'] == "add":
            if body['type'] == "mac":
                proc.set_classifier_table(mac_address, port)
            else:
                proc.set_classifier_table_with_vlan(
                    mac_address, port, body['vlan'])
        else:
            if body['type'] == "mac":
                proc.clear_classifier_table(mac_address, port)
            else:
                proc.clear_classifier_table_with_vlan(
                    mac_address, port, body['vlan'])


class V1MirrorHandler(BaseHandler, V1VFCommon):

    def __init__(self, controller):
        super(V1MirrorHandler, self).__init__(controller)
        self.type = spp_proc.TYPE_MIRROR

        self.set_route()

        self.install(self.check_sec_id)
        self.install(self.get_body)
        self.install(self.make_response)

    def set_route(self):
        self.route('/<sec_id:int>', 'GET', callback=self.mirror_get)
        self.route('/<sec_id:int>', 'DELETE', callback=self.vf_exit)
        self.route('/<sec_id:int>/components', 'POST',
                   callback=self.mirror_comp_start)
        self.route('/<sec_id:int>/components/<name>', 'DELETE',
                   callback=self.mirror_comp_stop)
        self.route('/<sec_id:int>/components/<name>/ports', 'PUT',
                   callback=self.mirror_comp_port)

    def mirror_get(self, proc):
        return self.convert_info(proc.get_status())

    def mirror_comp_start(self, proc, body):
        self.validate_comp_start(body, ["mirror"])
        proc.start_component(body['name'], body['core'], body['type'])

    def mirror_comp_stop(self, proc, name):
        proc.stop_component(name)

    def mirror_comp_port(self, proc, name, body):
        self.validate_comp_port(body)
        if body['action'] == "attach":
            proc.port_add(body['port'], body['dir'], name)
        else:
            proc.port_del(body['port'], body['dir'], name)


class V1NFVHandler(BaseHandler):

    def __init__(self, controller):
        super(V1NFVHandler, self).__init__(controller)
        self.type = spp_proc.TYPE_NFV

        self.set_route()

        self.install(self.check_sec_id)
        self.install(self.get_body)
        self.install(self.make_response)

    def set_route(self):
        self.route('/<sec_id:int>', 'GET', callback=self.nfv_get)
        self.route('/<sec_id:int>', 'DELETE', callback=self.nfv_exit)
        self.route('/<sec_id:int>/forward', 'PUT',
                   callback=self.nfv_forward)
        self.route('/<sec_id:int>/ports', 'PUT',
                   callback=self.nfv_port)
        self.route('/<sec_id:int>/patches', 'PUT',
                   callback=self.nfv_patch_add)
        self.route('/<sec_id:int>/patches', 'DELETE',
                   callback=self.nfv_patch_del)

    def nfv_get(self, proc):
        return proc.get_status()

    def _validate_nfv_forward(self, body):
        if 'action' not in body:
            raise KeyRequired('action')
        if body['action'] not in ["start", "stop"]:
            raise KeyInvalid('action', body['action'])

    def nfv_forward(self, proc, body):
        if body['action'] == "start":
            proc.forward()
        else:
            proc.stop()

    def _validate_nfv_port(self, body):
        for key in ['action', 'port']:
            if key not in body:
                raise KeyRequired(key)
        if body['action'] not in ["add", "del"]:
            raise KeyInvalid('action', body['action'])
        self._validate_port(body['port'])

    def nfv_port(self, proc, body):
        self._validate_nfv_port(body)

        if body['action'] == "add":
            proc.port_add(body['port'])
        else:
            proc.port_del(body['port'])

    def _validate_nfv_patch(self, body):
        for key in ['src', 'dst']:
            if key not in body:
                raise KeyRequired(key)
        self._validate_port(body['src'])
        self._validate_port(body['dst'])

    def nfv_patch_add(self, proc, body):
        self._validate_nfv_patch(body)
        proc.patch_add(body['src'], body['dst'])

    def nfv_patch_del(self, proc):
        proc.patch_reset()

    def nfv_exit(self, proc):
        self.ctrl.do_exit(proc.type, proc.id)
        proc.do_exit()


class V1PrimaryHandler(BaseHandler):

    def __init__(self, controller):
        super(V1PrimaryHandler, self).__init__(controller)

        self.set_route()

        self.install(self.make_response)
        self.install(self.get_body)

    def set_route(self):
        self.route('/status', 'GET', callback=self.get_status)
        self.route('/status', 'DELETE', callback=self.clear_status)
        self.route('/forward', 'PUT', callback=self.nfv_forward)
        self.route('/ports', 'PUT', callback=self.primary_port)
        self.route('/patches', 'PUT', callback=self.nfv_patch_add)
        self.route('/patches', 'DELETE', callback=self.nfv_patch_del)
        self.route('/launch', 'PUT', callback=self.launch_sec_proc)
        self.route('/', 'DELETE', callback=self.pri_exit)

    def _get_proc(self):
        """Get Primary object for requesting.

        This method is call everytime received request via REST API
        to confirm primary is alive.
        """

        proc = self.ctrl.procs.get(spp_proc.ID_PRIMARY)
        if proc is None:
            raise bottle.HTTPError(404, "primary not found.")
        return proc

    def convert_status(self, data):
        stat = {}
        try:
            stat = json.loads(data)
        except json.JSONDecodeError as e:
            print("%s" % e)
        return stat

    def get_status(self):
        proc = self._get_proc()
        return proc.get_status()

    def clear_status(self):
        proc = self._get_proc()
        proc.clear()

    # TODO(yasufum) change name `nfv` and make it to shared method
    def _validate_nfv_forward(self, body):
        if 'action' not in body:
            raise KeyRequired('action')
        if body['action'] not in ["start", "stop"]:
            raise KeyInvalid('action', body['action'])

    # TODO(yasufum) change name `nfv` and make it to shared method
    def nfv_forward(self, body):
        proc = self._get_proc()
        if body['action'] == "start":
            proc.forward()
        else:
            proc.stop()

    # TODO(yasufum) change name `nfv` and make it to shared method
    def _validate_nfv_port(self, body):
        for key in ['action', 'port']:
            if key not in body:
                raise KeyRequired(key)
        if body['action'] not in ["add", "del"]:
            raise KeyInvalid('action', body['action'])
        self._validate_port(body['port'])

    def primary_port(self, body):
        self._validate_nfv_port(body)
        proc = self._get_proc()

        if body['action'] == "add":
            proc.port_add(body['port'])
        else:
            proc.port_del(body['port'])

    # TODO(yasufum) change name `nfv` and make it to shared method
    def _validate_nfv_patch(self, body):
        for key in ['src', 'dst']:
            if key not in body:
                raise KeyRequired(key)
        self._validate_port(body['src'])
        self._validate_port(body['dst'])

    # TODO(yasufum) change name `nfv` and make it to shared method
    def nfv_patch_add(self, body):
        proc = self._get_proc()
        self._validate_nfv_patch(body)
        proc.patch_add(body['src'], body['dst'])

    # TODO(yasufum) change name `nfv` and make it to shared method
    def nfv_patch_del(self):
        proc = self._get_proc()
        proc.patch_reset()

    def launch_sec_proc(self, body):  # the arg should be "body"
        for key in ['client_id', 'proc_name', 'eal', 'app']:
            if key not in body:
                raise KeyRequired(key)

        proc = self._get_proc()
        proc.do_launch_sec_proc(body)

    def pri_exit(self):
        proc = self._get_proc()
        self.ctrl.do_exit(proc.type, proc.id)
        proc.do_exit()


class V1PcapHandler(BaseHandler):

    def __init__(self, controller):
        super(V1PcapHandler, self).__init__(controller)
        self.type = spp_proc.TYPE_PCAP

        self.set_route()

        self.install(self.check_sec_id)
        self.install(self.get_body)
        self.install(self.make_response)

    def set_route(self):
        self.route('/<sec_id:int>', 'GET', callback=self.pcap_get)
        self.route('/<sec_id:int>', 'DELETE', callback=self.pcap_exit)
        self.route('/<sec_id:int>/capture', 'PUT', callback=self.pcap_action)

    def pcap_get(self, proc):
        return proc.get_status()["info"]

    def _validate_pcap_action(self, body):
        if 'action' not in body:
            raise KeyRequired('action')
        if body['action'] not in ["start", "stop"]:
            raise KeyInvalid('action', body['action'])

    def pcap_action(self, proc, body):
        self._validate_pcap_action(body)
        if body['action'] == "start":
            proc.start()
        else:
            proc.stop()

    def pcap_exit(self, proc):
        self.ctrl.do_exit(proc.type, proc.id)
        proc.do_exit()
