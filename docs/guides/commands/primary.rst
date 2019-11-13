..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2010-2014 Intel Corporation
    Copyright(c) 2017-2019 Nippon Telegraph and Telephone Corporation


.. _commands_primary:

Primary Commands
================

Primary process is managed with ``pri`` command.

``pri`` command takes a sub command. They must be separated with delimiter
``;``. Some of sub commands take additional arguments.

.. code-block:: console

    spp > pri; SUB_CMD

All of Sub commands are referred with ``help`` command.

.. code-block:: console

    spp > help pri
    Send a command to primary process.

        Show resources and statistics, or clear it.

            spp > pri; status  # show status

            spp > pri; clear   # clear statistics

        Launch secondary process..

            # Launch nfv:1
            spp > pri; launch nfv 1 -l 1,2 -m 512 -- -n 1 -s 192.168....

            # Launch vf:2
            spp > pri; launch vf 2 -l 1,4-7 -m 512 -- --client-id 2 -s ...


.. _commands_primary_status:

status
------

Show status fo ``spp_primary`` and forwarding statistics of each of ports.

.. code-block:: console

    spp > pri; status
    - lcore_ids:
      - master: 0
    - stats
      - physical ports:
          ID          rx          tx    tx_drop  mac_addr
           0           0           0           0  56:48:4f:53:54:00
           1           0           0           0  56:48:4f:53:54:01
      - ring ports:
          ID          rx          tx     rx_drop     tx_drop
           0           0           0           0           0
           1           0           0           0           0
           2           0           0           0           0
           ...

If you run ``spp_primary`` with forwarder thread, status of the forwarder is
also displayed.

.. code-block:: console

    spp > pri; status
    - lcore_ids:
      - master: 0
      - slave: 1
    - forwarder:
      - status: idling
      - ports:
        - phy:0
        - phy:1
    - stats
      - physical ports:
          ID          rx          tx    tx_drop  mac_addr
           0           0           0           0  56:48:4f:53:54:00
           1           0           0           0  56:48:4f:53:54:01
      - ring ports:
          ID          rx          tx     rx_drop     tx_drop
           0           0           0           0           0
           1           0           0           0           0
           ...


.. _commands_primary_clear:

clear
-----

Clear statistics.

.. code-block:: console

    spp > pri; clear
    Clear port statistics.


.. _commands_primary_add:

add
---

Add a port with resource ID.

For example, adding ``ring:0`` by

.. code-block:: console

    spp > pri; add ring:0
    Add ring:0.

Or adding ``vhost:0`` by

.. code-block:: console

    spp > pri; add vhost:0
    Add vhost:0.


.. _commands_primary_patch:

patch
------

Create a path between two ports, source and destination ports.
This command just creates a path and does not start forwarding.

.. code-block:: console

    spp > pri; patch phy:0 ring:0
    Patch ports (phy:0 -> ring:0).


.. _commands_primary_forward:

forward
-------

Start forwarding.

.. code-block:: console

    spp > pri; forward
    Start forwarding.

Running status is changed from ``idling`` to ``running`` by
executing it.

.. code-block:: console

    spp > pri; status
    - lcore_ids:
      - master: 0
      - slave: 1
    - forwarder:
      - status: running
      - ports:
        - phy:0
        - phy:1
    ...


.. _commands_primary_stop:

stop
----

Stop forwarding.

.. code-block:: console

    spp > pri; stop
    Stop forwarding.

Running status is changed from ``running`` to ``idling`` by
executing it.

.. code-block:: console

    spp > pri; status
    - lcore_ids:
      - master: 0
      - slave: 1
    - forwarder:
      - status: idling
      - ports:
        - phy:0
        - phy:1
    ...


.. _commands_primary_del:

del
---

Delete a port of given resource UID.

.. code-block:: console

    spp > pri; del ring:0
    Delete ring:0.


.. _commands_primary_launch:

launch
------

Launch a secondary process.

Spp_primary is able to launch a secondary process with given type, secondary
ID and options of EAL and application itself. This is a list of supported type
of secondary processes.

  * nfv
  * vf
  * mirror
  * pcap

.. code-block:: console

    # spp_nfv with sec ID 1
    spp > pri; launch nfv 1 -l 1,2 -m 512 -- -n -s 192.168.1.100:6666

    # spp_vf with sec ID 2
    spp > pri; launch vf 2 -l 1,3-5 -m 512 -- --client-id -s 192.168.1.100:6666

You notice that ``--proc-type secondary`` is not given for launching secondary
processes. ``launch`` command adds this option before requesting to launch
the process so that you do not need to input this option by yourself.

``launch`` command supports TAB completion for type, secondary ID and the rest
of options. Some of EAL and application options are just a template, so you
should edit them before launching. Some of default params of options,
for instance, the number of lcores or the amount of memory, are changed from
``config`` command of :ref:`Common Commands<commands_common_config>`.

In terms of log, each of secondary processes are output its log messages to
files under ``log`` directory of project root. The name of log file is defined
with type of process and secondary ID. For instance, ``nfv 2``, the path of log
file is ``log/spp_nfv-2.log``.
