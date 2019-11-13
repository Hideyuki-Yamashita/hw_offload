# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Nippon Telegraph and Telephone Corporation

from .. import spp_common
from ..shell_lib import common
from ..spp_common import logger
import time


class SppPrimary(object):
    """Exec SPP primary command.

    SppPrimary class is intended to be used in Shell class as a delegator
    for running 'pri' command.

    'self.run()' is called from do_pri() and 'self.complete()' is called
    from complete_pri() of both of which is defined in Shell.
    """

    # All of primary commands used for validation and completion.
    PRI_CMDS = ['status', 'add', 'del', 'forward', 'stop', 'patch',
                'launch', 'clear']

    def __init__(self, spp_ctl_cli):
        self.spp_ctl_cli = spp_ctl_cli

        self.ports = []  # registered ports
        self.patches = []

        # Default args for `pri; launch`, used if given cli_config is invalid

        # Setup template of args for `pri; launch`
        temp = "-l {m_lcore},{s_lcores} "
        temp = temp + "{mem} "
        temp = temp + "-- "
        temp = temp + "{opt_sid} {sid} "  # '-n 1' or '--client-id 1'
        temp = temp + "-s {sec_addr} "  # '-s 192.168.1.100:6666'
        temp = temp + "{vhost_cli}"
        self.launch_template = temp

    def run(self, cmd, cli_config):
        """Called from do_pri() to Send command to primary process."""

        tmpary = cmd.split(' ')
        subcmd = tmpary[0]
        params = tmpary[1:]

        if not (subcmd in self.PRI_CMDS):
            print("Invalid pri command: '{}'".format(subcmd))
            return None

        # use short name
        common_err_codes = self.spp_ctl_cli.rest_common_error_codes

        if subcmd == 'status':
            res = self.spp_ctl_cli.get('primary/status')
            if res is not None:
                if res.status_code == 200:
                    self.print_status(res.json())
                elif res.status_code in common_err_codes:
                    # Print default error message
                    pass
                else:
                    print('Error: unknown response from status.')

        elif subcmd == 'add':
            self._run_add(params)

        elif subcmd == 'del':
            self._run_del(params)

        elif subcmd == 'forward' or cmd == 'stop':
            self._run_forward_or_stop(cmd)

        elif subcmd == 'patch':
            self._run_patch(params)

        elif subcmd == 'launch':
            wait_time = float(cli_config['sec_wait_launch']['val'])
            self._run_launch(params, wait_time)

        elif subcmd == 'clear':
            res = self.spp_ctl_cli.delete('primary/status')
            if res is not None:
                if res.status_code == 204:
                    print('Clear port statistics.')
                elif res.status_code in common_err_codes:
                    pass
                else:
                    print('Error: unknown response for clear.')

        else:
            print('Invalid pri command!')

    def do_exit(self):
        res = self.spp_ctl_cli.delete('primary')

        if res is not None:
            error_codes = self.spp_ctl_cli.rest_common_error_codes
            if res.status_code == 204:
                print('Exit primary')
            elif res.status_code in error_codes:
                pass
            else:
                print('Error: unknown response for exit.')

    def print_status(self, json_obj):
        """Parse SPP primary's status and print.

        Primary returns the status as JSON format, but it is just a little
        long.

            {
                "master-lcore": 0,
                "lcores": [0,1],
                "forwarder" {...},   # exists if slave thread is running
                "phy_ports": [
                    {
                        "eth": "56:48:4f:12:34:00",
                        "id": 0,
                        "rx": 78932932,
                        "tx": 78932931,
                        "tx_drop": 1,
                    }
                    ...
                ],
                "ring_ports": [
                    {
                        "id": 0,
                        "rx": 89283,
                        "rx_drop": 0,
                        "tx": 89283,
                        "tx_drop": 0
                    },
                    ...
                ]
            }

        It is formatted to be simple and more understandable.

            - lcore_ids:
                - master: 0
                - slave: 1
            - forwarder:
              - status: running
              - ports:
                - phy:0 -> phy:1
                - phy:1
            - stats
              - physical ports:
                  ID          rx          tx     tx_drop  mac_addr
                   0    78932932    78932931           1  56:48:4f:53:54:00
                   ...
              - ring ports:
                  ID          rx          tx     rx_drop     tx_drop
                   0       89283       89283           0           0
                   ...
        """

        try:
            if 'lcores' in json_obj:
                print('- lcore_ids:')
                print('  - master: {}'.format(json_obj['lcores'][0]))
                if len(json_obj['lcores']) > 1:
                    if len(json_obj['lcores']) == 2:
                        print('  - slave: {}'.format(json_obj['lcores'][1]))
                    else:
                        lcores = ', '.join([str(i)
                            for i in json_obj['lcores'][1:]])
                        print('  - slaves: [{}]'.format(lcores))

            sep = ' '
            if 'forwarder' in json_obj:
                print('- forwarder:')
                print('  - status: {}'.format(json_obj['forwarder']['status']))

                print('  - ports:')
                for port in json_obj['forwarder']['ports']:
                    dst = None
                    for patch in json_obj['forwarder']['patches']:
                        if patch['src'] == port:
                            dst = patch['dst']

                    if dst is None:
                        print('    - {}'.format(port))
                    else:
                        print('    - {} -> {}'.format(port, dst))

            if ('phy_ports' in json_obj) or ('ring_ports' in json_obj):
                print('- stats')

            if 'phy_ports' in json_obj:
                print('  - physical ports:')
                print('{s6}ID{s10}rx{s10}tx{s4}tx_drop  mac_addr'.format(
                      s4=sep*4, s6=sep*6, s10=sep*10))

                temp = '{s6}{portid:2}  {rx:10}  {tx:10}  {tx_d:10}  {eth}'
                for pports in json_obj['phy_ports']:
                    print(temp.format(s6=sep*6,
                        portid=pports['id'], rx=pports['rx'], tx=pports['tx'],
                        tx_d=pports['tx_drop'], eth=pports['eth']))

            if 'ring_ports' in json_obj:
                print('  - ring ports:')
                print('{s6}ID{s10}rx{s10}tx{s5}rx_drop{s5}tx_drop'.format(
                      s6=sep*6, s5=sep*5, s10=sep*10))
                temp = '{s6}{rid:2}  {rx:10}  {tx:10}  {rx_d:10}  {tx_d:10}'
                for rports in json_obj['ring_ports']:
                    print(temp.format(s6=sep*6,
                        rid=rports['id'], rx=rports['rx'], tx=rports['tx'],
                        rx_d=rports['rx_drop'], tx_d=rports['tx_drop']))

        except KeyError as e:
            logger.error('{} is not defined!'.format(e))

    # TODO(yasufum) make methods start with '_get' to be shared
    # because it is similar to nfv. _get_ports(self) is changed as
    # _get_ports(self, proc_type).
    def _get_ports(self):
        """Get all of ports as a list."""

        res = self.spp_ctl_cli.get('primary/status')
        if res is not None:
            error_codes = self.spp_ctl_cli.rest_common_error_codes
            if res.status_code == 200:
                return res.json()['forwarder']['ports']
            elif res.status_code in error_codes:
                pass
            else:
                print('Error: unknown response for get_ports.')

    def _get_patches(self):
        """Get all of patched ports as a list of dicts.

        Returned value is like as
          [{'src': 'phy:0', 'dst': 'ring:0'},
           {'src': 'ring:1', 'dst':'vhost:1'}, ...]
        """

        res = self.spp_ctl_cli.get('primary/status')
        if res is not None:
            error_codes = self.spp_ctl_cli.rest_common_error_codes
            if res.status_code == 200:
                return res.json()['forwarder']['patches']
            elif res.status_code in error_codes:
                pass
            else:
                print('Error: unknown response for get_patches.')

    def _get_ports_and_patches(self):
        """Get all of ports and patches at once.

        This method is to execute `_get_ports()` and `_get_patches()` at
        once to reduce request to spp-ctl. Returned value is a set of
        lists. You use this method as following.
          ports, patches = _get_ports_and_patches()
        """

        res = self.spp_ctl_cli.get('primary/status')
        if res is not None:
            error_codes = self.spp_ctl_cli.rest_common_error_codes
            if res.status_code == 200:
                ports = res.json()['forwarder']['ports']
                patches = res.json()['forwarder']['patches']
                return ports, patches
            elif res.status_code in error_codes:
                pass
            else:
                print('Error: unknown response 3.')

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

    def _get_empty_lcores(self):
        """Get lcore usage from spp-ctl for making launch options.

        Return value is a double dimension list of unsed lcores.
          [[0, 2, 4, ...], [1, 3, 5, ...]]

        To get the result, get CPU layout as an list first, then remove
        used lcores from the list.
        """

        sockets = []  # A set of CPU sockets.

        # Get list of CPU layout
        res = self.spp_ctl_cli.get('cpu_layout')
        if res is not None:
            if res.status_code == 200:
                try:
                    # Get layout of each of sockets as an array.
                    # [[0,1,2,3,..., 15], [16,17,18],...]]
                    for sock in res.json():
                        socket = []
                        for ent in sock['cores']:
                            socket.append(ent['lcores'])

                        socket = sum(socket, [])
                        socket.sort()
                        sockets.append(socket)

                except KeyError as e:
                    print('Error: {} is not defined!'.format(e))

        # Get list of used lcores.
        res = self.spp_ctl_cli.get('cpu_usage')
        if res is not None:
            if res.status_code == 200:
                try:
                    p_master_lcore = None
                    s_master_lcore = None
                    p_slave_lcores = []
                    s_slave_lcores = []

                    cpu_usage = res.json()
                    for ent in cpu_usage:
                        if ent['proc-type'] == 'primary':
                            p_master_lcore = ent['master-lcore']
                            p_slave_lcores.append(ent['lcores'])
                        else:
                            s_master_lcore = ent['master-lcore']
                            s_slave_lcores.append(ent['lcores'])

                    # Remove duplicated lcores and make them unique.
                    # sum() is used for flattening two dimension list.
                    p_slave_lcores = list(set(sum(p_slave_lcores, [])))
                    s_slave_lcores = list(set(sum(s_slave_lcores, [])))

                    # Remove used lcores from `sockets`.
                    for socket in sockets:
                        if p_master_lcore is not None:
                            if p_master_lcore in socket:
                                socket.remove(p_master_lcore)
                        if s_master_lcore is not None:
                            if s_master_lcore in socket:
                                socket.remove(s_master_lcore)
                        for l in p_slave_lcores:
                            if l in socket:
                                socket.remove(l)
                        for l in s_slave_lcores:
                            if l in socket:
                                socket.remove(l)

                except KeyError as e:
                    print('Error: {} is not defined!'.format(e))

        return sockets

    def _setup_launch_opts(self, sub_tokens, cli_config):
        """Make options for launch cmd from config params."""

        if 'max_secondary' in cli_config.keys():
            max_secondary = int(cli_config['max_secondary']['val'])

            if (sub_tokens[1] in spp_common.SEC_TYPES) and \
                    (int(sub_tokens[2])-1 in range(max_secondary)):
                ptype = sub_tokens[1]
                sid = sub_tokens[2]

                # Option of secondary ID is different between spp_nfv
                # and others.
                if ptype == 'nfv':
                    opt_sid = '-n'
                else:
                    opt_sid = '--client-id'

                # Need to replace port from `7777` of spp-ctl to `6666`
                # of secondary process.
                server_addr = common.current_server_addr()
                server_addr = server_addr.replace('7777', '6666')

                # Define number of slave lcores and other options
                # from config.
                if ptype == 'nfv':  # one slave lcore is enough
                    if 'sec_nfv_nof_lcores' in cli_config.keys():
                        tmpkey = 'sec_nfv_nof_lcores'
                        nof_slaves = int(
                                cli_config[tmpkey]['val'])
                elif ptype == 'vf':
                    if 'sec_vf_nof_lcores' in cli_config.keys():
                        nof_slaves = int(
                                cli_config['sec_vf_nof_lcores']['val'])
                elif ptype == 'mirror':  # two slave lcores
                    if 'sec_mirror_nof_lcores' in cli_config.keys():
                        tmpkey = 'sec_mirror_nof_lcores'
                        nof_slaves = int(
                                cli_config[tmpkey]['val'])
                elif ptype == 'pcap':  # at least two slave lcores
                    if 'sec_pcap_nof_lcores' in cli_config.keys():
                        tmpkey = 'sec_pcap_nof_lcores'
                        nof_slaves = int(
                                cli_config[tmpkey]['val'])
                    if 'sec_pcap_port' in cli_config.keys():
                        temp = '-c {}'.format(
                                cli_config['sec_pcap_port']['val'])

                        self.launch_template = '{} {}'.format(
                            self.launch_template, temp)

                # Flag for checking all params are valid or not.
                has_invalid_param = False

                # Get and flatten empty lcores on each of sockets.
                empty_lcores = self._get_empty_lcores()
                empty_lcores = sum(empty_lcores, [])

                # Check if enough number of lcores available.
                if len(empty_lcores) < nof_slaves:
                    common.print_compl_warinig(
                        'No available lcores remained!')
                    return []

                if 'sec_m_lcore' in cli_config.keys():
                    master_lcore = cli_config['sec_m_lcore']['val']
                else:
                    logger.error('Config "sec_m_lcore" is not defined!')
                    has_invalid_param = True

                # Decide lcore option based on configured number of lcores.
                slave_lcores = []
                for l in empty_lcores:
                    # Master lcore ID should be smaller than slaves.
                    if l > int(master_lcore):
                        slave_lcores.append(str(l))

                    # Check if required number of lcores are found.
                    if len(slave_lcores) > (nof_slaves - 1):
                        break

                # TODO(yasufum) make smarter form, for example,
                # change '1,2,3' to '1-3'.
                slave_lcores = ','.join(slave_lcores)

                if 'sec_mem' in cli_config.keys():
                    sec_mem = cli_config['sec_mem']['val']
                else:
                    logger.error('Config "sec_mem" is not defined!')
                    has_invalid_param = True

                if 'sec_vhost_cli' in cli_config.keys():
                    if cli_config['sec_vhost_cli']['val']:
                        vhost_client = '--vhost-client'
                    else:
                        vhost_client = ''
                else:
                    logger.error('Config "sec_vhost_cli" is not defined!')
                    has_invalid_param = True

                # Replace labels in template with params.
                if has_invalid_param is False:
                    candidates = [self.launch_template.format(
                        m_lcore=master_lcore, s_lcores=slave_lcores,
                        mem=sec_mem, opt_sid=opt_sid, sid=sid,
                        sec_addr=server_addr,
                        vhost_cli=vhost_client)]
                else:
                    candidates = []

        else:
            logger.error(
                    'Error: max_secondary is not defined in config')
            candidates = []

        return candidates

    # TODO(yasufum) add checking for cli_config has keys
    def complete(self, text, line, begidx, endidx, cli_config):
        """Completion for primary process commands.

        Called from complete_pri() to complete primary command.
        """

        try:
            candidates = []
            tokens = line.split(' ')

            # Parse command line
            if tokens[0].endswith(';'):

                # Show sub commands
                if len(tokens) == 2:
                    # Add sub commands
                    candidates = candidates + self.PRI_CMDS[:]

                else:
                    # Show args of `launch` sub command.
                    if tokens[1] == 'launch':
                        candidates = self._compl_launch(tokens[1:], cli_config)
                    elif tokens[1] == 'add':
                        candidates = self._compl_add(tokens[1:])
                    elif tokens[1] == 'del':
                        candidates = self._compl_del(tokens[1:])
                    elif tokens[1] == 'patch':
                        candidates = self._compl_patch(tokens[1:])

            if not text:
                completions = candidates
            else:
                completions = [p for p in candidates
                               if p.startswith(text)
                               ]

            return completions

        except Exception as e:
            print(e)

    def _compl_launch(self, sub_tokens, cli_config):
        candidates = []
        if len(sub_tokens) == 2:
            for pt in spp_common.SEC_TYPES:
                candidates.append('{}'.format(pt))

        elif len(sub_tokens) == 3:
            if 'max_secondary' in cli_config.keys():
                max_secondary = int(
                        cli_config['max_secondary']['val'])

                if sub_tokens[1] in spp_common.SEC_TYPES:
                    used_sec_ids = [str(i) for i in self._get_sec_ids()]
                    candidates = [
                            str(i+1) for i in range(max_secondary)
                            if str(i+1) not in used_sec_ids]
                    logger.debug(candidates)
            else:
                logger.error(
                        'Error: max_secondary is not defined in config')
                candidates = []

        elif len(sub_tokens) == 4:
            # Do not show candidate if given sec ID is already used.
            # Sec ID is contained as the 2nd entry in sub_tokens. Here is an
            # example of sub_tokens.
            #   ['launch', 'nfv', '1', '']
            used_sec_ids = [str(i) for i in self._get_sec_ids()]
            if sub_tokens[1] not in used_sec_ids:
                candidates = self._setup_launch_opts(
                        sub_tokens, cli_config)

        return candidates

    # TODO(yasufum): consider to merge nfv's.
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

    # TODO(yasufum): consider to merge nfv's.
    def _compl_del(self, sub_tokens):
        """Complete `del` command."""

        res = []
        # Del command consists of two tokens max, for instance,
        # `nfv 1; del ring:1`.
        if len(sub_tokens) < 3:
            tmp_ary = []

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

    # TODO(yasufum): consider to merge nfv's.
    def _compl_patch(self, sub_tokens):
        """Complete `patch` command."""

        # Patch command consists of three tokens max, for instance,
        # `nfv 1; patch phy:0 ring:1`.
        if len(sub_tokens) < 4:
            res = []

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

    def _get_sec_ids(self):
        sec_ids = []
        res = self.spp_ctl_cli.get('processes')
        if res is not None:
            if res.status_code == 200:
                try:
                    for proc in res.json():
                        if proc['type'] != 'primary':
                            sec_ids.append(proc['client-id'])
                except KeyError as e:
                    print('Error: {} is not defined!'.format(e))
            elif res.status_code in self.spp_ctl_cli.rest_common_error_codes:
                # Print default error message
                pass
            else:
                print('Error: unknown response for _get_sec_ids.')
        return sec_ids

    def _setup_opts_dict(self, opts_list):
        """Setup options for sending to spp-ctl as a request body.

        Options is setup from given list. If option has no value, None is
        assgined for the value. For example,
          ['-l', '1-2', --no-pci, '-m', '512', ...]
          => {'-l':'1-2', '--no-pci':None, '-m':'512', ...}
        """
        prekey = None
        opts_dict = {}
        for opt in opts_list:
            if opt.startswith('-'):
                opts_dict[opt] = None
                prekey = opt
            else:
                if prekey is not None:
                    opts_dict[prekey] = opt
                    prekey = None
        return opts_dict

    def _run_add(self, params):
        """Run `add` command."""

        if len(params) == 0:
            print('Port is required!')
        elif params[0] in self.ports:
            print("'%s' is already added." % params[0])
        else:
            self.ports = self._get_ports()

            req_params = {'action': 'add', 'port': params[0]}

            res = self.spp_ctl_cli.put('primary/ports', req_params)
            if res is not None:
                error_codes = self.spp_ctl_cli.rest_common_error_codes
                if res.status_code == 204:
                    print('Add %s.' % params[0])
                elif res.status_code in error_codes:
                    pass
                else:
                    print('Error: unknown response for add.')

    def _run_del(self, params):
        """Run `del` command."""

        if len(params) == 0:
            print('Port is required!')
        elif 'phy:' in params[0]:
            print("Cannot delete phy port '%s'." % params[0])
        else:
            self.patches = self._get_patches()

            # Patched ports should not be deleted.
            patched_ports = self._get_patched_ports()

            if params[0] in patched_ports:
                print("Cannot delete patched port '%s'." % params[0])
            else:
                req_params = {'action': 'del', 'port': params[0]}
                res = self.spp_ctl_cli.put('primary/ports', req_params)
                if res is not None:
                    error_codes = self.spp_ctl_cli.rest_common_error_codes
                    if res.status_code == 204:
                        print('Delete %s.' % params[0])
                    elif res.status_code in error_codes:
                        pass
                    else:
                        print('Error: unknown response for del.')

    def _run_forward_or_stop(self, cmd):
        """Run `forward` or `stop` command."""

        if cmd == 'forward':
            req_params = {'action': 'start'}
        elif cmd == 'stop':
            req_params = {'action': 'stop'}
        else:
            print('Unknown command. "forward" or "stop"?')

        res = self.spp_ctl_cli.put('primary/forward', req_params)

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
                print('Error: unknown response for forward/stop.')

    def _run_patch(self, params):
        """Run `patch` command."""

        if len(params) == 0:
            print('Params are required!')
        elif params[0] == 'reset':
            res = self.spp_ctl_cli.delete('primary/patches')
            if res is not None:
                error_codes = self.spp_ctl_cli.rest_common_error_codes
                if res.status_code == 204:
                    print('Clear all of patches.')
                elif res.status_code in error_codes:
                    pass
                else:
                    print('Error: unknown response for patch.')
        else:
            if len(params) < 2:
                print('Dst port is required!')
            else:
                req_params = {'src': params[0], 'dst': params[1]}
                res = self.spp_ctl_cli.put('primary/patches',
                                           req_params)
                if res is not None:
                    error_codes = self.spp_ctl_cli.rest_common_error_codes
                    if res.status_code == 204:
                        print('Patch ports (%s -> %s).' % (
                            params[0], params[1]))
                    elif res.status_code in error_codes:
                        pass
                    else:
                        print('Error: unknown response for patch.')

    def _run_launch(self, params, wait_time):
        """Launch secondary process.

        Parse `launch` command and send request to spp-ctl. Params of the
        consists of proc type, sec ID and arguments. It allows to skip some
        params which are completed. All of examples here are same.

        spp > lanuch nfv -l 1-2 ... -- -n 1 ...  # sec ID '1' is skipped
        spp > lanuch spp_nfv -l 1-2 ... -- -n 1 ...  # use 'spp_nfv' insteads
        """

        # Check params
        if len(params) < 2:
            print('Invalid syntax! Proc type, ID and options are required.')
            print('E.g. "nfv 1 -l 1-2 -m 512 -- -n 1 -s 192.168.1.100:6666"')
            return None

        proc_type = params[0]
        if params[1].startswith('-'):
            sec_id = None  # should be found later, or failed
            args = params[1:]
        else:
            sec_id = params[1]
            args = params[2:]

        if proc_type.startswith('spp_') is not True:
            proc_name = 'spp_' + proc_type
        else:
            proc_name = proc_type
            proc_type = proc_name[len('spp_'):]

        if proc_type not in spp_common.SEC_TYPES:
            print("'{}' is not supported in launch cmd.".format(proc_type))
            return None

        if '--' not in args:
            print('Arguments should include separator "--".')
            return None

        # Setup options of JSON sent to spp-ctl. Here is an example for
        # launching spp_nfv.
        #   {
        #      'client_id': '1',
        #      'proc_name': 'spp_nfv',
        #      'eal': {'-l': '1-2', '-m': '1024', ...},
        #      'app': {'-n': '1', '-s': '192.168.1.100:6666'}
        #   }
        idx_separator = args.index('--')
        eal_opts = args[:idx_separator]
        app_opts = args[(idx_separator+1):]

        if '--proc-type' not in args:
            eal_opts.append('--proc-type')
            eal_opts.append('secondary')

        opts = {'proc_name': proc_name}
        opts['eal'] = self._setup_opts_dict(eal_opts)
        opts['app'] = self._setup_opts_dict(app_opts)

        # Try to find sec_id from app options.
        if sec_id is None:
            if (proc_type == 'nfv') and ('-n' in opts['app']):
                sec_id = opts['app']['-n']
            elif ('--client-id' in opts['app']):  # vf, mirror or pcap
                sec_id = opts['app']['--client-id']
            else:
                print('Secondary ID is required!')
                return None

        if sec_id in self._get_sec_ids():
            print("Cannot add '{}' already used.".format(sec_id))
            return None

        opts['client_id'] = sec_id

        # Complete or correct sec_id.
        if proc_name == 'spp_nfv':
            if '-n' in opts['app'].keys():
                if (opts['app']['-n'] != sec_id):
                    opts['app']['-n'] = sec_id
            else:
                opts['app']['-n'] = sec_id
        else:  # vf, mirror or pcap
            if '--client-id' in opts['app'].keys():
                if (opts['app']['--client-id'] != sec_id):
                    opts['app']['--client-id'] = sec_id
            else:
                opts['app']['--client-id'] = sec_id

        # Check if sec ID is already used.
        used_sec_ids = [str(i) for i in self._get_sec_ids()]
        if sec_id in used_sec_ids:
            print('Secondary ID {sid} is already used!'.format(
                sid=sec_id))
            return None

        logger.debug('launch, {}'.format(opts))

        # Send request for launching secondary.
        res = self.spp_ctl_cli.put('primary/launch', opts)
        if res is not None:
            error_codes = self.spp_ctl_cli.rest_common_error_codes
            if res.status_code == 204:
                # Wait for launching secondary as best effort
                time.sleep(wait_time)

                print('Send request to launch {ptype}:{sid}.'.format(
                    ptype=proc_type, sid=sec_id))

            elif res.status_code in error_codes:
                pass
            else:
                print('Error: unknown response for launch.')

    @classmethod
    def help(cls):
        msg = """Send a command to primary process.

        Show resources and statistics, or clear it.
            spp > pri; status  # show status
            spp > pri; clear   # clear statistics

        Launch secondary process..
            # Launch nfv:1
            spp > pri; launch nfv 1 -l 1,2 -m 512 -- -n 1 -s 192.168....
            # Launch vf:2
            spp > pri; launch vf 2 -l 1,4-7 -m 512 -- --client-id 2 -s ...
        """

        print(msg)
