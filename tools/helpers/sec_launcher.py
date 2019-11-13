#!/usr/bin/env python
# coding: utf-8
"""SPP secondary launcher."""

import sys
import subprocess

if len(sys.argv) > 1:
    cmd = sys.argv[1:]
    subprocess.call(cmd)
