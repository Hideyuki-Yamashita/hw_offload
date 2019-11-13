..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2017-2019 Nippon Telegraph and Telephone Corporation


.. _spp_usecases_vf:

spp_vf
======

``spp_vf`` is a secondary process for providing L2 classification as a simple
pusedo SR-IOV features.

.. _spp_usecases_vf_cls_icmp:

Classify ICMP Packets
---------------------

To confirm classifying packets, sends ICMP packet from remote node by using
ping and watch the response.
Incoming packets through ``NIC0`` are classified based on destination address.

.. _figure_spp_vf_use_cases_nw_config:

.. figure:: ../images/usecases/vf_cls_icmp_nwconf.*
    :width: 80%

    Network Configuration


Setup
~~~~~

Launch ``spp-ctl`` and SPP CLI before primary and secondary processes.

.. code-block:: console

    # terminal 1
    $ python3 ./src/spp-ctl/spp-ctl -b 192.168.1.100

.. code-block:: console

    # terminal 2
    $ python3 ./src/spp.py -b 192.168.1.100

``spp_primary`` on the second lcore with ``-l 0`` and two ports ``-p 0x03``.

.. code-block:: console

    # terminal 3
    $ sudo ./src/primary/x86_64-native-linuxapp-gcc/spp_primary \
        -l 1 -n 4 \
        --socket-mem 512,512 \
        --huge-dir=/run/hugepages/kvm \
        --proc-type=primary \
        -- \
        -p 0x03 \
        -n 10 -s 192.168.1.100:5555

After ``spp_primary`` is launched, run secondary process ``spp_vf``.
In this case, lcore options is ``-l 2-6`` for one master thread and four
worker threads.

.. code-block:: console

     # terminal 4
     $ sudo ./src/vf/x86_64-native-linuxapp-gcc/spp_vf \
        -l 2-6 \
        -n 4 --proc-type=secondary \
        -- \
        --client-id 1 \
        -s 192.168.1.100:6666 \


Network Configuration
~~~~~~~~~~~~~~~~~~~~~

Configure network as described in :numref:`figure_spp_vf_use_cases_nw_config`
step by step.

First of all, setup worker threads from ``component`` command with lcore ID
and other options on local host ``host2``.

.. code-block:: none

    # terminal 2
    spp > vf 1; component start cls 3 classifier
    spp > vf 1; component start fwd1 4 forward
    spp > vf 1; component start fwd2 5 forward
    spp > vf 1; component start mgr 6 merge

Add ports for each of components as following.
The number of rx and tx ports are different for each of component's role.

.. code-block:: none

    # terminal 2

    # classifier
    spp > vf 1; port add phy:0 rx cls
    spp > vf 1; port add ring:0 tx cls
    spp > vf 1; port add ring:1 tx cls

    # forwarders
    spp > vf 1; port add ring:0 rx fwd1
    spp > vf 1; port add ring:2 tx fwd1
    spp > vf 1; port add ring:1 rx fwd2
    spp > vf 1; port add ring:3 tx fwd2

    # merger
    spp > vf 1; port add ring:2 rx mgr
    spp > vf 1; port add ring:3 rx mgr
    spp > vf 1; port add phy:1 tx mgr

You also need to configure MAC address table for classifier. In this case,
you need to register two MAC addresses. Although any MAC can be used,
you use ``52:54:00:12:34:56`` and ``52:54:00:12:34:58``.

.. code-block:: none

    # terminal 2
    spp > vf 1; classifier_table add mac 52:54:00:12:34:56 ring:0
    spp > vf 1; classifier_table add mac 52:54:00:12:34:58 ring:1


Send Packet from Remote Host
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Ensure NICs, ``ens0`` and ``ens1`` in this case, are upped on remote host
``host1``. You can up by using ifconfig if the status is down.

.. code-block:: console

    # terminal 1 on remote host
    # Configure ip address of ens0
    $ sudo ifconfig ens0 192.168.140.1 netmask 255.255.255.0 up

Add arp entries of MAC addresses statically to be resolved.

.. code-block:: console

    # terminal 1 on remote host
    # set MAC address
    $ sudo arp -i ens0 -s 192.168.140.2 52:54:00:12:34:56
    $ sudo arp -i ens0 -s 192.168.140.3 52:54:00:12:34:58

Start tcpdump command for capturing ``ens1``.

.. code-block:: console

    # terminal 2 on remote host
    $ sudo tcpdump -i ens1

Then, start ping in other terminals.

.. code-block:: console

    # terminal 3 on remote host
    # ping via NIC0
    $ ping 192.168.140.2

.. code-block:: console

    # terminal 4 on remote host
    # ping via NIC0
    $ ping 192.168.140.3

You can see ICMP Echo requests are received from ping on terminal 2.


.. _spp_vf_use_cases_shutdown_comps:

Shutdown spp_vf Components
~~~~~~~~~~~~~~~~~~~~~~~~~~

Basically, you can shutdown all of SPP processes with ``bye all``
command.
This section describes graceful shutting down.
First, delete entries of ``classifier_table`` and ports of components.

.. code-block:: none

    # terminal 2
    # Delete MAC address from Classifier
    spp > vf 1; classifier_table del mac 52:54:00:12:34:56 ring:0
    spp > vf 1; classifier_table del mac 52:54:00:12:34:58 ring:1

.. code-block:: none

    # terminal 2
    # classifier
    spp > vf 1; port del phy:0 rx cls
    spp > vf 1; port del ring:0 tx cls
    spp > vf 1; port del ring:1 tx cls

    # forwarders
    spp > vf 1; port del ring:0 rx fwd1
    spp > vf 1; port del vhost:0 tx fwd1
    spp > vf 1; port del ring:1 rx fwd2
    spp > vf 1; port del vhost:2 tx fwd2

    # mergers
    spp > vf 1; port del ring:2 rx mgr
    spp > vf 1; port del ring:3 rx mgr
    spp > vf 1; port del phy:0 tx mgr

Then, stop components.

.. code-block:: none

    # terminal 2
    spp > vf 1; component stop cls
    spp > vf 1; component stop fwd1
    spp > vf 1; component stop fwd2
    spp > vf 1; component stop mgr

You can confirm that worker threads are cleaned from ``status``.

.. code-block:: none

    spp > vf 1; status
    Basic Information:
      - client-id: 1
      - ports: [phy:0, phy:1]
      - lcore_ids:
        - master: 2
        - slaves: [3, 4, 5, 6]
    Classifier Table:
      No entries.
    Components:
      - core:3 '' (type: unuse)
      - core:4 '' (type: unuse)
      - core:5 '' (type: unuse)
      - core:6 '' (type: unuse)

Finally, terminate ``spp_vf`` by using ``exit`` or ``bye sec``.

.. code-block:: console

    spp > vf 1; exit


.. _spp_usecases_vf_ssh:

SSH Login to VMs
----------------

This usecase is to classify packets for ssh connections as another example.
Incoming packets are classified based on destination addresses and reterned
packets are aggregated before going out.

.. _figure_spp_usecase_vf_ssh_overview:

.. figure:: ../images/usecases/vf_ssh_overview.*
    :width: 58%

    Simple SSH Login


Setup
~~~~~

Launch ``spp-ctl`` and SPP CLI before primary and secondary processes.

.. code-block:: console

    # terminal 1
    $ python3 ./src/spp-ctl/spp-ctl -b 192.168.1.100

.. code-block:: console

    # terminal 2
    $ python3 ./src/spp.py -b 192.168.1.100

``spp_primary`` on the second lcore with ``-l 1`` and two ports ``-p 0x03``.

.. code-block:: console

    # terminal 3
    $ sudo ./src/primary/x86_64-native-linuxapp-gcc/spp_primary \
        -l 1 -n 4 \
        --socket-mem 512,512 \
        --huge-dir=/run/hugepages/kvm \
        --proc-type=primary \
        -- \
        -p 0x03 -n 10 -s 192.168.1.100:5555

Then, run secondary process ``spp_vf`` with ``-l 0,2-13`` which indicates
to use twelve lcores.

.. code-block:: console

    # terminal 4
    $ sudo ./src/vf/x86_64-native-linuxapp-gcc/spp_vf \
        -l 0,2-13 \
        -n 4 --proc-type=secondary \
        -- \
        --client-id 1 \
        -s 192.168.1.100:6666 --vhost-client


Network Configuration
~~~~~~~~~~~~~~~~~~~~~

Detailed netowrk configuration of :numref:`figure_spp_usecase_vf_ssh_overview`
is described below.
In this usecase, use two NICs on each of host1 and host2 for redundancy.

Incoming packets through NIC0 or NIC1 are classified based on destionation
address.

.. _figure_network_config:

.. figure:: ../images/usecases/vf_ssh_nwconfig.*
    :width: 100%

    Network Configuration SSH with spp_vhost

You need to input a little bit large amount of commands for the
configuration, or use ``playback`` command to load from config files.
You can load network configuration  from recipes in ``recipes/usecases/``
as following.

.. code-block:: none

    # terminal 2
    # Load config from recipe
    spp > playback recipes/usecases/spp_vf/ssh/1-start_components.rcp
    spp > playback recipes/usecases/spp_vf/ssh/2-add_port_path1.rcp
    ....

First of all, start components with names such as ``cls1``, ``fwd1`` or so.

.. code-block:: none

    # terminal 2
    spp > vf 1; component start cls1 2 classifier
    spp > vf 1; component start fwd1 3 forward
    spp > vf 1; component start fwd2 4 forward
    spp > vf 1; component start fwd3 5 forward
    spp > vf 1; component start fwd4 6 forward
    spp > vf 1; component start mgr1 7 merge

Each of components must have rx and tx ports for forwarding.
Add ports for each of components as following.
You notice that classifier has two tx ports and merger has two rx ports.

.. code-block:: console

    # terminal 2
    # classifier
    spp > vf 1; port add phy:0 rx cls1
    spp > vf 1; port add ring:0 tx cls1
    spp > vf 1; port add ring:1 tx cls1

    # forwarders
    spp > vf 1; port add ring:0 rx fwd1
    spp > vf 1; port add vhost:0 tx fwd1
    spp > vf 1; port add ring:1 rx fwd2
    spp > vf 1; port add vhost:2 tx fwd2
    spp > vf 1; port add vhost:0 rx fwd3
    spp > vf 1; port add ring:2 tx fwd3
    spp > vf 1; port add vhost:2 rx fwd4
    spp > vf 1; port add ring:3 tx fwd4

    # merger
    spp > vf 1; port add ring:2 rx mgr1
    spp > vf 1; port add ring:3 rx mgr1
    spp > vf 1; port add phy:0 tx mgr1

Classifier component decides the destination with MAC address by referring
``classifier_table``. MAC address and corresponging port is registered to the
table. In this usecase, you need to register two MAC addresses of targetting
VM for mgr1, and also mgr2 later.

.. code-block:: none

    # terminal 2
    # Register MAC addresses for mgr1
    spp > vf 1; classifier_table add mac 52:54:00:12:34:56 ring:0
    spp > vf 1; classifier_table add mac 52:54:00:12:34:58 ring:1

Configuration for the second login path is almost the same as the first path.

.. code-block:: none

    # terminal 2
    spp > vf 1; component start cls2 8 classifier
    spp > vf 1; component start fwd5 9 forward
    spp > vf 1; component start fwd6 10 forward
    spp > vf 1; component start fwd7 11 forward
    spp > vf 1; component start fwd8 12 forward
    spp > vf 1; component start mgr2 13 merge

Add ports to each of components.

.. code-block:: none

    # terminal 2
    # classifier
    spp > vf 1; port add phy:1 rx cls2
    spp > vf 1; port add ring:4 tx cls2
    spp > vf 1; port add ring:5 tx cls2

    # forwarders
    spp > vf 1; port add ring:4 rx fwd5
    spp > vf 1; port add vhost:1 tx fwd5
    spp > vf 1; port add ring:5 rx fwd6
    spp > vf 1; port add vhost:3 tx fwd6
    spp > vf 1; port add vhost:1 rx fwd7
    spp > vf 1; port add ring:6 tx fwd7
    spp > vf 1; port add vhost:3 rx fwd8
    spp > vf 1; port add ring:7 tx fwd8

    # merger
    spp > vf 1; port add ring:6 rx mgr2
    spp > vf 1; port add ring:7 rx mgr2
    spp > vf 1; port add phy:1 tx mgr2

Register MAC address entries to ``classifier_table`` for ``cls2``.

.. code-block:: console

    # terminal 2
    # Register MAC address to classifier
    spp > vf 1; classifier_table add mac 52:54:00:12:34:57 ring:4
    spp > vf 1; classifier_table add mac 52:54:00:12:34:59 ring:5


.. _spp_usecases_vf_ssh_setup_vms:

Setup VMs
~~~~~~~~~

Launch two VMs with virsh command.
Setup for virsh is described in :ref:`spp_gsg_howto_virsh`.
In this case, VMs are named as ``spp-vm1`` and ``spp-vm2``.

.. code-block:: console

    # terminal 5
    $ virsh start spp-vm1  # VM1
    $ virsh start spp-vm2  # VM2

After VMs are launched, login to ``spp-vm1`` first to configure.

.. note::

    To avoid asked for unknown keys while login VMs, use
    ``-o StrictHostKeyChecking=no`` option for ssh.

    .. code-block:: console

        $ ssh -o StrictHostKeyChecking=no sppuser at 192.168.122.31

Up interfaces and disable TCP offload to avoid ssh login is failed.

.. code-block:: console

    # terminal 5
    # up interfaces
    $ sudo ifconfig ens4 inet 192.168.140.21 netmask 255.255.255.0 up
    $ sudo ifconfig ens5 inet 192.168.150.22 netmask 255.255.255.0 up

    # disable TCP offload
    $ sudo ethtool -K ens4 tx off
    $ sudo ethtool -K ens5 tx off

Configuration of ``spp-vm2`` is almost similar to ``spp-vm1``.

.. code-block:: console

    # terminal 5
    # up interfaces
    $ sudo ifconfig ens4 inet 192.168.140.31 netmask 255.255.255.0 up
    $ sudo ifconfig ens5 inet 192.168.150.32 netmask 255.255.255.0 up

    # disable TCP offload
    $ sudo ethtool -K ens4 tx off
    $ sudo ethtool -K ens5 tx off


Login to VMs
~~~~~~~~~~~~

Now, you can login to VMs from the remote host1.

.. code-block:: console

    # terminal 5
    # spp-vm1 via NIC0
    $ ssh sppuser@192.168.140.21

    # spp-vm1 via NIC1
    $ ssh sppuser@192.168.150.22

    # spp-vm2 via NIC0
    $ ssh sppuser@192.168.140.31

    # spp-vm2 via NIC1
    $ ssh sppuser@192.168.150.32


.. _spp_usecases_vf_ssh_shutdown:

Shutdown spp_vf Components
~~~~~~~~~~~~~~~~~~~~~~~~~~

Basically, you can shutdown all of SPP processes with ``bye all``
command.
This section describes graceful shutting down.

First, delete entries of ``classifier_table`` and ports of components
for the first SSH login path.

.. code-block:: none

    # terminal 2
    # Delete MAC address from table
    spp > vf 1; classifier_table del mac 52:54:00:12:34:56 ring:0
    spp > vf 1; classifier_table del mac 52:54:00:12:34:58 ring:1

Delete ports.

.. code-block:: none

    # terminal 2
    # classifier
    spp > vf 1; port del phy:0 rx cls1
    spp > vf 1; port del ring:0 tx cls1
    spp > vf 1; port del ring:1 tx cls1

    # forwarders
    spp > vf 1; port del ring:0 rx fwd1
    spp > vf 1; port del vhost:0 tx fwd1
    spp > vf 1; port del ring:1 rx fwd2
    spp > vf 1; port del vhost:2 tx fwd2
    spp > vf 1; port del vhost:0 rx fwd3
    spp > vf 1; port del ring:2 tx fwd3
    spp > vf 1; port del vhost:2 rx fwd4
    spp > vf 1; port del ring:3 tx fwd4

    # merger
    spp > vf 1; port del ring:2 rx mgr1
    spp > vf 1; port del ring:3 rx mgr1
    spp > vf 1; port del phy:0 tx mgr1

Then, stop components.

.. code-block:: none

    # terminal 2
    # Stop component to spp_vf
    spp > vf 1; component stop cls1
    spp > vf 1; component stop fwd1
    spp > vf 1; component stop fwd2
    spp > vf 1; component stop fwd3
    spp > vf 1; component stop fwd4
    spp > vf 1; component stop mgr1

Second, do termination for the second path.
Delete entries from the table and ports from each of components.

.. code-block:: none

    # terminal 2
    # Delete MAC address from Classifier
    spp > vf 1; classifier_table del mac 52:54:00:12:34:57 ring:4
    spp > vf 1; classifier_table del mac 52:54:00:12:34:59 ring:5

.. code-block:: none

    # terminal 2
    # classifier2
    spp > vf 1; port del phy:1 rx cls2
    spp > vf 1; port del ring:4 tx cls2
    spp > vf 1; port del ring:5 tx cls2

    # forwarder
    spp > vf 1; port del ring:4 rx fwd5
    spp > vf 1; port del vhost:1 tx fwd5
    spp > vf 1; port del ring:5 rx fwd6
    spp > vf 1; port del vhost:3 tx fwd6
    spp > vf 1; port del vhost:1 rx fwd7
    spp > vf 1; port del ring:6 tx fwd7
    spp > vf 1; port del vhost:3 tx fwd8
    spp > vf 1; port del ring:7 rx fwd8

    # merger
    spp > vf 1; port del ring:6 rx mgr2
    spp > vf 1; port del ring:7 rx mgr2
    spp > vf 1; port del phy:1 tx mgr2

Then, stop components.

.. code-block:: none

    # terminal 2
    # Stop component to spp_vf
    spp > vf 1; component stop cls2
    spp > vf 1; component stop fwd5
    spp > vf 1; component stop fwd6
    spp > vf 1; component stop fwd7
    spp > vf 1; component stop fwd8
    spp > vf 1; component stop mgr2

Exit spp_vf
~~~~~~~~~~~

Terminate spp_vf.

.. code-block:: none

    # terminal 2
    spp > vf 1; exit
