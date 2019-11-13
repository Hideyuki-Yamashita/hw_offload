..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2019 Nippon Telegraph and Telephone Corporation

.. _spp_tools_helpers_tools:

CPU Layout
==========

This tool is a customized script of DPDK's user tool ``cpu_layout.py``. It is
used from ``spp-ctl`` to get CPU layout. The behaviour of this script is same
as original one if you just run on terminal.

.. code-block:: console

    $ python3 tools/helpers/cpu_layout.py
    ======================================================================
    Core and Socket Information (as reported by '/sys/devices/system/cpu')
    ======================================================================

    cores =  [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15]
    sockets =  [0]

            Socket 0
            --------
    Core 0  [0]
    Core 1  [1]
    ...

Customized version of ``cpu_layout.py`` accepts an additional option
``--json`` to output the result in JSON format.

.. code-block:: console

    # Output in JSON format
    $ python3 tools/helpers/cpu_layout.py --json | jq
    [
      {
        "socket_id": 0,
        "cores": [
          {
            "core_id": 1,
            "cpus": [
              1
            ]
          },
          {
            "core_id": 0,
            "cpus": [
              0
            ]
          },
          ...
      }
    ]

You can almost the same result from ``spp-ctl``, but the order of params are
just different.

.. code-block:: console

    # Retrieve CPU layout via REST API
    $ curl -X GET http://192.168.1.100:7777/v1/cpus | jq
      % Total    % Received % Xferd  Average Speed   Time    Time     Time  Current
                                     Dload  Upload   Total   Spent    Left  Speed
    100   505  100   505    0     0  18091      0 --:--:-- --:--:-- --:--:-- 18703
    [
      {
        "cores": [
          {
            "cpus": [
              1
            ],
            "core_id": 1
          },
          {
            "cpus": [
              0
            ],
            "core_id": 0
          },
          ...
        ],
        "socket_id": 0
      }
    ]


Secondary Process Launcher
==========================

It is very simple python script used to lauch a secondary process from other
program. It is intended to be used from spp_primary for launching. Here is
whole lines of the script.

.. code-block:: python

    #!/usr/bin/env python
    # coding: utf-8
    """SPP secondary launcher."""

    import sys
    import subprocess

    if len(sys.argv) > 1:
        cmd = sys.argv[1:]
        subprocess.call(cmd)

As you may notice, it just runs given name or path of command with options,
so you can any of command other than SPP secondary processes. However, it
might be nouse for almost of users.

The reason of why this script is required is to launch secondary process from
``spp_primary`` indirectly to avoid launched secondaries to be zombies finally.
In addtion, secondary processes other than ``spp_nfv`` do not work correctly
after launched with execv() or other siblings directly from ``spp_primary``.
