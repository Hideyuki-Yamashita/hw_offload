# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017-2018 Nippon Telegraph and Telephone Corporation

import re
from ..shell_lib import common
from ..spp_common import logger
from .. import spp_ctl_client


class SppCtlServer(object):
    """Execute server command for switching spp-ctl.

    SppCtlServer class is intended to be used in Shell class as a delegator
    for running 'server' command.

    'self.run()' is called from do_server() and 'self.complete()' is called
    from complete_server() of both of which is defined in Shell.
    """

    SERVER_CMDS = ['list', 'add', 'del']

    def __init__(self, spp_cli_objs):
        self.spp_cli_objs = spp_cli_objs
        self.current_idx = 0  # index of current server in use

    def run(self, commands):
        args = re.sub(r'\s+', ' ', commands).split(' ')
        if '' in args:
            args.remove('')

        if len(args) == 0 or args[0] == 'list':
            self._show_server_list()

        elif args[0] == 'add':
            if len(args) < 2:
                print('Error: server address is requiired.')
            else:
                self._register_server(args[1])

        elif args[0] == 'del':
            if len(args) < 2:
                print('Error: server address or index is requiired.')
            else:
                self._unregister_server(args[1])

        else:  # show servers if index is given.
            idx = int(args[0]) - 1
            if idx >= 0:
                self._switch_to(idx)
            else:
                print('Server index should be > 0 !')

    def complete(self, text, line, begidx, endidx):
        """Completion for server command.

        Called from complete_server() to complete server command.
        """

        candidates = []
        tokens = line.split(' ')

        # Show sub commands and indices of servers.
        if len(tokens) == 2:
            # Add server IDs
            for i in range(len(self.spp_cli_objs)):
                candidates.append(str(i + 1))

            # Add sub commands
            candidates = candidates + self.SERVER_CMDS[:]

            if not text:
                completions = candidates
            else:
                completions = [p for p in candidates
                               if p.startswith(text)
                               ]

        # Show args of `del` sub command.
        elif len(tokens) == 3:
            if tokens[1] == 'del':
                for i in range(len(self.spp_cli_objs)):
                    # Add both of indices and addresses to candidates.
                    candidates.append(str(i + 1))
                    candidates.append('{}:{}'.format(
                        self.spp_cli_objs[i].ip_addr,
                        self.spp_cli_objs[i].port))

                # Remove current server from candidates.
                if (str(self.current_idx + 1)) in candidates:
                    candidates.remove(str(self.current_idx + 1))
                    candidates.remove('{}:{}'.format(
                            self.spp_cli_objs[self.current_idx].ip_addr,
                            self.spp_cli_objs[self.current_idx].port))

            if not text:
                completions = candidates
            else:
                completions = [p for p in candidates
                               if p.startswith(text)
                               ]

        return completions

    def get_current_server(self):
        return self.spp_cli_objs[self.current_idx]

    def _show_server_list(self):
        cnt = 1
        for cli_obj in self.spp_cli_objs:
            # Put a mark to current server.
            if cnt == self.current_idx + 1:
                current = '*'
            else:
                current = ''

            print('  %d: %s:%s %s' % (
                  cnt, cli_obj.ip_addr, cli_obj.port, current))
            cnt += 1

    def _switch_to(self, idx):
        """Switch to server of given index."""

        if len(self.spp_cli_objs) > idx:
            self.current_idx = idx
            cli_obj = self.spp_cli_objs[self.current_idx]

            common.set_current_server_addr(
                 cli_obj.ip_addr, cli_obj.port)

            print('Switch spp-ctl to "{}: {}:{}".'.format(
                 idx+1, cli_obj.ip_addr, cli_obj.port))
        else:
            print('Cannot switch to no existing server "{}".'.format(
                idx + 1))

    def _parse_addr(self, addr):
        """Parse IP address and port of `ipaddr:port` format.

        Return IP address and port as a tuple of (ipaddr, port). This
        is an example.
            ipaddr, port = self._parse_addr('192.168.11.100:7777')
        """
        if ':' in addr:
            ipaddr, port = addr.split(':')
            if common.is_valid_port(port) is False:
                print('Error: Invalid port in "{}".'.format(port))
                port = None
            else:
                port = int(port)
        else:
            ipaddr = addr
            port = 7777

        if common.is_valid_ipv4_addr(ipaddr) is False:
            print('Error: Invalid address "{}".'.format(ipaddr))
            ipaddr = None

        return ipaddr, port

    def _is_address_registered(self, ipaddr, port):
        """Check if given address is included server list."""

        if (ipaddr is not None) and (port is not None):
            for cli_obj in self.spp_cli_objs:
                if (cli_obj.ip_addr == ipaddr) and \
                        (cli_obj.port == int(port)):
                    return True

        return False

    def _register_server(self, addr):
        """Add server of given address to server list."""

        ipaddr, port = self._parse_addr(addr)

        if (ipaddr is None) or (port is None):
            return None

        if self._is_address_registered(ipaddr, port) is True:
            print('"{}:{}" is already registered.'.format(
                ipaddr, port))
            return None

        spp_ctl_cli = spp_ctl_client.SppCtlClient(ipaddr, port)
        if spp_ctl_cli.is_server_running() is False:
            print('Is not spp-ctl on "{}:{}", nor correct address?'.format(
                  ipaddr, port))
            return False
        else:
            self.spp_cli_objs.append(spp_ctl_cli)
            print('Registered spp-ctl "{}:{}".'.format(ipaddr, port))

        return True

    def _unregister_server(self, arg):
        """Delete server of given address from server list.

        Server is specified with ID or address.
        """

        removed = None  # contains instance of removed SppCtlClient.

        # Check if given ID is negative and remove `-` because this method
        # uses `str.isdigit()`, but it cannot be used for negative number.
        if arg.startswith('-'):
            is_arg_negative = True
            arg = arg[1:]
        else:
            is_arg_negative = False

        # Specified with server ID.
        if str.isdigit(arg):
            if is_arg_negative:  # return to negative num.
                idx = -1 * int(arg) - 1
            else:
                idx = int(arg) - 1

            if idx < 0:  # negative ID is invalid
                print('Cannot del server of invalid ID "{}"!'.format(
                    idx + 1))
            elif (idx > len(self.spp_cli_objs) - 1):
                print('Cannot del server "{}" not exist!'.format(idx + 1))
            elif idx == self.current_idx:
                print('Cannot del server "{}" in use!'.format(
                    self.current_idx + 1))
            else:
                removed = self.spp_cli_objs.pop(idx)

        # Specified with address of the server.
        else:
            ipaddr, port = self._parse_addr(arg)
            if (ipaddr is None) or (port is None):
                return None

            if self._is_address_registered(ipaddr, port) is False:
                print('"{}:{}" is not registered.'.format(ipaddr, port))
                return None

            cur_serv = self.spp_cli_objs[self.current_idx]
            if (ipaddr == cur_serv.ip_addr) and (port == cur_serv.port):
                print('Cannot del server "{}:{}" in use!'.format(
                    ipaddr, port))
                return None

            cnt = 0
            for spp_cli in self.spp_cli_objs:
                if (ipaddr == spp_cli.ip_addr) and (port == spp_cli.port):
                    removed = self.spp_cli_objs.pop(cnt)
                    break
                cnt += 1

        if removed is not None:
            # Update current_idx if removed is before current in the order.
            if self.current_idx >= len(self.spp_cli_objs):
                self.current_idx = self.current_idx - 1

            print('Unregistered spp-ctl "{}:{}".'.format(
                removed.ip_addr, removed.port))

        return True

    @classmethod
    def help(cls):
        msg = """Switch SPP REST API server.

        Show a list of servers. '*' means that it is under the control.

            spp > server  # or 'server list'
              1: 192.168.1.101:7777 *
              2: 192.168.1.102:7777

        Switch to the second node with index or address.

            spp > server 2
            Switch spp-ctl to "2: 192.168.1.102:7777".

            # It is the same
            spp > server 192.168.1.101  # no need port if default
            Switch spp-ctl to "1: 192.168.1.101:7777".

        Register or unregister a node by using 'add' or 'del' command.
        For unregistering, node is also specified with index.

            # Register third node
            spp > server add 192.168.122.177
            Registered spp-ctl "192.168.122.177:7777".

            # Unregister second one
            spp > server del 2  # or 192.168.1.102
            Unregistered spp-ctl "192.168.1.102:7777".
        """

        print(msg)
