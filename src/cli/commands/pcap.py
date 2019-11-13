# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2019 Nippon Telegraph and Telephone Corporation


class SppPcap(object):
    """Exec spp_pcap command.

    SppPcap class is intended to be used in Shell class as a delegator
    for running 'pcap' command.

    'self.command()' is called from do_pcap() and 'self.complete()' is called
    from complete_() of both of which is defined in Shell.
    """

    # All of commands and sub-commands used for validation and completion.
    PCAP_CMDS = { 'status': None, 'start': None, 'stop': None, 'exit': None}

    WORKER_TYPES = ['receive', 'write']

    def __init__(self, spp_ctl_cli, sec_id, use_cache=False):
        self.spp_ctl_cli = spp_ctl_cli
        self.sec_id = sec_id

        # Update 'self.worker_names' and 'self.unused_core_ids' each time
        # 'self.run()' is called if it is 'False'.
        # True to 'True' if you do not wait for spp_pcap's response.
        self.use_cache = use_cache

        # Names and core IDs of worker threads
        pcap_status = self._get_status(self.sec_id)

        core_ids = pcap_status['core_ids']
        for wk in pcap_status['workers']:
            if wk['core_id'] in core_ids:
                core_ids.remove(wk['core_id'])
        self.unused_core_ids = core_ids  # used while completion to exclude

        self.workers = pcap_status['workers']
        self.worker_names = [attr['role'] for attr in pcap_status['workers']]

    def run(self, cmdline):
        """Called from do_sec() to Send command to secondary process."""

        # update status each time if configured not to use cache
        if self.use_cache is False:
            pcap_status = self._get_status(self.sec_id)

            core_ids = pcap_status['core_ids']
            for wk in pcap_status['workers']:
                if wk['core_id'] in core_ids:
                    core_ids.remove(wk['core_id'])
            self.unused_core_ids = core_ids  # used while completion to exclude

            self.workers = pcap_status['workers']
            self.worker_names = [attr['role']
                                 for attr in pcap_status['workers']]

        cmd = cmdline.split(' ')[0]
        params = cmdline.split(' ')[1:]

        if cmd == 'status':
            res = self.spp_ctl_cli.get('pcaps/%d' % self.sec_id)
            if res is not None:
                error_codes = self.spp_ctl_cli.rest_common_error_codes
                if res.status_code == 200:
                    self.print_status(res.json())
                elif res.status_code in error_codes:
                    pass
                else:
                    print('Error: unknown response.')

        elif cmd == 'start':
            req_params = {'action': 'start'}
            res = self.spp_ctl_cli.put('pcaps/%d/capture'
                                       % (self.sec_id), req_params)
            if res is not None:
                error_codes = self.spp_ctl_cli.rest_common_error_codes
                if res.status_code == 204:
                    print("Start packet capture.")
                elif res.status_code in error_codes:
                    pass
                else:
                    print('Error: unknown response.')

        elif cmd == 'stop':
            req_params = {'action': 'stop'}
            res = self.spp_ctl_cli.put('pcaps/%d/capture'
                                       % (self.sec_id), req_params)
            if res is not None:
                error_codes = self.spp_ctl_cli.rest_common_error_codes
                if res.status_code == 204:
                    print("Stop packet capture.")
                elif res.status_code in error_codes:
                    pass
                else:
                    print('Error: unknown response.')

        elif cmd == 'exit':
            res = self.spp_ctl_cli.delete('pcaps/%d' % (self.sec_id))
            if res is not None:
                error_codes = self.spp_ctl_cli.rest_common_error_codes
                if res.status_code == 204:
                    print("Exit pcap {}.".format(self.sec_id))
                elif res.status_code in error_codes:
                    pass
                else:
                    print('Error: unknown response.')

        else:
            print('Invalid command "{}".'.format(cmd))

    def print_status(self, json_obj):
        """Parse and print message from SPP PCAP.

        Print status received from spp_pcap.

          spp > pcap 1; status
          Basic Information:
            - client-id: 3
            - satus: running
            - lcore_ids:
              - master: 1
              - slaves: [2, 3, 3, 4, 5, 6]
          Components:
            - core:2, receive
              - rx: phy:0
            - core:3, write
              - file: /tmp/spp_pcap.20181108110600.phy0.1.1.pcap
            - core:4, write
              - file: /tmp/spp_pcap.20181108110600.phy0.2.1.pcap
            - core:5, write
              - file: /tmp/spp_pcap.20181108110600.phy0.3.1.pcap
            ...

        """

        # Extract slave lcore IDs first
        slave_lcore_ids = []
        for worker in json_obj['core']:
            slave_lcore_ids.append(str(worker['core']))

        # Basic Information
        print('Basic Information:')
        print('  - client-id: {}'.format(json_obj['client-id']))
        print('  - status: {}'.format(json_obj['status']))
        print('  - lcore_ids:')
        print('    - master: {}'.format(json_obj['master-lcore']))
        print('    - slaves: [{}]'.format(', '.join(slave_lcore_ids)))

        # Componennts
        print('Components:')
        for worker in json_obj['core']:
            if 'role' in worker.keys():
                print("  - core:{core_id} {role}".format(
                        core_id=worker['core'], role=worker['role']))

                if worker['role'] == 'receive':
                    pt = worker['rx_port'][0]['port']
                    msg = '    - {direction}: {res_id}'
                    print(msg.format(direction='rx', res_id=pt))
                else:
                    print('    - filename: {}'.format(worker['filename']))

    def complete(self, sec_ids, text, line, begidx, endidx):
        """Completion for spp_pcap commands.

        Called from complete_pcap() to complete.
        """

        try:
            completions = []
            tokens = line.split(';')

            if len(tokens) == 2:
                sub_tokens = tokens[1].split(' ')

                if len(sub_tokens) == 1:
                    if not (sub_tokens[0] in self.PCAP_CMDS.keys()):
                        completions = self._compl_first_tokens(sub_tokens[0])
                else:
                    if sub_tokens[0] == 'status':
                        if len(sub_tokens) < 2:
                            if 'status'.startswith(sub_tokens[1]):
                                completions = ['status']

                    elif sub_tokens[0] == 'start':
                        if len(sub_tokens) < 2:
                            if 'start'.startswith(sub_tokens[1]):
                                completions = ['start']

                    elif sub_tokens[0] == 'stop':
                        if len(sub_tokens) < 2:
                            if 'stop'.startswith(sub_tokens[1]):
                                completions = ['stop']
            return completions
        except Exception as e:
            print(e)

    def _compl_first_tokens(self, token):
        res = []
        for kw in self.PCAP_CMDS.keys():
            if kw.startswith(token):
                res.append(kw)
        return res

    def _get_status(self, sec_id):
        """Get status of spp_pcap.

        To update status of the instance of SppPcap, get the status from
        spp-ctl. This method returns the result as a dict. For considering
        behaviour of spp_pcap, it is enough to return worker's name and core
        IDs as the status, but might need to be update for future updates.

        # return worker's role and used core IDs, and all of core IDs.
        {
          'workers': [
            {'role': 'receive', 'core_id': 5},
            {'role': 'write', 'core_id': 6},
            ...
          ],
          'core_ids': [5, 6, 7, ...]
        }

        """

        status = {'workers': [], 'core_ids': []}
        res = self.spp_ctl_cli.get('pcaps/%d' % self.sec_id)
        if res is not None:
            if res.status_code == 200:
                json_obj = res.json()

                if 'core' in json_obj.keys():
                    for wk in json_obj['core']:
                        if 'core' in wk.keys():
                            if 'role' in wk.keys():
                                status['workers'].append(
                                        {'role': wk['role'],
                                         'core_id': wk['core']})
                            status['core_ids'].append(wk['core'])

        return status

    @classmethod
    def help(cls):
        msg = """Send a command to spp_pcap.

        Spp_pcap is a secondary process for capturing incoming packets.

        'start' for launching a worker is replaced with 'stop' for
        terminating. 'exit' for spp_pcap terminating.

        Examples:

        # (1) show status of worker threads and resources
        spp > pcap 1; status

        # (2) launch or terminate capture thread
        spp > pcap 1; start
        spp > pcap 1; stop

        # (3) terminate spp_pcap secondaryd
        spp > pcap 1; exit
        """

        print(msg)
