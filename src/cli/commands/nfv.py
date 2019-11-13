# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Nippon Telegraph and Telephone Corporation

from .. import spp_common


class SppNfv(object):
    """Exec spp_nfv command.

    SppNfv lass is intended to be used in Shell class as a delegator for
    running 'nfv' command.

    'self.command()' is called from do_nfv() and 'self.complete()' is
    called from complete_nfv() of both of which is defined in Shell.
    """

    # All of spp_nfv commands used for validation and completion.
    NFV_CMDS = ['status', 'exit', 'forward', 'stop', 'add', 'patch',
                'del']

    def __init__(self, spp_ctl_cli, sec_id, use_cache=False):
        """Initialize SppNfv.

        Turn use_cache `True` if you do not request to spp-ctl each
        time.
        """

        self.spp_ctl_cli = spp_ctl_cli
        self.sec_id = sec_id
        self.ports = []  # registered ports
        self.patches = []

        # Call REST API each time of completion if it is True.
        self.use_cache = use_cache

    def run(self, cmdline):
        """Called from do_nfv() to Send command to secondary process."""

        cmd = cmdline.split(' ')[0]
        params = cmdline.split(' ')[1:]

        if cmd == 'status':
            self._run_status()

        elif cmd == 'add':
            self._run_add(params)

        elif cmd == 'del':
            self._run_del(params)

        elif cmd == 'forward' or cmd == 'stop':
            self._run_forward_or_stop(cmd)

        elif cmd == 'patch':
            self._run_patch(params)

        elif cmd == 'exit':
            self._run_exit()

        else:
            print('Invalid command "%s".' % cmd)

    def print_nfv_status(self, json_obj):
        """Parse and print message from SPP secondary.

        Print status received from secondary.

          spp > nfv 1;status
          - status: idling
          - lcores: [1, 2]
          - ports:
            - phy:0 -> ring:0
            - phy:1
        """

        nfv_attr = json_obj
        print('- status: {}'.format(nfv_attr['status']))
        print('- lcore_ids:')
        print('  - master: {}'.format(nfv_attr['master-lcore']))
        # remove master and show remained
        nfv_attr['lcores'].remove(nfv_attr['master-lcore'])
        if len(nfv_attr['lcores']) > 1:
            print('  - slaves: {}'.format(nfv_attr['lcores']))
        else:
            print('  - slave: {}'.format(nfv_attr['lcores'][0]))
        print('- ports:')
        for port in nfv_attr['ports']:
            dst = None
            for patch in nfv_attr['patches']:
                if patch['src'] == port:
                    dst = patch['dst']

            if dst is None:
                print('  - {}'.format(port))
            else:
                print('  - {} -> {}'.format(port, dst))

    # TODO(yasufum) change name starts with '_' as private
    def get_ports(self):
        """Get all of ports as a list."""

        res = self.spp_ctl_cli.get('nfvs/%d' % self.sec_id)
        if res is not None:
            error_codes = self.spp_ctl_cli.rest_common_error_codes
            if res.status_code == 200:
                return res.json()['ports']
            elif res.status_code in error_codes:
                pass
            else:
                print('Error: unknown response.')

    # TODO(yasufum) change name starts with '_' as private
    def get_patches(self):
        """Get all of patched ports as a list of dicts.

        Returned value is like as
          [{'src': 'phy:0', 'dst': 'ring:0'},
           {'src': 'ring:1', 'dst':'vhost:1'}, ...]
        """

        res = self.spp_ctl_cli.get('nfvs/%d' % self.sec_id)
        if res is not None:
            error_codes = self.spp_ctl_cli.rest_common_error_codes
            if res.status_code == 200:
                return res.json()['patches']
            elif res.status_code in error_codes:
                pass
            else:
                print('Error: unknown response.')

    # TODO(yasufum) change name starts with '_' as private
    def _get_ports_and_patches(self):
        """Get all of ports and patches at once.

        This method is to execute `get_ports()` and `get_patches()` at
        once to reduce request to spp-ctl. Returned value is a set of
        lists. You use this method as following.
          ports, patches = _get_ports_and_patches()
        """

        res = self.spp_ctl_cli.get('nfvs/%d' % self.sec_id)
        if res is not None:
            error_codes = self.spp_ctl_cli.rest_common_error_codes
            if res.status_code == 200:
                ports = res.json()['ports']
                patches = res.json()['patches']
                return ports, patches
            elif res.status_code in error_codes:
                pass
            else:
                print('Error: unknown response.')

    def _get_patched_ports(self):
        """Get all of patched ports as a list.

        This method is to get a list of patched ports instead of a dict.
        You use this method if you simply get patches without `src` and
        `dst`.
        """

        patched_ports = []
        for pport in self.patches:
            patched_ports.append(pport['src'])
            patched_ports.append(pport['dst'])
        return list(set(patched_ports))

    def complete(self, sec_ids, text, line, begidx, endidx):
        """Completion for spp_nfv commands.

        Called from complete_nfv() to complete secondary command.
        """

        try:
            completions = []
            tokens = line.split(';')

            if len(tokens) == 2:
                sub_tokens = tokens[1].split(' ')

                if len(sub_tokens) == 1:
                    if not (sub_tokens[0] in self.NFV_CMDS):
                        completions = self._compl_first_tokens(sub_tokens[0])

                else:
                    if sub_tokens[0] in ['status', 'exit', 'forward', 'stop']:
                        if len(sub_tokens) < 2:
                            if sub_tokens[0].startswith(sub_tokens[1]):
                                completions = [sub_tokens[0]]

                    elif sub_tokens[0] == 'add':
                        completions = self._compl_add(sub_tokens)

                    elif sub_tokens[0] == 'del':
                        completions = self._compl_del(sub_tokens)

                    elif sub_tokens[0] == 'patch':
                        completions = self._compl_patch(sub_tokens)

            return completions

        except Exception as e:
            print(e)

    def _compl_first_tokens(self, token):
        """Complete spp_nfv command."""

        res = []
        for kw in self.NFV_CMDS:
            if kw.startswith(token):
                res.append(kw)
        return res

    def _compl_add(self, sub_tokens):
        """Complete `add` command."""

        if len(sub_tokens) < 3:
            res = []

            port_types = spp_common.PORT_TYPES[:]
            port_types.remove('phy')

            for kw in port_types:
                if kw.startswith(sub_tokens[1]):
                    res.append(kw + ':')
            return res

    def _compl_del(self, sub_tokens):
        """Complete `del` command."""

        res = []
        # Del command consists of two tokens max, for instance,
        # `nfv 1; del ring:1`.
        if len(sub_tokens) < 3:
            tmp_ary = []

            if self.use_cache is False:
                self.ports, self.patches = self._get_ports_and_patches()

            # Patched ports should not be included in the candidate of del.
            patched_ports = self._get_patched_ports()

            # Remove ports already used from candidate.
            for kw in self.ports:
                if not (kw in patched_ports):
                    if sub_tokens[1] == '':
                        tmp_ary.append(kw)
                    elif kw.startswith(sub_tokens[1]):
                        if ':' in sub_tokens[1]:  # exp, 'ring:' or 'ring:0'
                            tmp_ary.append(kw.split(':')[1])
                        else:
                            tmp_ary.append(kw)

            # Physical port cannot be removed.
            for p in tmp_ary:
                if not p.startswith('phy:'):
                    res.append(p)

        return res

    def _compl_patch(self, sub_tokens):
        """Complete `patch` command."""

        # Patch command consists of three tokens max, for instance,
        # `nfv 1; patch phy:0 ring:1`.
        if len(sub_tokens) < 4:
            res = []

            if self.use_cache is False:
                self.ports, self.patches = self._get_ports_and_patches()

            # Get patched ports of src and dst to be used for completion.
            src_ports = []
            dst_ports = []
            for pt in self.patches:
                src_ports.append(pt['src'])
                dst_ports.append(pt['dst'])

            # Remove patched ports from candidates.
            target_idx = len(sub_tokens) - 1  # target is src or dst
            tmp_ports = self.ports[:]  # candidates
            if target_idx == 1:  # find src port
                # If some of ports are patched, `reset` should be included.
                if self.patches != []:
                    tmp_ports.append('reset')
                for pt in src_ports:
                    tmp_ports.remove(pt)  # remove patched ports
            else:  # find dst port
                # If `reset` is given, no need to show dst ports.
                if sub_tokens[target_idx - 1] == 'reset':
                    tmp_ports = []
                else:
                    for pt in dst_ports:
                        tmp_ports.remove(pt)

            # Return candidates.
            for kw in tmp_ports:
                if kw.startswith(sub_tokens[target_idx]):
                    # Completion does not work correctly if `:` is included in
                    # tokens. Required to create keyword only after `:`.
                    if ':' in sub_tokens[target_idx]:  # 'ring:' or 'ring:0'
                        res.append(kw.split(':')[1])  # add only after `:`
                    else:
                        res.append(kw)

            return res

    def _run_status(self):
        """Run `status` command."""

        res = self.spp_ctl_cli.get('nfvs/%d' % self.sec_id)
        if res is not None:
            error_codes = self.spp_ctl_cli.rest_common_error_codes
            if res.status_code == 200:
                self.print_nfv_status(res.json())
            elif res.status_code in error_codes:
                pass
            else:
                print('Error: unknown response.')

    def _run_add(self, params):
        """Run `add` command."""

        if len(params) == 0:
            print('Port is required!')
        elif params[0] in self.ports:
            print("'%s' is already added." % params[0])
        else:
            if self.use_cache is False:
                self.ports = self.get_ports()

            req_params = {'action': 'add', 'port': params[0]}

            res = self.spp_ctl_cli.put('nfvs/%d/ports' %
                                       self.sec_id, req_params)
            if res is not None:
                error_codes = self.spp_ctl_cli.rest_common_error_codes
                if res.status_code == 204:
                    if self.use_cache is True:
                        if not (params[0] in self.ports):
                            self.ports.append(params[0])
                    print('Add %s.' % params[0])
                elif res.status_code in error_codes:
                    pass
                else:
                    print('Error: unknown response.')

    def _run_del(self, params):
        """Run `del` command."""

        if len(params) == 0:
            print('Port is required!')
        elif 'phy:' in params[0]:
            print("Cannot delete phy port '%s'." % params[0])
        else:
            if self.use_cache is False:
                self.patches = self.get_patches()

            # Patched ports should not be deleted.
            patched_ports = self._get_patched_ports()

            if params[0] in patched_ports:
                print("Cannot delete patched port '%s'." % params[0])
            else:
                req_params = {'action': 'del', 'port': params[0]}
                res = self.spp_ctl_cli.put('nfvs/%d/ports' %
                                           self.sec_id, req_params)
                if res is not None:
                    error_codes = self.spp_ctl_cli.rest_common_error_codes
                    if res.status_code == 204:
                        if self.use_cache is True:
                            if params[0] in self.ports:
                                self.ports.remove(params[0])
                        print('Delete %s.' % params[0])
                    elif res.status_code in error_codes:
                        pass
                    else:
                        print('Error: unknown response.')

    def _run_forward_or_stop(self, cmd):
        """Run `forward` or `stop` command.

        Spp-ctl accepts this two commands via single API, so this method
        runs one of which commands by referring `cmd` param.
        """

        if cmd == 'forward':
            req_params = {'action': 'start'}
        elif cmd == 'stop':
            req_params = {'action': 'stop'}
        else:
            print('Unknown command. "forward" or "stop"?')

        res = self.spp_ctl_cli.put('nfvs/%d/forward' %
                                   self.sec_id, req_params)
        if res is not None:
            error_codes = self.spp_ctl_cli.rest_common_error_codes
            if res.status_code == 204:
                if cmd == 'forward':
                    print('Start forwarding.')
                else:
                    print('Stop forwarding.')
            elif res.status_code in error_codes:
                pass
            else:
                print('Error: unknown response.')

    def _run_patch(self, params):
        """Run `patch` command."""

        if len(params) == 0:
            print('Params are required!')
        elif params[0] == 'reset':
            res = self.spp_ctl_cli.delete('nfvs/%d/patches' % self.sec_id)
            if res is not None:
                error_codes = self.spp_ctl_cli.rest_common_error_codes
                if res.status_code == 204:
                    print('Clear all of patches.')
                elif res.status_code in error_codes:
                    pass
                else:
                    print('Error: unknown response.')
        else:
            if len(params) < 2:
                print('Dst port is required!')
            else:
                req_params = {'src': params[0], 'dst': params[1]}
                res = self.spp_ctl_cli.put(
                        'nfvs/%d/patches' % self.sec_id, req_params)
                if res is not None:
                    error_codes = self.spp_ctl_cli.rest_common_error_codes
                    if res.status_code == 204:
                        print('Patch ports (%s -> %s).' % (
                            params[0], params[1]))
                    elif res.status_code in error_codes:
                        pass
                    else:
                        print('Error: unknown response.')

    def _run_exit(self):
        """Run `exit` command."""

        res = self.spp_ctl_cli.delete('nfvs/%d' % self.sec_id)
        if res is not None:
            error_codes = self.spp_ctl_cli.rest_common_error_codes
            if res.status_code == 204:
                print('Exit nfv %d' % self.sec_id)
            elif res.status_code in error_codes:
                pass
            else:
                print('Error: unknown response.')

    @classmethod
    def help(cls):
        msg = """Send a command to spp_nfv specified with ID.

        Spp_nfv is specified with secondary ID and takes sub commands.

          spp > nfv 1; status
          spp > nfv 1; add ring:0
          spp > nfv 1; patch phy:0 ring:0

        You can refer all of sub commands by pressing TAB after
        'nfv 1;'.

          spp > nfv 1;  # press TAB
          add     del     exit    forward patch   status  stop
        """

        print(msg)
