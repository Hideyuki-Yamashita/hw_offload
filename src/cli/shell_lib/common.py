#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017-2018 Nippon Telegraph and Telephone Corporation

import os
import re
from .. import spp_common

CHAR_EXCEPT = '-'


def decorate_dir(curdir, filelist):
    """Add '/' the end of dirname for path completion

    'filelist' is a list of files contained in a directory.
    """

    res = []
    for f in filelist:
        if os.path.isdir('%s/%s' % (curdir, f)):
            res.append('%s/' % f)
        else:
            res.append(f)
    return res


def compl_common(text, line, ftype=None):
    """File path completion for 'complete_*' method.

    This is a helper method for `complete_*` methods implemented in a
    sub class of Cmd. `text` and `line` are arguments of `complete_*`.

    `complete_*` is a member method of Cmd class and called if TAB key
    is pressed in a command defiend by `do_*`. `text` and `line` are
    contents of command line. For example, if you type TAB after
    'command arg1 ar', the last token 'ar' is assigned to 'text' and
    whole line 'command arg1 ar' is assigned to 'line'.

    `ftype` is used to filter the result of completion. If you give 'py'
    or 'python', the result only includes python scripts.
      * 'py' or 'python': python scripts.
      * 'log': log files of extension is 'log'.
      * 'directory': get only directories, excludes files.
      * 'file' get only files, excludes directories.

    NOTE:
    Cmd treats '/' and '-' as special characters. If you type TAB after
    them, empty text '' is assigned to `text`. For example after
    'aaa b/', `text` is not 'b/' but ''. It means that completion might
    not work correctly if given path includes these characters while
    getting candidates with `os.getcwd()` or `os.listdir()`.

    This method is implemented to complete the path correctly even if
    special characters are included.
    """

    # spp_common.logger.debug('completion, text = "%s"' % text)

    tokens = line.split(' ')
    target = tokens[-1]  # get argument given by user

    if text == '':  # tab is typed after command name or '/'

        # spp_common.logger.debug("tokens: %s" % tokens)
        # spp_common.logger.debug("target: '%s'" % target)

        # check chars Cmd treats as delimiter
        if target.endswith(CHAR_EXCEPT):
            target_dir = '/'.join(target.split('/')[0:-1])
            target = target.split('/')[-1]
        else:
            target_dir = target

        # spp_common.logger.debug("text is '', tokens: '%s'" % tokens)
        # spp_common.logger.debug("text is '', target_dir: '%s'" % target_dir)

        if target_dir == '':  # no dirname means current dir
            decorated_list = decorate_dir(
                '.', os.listdir(os.getcwd()))
        else:  # after '/'
            decorated_list = decorate_dir(
                target_dir, os.listdir(target_dir))

        # spp_common.logger.debug('decorated_list: %s' % decorated_list)

        res = []
        if target.endswith(CHAR_EXCEPT):
            for d in decorated_list:
                if d.startswith(target):

                    nof_delims_t = target.count(CHAR_EXCEPT)
                    nof_delims_d = d.count(CHAR_EXCEPT)
                    idx = nof_delims_d - nof_delims_t + 1
                    d = d.split(CHAR_EXCEPT)[(-idx):]
                    d = CHAR_EXCEPT.join(d)

                    res.append(d)
        else:
            res = decorated_list

    else:  # tab is typed in the middle of a word

        # spp_common.logger.debug("text is not '', tokens: '%s'" % tokens)
        # spp_common.logger.debug("text is not '', target: '%s'" % target)

        if '/' in target:  # word is a path such as 'path/to/file'
            seg = target.split('/')[-1]  # word to be completed
            target_dir = '/'.join(target.split('/')[0:-1])

            # spp_common.logger.debug('word be completed: "%s"' % seg)
            # spp_common.logger.debug('target_dir: "%s"' % target_dir)

        else:
            seg = text
            target_dir = os.getcwd()

        matched = []
        for t in os.listdir(target_dir):
            if t.find(seg) == 0:  # get words matched with 'seg'
                matched.append(t)
        decorated_list = decorate_dir(target_dir, matched)

        # spp_common.logger.debug('decorated_list: %s' % decorated_list)

        res = []
        target_last = target.split('/')[-1]
        if CHAR_EXCEPT in target_last:
            for d in decorated_list:
                if d.startswith(seg):

                    # spp_common.logger.debug('pd: %s' % d)

                    nof_delims_t = target_last.count(CHAR_EXCEPT)
                    nof_delims_d = d.count(CHAR_EXCEPT)
                    idx = nof_delims_d - nof_delims_t + 1
                    d = d.split(CHAR_EXCEPT)[(-idx):]
                    d = CHAR_EXCEPT.join(d)

                    # spp_common.logger.debug('ad: %s' % d)

                res.append(d)
        else:
            res = decorated_list

    # spp_common.logger.debug('res: %s' % res)

    if ftype is not None:  # filtering by ftype
        completions = []
        if ftype == 'directory':
            for fn in res:
                if fn[-1] == '/':
                    completions.append(fn)
        elif ftype == 'py' or ftype == 'python':
            for fn in res:
                if fn[-3:] == '.py':
                    completions.append(fn)
        elif ftype == 'log':
            for fn in res:
                if fn[-3:] == '.log':
                    completions.append(fn)
        elif ftype == 'file':
            for fn in res:
                if fn[-1] != '/':
                    completions.append(fn)
        else:
            completions = res
    else:
        completions = res

    return completions


def is_comment_line(line):
    """Find commend line to not to interpret as a command

    Return True if given line is a comment, or False.
    Supported comment styles are
      * python ('#')
      * C ('//')
    """

    input_line = line.strip()
    if len(input_line) > 0:
        if (input_line[0] == '#') or (input_line[0:2] == '//'):
            return True
        else:
            return False


def is_valid_ipv4_addr(ipaddr):
    ip_nums = ipaddr.split('.')

    if len(ip_nums) != 4:
        return False

    for num in ip_nums:
        num = int(num)
        if (num < 0) or (num > 255):
            return False

    return True


def is_valid_port(port_num):
    num = int(port_num)
    if (num < 1023) or (num > 65535):
        return False

    return True


def current_server_addr():
    return spp_common.cur_server_addr


def set_current_server_addr(ipaddr, port):
    spp_common.cur_server_addr = '{}:{}'.format(ipaddr, port)


def validate_config_val(key, val):
    """Check if given value is valid for config."""

    # Check int value
    should_be_int_vals = [
            'max_secondary', 'sec_m_lcore', 'sec_nfv_nof_lcores',
            'sec_vf_nof_lcores', 'sec_mirror_nof_lcores',
            'sec_pcap_nof_lcores']
    if key in should_be_int_vals:
        if re.match(r'\d+$', val) is not None:
            return True
        else:
            return False

    # topo_size should be percentage or ratio.
    if key == 'topo_size':
        matched = re.match(r'(\d+)%$', val)
        if matched is not None:
            percentage = int(matched.group(1))
            if percentage > 0 and percentage <= 100:
                return True
            else:
                return False

        matched = re.match(r'([0-1]\.\d+)$', val)
        if (matched is not None) or (val == '1'):
            if val == '1':
                ratio = 1.0
            else:
                ratio = float(matched.group(1))
            if ratio > 0 and ratio <= 1:
                return True
            else:
                return False
        else:
            return False

    # Check pcap port.
    # TODO(yasufum) confirm cap_ports is correct
    cap_ports = ['phy', 'ring']
    if key == 'sec_pcap_port':
        matched = re.match(r'(\w+):(\d+)$', val)
        if matched is not None:
            if matched.group(1) in cap_ports:
                return True
            return False
        else:
            return False

    # Check memory option
    if key == 'sec_mem':
        # Match '-m 512' or so.
        matched = re.match(r'-m\s+\d+$', val)
        if matched is not None:
            return True

        # Match '--socket-mem 512', '--socket-mem 512,512' or so.
        matched = re.match(r'--socket-mem\s+([1-9,]+\d+)$', val)
        if matched is not None:
            return True
        else:
            return False

    # No need to check others.
    return True


def print_compl_warinig(msg):
    """Print warning message for completion.

    Printing message while complition disturbs user's input, but it should be
    printed in some error cases. This method is just for printing warning
    message simply.
    """

    print('// WARN: {msg}'.format(msg=msg))
