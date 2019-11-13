# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2015-2019 Intel Corporation

import cmd
from .commands import bye
from .commands import pri
from .commands import nfv
from .commands import server
from .commands import topo
from .commands import vf
from .commands import mirror
from .commands import pcap
from .commands import help_msg
import os
import re
import readline
from .shell_lib import common
from . import spp_common
from .spp_common import logger
import subprocess
import time
import yaml


class Shell(cmd.Cmd, object):
    """SPP command prompt."""

    WAIT_PRI_INTERVAL = 0.5  # sec
    WAIT_PRI_TIMEOUT = 20    # sec

    def __init__(self, spp_cli_objs, config, wait_pri=False, use_cache=False):

        # Load default config, can be changed via `config` command
        try:
            if config is not None:
                config_path = "{}/{}".format(
                        os.getcwd(), config)
            else:
                config_path = "{}/config/default.yml".format(
                        os.path.dirname(__file__))

            self.cli_config = yaml.load(open(config_path),
                                        Loader=yaml.FullLoader)

            # TODO(yasufum) add validating config params with
            # common.validate_config_val() here. Exit if it is invalid.

        except IOError as e:
            print('Error: No config file found!')
            print(e)
            exit()

        self.hist_file = os.path.expanduser('~/.spp_history')
        self.plugin_dir = 'plugins'

        # Commands not included in history
        self.hist_except = ['bye', 'exit', 'history', 'redo']

        # Shell settings which are reserved vars of Cmd class.
        # `intro` is to be shown as a welcome message.
        self.intro = 'Welcome to the SPP CLI. ' + \
                     'Type `help` or `?` to list commands.\n'
        self.prompt = self.cli_config['prompt']['val']  # command prompt

        # Recipe file to be recorded with `record` command
        self.recorded_file = None

        # setup history file
        if os.path.exists(self.hist_file):
            readline.read_history_file(self.hist_file)
        else:
            readline.write_history_file(self.hist_file)

        cmd.Cmd.__init__(self)
        self.spp_ctl_server = server.SppCtlServer(spp_cli_objs)
        self.spp_ctl_cli = spp_cli_objs[0]
        self.use_cache = use_cache
        self.init_spp_procs()
        self.spp_topo = topo.SppTopo(
                self.spp_ctl_cli, {}, self.cli_config)

        common.set_current_server_addr(
                self.spp_ctl_cli.ip_addr, self.spp_ctl_cli.port)

        if wait_pri is True:
            self._wait_pri_launched()

    def init_spp_procs(self):
        """Initialize delegators of SPP processes.

        Delegators accept a command and sent it to SPP proesses. This method
        is also called from `postcmd()` method to update it to the latest
        status.
        """

        self.primary = pri.SppPrimary(self.spp_ctl_cli)

        self.secondaries = {}
        self.secondaries['nfv'] = {}
        for sec_id in self.spp_ctl_cli.get_sec_ids('nfv'):
            self.secondaries['nfv'][sec_id] = nfv.SppNfv(
                    self.spp_ctl_cli, sec_id)

        self.secondaries['vf'] = {}
        for sec_id in self.spp_ctl_cli.get_sec_ids('vf'):
            self.secondaries['vf'][sec_id] = vf.SppVf(
                    self.spp_ctl_cli, sec_id)

        self.secondaries['mirror'] = {}
        for sec_id in self.spp_ctl_cli.get_sec_ids('mirror'):
            self.secondaries['mirror'][sec_id] = mirror.SppMirror(
                    self.spp_ctl_cli, sec_id)

        self.secondaries['pcap'] = {}
        for sec_id in self.spp_ctl_cli.get_sec_ids('pcap'):
            self.secondaries['pcap'][sec_id] = pcap.SppPcap(
                    self.spp_ctl_cli, sec_id)

    def _wait_pri_launched(self):
        """Wait for launching spp_primary."""

        print('Waiting for spp_primary is ready ...',
                end='', flush=True)
        wait_cnt = self.WAIT_PRI_TIMEOUT / self.WAIT_PRI_INTERVAL
        cnt = 0
        is_pri_ready = False
        while cnt < wait_cnt:
            res = self.spp_ctl_cli.get('processes')
            if res is not None:
                if res.status_code == 200:
                    pri_obj = None
                    try:
                        proc_objs = res.json()
                        for proc_obj in proc_objs:
                            if proc_obj['type'] == 'primary':
                                pri_obj = proc_obj
                    except KeyError as e:
                        print('Error: {} is not defined!'.format(e))

                    if pri_obj is not None:
                        is_pri_ready = True
                        break
            time.sleep(self.WAIT_PRI_INTERVAL)
            print('.', end='', flush=True)
            cnt += 1

        t = cnt * self.WAIT_PRI_INTERVAL
        if is_pri_ready is True:
            print(' OK! ({}[sec])'.format(t))
        else:
            print(' Timeout! ({}[sec])'.format(t))

    # Called everytime after running command. `stop` is returned from `do_*`
    # method and SPP CLI is terminated if it is True. It means that only
    # `do_bye` and  `do_exit` return True.
    def postcmd(self, stop, line):
        if self.use_cache is False:
            self.init_spp_procs()

        # TODO(yasufum) do not add to history if command is failed.
        if line.strip().split(' ')[0] not in self.hist_except:
            readline.write_history_file(self.hist_file)
        return stop

    def default(self, line):
        """Define defualt behaviour.

        If user input is comment styled, controller simply echo
        as a comment.
        """

        if common.is_comment_line(line):
            print("{}".format(line.strip()))

        else:
            super(Shell, self).default(line)

    def emptyline(self):
        """Do nothin for empty input.

        It override Cmd.emptyline() which runs previous input as default
        to do nothing.
        """
        pass

    def print_status(self):
        """Display information about connected clients."""

        print('- spp-ctl:')
        print('  - address: {}:{}'.format(self.spp_ctl_cli.ip_addr,
                                      self.spp_ctl_cli.port))
        res = self.spp_ctl_cli.get('processes')
        if res is not None:
            if res.status_code == 200:
                try:
                    proc_objs = res.json()
                    pri_obj = None
                    sec_obj = {}
                    sec_obj['nfv'] = []
                    sec_obj['vf'] = []
                    sec_obj['mirror'] = []
                    sec_obj['pcap'] = []

                    for proc_obj in proc_objs:
                        if proc_obj['type'] == 'primary':
                            pri_obj = proc_obj
                        else:
                            sec_obj[proc_obj['type']].append(proc_obj)

                    print('- primary:')
                    if pri_obj is not None:
                        print('  - status: running')
                    else:
                        print('  - status: not running')

                    print('- secondary:')
                    print('  - processes:')
                    cnt = 1
                    for pt in ['nfv', 'vf', 'mirror', 'pcap']:
                        for obj in sec_obj[pt]:
                            print('    {}: {}:{}'.format(
                                    cnt, obj['type'], obj['client-id']))
                            cnt += 1
                except KeyError as e:
                    print('Error: {} is not defined!'.format(e))
            elif res.status_code in self.spp_ctl_cli.rest_common_error_codes:
                pass
            else:
                print('Error: unknown response.')

    def is_patched_ids_valid(self, id1, id2, delim=':'):
        """Check if port IDs are valid

        Supported format is port ID of integer or resource UID such as
        'phy:0' or 'ring:1'. Default delimiter ':' can be overwritten
        by giving 'delim' option.
        """

        if str.isdigit(id1) and str.isdigit(id2):
            return True
        else:
            ptn = r"\w+\%s\d+" % delim  # Match "phy:0" or "ring:1" or so
            if re.match(ptn, id1) and re.match(ptn, id2):
                pt1 = id1.split(delim)[0]
                pt2 = id2.split(delim)[0]
                if (pt1 in spp_common.PORT_TYPES) \
                        and (pt2 in spp_common.PORT_TYPES):
                    return True
        return False

    def clean_cmd(self, cmdstr):
        """remove unwanted spaces to avoid invalid command error"""

        tmparg = re.sub(r'\s+', " ", cmdstr)
        res = re.sub(r'\s?;\s?', ";", tmparg)
        return res

    def _is_sec_registered(self, ptype, sid):
        """Check secondary process is registered.

        Return True if registered, or print error and return False if not.
        """

        if sid in self.secondaries[ptype]:
            return True
        else:
            print('"{ptype} {sid}" does not exist.'.format(
                ptype=ptype, sid=sid))
            return False

    def precmd(self, line):
        """Called before running a command

        It is called for checking a contents of command line.
        """
        if self.use_cache is False:
            self.init_spp_procs()

        if self.recorded_file:
            if not (
                    ('playback' in line) or
                    ('bye' in line) or
                    ('exit' in line)):
                self.recorded_file.write("%s\n" % line)
        return line

    def close(self):
        """Close record file"""

        if self.recorded_file:
            print("Closing file")
            self.recorded_file.close()
            self.recorded_file = None

    def do_server(self, commands):
        """Switch SPP REST API server."""

        self.spp_ctl_server.run(commands)
        self.spp_ctl_cli = self.spp_ctl_server.get_current_server()

    def help_server(self):
        """Print help message of server command."""
        server.SppCtlServer.help()

    def complete_server(self, text, line, begidx, endidx):
        """Completion for server command."""

        line = self.clean_cmd(line)
        res = self.spp_ctl_server.complete(text, line, begidx, endidx)
        return res

    def do_status(self, _):
        """Display status info of SPP processes."""
        self.print_status()

    def help_status(self):
        """Print help message of status command."""
        print(help_msg.cmds['status'])

    def do_pri(self, command):
        """Send a command to primary process."""

        # Remove unwanted spaces and first char ';'
        command = self.clean_cmd(command)[1:]

        if logger is not None:
            logger.info("Receive pri command: '%s'" % command)

        self.primary.run(command, self.cli_config)

    def help_pri(self):
        """Print help message of pri command."""
        pri.SppPrimary.help()

    def complete_pri(self, text, line, begidx, endidx):
        """Completion for primary process commands."""

        line = re.sub(r';\s*', "; ", line)
        line = re.sub(r'\s+', " ", line)
        return self.primary.complete(
                text, line, begidx, endidx,
                self.cli_config)

    def do_nfv(self, cmd):
        """Send a command to spp_nfv specified with ID."""

        # remove unwanted spaces to avoid invalid command error
        tmparg = self.clean_cmd(cmd)
        cmds = tmparg.split(';')
        if len(cmds) < 2:
            print("Required an ID and ';' before the command.")
        elif str.isdigit(cmds[0]):
            if self._is_sec_registered('nfv', int(cmds[0])):
                self.secondaries['nfv'][int(cmds[0])].run(cmds[1])
        else:
            print('Invalid command: {}'.format(tmparg))

    def help_nfv(self):
        """Print help message of nfv command."""
        nfv.SppNfv.help()

    def complete_nfv(self, text, line, begidx, endidx):
        """Completion for nfv command."""

        if self.use_cache is False:
            self.init_spp_procs()

        line = self.clean_cmd(line)

        tokens = line.split(';')
        if len(tokens) == 1:
            # Add SppNfv of sec_id if it is not exist
            sec_ids = self.spp_ctl_cli.get_sec_ids('nfv')
            for idx in sec_ids:
                if self.secondaries['nfv'][idx] is None:
                    self.secondaries['nfv'][idx] = nfv.SppNfv(
                            self.spp_ctl_cli, idx)

            if len(line.split()) == 1:
                res = [str(i)+';' for i in sec_ids]
            else:
                if not (';' in line):
                    res = [str(i)+';'
                           for i in sec_ids
                           if (str(i)+';').startswith(text)]
            return res
        elif len(tokens) == 2:
            first_tokens = tokens[0].split(' ')  # 'nfv 1' => ['nfv', '1']
            if len(first_tokens) == 2:
                idx = int(first_tokens[1])

                # Add SppVf of sec_id if it is not exist
                if self.secondaries['nfv'][idx] is None:
                    self.secondaries['nfv'][idx] = nfv.SppNfv(
                            self.spp_ctl_cli, idx)

                res = self.secondaries['nfv'][idx].complete(
                        self.spp_ctl_cli.get_sec_ids('nfv'),
                        text, line, begidx, endidx)

                # logger.info(res)
                return res

    def do_vf(self, cmd):
        """Send a command to spp_vf."""

        # remove unwanted spaces to avoid invalid command error
        tmparg = self.clean_cmd(cmd)
        cmds = tmparg.split(';')
        if len(cmds) < 2:
            print("Required an ID and ';' before the command.")
        elif str.isdigit(cmds[0]):

            if self._is_sec_registered('vf', int(cmds[0])):
                self.secondaries['vf'][int(cmds[0])].run(cmds[1])
        else:
            print('Invalid command: {}'.format(tmparg))

    def help_vf(self):
        """Print help message of vf command."""
        vf.SppVf.help()

    def complete_vf(self, text, line, begidx, endidx):
        """Completion for vf command."""

        if self.use_cache is False:
            self.init_spp_procs()

        line = self.clean_cmd(line)

        tokens = line.split(';')
        if len(tokens) == 1:
            # Add SppVf of sec_id if it is not exist
            sec_ids = self.spp_ctl_cli.get_sec_ids('vf')
            for idx in sec_ids:
                if self.secondaries['vf'][idx] is None:
                    self.secondaries['vf'][idx] = vf.SppVf(
                            self.spp_ctl_cli, idx)

            if len(line.split()) == 1:
                res = [str(i)+';' for i in sec_ids]
            else:
                if not (';' in line):
                    res = [str(i)+';'
                           for i in sec_ids
                           if (str(i)+';').startswith(text)]
            return res
        elif len(tokens) == 2:
            first_tokens = tokens[0].split(' ')  # 'vf 1' => ['vf', '1']
            if len(first_tokens) == 2:
                idx = int(first_tokens[1])

                # Add SppVf of sec_id if it is not exist
                if self.secondaries['vf'][idx] is None:
                    self.secondaries['vf'][idx] = vf.SppVf(
                            self.spp_ctl_cli, idx)

                return self.secondaries['vf'][idx].complete(
                        self.spp_ctl_cli.get_sec_ids('vf'),
                        text, line, begidx, endidx)

    def do_mirror(self, cmd):
        """Send a command to spp_mirror."""

        # remove unwanted spaces to avoid invalid command error
        tmparg = self.clean_cmd(cmd)
        cmds = tmparg.split(';')
        if len(cmds) < 2:
            print("Required an ID and ';' before the command.")
        elif str.isdigit(cmds[0]):
            if self._is_sec_registered('mirror', int(cmds[0])):
                self.secondaries['mirror'][int(cmds[0])].run(cmds[1])
        else:
            print('Invalid command: {}'.format(tmparg))

    def help_mirror(self):
        """Print help message of mirror command."""
        mirror.SppMirror.help()

    def complete_mirror(self, text, line, begidx, endidx):
        """Completion for mirror command."""

        if self.use_cache is False:
            self.init_spp_procs()

        line = self.clean_cmd(line)

        tokens = line.split(';')
        if len(tokens) == 1:
            # Add SppMirror of sec_id if it is not exist
            sec_ids = self.spp_ctl_cli.get_sec_ids('mirror')
            for idx in sec_ids:
                if self.secondaries['mirror'][idx] is None:
                    self.secondaries['mirror'][idx] = mirror.SppMirror(
                            self.spp_ctl_cli, idx)

            if len(line.split()) == 1:
                res = [str(i)+';' for i in sec_ids]
            else:
                if not (';' in line):
                    res = [str(i)+';'
                           for i in sec_ids
                           if (str(i)+';').startswith(text)]
            return res
        elif len(tokens) == 2:
            # Split tokens like as from 'mirror 1' to ['mirror', '1']
            first_tokens = tokens[0].split(' ')
            if len(first_tokens) == 2:
                idx = int(first_tokens[1])

                # Add SppMirror of sec_id if it is not exist
                if self.secondaries['mirror'][idx] is None:
                    self.secondaries['mirror'][idx] = mirror.SppMirror(
                            self.spp_ctl_cli, idx)

                return self.secondaries['mirror'][idx].complete(
                        self.spp_ctl_cli.get_sec_ids('mirror'),
                        text, line, begidx, endidx)

    def do_pcap(self, cmd):
        """Send a command to spp_pcap."""

        # remove unwanted spaces to avoid invalid command error
        tmparg = self.clean_cmd(cmd)
        cmds = tmparg.split(';')
        if len(cmds) < 2:
            print("Required an ID and ';' before the command.")
        elif str.isdigit(cmds[0]):
            if self._is_sec_registered('pcap', int(cmds[0])):
                self.secondaries['pcap'][int(cmds[0])].run(cmds[1])
        else:
            print('Invalid command: {}'.format(tmparg))

    def help_pcap(self):
        """Print help message of pcap command."""
        pcap.SppPcap.help()

    def complete_pcap(self, text, line, begidx, endidx):
        """Completion for pcap command."""

        if self.use_cache is False:
            self.init_spp_procs()

        line = self.clean_cmd(line)

        tokens = line.split(';')
        if len(tokens) == 1:
            # Add SppPcap of sec_id if it is not exist
            sec_ids = self.spp_ctl_cli.get_sec_ids('pcap')
            for idx in sec_ids:
                if self.secondaries['pcap'][idx] is None:
                    self.secondaries['pcap'][idx] = pcap.SppPcap(
                            self.spp_ctl_cli, idx)

            if len(line.split()) == 1:
                res = [str(i)+';' for i in sec_ids]
            else:
                if not (';' in line):
                    res = [str(i)+';'
                           for i in sec_ids
                           if (str(i)+';').startswith(text)]
            return res
        elif len(tokens) == 2:
            # Split tokens like as from 'pcap 1' to ['pcap', '1']
            first_tokens = tokens[0].split(' ')
            if len(first_tokens) == 2:
                idx = int(first_tokens[1])

                # Add SppPcap of sec_id if it is not exist
                if self.secondaries['pcap'][idx] is None:
                    self.secondaries['pcap'][idx] = pcap.SppPcap(
                            self.spp_ctl_cli, idx)

                return self.secondaries['pcap'][idx].complete(
                        self.spp_ctl_cli.get_sec_ids('pcap'),
                        text, line, begidx, endidx)

    def do_record(self, fname):
        """Save commands as a recipe file."""

        if fname == '':
            print("Record file is required!")
        else:
            self.recorded_file = open(fname, 'w')

    def help_record(self):
        """Print help message of record command."""
        print(help_msg.cmds['record'])

    def complete_record(self, text, line, begidx, endidx):
        return common.compl_common(text, line)

    def do_playback(self, fname):
        """Setup a network configuration from recipe file."""

        if fname == '':
            print("Record file is required!")
        else:
            self.close()
            try:
                with open(fname) as recorded_file:
                    lines = []
                    for line in recorded_file:
                        if not common.is_comment_line(line):
                            lines.append("# %s" % line)
                        lines.append(line)
                    self.cmdqueue.extend(lines)
            except IOError:
                message = "Error: File does not exist."
                print(message)

    def help_playback(self):
        """Print help message of playback command."""
        print(help_msg.cmds['playback'])

    def complete_playback(self, text, line, begidx, endidx):
        return common.compl_common(text, line)

    def do_config(self, args):
        """Show or update config."""

        tokens = args.strip().split(' ')
        if len(tokens) == 1:
            key = tokens[0]
            if key == '':
                for k, v in self.cli_config.items():
                    print('- {}: "{}"\t# {}'.format(k, v['val'], v['desc']))
            elif key in self.cli_config.keys():
                print('- {}: "{}"\t# {}'.format(
                    key, self.cli_config[key]['val'],
                    self.cli_config[key]['desc']))
            else:
                res = {}
                for k, v in self.cli_config.items():
                    if k.startswith(key):
                        res[k] = {'val': v['val'], 'desc': v['desc']}
                for k, v in res.items():
                    print('- {}: "{}"\t# {}'.format(k, v['val'], v['desc']))

        elif len(tokens) > 1:
            key = tokens[0]
            if key in self.cli_config.keys():
                for s in ['"', "'"]:
                    args = args.replace(s, '')

                val = args[(len(key) + 1):]
                if common.validate_config_val(key, val):
                    self.cli_config[key]['val'] = val
                    print('Set {k}: "{v}"'.format(k=key, v=val))
                else:
                    print('Invalid value "{v}" for "{k}"'.format(
                        k=key, v=val))

                # Command prompt should be updated immediately
                if key == 'prompt':
                    self.prompt = self.cli_config['prompt']['val']
                elif key == 'topo_size':
                    self.spp_topo.resize(
                            self.cli_config['topo_size']['val'])

    def help_config(self):
        """Print help message of config command."""
        print(help_msg.cmds['config'])

    def complete_config(self, text, line, begidx, endidx):
        candidates = []
        tokens = line.strip().split(' ')

        if len(tokens) == 1:
            candidates = self.cli_config.keys()
        elif len(tokens) == 2:
            if text:
                candidates = self.cli_config.keys()

        if not text:
            completions = candidates
        else:
            logger.debug(candidates)
            completions = [p for p in candidates
                           if p.startswith(text)
                           ]
        return completions

    def do_pwd(self, args):
        """Show corrent directory."""
        print(os.getcwd())

    def help_pwd(self):
        """Print help message of pwd command."""
        print(help_msg.cmds['pwd'])

    def do_ls(self, args):
        """Show a list of specified directory."""

        if args == '' or os.path.isdir(args):
            c = 'ls -F %s' % args
            subprocess.call(c, shell=True)
        else:
            print("No such a directory.")

    def help_ls(self):
        """Print help message of ls command."""
        print(help_msg.cmds['ls'])

    def complete_ls(self, text, line, begidx, endidx):
        return common.compl_common(text, line)

    def do_cd(self, args):
        """Change current directory."""

        if os.path.isdir(args):
            os.chdir(args)
            print(os.getcwd())
        else:
            print("No such a directory.")

    def help_cd(self):
        """Print help message of cd command."""
        print(help_msg.cmds['cd'])

    def complete_cd(self, text, line, begidx, endidx):
        return common.compl_common(text, line, 'directory')

    def do_mkdir(self, args):
        """Create a new directory."""

        c = 'mkdir -p %s' % args
        subprocess.call(c, shell=True)

    def help_mkdir(self):
        """Print help message of mkdir command."""
        print(help_msg.cmds['mkdir'])

    def complete_mkdir(self, text, line, begidx, endidx):
        return common.compl_common(text, line)

    def do_bye(self, args):
        """Terminate SPP processes and controller."""

        cmds = args.split(' ')
        if cmds[0] == '':  # terminate SPP CLI itself
            self.do_exit('')
            return True
        else:  # terminate other SPP processes
            spp_bye = bye.SppBye()
            spp_bye.run(args, self.primary, self.secondaries)

    def help_bye(self):
        """Print help message of bye command."""
        bye.SppBye.help()

    def complete_bye(self, text, line, begidx, endidx):
        """Completion for bye commands"""

        spp_bye = bye.SppBye()
        return spp_bye.complete(text, line, begidx, endidx)

    def do_cat(self, arg):
        """View contents of a file."""
        if os.path.isfile(arg):
            c = 'cat %s' % arg
            subprocess.call(c, shell=True)
        else:
            print("No such a directory.")

    def help_cat(self):
        """Print help message of cat command."""
        print(help_msg.cmds['cat'])

    def do_redo(self, args):
        """Execute command of index of history."""

        idx = int(args)
        cmdline = None
        cnt = 1
        try:
            for line in open(self.hist_file):
                if cnt == idx:
                    cmdline = line.strip()
                    break
                cnt += 1

            if cmdline.find('pri;') > -1:
                cmdline = cmdline.replace(';', ' ;')
                print(cmdline)
            cmd_ary = cmdline.split(' ')
            cmd = cmd_ary.pop(0)
            cmd_options = ' '.join(cmd_ary)
            eval('self.do_%s(cmd_options)' % cmd)
        except IOError:
            print('Error: Cannot open history file "%s"' % self.hist_file)

    def help_redo(self):
        """Print help message of redo command."""
        print(help_msg.cmds['redo'])

    def do_history(self, arg):
        """Show command history."""

        try:
            f = open(self.hist_file)

            # setup output format
            nof_lines = len(f.readlines())
            f.seek(0)
            lines_digit = len(str(nof_lines))
            hist_format = '  %' + str(lines_digit) + 'd  %s'

            cnt = 1
            for line in f:
                line_s = line.strip()
                print(hist_format % (cnt, line_s))
                cnt += 1
            f.close()
        except IOError:
            print('Error: Cannot open history file "%s"' % self.hist_file)

    def help_history(self):
        """Print help message of history command."""
        print(help_msg.cmds['history'])

    def complete_cat(self, text, line, begidx, endidx):
        return common.compl_common(text, line)

    def do_less(self, arg):
        """View contents of a file."""

        if os.path.isfile(arg):
            c = 'less %s' % arg
            subprocess.call(c, shell=True)
        else:
            print("No such a directory.")

    def help_less(self):
        """Print help message of less command."""
        print(help_msg.cmds['less'])

    def complete_less(self, text, line, begidx, endidx):
        return common.compl_common(text, line)

    def do_exit(self, args):
        """Terminate SPP controller process."""

        self.close()
        print('Thank you for using Soft Patch Panel')
        return True

    def help_exit(self):
        """Print help message of exit command."""
        print(help_msg.cmds['exit'])

    def do_inspect(self, args):
        """Print attributes of Shell for debugging."""

        from pprint import pprint
        if args == '':
            pprint(vars(self))

    def help_inspect(self):
        """Print help message of inspect command."""
        print(help_msg.cmds['inspect'])

    def terms_topo_subgraph(self):
        """Define terms of topo_subgraph command."""

        return ['add', 'del']

    def do_topo_subgraph(self, args):
        """Edit subgarph for topo command."""

        # logger.info("Topo initialized with sec IDs %s" % sec_ids)

        # delimiter of node in dot file
        delim_node = '_'

        args_cleaned = re.sub(r"\s+", ' ', args).strip()
        # Show subgraphs if given no argments
        if (args_cleaned == ''):
            if len(self.spp_topo.subgraphs) == 0:
                print("No subgraph.")
            else:
                for label, subg in self.spp_topo.subgraphs.items():
                    print('label: {}\tsubgraph: "{}"'.format(label, subg))
        else:  # add or del
            tokens = args_cleaned.split(' ')
            # Add subgraph
            if tokens[0] == 'add':
                if len(tokens) == 3:
                    label = tokens[1]
                    subg = tokens[2]
                    if ',' in subg:
                        subg = re.sub(r'%s' % delim_node, ':', subg)
                        subg = re.sub(r",", ";", subg)

                    # TODO(yasufum) add validation for subgraph
                    self.spp_topo.subgraphs[label] = subg
                    print("Add subgraph '{}'".format(label))
                else:
                    print("Invalid syntax '{}'!".format(args_cleaned))
            # Delete subgraph
            elif ((tokens[0] == 'del') or
                    (tokens[0] == 'delete') or
                    (tokens[0] == 'remove')):
                del(self.spp_topo.subgraphs[tokens[1]])
                print("Delete subgraph '{}'".format(tokens[1]))

            else:
                print("Ivalid subcommand '{}'!".format(tokens[0]))

    def help_topo_subgraph(self):
        """Print help message of topo_subgraph command."""
        topo.SppTopo.help_subgraph()

    def complete_topo_subgraph(self, text, line, begidx, endidx):
        terms = self.terms_topo_subgraph()

        tokens = re.sub(r"\s+", ' ', line).strip().split(' ')
        if text == '':
            if len(tokens) == 1:
                return terms
            elif len(tokens) == 2 and tokens[1] == 'del':
                return self.spp_topo.subgraphs.keys()
        elif text != '':
            completions = []
            if len(tokens) == 3 and tokens[1] == 'del':
                for t in self.spp_topo.subgraphs.keys():
                    if t.startswith(tokens[2]):
                        completions.append(t)
            elif len(tokens) == 2:
                for t in terms:
                    if t.startswith(text):
                        completions.append(t)
            return completions
        else:
            pass

    def do_topo(self, args):
        """Output network topology."""
        self.spp_topo.run(args)

    def help_topo(self):
        """Print help message of topo command."""
        topo.SppTopo.help()

    def complete_topo(self, text, line, begidx, endidx):

        return self.spp_topo.complete(text, line, begidx, endidx)

    def do_load_cmd(self, args):
        """Load command plugin."""

        args = re.sub(',', ' ', args)
        args = re.sub(r'\s+', ' ', args)
        list_args = args.split(' ')

        libdir = self.plugin_dir
        mod_name = list_args[0]
        method_name = 'do_%s' % mod_name
        exec('from .%s import %s' % (libdir, mod_name))
        do_cmd = '%s.%s' % (mod_name, method_name)
        exec('Shell.%s = %s' % (method_name, do_cmd))

        print("Module '{}' loaded.".format(mod_name))

    def help_load_cmd(self):
        """Print help message of load_cmd command."""
        print(help_msg.cmds['load_cmd'])

    def complete_load_cmd(self, text, line, begidx, endidx):
        """Complete command plugins

        Search under `plugin_dir` with compl_common() method.
        This method is intended to be used for searching current
        directory, but not in this case. If text is not '',
        compl_common() does not work correctly and do filtering
        for the result by self.
        """

        curdir = os.path.dirname(__file__)
        res = common.compl_common(
            '', '%s/%s' % (curdir, self.plugin_dir), 'py')

        completions = []
        for t in res:
            if text == '':
                if t[:2] != '__':
                    completions.append(t[:-3])
            else:
                if t[:len(text)] == text:
                    completions.append(t[:-3])
        return completions
