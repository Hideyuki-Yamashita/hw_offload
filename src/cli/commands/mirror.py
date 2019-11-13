# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Nippon Telegraph and Telephone Corporation


class SppMirror(object):
    """Exec spp_mirror command.

    SppMirror class is intended to be used in Shell class as a delegator
    for running 'mirror' command.

    'self.command()' is called from do_mirror() and 'self.complete()' is called
    from complete_() of both of which is defined in Shell.
    """

    # All of commands and sub-commands used for validation and completion.
    MIRROR_CMDS = {
            'status': None,
            'exit': None,
            'component': ['start', 'stop'],
            'port': ['add', 'del']}

    WORKER_TYPES = ['mirror']

    def __init__(self, spp_ctl_cli, sec_id, use_cache=False):
        self.spp_ctl_cli = spp_ctl_cli
        self.sec_id = sec_id

        # Update 'self.worker_names' and 'self.unused_core_ids' each time
        # 'self.run()' is called if it is 'False'.
        # True to 'True' if you do not wait for spp_mirror's response.
        self.use_cache = use_cache

        # Names and core IDs of worker threads
        mirror_status = self._get_status(self.sec_id)

        core_ids = mirror_status['core_ids']
        for wk in mirror_status['workers']:
            if wk['core_id'] in core_ids:
                core_ids.remove(wk['core_id'])
        self.unused_core_ids = core_ids  # used while completion to exclude

        self.workers = mirror_status['workers']
        self.worker_names = [attr['name'] for attr in mirror_status['workers']]

    def run(self, cmdline):
        """Called from do_sec() to Send command to secondary process."""

        # update status each time if configured not to use cache
        if self.use_cache is False:
            mirror_status = self._get_status(self.sec_id)

            core_ids = mirror_status['core_ids']
            for wk in mirror_status['workers']:
                if wk['core_id'] in core_ids:
                    core_ids.remove(wk['core_id'])
            self.unused_core_ids = core_ids  # used while completion to exclude

            self.workers = mirror_status['workers']
            self.worker_names = [attr['name'] for attr in mirror_status['workers']]

        cmd = cmdline.split(' ')[0]
        params = cmdline.split(' ')[1:]

        if cmd == 'status':
            self._run_status()

        elif cmd == 'component':
            self._run_component(params)

        elif cmd == 'port':
            self._run_port(params)

        elif cmd == 'exit':
            self._run_exit()

        else:
            print('Invalid command "%s".' % cmd)

    def print_status(self, json_obj):
        """Parse and print message from SPP VF.

        Print status received from spp_mirror.

          spp > mirror 1; status
          Basic Information:
            - client-id: 3
            - ports: [phy:0, phy:1]
            - lcore_ids:
              - master: 1
              - slaves: [2, 3]
          Components:
            - core:1, "mr1" (type: mirror)
              - rx: ring:0
              - tx: [vhost:0, vhost:1]
            - core:2, "mr2" (type: mirror)
              - rx:
              - tx:
            ...

        """

        # Extract slave lcore IDs first
        slave_lcore_ids = []
        for worker in json_obj['components']:
            slave_lcore_ids.append(str(worker['core']))

        # Basic Information
        print('Basic Information:')
        print('  - client-id: %d' % json_obj['client-id'])
        print('  - ports: [%s]' % ', '.join(json_obj['ports']))
        print('  - lcore_ids:')
        print('    - master: {}'.format(json_obj['master-lcore']))
        if len(slave_lcore_ids) > 1:
            print('    - slaves: [{}]'.format(', '.join(slave_lcore_ids)))
        else:
            print('    - slave: {}'.format(slave_lcore_ids[0]))

        # Componennts
        print('Components:')
        for worker in json_obj['components']:
            if 'name' in worker.keys():
                print("  - core:%d '%s' (type: %s)" % (
                      worker['core'], worker['name'], worker['type']))

                if worker['type'] == 'mirror':
                    pt = ''
                    if len(worker['rx_port']) > 0:
                        pt = worker['rx_port'][0]['port']
                    msg = '    - %s: %s'
                    print(msg % ('rx', pt))

                    tx_ports = []
                    if len(worker['tx_port']) > 0:
                        msg = '    - %s: [%s]'
                        for tp in worker['tx_port']:
                            tx_ports.append(tp['port'])

                    print(msg % ('tx', ', '.join(tx_ports)))

            else:
                # TODO(yasufum) should change 'unuse' to 'unused'
                print("  - core:%d '' (type: unuse)" % worker['core'])

    def complete(self, sec_ids, text, line, begidx, endidx):
        """Completion for spp_mirrorcommands.

        Called from complete_mirror() to complete.
        """

        try:
            completions = []
            tokens = line.split(';')

            if len(tokens) == 2:
                sub_tokens = tokens[1].split(' ')

                if len(sub_tokens) == 1:
                    if not (sub_tokens[0] in self.MIRROR_CMDS.keys()):
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
            return completions
        except Exception as e:
            print(e)

    def _compl_first_tokens(self, token):
        res = []
        for kw in self.MIRROR_CMDS.keys():
            if kw.startswith(token):
                res.append(kw)
        return res

    def _get_status(self, sec_id):
        """Get status of spp_mirror.

        To update status of the instance of SppMirror, get the status from
        spp-ctl. This method returns the result as a dict. For considering
        behaviour of spp_mirror, it is enough to return worker's name and core
        IDs as the status, but might need to be update for future updates.

        # return worker's name and used core IDs, and all of core IDs.
        {
          'workers': [
            {'name': 'mr1', 'core_id': 5},
            {'name': 'mr2', 'core_id': 6},
            ...
          ],
          'core_ids': [5, 6, 7, ...]
        }

        """

        status = {'workers': [], 'core_ids': []}
        res = self.spp_ctl_cli.get('mirrors/%d' % self.sec_id)
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
        res = self.spp_ctl_cli.get('mirrors/%d' % self.sec_id)
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
            res = self.spp_ctl_cli.post('mirrors/%d/components' % self.sec_id,
                                        req_params)
            if res is not None:
                error_codes = self.spp_ctl_cli.rest_common_error_codes
                if res.status_code == 204:
                    print("Succeeded to start component '%s' on core:%d"
                          % (req_params['name'], req_params['core']))
                    self.worker_names.append(req_params['name'])
                    self.unused_core_ids.remove(req_params['core'])
                elif res.status_code in error_codes:
                    pass
                else:
                    print('Error: unknown response.')

        elif params[0] == 'stop':
            res = self.spp_ctl_cli.delete('mirrors/%d/components/%s' % (
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
        if len(params) == 4:
            if params[0] == 'add':
                action = 'attach'
            elif params[0] == 'del':
                action = 'detach'
            else:
                print('Error: Invalid action.')
                return None

            req_params = {'action': action, 'port': params[1],
                          'dir': params[2]}

        res = self.spp_ctl_cli.put('mirrors/%d/components/%s/ports'
                                   % (self.sec_id, params[3]), req_params)
        if res is not None:
            error_codes = self.spp_ctl_cli.rest_common_error_codes
            if res.status_code == 204:
                print("Succeeded to %s port" % params[0])
            elif res.status_code in error_codes:
                pass
            else:
                print('Error: unknown response.')

    def _run_exit(self):
        """Run `exit` command."""

        res = self.spp_ctl_cli.delete('mirrors/%d' % self.sec_id)
        if res is not None:
            error_codes = self.spp_ctl_cli.rest_common_error_codes
            if res.status_code == 204:
                print('Exit mirror %d.' % self.sec_id)
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
            return res

    @classmethod
    def help(cls):
        msg = """Send a command to spp_mirror.

        spp_mirror is a secondary process for duplicating incoming
        packets to be used as similar to TaaS in OpenStack. This
        command has four sub commands.
          * status
          * component
          * port

        Each of sub commands other than 'status' takes several parameters
        for detailed operations. Notice that 'start' for launching a worker
        is replaced with 'stop' for terminating. 'add' is also replaced with
        'del' for deleting.

        Examples:

        # (1) show status of worker threads and resources
        spp > mirror 1; status

        # (2) launch or terminate a worker thread with arbitrary name
        #   NAME: arbitrary name used as identifier
        #   CORE_ID: one of unused cores referred from status
        spp > mirror 1; component start NAME CORE_ID mirror
        spp > mirror 1; component stop NAME CORE_ID mirror

        # (3) add or delete a port to worker of NAME
        #   RES_UID: resource UID such as 'ring:0' or 'vhost:1'
        #   DIR: 'rx' or 'tx'
        spp > mirror 1; port add RES_UID DIR NAME
        spp > mirror 1; port del RES_UID DIR NAME
        """

        print(msg)
