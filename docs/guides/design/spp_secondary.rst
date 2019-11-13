..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2019 Nippon Telegraph and Telephone Corporation


.. _spp_design_spp_sec:

SPP Secondary
=============

SPP secondary process is a worker process in client-server multp-process
application model. Basically, the role of secondary process is to connenct
each of application running on host, containers or VMs for packet forwarding.
Spp secondary process forwards packets from source port to destination port
with DPDK's high-performance forwarding mechanizm. In other word, it behaves
as a cable to connect two patches ports.

All of secondary processes are able to attach ring PMD and vhost PMD ports
for sending or receiving packets with other processes. Ring port is used to
communicate with a process running on host or container if it is implemented
as secondary process to access shared ring memory.
Vhost port is used for a process on container or VM and implemented as primary
process, and no need to access shared memory of SPP primary.

In addition to the basic forwarding, SPP secondary process provides several
networking features. One of the typical example is packet cauture.
``spp_nfv`` is the simplest SPP secondary and used to connect two of processes
or other feature ports including PCAP PMD port. PCAP PMD is to dump packets to
a file or retrieve from.

There are more specific or funcional features than ``spp_nfv``. ``spp_vf`` is
a simple pseudo SR-IOV feature for classifying or merging packets.
``spp_mirror`` is to duplicate incoming packets to several destination ports.


.. _spp_design_spp_sec_nfv:

spp_nfv
-------

``spp_nfv`` is the simplest SPP secondary to connect two of processes or other
feature ports. Each of ``spp_nfv`` processes has a list of entries including
source and destination ports, and forwards packets by referring the list.
It means that one ``spp_nfv`` might have several forwarding paths, but
throughput is gradually decreased if it has too much paths.
This list is implemented as an array of ``port`` structure and named
``ports_fwd_array``. The index of ``ports_fwd_array`` is the same as unique
port ID.

.. code-block:: c

    struct port {
      int in_port_id;
      int out_port_id;
      ...
    };
    ...

    /* ports_fwd_array is an array of port */
    static struct port ports_fwd_array[RTE_MAX_ETHPORTS];

:numref:`figure_design_spp_sec_nfv_port_fwd_array` describes an example of
forwarding between ports. In this case, ``spp_nfv`` is responsible for
forwarding from ``port#0`` to ``port#2``. You notice that each of ``out_port``
entry has the destination port ID.

.. _figure_design_spp_sec_nfv_port_fwd_array:

.. figure:: ../images/design/spp_design_spp_sec_nfv.*
   :width: 80%

   Forwarding by referring ports_fwd_array

``spp_nfv`` consists of main thread and worker thread to update the entry
while running the process. Main thread is for waiting user command for
updating the entry. Worker thread is for dedicating packet forwarding.
:numref:`figure_design_spp_sec_nfv_threads` describes tasks in each of
threads. Worker thread is launched from main thread after initialization.
In worker thread, it starts forwarding if user send forward command and
main thread accepts it.

.. _figure_design_spp_sec_nfv_threads:

.. figure:: ../images/design/spp_design_spp_sec_nfv_threads.*
   :width: 70%

   Main thread and worker thread in spp_nfv


.. _spp_design_spp_sec_vf:

spp_vf
------

``spp_vf`` provides a SR-IOV like network feature.

``spp_vf`` forwards incoming packets to several destination VMs by referring
MAC address like as a Virtual Function (VF) of SR-IOV.

``spp_vf`` is a  multi-process and multi-thread application.
Each of ``spp_vf`` has one manager thread and worker threads called as
components.
The manager thread provides a function for parsing a command and creating the
components.
The component threads have its own multiple components, ports and classifier
tables including Virtual MAC address.
There are three types of components, ``forwarder``,
``merger`` and ``classifier``.

This is an example of network configuration, in which one
``classifier``,
one merger and four forwarders are running in ``spp_vf`` process
for two destinations of vhost interface.
Incoming packets from rx on host1 are sent to each of vhosts of VM
by looking up destination MAC address in the packet.

.. figure:: ../images/design/spp_vf_overview.*
    :width: 72%

    Classification of spp_vf for two VMs


Forwarder
^^^^^^^^^

Simply forwards packets from rx to tx port.
Forwarder does not start forwarding until when at least one rx and one tx are
added.

Merger
^^^^^^

Receives packets from multiple rx ports to aggregate
packets and sends to a desctination port.
Merger does not start forwarding until when at least two rx and one tx are
added.

Classifier
^^^^^^^^^^

Sends packets to multiple tx ports based on entries of
MAC address and destination port in a classifier table.
This component also supports VLAN tag.

For VLAN addressing, classifier has other tables than defalut.
Classifier prepares tables for each of VLAN ID and decides
which of table is referred
if TPID (Tag Protocol Indetifier) is included in a packet and
equals to 0x8100 as defined in IEEE 802.1Q standard.
Classifier does not start forwarding until when at least one rx and two tx
are added.


.. _spp_design_spp_sec_mirror:

spp_mirror
----------

``spp_mirror`` is an implementation of
`TaaS
<https://docs.openstack.org/dragonflow/latest/specs/tap_as_a_service.html>`_
as a SPP secondary process for port mirroring.
TaaS stands for TAP as a Service.
The keyword ``mirror`` means that it duplicates incoming packets and forwards
to additional destination.

Mirror
^^^^^^

``mirror`` component has one ``rx`` port and two ``tx`` ports. Incoming packets
from ``rx`` port are duplicated and sent to each of ``tx`` ports.

.. _figure_spp_mirror_design:

.. figure:: ../images/design/spp_mirror_design.*
    :width: 45%

    Spp_mirror component

In general, copying packet is time-consuming because it requires to make a new
region on memory space. Considering to minimize impact for performance,
``spp_mirror`` provides a choice of copying methods, ``shallowocopy`` or
``deepcopy``.
The difference between those methods is ``shallowocopy`` does not copy whole of
packet data but share without header actually.
``shallowcopy`` is to share mbuf between packets to get better performance
than ``deepcopy``, but it should be used for read only for the packet.

.. note::

    ``shallowcopy`` calls ``rte_pktmbuf_clone()`` internally and
    ``deepcopy`` create a new mbuf region.

You should choose ``deepcopy`` if you use VLAN feature to make no change for
original packet while copied packet is modified.


.. _spp_design_spp_sec_pcap:

spp_pcap
--------

SPP provides a connectivity between VM and NIC as a virtual patch panel.
However, for more practical use, operator and/or developer needs to capture
packets. For such use, spp_pcap provides packet capturing feature from
specific port. It is aimed to capture up to 10Gbps packets.

``spp_pcap`` is a SPP secondary process for capturing packets from specific
``port``. :numref:`figure_spp_pcap_overview` shows an overview of use of
``spp_pcap`` in which ``spp_pcap`` process receives packets from ``phy:0``
for capturing.

.. note::

   ``spp_pcap`` supports only two types of ports for capturing, ``phy``
   and ``ring``, currently.

.. _figure_spp_pcap_overview:

.. figure:: ../images/design/spp_pcap_overview.*
   :width: 50%

   Overview of spp_pcap

``spp_pcap`` cosisits of main thread, ``receiver`` thread and one or more
``wirter`` threads. As design policy, the number of ``receiver`` is fixed
to 1 because to make it simple and it is enough for task of receiving.
``spp_pcap`` requires at least three lcores, and assign to from master,
``receiver`` and then the rest of ``writer`` threads respectively.

Incoming packets are received by ``receiver`` thread and transferred to
``writer`` threads via ring buffers between threads.

Several ``writer`` work in parallel to store packets as files in LZ4
format. You can capture a certain amount of heavy traffic by using much
``writer`` threads.

:numref:`figure_spp_pcap_design` shows an usecase of ``spp_pcap`` in which
packets from ``phy:0`` are captured by using three ``writer`` threads.

.. _figure_spp_pcap_design:

.. figure:: ../images/design/spp_pcap_design.*
    :width: 55%

    spp_pcap internal structure
