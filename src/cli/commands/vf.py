# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Nippon Telegraph and Telephone Corporation


class SppVf(object):
    """Exec SPP VF command.

    SppVf class is intended to be used in Shell class as a delegator
    for running 'vf' command.

    'self.command()' is called from do_vf() and 'self.complete()' is called
    from complete_vf() of both of which is defined in Shell.
    """

    # All of commands and sub-commands used for validation and completion.
    VF_CMDS = {
            'status': None,
            'exit': None,
            'component': ['start', 'stop'],
            'port': ['add', 'del'],
            'classifier_table': ['add', 'del']}

    WORKER_TYPES = ['forward', 'merge', 'classifier']

    def __init__(self, spp_ctl_cli, sec_id, use_cache=False):
        self.spp_ctl_cli = spp_ctl_cli
        self.sec_id = sec_id

        # Update 'self.worker_names' and 'self.unused_core_ids' each time
        # 'self.run()' is called if it is 'False'.
        # True to 'True' if you do not wait for spp_vf's response.
        self.use_cache = use_cache

        # Names and core IDs of worker threads
        vf_status = self._get_status(self.sec_id)

        core_ids = vf_status['core_ids']
        for wk in vf_status['workers']:
            if wk['core_id'] in core_ids:
                core_ids.remove(wk['core_id'])
        self.unused_core_ids = core_ids  # used while completion to exclude

        self.workers = vf_status['workers']
        self.worker_names = [attr['name'] for attr in vf_status['workers']]

    def run(self, cmdline):
        """Called from do_sec() to Send command to secondary process."""

        # update status each time if configured not to use cache
        if self.use_cache is False:
            vf_status = self._get_status(self.sec_id)

            core_ids = vf_status['core_ids']
            for wk in vf_status['workers']:
                if wk['core_id'] in core_ids:
                    core_ids.remove(wk['core_id'])
            self.unused_core_ids = core_ids  # used while completion to exclude

            self.workers = vf_status['workers']
            self.worker_names = [attr['name'] for attr in vf_status['workers']]

        cmd = cmdline.split(' ')[0]
        params = cmdline.split(' ')[1:]

        if cmd == 'status':
            self._run_status()

        elif cmd == 'component':
            self._run_component(params)

        elif cmd == 'port':
            self._run_port(params)

        elif cmd == 'classifier_table':
            self._run_cls_table(params)

        elif cmd == 'exit':
            self._run_exit()

        else:
            print('Invalid command "%s".' % cmd)

    def print_status(self, json_obj):
        """Parse and print message from SPP VF.

        Print status received from spp_vf.

          spp > vf; status
          Basic Information:
            - client-id: 3
            - ports: [phy:0, phy:1]
            - lcore_ids:
              - master: 1
              - slaves: [2, 3]
          Classifier Table:
            - "FA:16:3E:7D:CC:35", ring:0
            - "FA:17:3E:7D:CC:55", ring:1
          Components:
            - core:1, "fwdr1" (type: forwarder)
              - rx: ring:0
              - tx: vhost:0
            - core:2, "mgr11" (type: merger)
              - rx: ring:1, vlan (operation: add, id: 101, pcp: 0)
              - tx: ring:2, vlan (operation: del)
            ...

        """

        # Extract slave lcore IDs first
        slave_lcore_ids = []
        for worker in json_obj['components']:
            slave_lcore_ids.append(str(worker['core']))

        # Basic Information
        print('Basic Information:')
        print('  - client-id: {}'.format(json_obj['client-id']))
        print('  - ports: [{}]'.format(', '.join(json_obj['ports'])))
        print('  - lcore_ids:')
        print('    - master: {}'.format(json_obj['master-lcore']))
        print('    - slaves: [{}]'.format(', '.join(slave_lcore_ids)))

        # Classifier Table
        print('Classifier Table:')
        if len(json_obj['classifier_table']) == 0:
            print('  No entries.')
        for ct in json_obj['classifier_table']:
            print('  - %s, %s' % (ct['value'], ct['port']))

        # Componennts
        print('Components:')
        for worker in json_obj['components']:
            if 'name' in worker.keys():
                print("  - core:%d '%s' (type: %s)" % (
                      worker['core'], worker['name'], worker['type']))
                for pt_dir in ['rx', 'tx']:
                    pt = '%s_port' % pt_dir
                    for attr in worker[pt]:
                        if attr['vlan']['operation'] == 'add':
                            msg = '    - %s: %s ' + \
                                  '(vlan operation: %s, id: %d, pcp: %d)'
                            print(msg % (pt_dir, attr['port'],
                                         attr['vlan']['operation'],
                                         attr['vlan']['id'],
                                         attr['vlan']['pcp']))
                        elif attr['vlan']['operation'] == 'del':
                            msg = '    - %s: %s (vlan operation: %s)'
                            print(msg % (pt_dir, attr['port'],
                                  attr['vlan']['operation']))
                        else:
                            msg = '    - %s: %s'
                            print(msg % (pt_dir, attr['port']))

            else:
                # TODO(yasufum) should change 'unuse' to 'unused'
                print("  - core:%d '' (type: unuse)" % worker['core'])

    def complete(self, sec_ids, text, line, begidx, endidx):
        """Completion for spp_vf commands.

        Called from complete_vf() to complete.
        """

        try:
            completions = []
            tokens = line.split(';')

            if len(tokens) == 2:
                sub_tokens = tokens[1].split(' ')

                # VF_CMDS = {
                #         'status': None,
                #         'component': ['start', 'stop'],
                #         'port': ['add', 'del'],
                #         'classifier_table': ['add', 'del']}

                if len(sub_tokens) == 1:
                    if not (sub_tokens[0] in self.VF_CMDS.keys()):
                        completions = self._compl_first_tokens(sub_tokens[0])
                else:
                    if sub_tokens[0] == 'status':
                        if len(sub_tokens) < 2:
                            if 'status'.startswith(sub_tokens[1]):
                                completions = ['status']

                    elif sub_tokens[0] == 'component':
                        completions = self._compl_component(sub_tokens)

                    elif sub_tokens[0] == 'port':
                        completions = self._compl_port(sub_tokens)

                    elif sub_tokens[0] == 'classifier_table':
                        completions = self._compl_cls_table(sub_tokens)
            return completions
        except Exception as e:
            print(e)

    def _compl_first_tokens(self, token):
        res = []
        for kw in self.VF_CMDS.keys():
            if kw.startswith(token):
                res.append(kw)
        return res

    def _get_status(self, sec_id):
        """Get status of spp_vf.

        To update status of the instance of SppVf, get the status from
        spp-ctl. This method returns the result as a dict. For considering
        behaviour of spp_vf, it is enough to return worker's name and core
        IDs as the status, but might need to be update for future updates.

        # return worker's name and used core IDs, and all of core IDs.
        {
          'workers': [
            {'name': 'fw1', 'core_id': 5},
            {'name': 'mg1', 'core_id': 6},
            ...
          ],
          'core_ids': [5, 6, 7, ...]
        }

        """

        status = {'workers': [], 'core_ids': []}
        res = self.spp_ctl_cli.get('vfs/%d' % self.sec_id)
        if res is not None:
            if res.status_code == 200:
                json_obj = res.json()

                if 'components' in json_obj.keys():
                    for wk in json_obj['components']:
                        if 'core' in wk.keys():
                            if 'name' in wk.keys():
                                status['workers'].append(
                                        {'name': wk['name'],
                                            'core_id': wk['core']})
                            status['core_ids'].append(wk['core'])

        return status

    def _run_status(self):
        res = self.spp_ctl_cli.get('vfs/%d' % self.sec_id)
        if res is not None:
            error_codes = self.spp_ctl_cli.rest_common_error_codes
            if res.status_code == 200:
                self.print_status(res.json())
            elif res.status_code in error_codes:
                pass
            else:
                print('Error: unknown response.')

    def _run_component(self, params):
        if params[0] == 'start':
            req_params = {'name': params[1], 'core': int(params[2]),
                          'type': params[3]}
            res = self.spp_ctl_cli.post('vfs/%d/components' % self.sec_id,
                                        req_params)
            if res is not None:
                error_codes = self.spp_ctl_cli.rest_common_error_codes
                if res.status_code == 204:
                    print("Succeeded to start component '%s' on core:%d"
                          % (req_params['name'], req_params['core']))
                    self.worker_names.append(req_params['name'])
                    if req_params['core'] in self.unused_core_ids:
                        self.unused_core_ids.remove(req_params['core'])
                elif res.status_code in error_codes:
                    pass
                else:
                    print('Error: unknown response.')

        elif params[0] == 'stop':
            res = self.spp_ctl_cli.delete('vfs/%d/components/%s' % (
                                          self.sec_id, params[1]))
            if res is not None:
                error_codes = self.spp_ctl_cli.rest_common_error_codes
                if res.status_code == 204:
                    print("Succeeded to delete component '%s'" % params[1])

                    # update workers and core IDs
                    if params[1] in self.worker_names:
                        self.worker_names.remove(params[1])
                    for wk in self.workers:
                        if wk['name'] == params[1]:
                            self.unused_core_ids.append(wk['core_id'])
                            self.workers.remove(wk)
                            break
                elif res.status_code in error_codes:
                    pass
                else:
                    print('Error: unknown response.')

    def _run_port(self, params):
        req_params = None
        if len(params) == 4:
            if params[0] == 'add':
                action = 'attach'
            elif params[0] == 'del':
                action = 'detach'
            else:
                print('Error: Invalid action.')
                return None

            req_params = {'action': action, 'port': params[1],
                          'dir': params[2],
                          'vlan': {'operation': 'none',
                                   'id': 'none',
                                   'pcp': 'none'}}

        elif len(params) == 5:  # delete vlan with 'port add' command
            # TODO(yasufum) Syntax for deleting vlan should be modified
            #               because deleting with 'port add' is terrible!
            action = 'attach'
            req_params = {'action': action, 'port': params[1],
                          'dir': params[2],
                          'vlan': {'operation': 'del',
                                   'id': 'none',
                                   'pcp': 'none'}}

        elif len(params) == 7:
            action = 'attach'
            if params[4] == 'add_vlantag':
                op = 'add'
            elif params[4] == 'del_vlantag':
                op = 'del'
            req_params = {'action': action, 'port': params[1],
                          'dir': params[2],
                          'vlan': {'operation': op, 'id': int(params[5]),
                                   'pcp': int(params[6])}}
        else:
            print('Error: Invalid syntax.')

        if req_params is not None:
            res = self.spp_ctl_cli.put('vfs/%d/components/%s/ports'
                                       % (self.sec_id, params[3]), req_params)
            if res is not None:
                error_codes = self.spp_ctl_cli.rest_common_error_codes
                if res.status_code == 204:
                    print("Succeeded to %s port" % params[0])
                elif res.status_code in error_codes:
                    pass
                else:
                    print('Error: unknown response.')

    def _run_cls_table(self, params):
        req_params = None
        if len(params) == 4:
            req_params = {'action': params[0], 'type': params[1],
                          'mac_address': params[2], 'port': params[3]}

        elif len(params) == 5:
            req_params = {'action': params[0], 'type': params[1],
                          'vlan': params[2], 'mac_address': params[3],
                          'port': params[4]}
        else:
            print('Error: Invalid syntax.')

        if req_params is not None:
            req = 'vfs/%d/classifier_table' % self.sec_id
            res = self.spp_ctl_cli.put(req, req_params)

            if res is not None:
                error_codes = self.spp_ctl_cli.rest_common_error_codes
                if res.status_code == 204:
                    print("Succeeded to %s" % params[0])
                elif res.status_code in error_codes:
                    pass
                else:
                    print('Error: unknown response.')

    def _run_exit(self):
        """Run `exit` command."""

        res = self.spp_ctl_cli.delete('vfs/%d' % self.sec_id)
        if res is not None:
            error_codes = self.spp_ctl_cli.rest_common_error_codes
            if res.status_code == 204:
                print('Exit vf %d.' % self.sec_id)
            elif res.status_code in error_codes:
                pass
            else:
                print('Error: unknown response.')

    def _compl_component(self, sub_tokens):
        if len(sub_tokens) < 6:
            subsub_cmds = ['start', 'stop']
            res = []
            if len(sub_tokens) == 2:
                for kw in subsub_cmds:
                    if kw.startswith(sub_tokens[1]):
                        res.append(kw)
            elif len(sub_tokens) == 3:
                # 'start' takes any of names and no need
                #  check, required only for 'stop'.
                if sub_tokens[1] == 'start':
                    if 'NAME'.startswith(sub_tokens[2]):
                        res.append('NAME')
                if sub_tokens[1] == 'stop':
                    for kw in self.worker_names:
                        if kw.startswith(sub_tokens[2]):
                            res.append(kw)
            elif len(sub_tokens) == 4:
                if sub_tokens[1] == 'start':
                    for cid in [str(i) for i in self.unused_core_ids]:
                        if cid.startswith(sub_tokens[3]):
                            res.append(cid)
            elif len(sub_tokens) == 5:
                if sub_tokens[1] == 'start':
                    for wk_type in self.WORKER_TYPES:
                        if wk_type.startswith(sub_tokens[4]):
                            res.append(wk_type)
            return res

    def _compl_port(self, sub_tokens):
        if len(sub_tokens) < 9:
            subsub_cmds = ['add', 'del']
            res = []
            if len(sub_tokens) == 2:
                for kw in subsub_cmds:
                    if kw.startswith(sub_tokens[1]):
                        res.append(kw)
            elif len(sub_tokens) == 3:
                if sub_tokens[1] in subsub_cmds:
                    if 'RES_UID'.startswith(sub_tokens[2]):
                        res.append('RES_UID')
            elif len(sub_tokens) == 4:
                if sub_tokens[1] in subsub_cmds:
                    for direction in ['rx', 'tx']:
                        if direction.startswith(sub_tokens[3]):
                            res.append(direction)
            elif len(sub_tokens) == 5:
                if sub_tokens[1] in subsub_cmds:
                    for kw in self.worker_names:
                        if kw.startswith(sub_tokens[4]):
                            res.append(kw)
            elif len(sub_tokens) == 6:
                if sub_tokens[1] == 'add':
                    for kw in ['add_vlantag', 'del_vlantag']:
                        if kw.startswith(sub_tokens[5]):
                            res.append(kw)
            elif len(sub_tokens) == 7:
                if sub_tokens[1] == 'add' and sub_tokens[5] == 'add_vlantag':
                    if 'VID'.startswith(sub_tokens[6]):
                        res.append('VID')
            elif len(sub_tokens) == 8:
                if sub_tokens[1] == 'add' and sub_tokens[5] == 'add_vlantag':
                    if 'PCP'.startswith(sub_tokens[7]):
                        res.append('PCP')
            return res

    def _compl_cls_table(self, sub_tokens):
        if len(sub_tokens) < 7:
            subsub_cmds = ['add', 'del']
            res = []

            if len(sub_tokens) == 2:
                for kw in subsub_cmds:
                    if kw.startswith(sub_tokens[1]):
                        res.append(kw)

            elif len(sub_tokens) == 3:
                if sub_tokens[1] in subsub_cmds:
                    for kw in ['mac', 'vlan']:
                        if kw.startswith(sub_tokens[2]):
                            res.append(kw)

            elif len(sub_tokens) == 4:
                if sub_tokens[1] == 'add':
                    if sub_tokens[2] == 'mac':
                        if 'MAC_ADDR'.startswith(sub_tokens[3]):
                            res.append('MAC_ADDR')
                    elif sub_tokens[2] == 'vlan':
                        if 'VID'.startswith('VID'):
                            res.append('VID')
                elif sub_tokens[1] == 'del':
                    if sub_tokens[2] == 'mac':
                        if 'MAC_ADDR'.startswith(sub_tokens[3]):
                            res.append('MAC_ADDR')
                    if sub_tokens[2] == 'vlan':
                        if 'VID'.startswith(sub_tokens[3]):
                                res.append('VID')

            elif len(sub_tokens) == 5:
                if sub_tokens[1] == 'add':
                    if sub_tokens[2] == 'mac':
                        if 'RES_UID'.startswith(sub_tokens[4]):
                            res.append('RES_UID')
                    elif sub_tokens[2] == 'vlan':
                        if 'MAC_ADDR'.startswith(sub_tokens[4]):
                            res.append('MAC_ADDR')
                if sub_tokens[1] == 'del':
                    if sub_tokens[2] == 'mac':
                        if 'RES_UID'.startswith(sub_tokens[4]):
                            res.append('RES_UID')
                    elif sub_tokens[2] == 'vlan':
                        if 'MAC_ADDR'.startswith(sub_tokens[4]):
                            res.append('MAC_ADDR')

            elif len(sub_tokens) == 6:
                if sub_tokens[1] in subsub_cmds and \
                        sub_tokens[2] == 'vlan':
                            if 'RES_UID'.startswith(sub_tokens[5]):
                                res.append('RES_UID')
            return res

    @classmethod
    def help(cls):
        msg = """Send a command to spp_vf.

        SPP VF is a secondary process for pseudo SR-IOV features. This
        command has four sub commands.
          * status
          * component
          * port
          * classifier_table

        Each of sub commands other than 'status' takes several parameters
        for detailed operations. Notice that 'start' for launching a worker
        is replaced with 'stop' for terminating. 'add' is also replaced with
        'del' for deleting.

        Examples:

        # (1) show status of worker threads and resources
        spp > vf 1; status

        # (2) launch or terminate a worker thread with arbitrary name
        #   NAME: arbitrary name used as identifier
        #   CORE_ID: one of unused cores referred from status
        #   ROLE: role of workers, 'forward', 'merge' or 'classifier'
        spp > vf 1; component start NAME CORE_ID ROLE
        spp > vf 1; component stop NAME CORE_ID ROLE

        # (3) add or delete a port to worker of NAME
        #   RES_UID: resource UID such as 'ring:0' or 'vhost:1'
        #   DIR: 'rx' or 'tx'
        spp > vf 1; port add RES_UID DIR NAME
        spp > vf 1; port del RES_UID DIR NAME

        # (4) add or delete a port with vlan ID to worker of NAME
        #   VID: vlan ID
        #   PCP: priority code point defined in IEEE 802.1p
        spp > vf 1; port add RES_UID DIR NAME add_vlantag VID PCP
        spp > vf 1; port del RES_UID DIR NAME add_vlantag VID PCP

        # (5) add a port of deleting vlan tag
        spp > vf 1; port add RES_UID DIR NAME del_vlantag

        # (6) add or delete an entry of MAC address and resource to classify
        spp > vf 1; classifier_table add mac MAC_ADDR RES_UID
        spp > vf 1; classifier_table del mac MAC_ADDR RES_UID

        # (7) add or delete an entry of MAC address and resource with vlan ID
        spp > vf 1; classifier_table add vlan VID MAC_ADDR RES_UID
        spp > vf 1; classifier_table del vlan VID MAC_ADDR RES_UID
        """

        print(msg)
