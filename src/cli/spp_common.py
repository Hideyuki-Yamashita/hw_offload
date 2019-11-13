#!/usr/bin/env python3
# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2015-2016 Intel Corporation

import logging
import os

# Type definitions.
PORT_TYPES = ['phy', 'ring', 'vhost', 'pcap', 'nullpmd']
SEC_TYPES = ['nfv', 'vf', 'mirror', 'pcap']

LOGFILE = 'spp_cli.log'  # name of logfile under `/src/controller/log/`

# Current server under management of SPP CLI.
cur_server_addr = None

# Setup logger object
logger = logging.getLogger(__name__)
# handler = logging.StreamHandler()
os.system("mkdir -p %s/log" % (os.path.dirname(__file__)))

logfile = '%s/log/%s' % (os.path.dirname(__file__), LOGFILE)
handler = logging.FileHandler(logfile)
handler.setLevel(logging.DEBUG)
formatter = logging.Formatter(
    '%(asctime)s,[%(filename)s][%(name)s][%(levelname)s]%(message)s')
handler.setFormatter(formatter)
logger.setLevel(logging.DEBUG)
logger.addHandler(handler)
