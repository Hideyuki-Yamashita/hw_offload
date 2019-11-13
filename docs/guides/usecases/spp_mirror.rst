.. SPDX-License-Identifier: BSD-3-Clause
   Copyright(c) 2019 Nippon Telegraph and Telephone Corporation


.. _spp_usecases_mirror:

spp_mirror
==========

Duplicate Packets
-----------------

Simply duplicate incoming packets and send to two destinations.
Remote ``host1`` sends ARP packets by using ping command and
``spp_mirror`` running on local ``host2`` duplicates packets to
destination ports.


Network Configuration
~~~~~~~~~~~~~~~~~~~~~

Detailed configuration is described in
:numref:`figure_spp_mirror_use_cases_nw_config`.
In this diagram, incoming packets from ``phy:0`` are mirrored.
In ``spp_mirror`` process, worker thread ``mir`` copies incoming packets and
sends to two destinations ``phy:1`` and ``phy:2``.

.. _figure_spp_mirror_use_cases_nw_config:

.. figure:: ../images/usecases/mirror_dup_nwconf.*
     :width: 75%

     Duplicate packets with spp_mirror


Setup SPP
~~~~~~~~~

Change directory to spp and confirm that it is already compiled.

.. code-block:: console

    $ cd /path/to/spp

Launch ``spp-ctl`` before launching SPP primary and secondary processes.
You also need to launch ``spp.py``  if you use ``spp_mirror`` from CLI.
``-b`` option is for binding IP address to communicate other SPP processes,
but no need to give it explicitly if ``127.0.0.1`` or ``localhost`` .

.. code-block:: console

    # terminal 1
    # Launch spp-ctl
    $ python3 ./src/spp-ctl/spp-ctl -b 192.168.1.100

.. code-block:: console

    # terminal 2
    # Launch SPP CLI
    $ python3 ./src/spp.py -b 192.168.1.100

Start ``spp_primary`` with core list option ``-l 1`` and
three ports ``-p 0x07``.

.. code-block:: console

   # terminal 3
   $ sudo ./src/primary/x86_64-native-linuxapp-gcc/spp_primary \
       -l 1 -n 4 \
       --socket-mem 512,512 \
       --huge-dir=/run/hugepages/kvm \
       --proc-type=primary \
       -- \
       -p 0x07 -n 10 -s 192.168.1.100:5555


Launch spp_mirror
~~~~~~~~~~~~~~~~~

Run secondary process ``spp_mirror``.

.. code-block:: console

    # terminal 4
    $ sudo ./src/mirror/x86_64-native-linuxapp-gcc/app/spp_mirror \
     -l 0,2 -n 4 \
     --proc-type secondary \
     -- \
     --client-id 1 \
     -s 192.168.1.100:6666 \

Start mirror component with core ID 2.

.. code-block:: console

    # terminal 2
    spp > mirror 1; component start mir 2 mirror

Add ``phy:0`` as rx port, and ``phy:1`` and ``phy:2`` as tx ports.

.. code-block:: none

    # terminal 2
    # add ports to mir
    spp > mirror 1; port add phy:0 rx mir
    spp > mirror 1; port add phy:1 tx mir
    spp > mirror 1; port add phy:2 tx mir


Duplicate Packets
~~~~~~~~~~~~~~~~~

To check packets are mirrored, you run tcpdump for ``ens1`` and ``ens2``.
As you run ping for ``ens0`` next, you will see the same ARP requests trying
to resolve ``192.168.140.21`` on terminal 1 and 2.

.. code-block:: console

    # terminal 1 at host1
    # capture on ens1
    $ sudo tcpdump -i ens1
    tcpdump: verbose output suppressed, use -v or -vv for full protocol decode
    listening on ens1, link-type EN10MB (Ethernet), capture size 262144 bytes
    21:18:44.183261 ARP, Request who-has 192.168.140.21 tell R740n15, length 28
    21:18:45.202182 ARP, Request who-has 192.168.140.21 tell R740n15, length 28
    ....

.. code-block:: console

    # terminal 2 at host1
    # capture on ens2
    $ sudo tcpdump -i ens2
    tcpdump: verbose output suppressed, use -v or -vv for full protocol decode
    listening on ens2, link-type EN10MB (Ethernet), capture size 262144 bytes
    21:18:44.183261 ARP, Request who-has 192.168.140.21 tell R740n15, length 28
    21:18:45.202182 ARP, Request who-has 192.168.140.21 tell R740n15, length 28
    ...

Start to send ARP request with ping.

.. code-block:: console

   # terminal 3 at host1
   # send packet from NIC0
   $ ping 192.168.140.21 -I ens0


Stop Mirroring
~~~~~~~~~~~~~~

Delete ports for components.

.. code-block:: none

   # Delete port for mir
   spp > mirror 1; port del phy:0 rx mir
   spp > mirror 1; port del phy:1 tx mir
   spp > mirror 1; port del phy:2 tx mir

Next, stop components.

.. code-block:: console

   # Stop mirror
   spp > mirror 1; component stop mir 2 mirror

   spp > mirror 1; status
   Basic Information:
     - client-id: 1
     - ports: [phy:0, phy:1]
     - lcore_ids:
       - master: 0
       - slave: 2
   Components:
     - core:2 '' (type: unuse)

Finally, terminate ``spp_mirror`` to finish this usecase.

.. code-block:: console

    spp > mirror 1; exit


.. _spp_usecases_mirror_monitor:

Monitoring Packets
------------------

Duplicate classified packets for monitoring before going to a VM.
In this usecase, we are only interested in packets going to ``VM1``.
Although you might be able to run packet monitor app on host,
run monitor on ``VM3`` considering more NFV like senario.
You use ``spp_mirror`` for copying, and ``spp_vf`` classifying packets.

.. _figure_usecase_monitor_overview:

.. figure:: ../images/usecases/mirror_monitor_overview.*
   :width: 60%

   Monitoring with spp_mirror


Setup SPP and VMs
~~~~~~~~~~~~~~~~~

Launch ``spp-ctl`` before launching SPP primary and secondary processes.
You also need to launch ``spp.py``  if you use ``spp_vf`` from CLI.
``-b`` option is for binding IP address to communicate other SPP processes,
but no need to give it explicitly if ``127.0.0.1`` or ``localhost`` although
doing explicitly in this example to be more understandable.

.. code-block:: console

    # terminal 1
    $ python3 ./src/spp-ctl/spp-ctl -b 192.168.1.100

.. code-block:: console

    # terminal 2
    $ python3 ./src/spp.py -b 192.168.1.100

Start spp_primary with core list option ``-l 1``.

.. code-block:: console

    # terminal 3
    # Type the following in different terminal
    $ sudo ./src/primary/x86_64-native-linuxapp-gcc/spp_primary \
        -l 1 -n 4 \
        --socket-mem 512,512 \
        --huge-dir=/run/hugepages/kvm \
        --proc-type=primary \
        -- \
        -p 0x03 \
        -n 10 -s 192.168.1.100:5555


Netowrk Configuration
~~~~~~~~~~~~~~~~~~~~~

Detailed configuration of :numref:`figure_usecase_monitor_overview`
is described in :numref:`figure_usecase_monitor_nwconfig`.
In this senario, worker thread ``mir`` copies incoming packets
from though ``ring:0``.
Then, sends to orignal destination ``VM1`` and anohter one ``VM3``.

.. _figure_usecase_monitor_nwconfig:

.. figure:: ../images/usecases/mirror_monitor_nwconf.*
     :width: 80%

     Network configuration of monitoring packets

Launch ``VM1``, ``VM2`` and ``spp_vf`` with core list ``-l 0,2-8``.

.. code-block:: console

   # terminal 4
   $ sudo ./src/vf/x86_64-native-linuxapp-gcc/spp_vf \
       -l 0,2-8 \
       -n 4 --proc-type secondary \
       -- \
       --client-id 1 \
       -s 192.168.1.100:6666 \
       --vhost-client


Start components in ``spp_vf``.

.. code-block:: none

   # terminal 2
   spp > vf 1; component start cls 2 classifier
   spp > vf 1; component start mgr 3 merge
   spp > vf 1; component start fwd1 4 forward
   spp > vf 1; component start fwd2 5 forward
   spp > vf 1; component start fwd3 6 forward
   spp > vf 1; component start fwd4 7 forward
   spp > vf 1; component start fwd5 8 forward

Add ports for components.

.. code-block:: none

   # terminal 2
   spp > vf 1; port add phy:0 rx cls
   spp > vf 1; port add ring:0 tx cls
   spp > vf 1; port add ring:1 tx cls

   spp > vf 1; port add ring:2 rx mgr
   spp > vf 1; port add ring:3 rx mgr
   spp > vf 1; port add phy:0 tx mgr

   spp > vf 1; port add ring:5 rx fwd1
   spp > vf 1; port add vhost:0 tx fwd1

   spp > vf 1; port add ring:1 rx fwd2
   spp > vf 1; port add vhost:2 tx fwd2

   spp > vf 1; port add vhost:1 rx fwd3
   spp > vf 1; port add ring:2 tx fwd3

   spp > vf 1; port add vhost:3 rx fwd4
   spp > vf 1; port add ring:3 tx fwd4

   spp > vf 1; port add ring:4 rx fwd5
   spp > vf 1; port add vhost:4 tx fwd5


Add classifier table entries.

.. code-block:: none

   # terminal 2
   spp > vf 1; classifier_table add mac 52:54:00:12:34:56 ring:0
   spp > vf 1; classifier_table add mac 52:54:00:12:34:58 ring:1


Launch spp_mirror
~~~~~~~~~~~~~~~~~

Run ``spp_mirror``.

.. code-block:: console

    # terminal 6
    $ sudo ./src/mirror/x86_64-native-linuxapp-gcc/app/spp_mirror \
      -l 0,9 \
      -n 4 --proc-type secondary \
      -- \
      --client-id 2 \
      -s 192.168.1.100:6666 \
      --vhost-client

Start mirror component with lcore ID 9.

.. code-block:: console

    # terminal 2
    spp > mirror 2; component start mir 9 mirror

Add ``ring:0`` as rx port, ``ring:4`` and ``ring:5`` as tx ports.

.. code-block:: none

   # terminal 2
   spp > mirror 2; port add ring:0 rx mir
   spp > mirror 2; port add ring:4 tx mir
   spp > mirror 2; port add ring:5 tx mir


Receive Packet on VM3
~~~~~~~~~~~~~~~~~~~~~

You can capture incoming packets on ``VM3`` and compare it with on ``VM1``.
To capture incoming packets , use tcpdump for the interface,
``ens4`` in this case.

.. code-block:: console

    # terminal 5
    # capture on ens4 of VM1
    $ tcpdump -i ens4

.. code-block:: console

    # terminal 7
    # capture on ens4 of VM3
    $ tcpdump -i ens4

You send packets from the remote ``host1`` and confirm packets are received.
IP address is the same as :ref:`Usecase of spp_vf<spp_usecases_vf>`.

.. code-block:: console

    # Send packets from host1
    $ ping 192.168.140.21



Stop Mirroring
~~~~~~~~~~~~~~

Graceful shutdown of secondary processes is same as previous usecases.
