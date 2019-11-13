..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2010-2014 Intel Corporation
    Copyright(c) 2017-2019 Nippon Telegraph and Telephone Corporation


.. _spp_usecases_nfv:

spp_nfv
=======


.. _spp_usecases_nfv_single_spp_nfv:

Single spp_nfv
--------------

The most simple usecase mainly for testing performance of packet
forwarding on host.
One ``spp_nfv`` and two physical ports.

In this usecase, try to configure two senarios.

- Configure ``spp_nfv`` as L2fwd
- Configure ``spp_nfv`` for Loopback


First of all, Check the status of ``spp_nfv`` from SPP CLI.

.. code-block:: console

    spp > nfv 1; status
    - status: idling
    - lcore_ids:
      - master: 1
      - slave: 2
    - ports:
      - phy:0
      - phy:1

This status message explains that ``nfv 1`` has two physical ports.


Configure spp_nfv as L2fwd
~~~~~~~~~~~~~~~~~~~~~~~~~~

Assing the destination of ports with ``patch`` subcommand and
start forwarding.
Patch from ``phy:0`` to ``phy:1`` and ``phy:1`` to ``phy:0``,
which means it is bi-directional connection.

.. code-block:: console

    spp > nfv 1; patch phy:0 phy:1
    Patch ports (phy:0 -> phy:1).
    spp > nfv 1; patch phy:1 phy:0
    Patch ports (phy:1 -> phy:0).
    spp > nfv 1; forward
    Start forwarding.

Confirm that status of ``nfv 1`` is updated to ``running`` and ports are
patches as you defined.

.. code-block:: console

    spp > nfv 1; status
    - status: running
    - lcore_ids:
      - master: 1
      - slave: 2
    - ports:
      - phy:0 -> phy:1
      - phy:1 -> phy:0

.. _figure_spp_nfv_as_l2fwd:

.. figure:: ../images/setup/use_cases/spp_nfv_l2fwd.*
   :width: 60%

   spp_nfv as l2fwd


Stop forwarding and reset patch to clear configuration.
``patch reset`` is to clear all of patch configurations.

.. code-block:: console

    spp > nfv 1; stop
    Stop forwarding.
    spp > nfv 1; patch reset
    Clear all of patches.


Configure spp_nfv for Loopback
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Patch ``phy:0`` to ``phy:0`` and ``phy:1`` to ``phy:1``
for loopback.

.. code-block:: console

    spp > nfv 1; patch phy:0 phy:0
    Patch ports (phy:0 -> phy:0).
    spp > nfv 1; patch phy:1 phy:1
    Patch ports (phy:1 -> phy:1).
    spp > nfv 1; forward
    Start forwarding.


Dual spp_nfv
------------

Use case for testing performance of packet forwarding
with two ``spp_nfv`` on host.
Throughput is expected to be better than
:ref:`Single spp_nfv<spp_usecases_nfv_single_spp_nfv>`
usecase because bi-directional forwarding of single ``spp_nfv`` is shared
with two of uni-directional forwarding between dual ``spp_nfv``.

In this usecase, configure two senarios almost similar to previous section.

- Configure Two ``spp_nfv`` as L2fwd
- Configure Two ``spp_nfv`` for Loopback


Configure Two spp_nfv as L2fwd
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Assing the destination of ports with ``patch`` subcommand and
start forwarding.
Patch from ``phy:0`` to ``phy:1`` for ``nfv 1`` and
from ``phy:1`` to ``phy:0`` for ``nfv 2``.

.. code-block:: console

    spp > nfv 1; patch phy:0 phy:1
    Patch ports (phy:0 -> phy:1).
    spp > nfv 2; patch phy:1 phy:0
    Patch ports (phy:1 -> phy:0).
    spp > nfv 1; forward
    Start forwarding.
    spp > nfv 2; forward
    Start forwarding.

.. _figure_spp_two_nfv_as_l2fwd:

.. figure:: ../images/setup/use_cases/spp_two_nfv_l2fwd.*
   :width: 60%

   Two spp_nfv as l2fwd


Configure two spp_nfv for Loopback
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Patch ``phy:0`` to ``phy:0`` for ``nfv 1`` and
``phy:1`` to ``phy:1`` for ``nfv 2`` for loopback.

.. code-block:: console

    spp > nfv 1; patch phy:0 phy:0
    Patch ports (phy:0 -> phy:0).
    spp > nfv 2; patch phy:1 phy:1
    Patch ports (phy:1 -> phy:1).
    spp > nfv 1; forward
    Start forwarding.
    spp > nfv 2; forward
    Start forwarding.

.. _figure_spp_two_nfv_loopback:

.. figure:: ../images/setup/use_cases/spp_two_nfv_loopback.*
   :width: 62%

   Two spp_nfv for loopback


Dual spp_nfv with Ring PMD
--------------------------

In this usecase, configure two senarios by using ring PMD.

- Uni-Directional L2fwd
- Bi-Directional L2fwd

Ring PMD
~~~~~~~~

Ring PMD is an interface for communicating between secondaries on host.
The maximum number of ring PMDs is defined as ``-n``  option of
``spp_primary`` and ring ID is started from 0.

Ring PMD is added by using ``add`` subcommand.
All of ring PMDs is showed with ``status`` subcommand.

.. code-block:: console

    spp > nfv 1; add ring:0
    Add ring:0.
    spp > nfv 1; status
    - status: idling
    - lcore_ids:
      - master: 1
      - slave: 2
    - ports:
      - phy:0
      - phy:1
      - ring:0

Notice that ``ring:0`` is added to ``nfv 1``.
You can delete it with ``del`` command if you do not need to
use it anymore.

.. code-block:: console

    spp > nfv 1; del ring:0
    Delete ring:0.
    spp > nfv 1; status
    - status: idling
    - lcore_ids:
      - master: 1
      - slave: 2
    - ports:
      - phy:0
      - phy:1


Uni-Directional L2fwd
~~~~~~~~~~~~~~~~~~~~~

Add a ring PMD and connect two ``spp_nvf`` processes.
To configure network path, add ``ring:0`` to ``nfv 1`` and ``nfv 2``.
Then, connect it with ``patch`` subcommand.

.. code-block:: console

    spp > nfv 1; add ring:0
    Add ring:0.
    spp > nfv 2; add ring:0
    Add ring:0.
    spp > nfv 1; patch phy:0 ring:0
    Patch ports (phy:0 -> ring:0).
    spp > nfv 2; patch ring:0 phy:1
    Patch ports (ring:0 -> phy:1).
    spp > nfv 1; forward
    Start forwarding.
    spp > nfv 2; forward
    Start forwarding.

.. _figure_spp_uni_directional_l2fwd:

.. figure:: ../images/setup/use_cases/spp_unidir_l2fwd.*
   :width: 72%

   Uni-Directional l2fwd


Bi-Directional L2fwd
~~~~~~~~~~~~~~~~~~~~

Add two ring PMDs to two ``spp_nvf`` processes.
For bi-directional forwarding,
patch ``ring:0`` for a path from ``nfv 1`` to ``nfv 2``
and ``ring:1`` for another path from ``nfv 2`` to ``nfv 1``.

First, add ``ring:0`` and ``ring:1`` to ``nfv 1``.

.. code-block:: console

    spp > nfv 1; add ring:0
    Add ring:0.
    spp > nfv 1; add ring:1
    Add ring:1.
    spp > nfv 1; status
    - status: idling
    - lcore_ids:
      - master: 1
      - slave: 2
    - ports:
      - phy:0
      - phy:1
      - ring:0
      - ring:1

Then, add ``ring:0`` and ``ring:1`` to ``nfv 2``.

.. code-block:: console

    spp > nfv 2; add ring:0
    Add ring:0.
    spp > nfv 2; add ring:1
    Add ring:1.
    spp > nfv 2; status
    - status: idling
    - lcore_ids:
      - master: 1
      - slave: 3
    - ports:
      - phy:0
      - phy:1
      - ring:0
      - ring:1

.. code-block:: console

    spp > nfv 1; patch phy:0 ring:0
    Patch ports (phy:0 -> ring:0).
    spp > nfv 1; patch ring:1 phy:0
    Patch ports (ring:1 -> phy:0).
    spp > nfv 2; patch phy:1 ring:1
    Patch ports (phy:1 -> ring:0).
    spp > nfv 2; patch ring:0 phy:1
    Patch ports (ring:0 -> phy:1).
    spp > nfv 1; forward
    Start forwarding.
    spp > nfv 2; forward
    Start forwarding.

.. _figure_spp_bi_directional_l2fwd:

.. figure:: ../images/setup/use_cases/spp_bidir_l2fwd.*
   :width: 72%

   Bi-Directional l2fwd


Single spp_nfv with Vhost PMD
-----------------------------

Vhost PMD
~~~~~~~~~

Vhost PMD is an interface for communicating between on hsot and guest VM.
As described in
:ref:`How to Use<spp_gsg_howto_use>`,
vhost must be created by ``add`` subcommand before the VM is launched.


Setup Vhost PMD
~~~~~~~~~~~~~~~

In this usecase, add ``vhost:0`` to ``nfv 1`` for communicating
with the VM.
First, check if ``/tmp/sock0`` is already exist.
You should remove it already exist to avoid a failure of socket file
creation.

.. code-block:: console

    # remove sock0 if already exist
    $ ls /tmp | grep sock
    sock0 ...
    $ sudo rm /tmp/sock0

Create ``/tmp/sock0`` from ``nfv 1``.

.. code-block:: console

    spp > nfv 1; add vhost:0
    Add vhost:0.


.. _usecase_spp_nfv_l2fwd_vhost_nw:

Setup Network Configuration in spp_nfv
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Launch a VM by using the vhost interface created in the previous step.
Lauunching VM is described in
:ref:`How to Use<spp_gsg_howto_use>`.

Patch ``phy:0`` to ``vhost:0`` and ``vhost:1`` to ``phy:1`` from ``nfv 1``
running on host.

.. code-block:: console

    spp > nfv 1; patch phy:0 vhost:0
    Patch ports (phy:0 -> vhost:0).
    spp > nfv 1; patch vhost:1 phy:1
    Patch ports (vhost:1 -> phy:1).
    spp > nfv 1; forward
    Start forwarding.

Finally, start forwarding inside the VM by using two vhost ports
to confirm that network on host is configured.

.. code-block:: console

    $ sudo $RE_SDK/examples/build/l2fwd -l 0-1 -- -p 0x03

.. _figure_spp_nfv_l2fwd_vhost:

.. figure:: ../images/setup/use_cases/spp_nfv_l2fwd_vhost.*
   :width: 72%

   Single spp_nfv with vhost PMD

Single spp_nfv with PCAP PMD
-----------------------------

PCAP PMD
~~~~~~~~

Pcap PMD is an interface for capturing or restoring traffic.
For usign pcap PMD, you should set ``CONFIG_RTE_LIBRTE_PMD_PCAP``
and ``CONFIG_RTE_PORT_PCAP`` to ``y`` and compile DPDK before SPP.
Refer to
:ref:`Install DPDK and SPP<setup_install_dpdk_spp>`
for details of setting up.

Pcap PMD has two different streams for rx and tx.
Tx device is for capturing packets and rx is for restoring captured
packets.
For rx device, you can use any of pcap files other than SPP's pcap PMD.

To start using pcap pmd, just using ``add`` subcommand as ring.
Here is an example for creating pcap PMD ``pcap:1``.

.. code-block:: console

    spp > nfv 1; add pcap:1

After running it, you can find two of pcap files in ``/tmp``.

.. code-block:: console

    $ ls /tmp | grep pcap$
    spp-rx1.pcap
    spp-tx1.pcap

If you already have a dumped file, you can use it by it putting as
``/tmp/spp-rx1.pcap`` before running the ``add`` subcommand.
SPP does not overwrite rx pcap file if it already exist,
and it just overwrites tx pcap file.

Capture Incoming Packets
~~~~~~~~~~~~~~~~~~~~~~~~

As the first usecase, add a pcap PMD and capture incoming packets from
``phy:0``.

.. code-block:: console

    spp > nfv 1; add pcap 1
    Add pcap:1.
    spp > nfv 1; patch phy:0 pcap:1
    Patch ports (phy:0 -> pcap:1).
    spp > nfv 1; forward
    Start forwarding.

.. _figure_spp_pcap_incoming:

.. figure:: ../images/setup/use_cases/spp_pcap_incoming.*
   :width: 60%

   Rapture incoming packets

In this example, we use pktgen.
Once you start forwarding packets from pktgen, you can see
that the size of ``/tmp/spp-tx1.pcap`` is increased rapidly
(or gradually, it depends on the rate).

.. code-block:: console

    Pktgen:/> set 0 size 1024
    Pktgen:/> start 0

To stop capturing, simply stop forwarding of ``spp_nfv``.

.. code-block:: console

    spp > nfv 1; stop
    Stop forwarding.

You can analyze the dumped pcap file with other tools like as wireshark.

Restore dumped Packets
~~~~~~~~~~~~~~~~~~~~~~

In this usecase, use dumped file in previsou section.
Copy ``spp-tx1.pcap`` to ``spp-rx2.pcap`` first.

.. code-block:: console

    $ sudo cp /tmp/spp-tx1.pcap /tmp/spp-rx2.pcap

Then, add pcap PMD ``pcap:2`` to another ``spp_nfv``.

.. code-block:: console

    spp > nfv 2; add pcap:2
    Add pcap:2.

.. _figure_spp_pcap_restoring:

.. figure:: ../images/setup/use_cases/spp_pcap_restoring.*
   :width: 60%

   Restore dumped packets

You can find that ``spp-tx2.pcap`` is creaeted and ``spp-rx2.pcap``
still remained.

.. code-block:: console

    $ ls -al /tmp/spp*.pcap
    -rw-r--r-- 1 root root         24  ...  /tmp/spp-rx1.pcap
    -rw-r--r-- 1 root root 2936703640  ...  /tmp/spp-rx2.pcap
    -rw-r--r-- 1 root root 2936703640  ...  /tmp/spp-tx1.pcap
    -rw-r--r-- 1 root root          0  ...  /tmp/spp-tx2.pcap

To confirm packets are restored, patch ``pcap:2`` to ``phy:1``
and watch received packets on pktgen.

.. code-block:: console

    spp > nfv 2; patch pcap:2 phy:1
    Patch ports (pcap:2 -> phy:1).
    spp > nfv 2; forward
    Start forwarding.

After started forwarding, you can see that packet count is increased.
