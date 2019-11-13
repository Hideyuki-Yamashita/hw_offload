..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2019 Nippon Telegraph and Telephone Corporation


.. _usecase_multi_node:

Multiple Nodes
==============

SPP provides multi-node support for configuring network across several nodes
from SPP CLI. You can configure each of nodes step by step.

In :numref:`figure_spp_multi_nodes_vhost`, there are four nodes on which
SPP and service VMs are running. Host1 behaves as a patch panel for connecting
between other nodes. A request is sent from a VM on host2 to a VM on host3 or
host4. Host4 is a backup server for host3 and replaced with host3 by changing
network configuration. Blue lines are paths for host3 and red lines are for
host4, and changed alternatively.

.. _figure_spp_multi_nodes_vhost:

.. figure:: ../images/setup/use_cases/spp_multi_nodes_vhost.*
   :width: 100%

   Multiple nodes example

Launch SPP on Multiple Nodes
----------------------------

Before SPP CLI, launch spp-ctl on each of nodes. You should give IP address
with ``-b`` option to be accessed from outside of the node.
This is an example for launching spp-ctl on host1.

.. code-block:: console

    # Launch on host1
    $ python3 src/spp-ctl/spp-ctl -b 192.168.11.101

You also need to launch it on host2, host3 and host4 in each of terminals.

After all of spp-ctls are lauched, launch SPP CLI with four ``-b`` options
for each of hosts. SPP CLI is able to be launched on any of nodes.

.. code-block:: console

    # Launch SPP CLI
    $ python3 src/spp.py -b 192.168.11.101 \
        -b 192.168.11.102 \
        -b 192.168.11.103 \
        -b 192.168.11.104 \

Or you can add nodes after launching SPP CLI. Here is an example of
launching it with first node, and adding the rest of nodes after.

.. code-block:: console

    # Launch SPP CLI
    $ python3 src/spp.py -b 192.168.11.101
    Welcome to the spp.  Type help or ? to list commands.

    spp > server add 192.168.11.102
    Registered spp-ctl "192.168.11.102:7777".
    spp > server add 192.168.11.103
    Registered spp-ctl "192.168.11.103:7777".
    spp > server add 192.168.11.104
    Registered spp-ctl "192.168.11.104:7777".

If you have succeeded to launch all of ``spp-ctl`` processes before,
you can find them by running ``sever list`` command.

.. code-block:: console

    # Launch SPP CLI
    spp > server list
      1: 192.168.1.101:7777 *
      2: 192.168.1.102:7777
      3: 192.168.1.103:7777
      4: 192.168.1.104:7777

You might notice that first entry is marked with ``*``. It means that
the current node under the management is the first node.

Switch Nodes
------------

SPP CLI manages a node marked with ``*``. If you configure other nodes,
change the managed node with ``server`` command.
Here is an example to switch to third node.

.. code-block:: console

    # Launch SPP CLI
    spp > server 3
    Switch spp-ctl to "3: 192.168.1.103:7777".

And the result after changed to host3.

.. code-block:: console

    spp > server list
      1: 192.168.1.101:7777
      2: 192.168.1.102:7777
      3: 192.168.1.103:7777 *
      4: 192.168.1.104:7777

You can also confirm this change by checking IP address of spp-ctl from
``status`` command.

.. code-block:: console

    spp > status
    - spp-ctl:
      - address: 192.168.1.103:7777
    - primary:
      - status: not running
    ...

Configure Patch Panel Node
--------------------------

First of all of the network configuration, setup blue lines on host1
described in :numref:`figure_spp_multi_nodes_vhost`.
You should confirm the managed server is host1.

.. code-block:: console

    spp > server list
      1: 192.168.1.101:7777 *
      2: 192.168.1.102:7777
      ...

Patch two sets of physical ports and start forwarding.

.. code-block:: console

    spp > nfv 1; patch phy:1 phy:2
    Patch ports (phy:1 -> phy:2).
    spp > nfv 1; patch phy:3 phy:0
    Patch ports (phy:3 -> phy:0).
    spp > nfv 1; forward
    Start forwarding.

Configure Service VM Nodes
--------------------------

It is almost similar as
:ref:`Setup Network Configuration in spp_nfv<usecase_spp_nfv_l2fwd_vhost_nw>`
to setup for host2, host3, and host4.

For host2, swith server to host2 and run nfv commands.

.. code-block:: console

    # switch to server 2
    spp > server 2
    Switch spp-ctl to "2: 192.168.1.102:7777".

    # configure
    spp > nfv 1; patch phy:0 vhost:0
    Patch ports (phy:0 -> vhost:0).
    spp > nfv 1; patch vhost:0 phy:1
    Patch ports (vhost:0 -> phy:1).
    spp > nfv 1; forward
    Start forwarding.

Then, swith to host3 and host4 for doing the same configuration.

Change Path to Backup Node
--------------------------

Finally, change path from blue lines to red lines.

.. code-block:: console

    # switch to server 1
    spp > server 2
    Switch spp-ctl to "2: 192.168.1.102:7777".

    # remove blue path
    spp > nfv 1; stop
    Stop forwarding.
    spp > nfv 1; patch reset

    # configure red path
    spp > nfv 2; patch phy:1 phy:4
    Patch ports (phy:1 -> phy:4).
    spp > nfv 2; patch phy:5 phy:0
    Patch ports (phy:5 -> phy:0).
    spp > nfv 2; forward
    Start forwarding.
