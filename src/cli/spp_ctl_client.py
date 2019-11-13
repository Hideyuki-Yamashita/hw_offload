#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Nippon Telegraph and Telephone Corporation

import requests


class SppCtlClient(object):

    rest_common_error_codes = [400, 404, 500]

    def __init__(self, ip_addr='localhost', port=7777, api_ver='v1'):
        self.base_url = 'http://%s:%d/%s' % (ip_addr, port, api_ver)
        self.ip_addr = ip_addr
        self.port = port

    def request_handler(func):
        """Request handler for spp-ctl.

        Decorator for handling a http request of 'requests' library.
        It receives one of the methods 'get', 'put', 'post' or 'delete'
        as 'func' argment.
        """

        def wrapper(self, *args, **kwargs):
            try:
                res = func(self, *args, **kwargs)

                # TODO(yasufum) revise print message to more appropriate
                # for spp.py.
                if res.status_code == 400:
                    print('Syntax or lexical error, or SPP returns ' +
                          'error for the request.')
                elif res.status_code == 404:
                    print('URL is not supported, or no SPP process ' +
                          'of client-id in a URL.')
                elif res.status_code == 500:
                    print('System error occured in spp-ctl.')

                return res
            except requests.exceptions.ConnectionError:
                print('Error: Failed to connect to spp-ctl.')
                return None
        return wrapper

    @request_handler
    def get(self, req):
        url = '%s/%s' % (self.base_url, req)
        return requests.get(url)

    @request_handler
    def put(self, req, params):
        url = '%s/%s' % (self.base_url, req)
        return requests.put(url, json=params)

    @request_handler
    def post(self, req, params):
        url = '%s/%s' % (self.base_url, req)
        return requests.post(url, json=params)

    @request_handler
    def delete(self, req):
        url = '%s/%s' % (self.base_url, req)
        return requests.delete(url)

    def is_server_running(self):
        if self.get('processes') is None:
            return False
        else:
            return True

    def get_sec_ids(self, ptype):
        """Return a list of IDs of running secondary processes.

        'ptype' is 'nfv', 'vf' or 'mirror' or 'pcap'.
        """

        ids = []
        res = self.get('processes')
        if res is not None:
            if res.status_code == 200:
                try:
                    for ent in res.json():
                        if ent['type'] == ptype:
                            ids.append(ent['client-id'])
                except KeyError as e:
                    print('Error: {} is not defined!'.format(e))
        return ids

    def get_sec_procs(self, ptype):
        """Get secondary processes info of given type.

        Processes info from spp-ctl is retrieved as JSON, then loaded with
        json() before returned.
        """

        sec_procs = []
        error_codes = self.rest_common_error_codes

        for sec_id in self.get_sec_ids(ptype):
            # NOTE: take care API's proc type are defined as plural such as
            # 'nfvs', 'vfs' or so.
            res = self.get('{pt}s/{sid}'.format(pt=ptype, sid=sec_id))
            if res.status_code == 200:
                sec_procs.append(res.json())
            elif res.status_code in error_codes:
                # TODO(yasufum) Print default error message
                pass
            else:
                # Ignore unknown response because no problem for drawing graph
                pass
        return sec_procs
