..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2017-2018 Nippon Telegraph and Telephone Corporation

.. _spp_container_usecases:

Use Cases
=========

SPP Container provides an easy way to configure network path
for DPDK application running on containers.
It is useful for testing your NFV applications with ``testpmd`` or
``pktgen`` quickly, or providing a reproducible environment for evaluation
with a configuration files.

In addition, using container requires less CPU and memory resources
comparing with using virtual machines.
It means that users can try to test variety kinds of use cases without
using expensive servers.

This chapter describes examples of simple use cases of SPP container.

.. note::

    As described in
    :ref:`sppc_gs_build_docker_imgs`
    section, you had better to use Ubuntu 16.04 with
    ``--dist-ver`` option because SPP container is not stable for running
    on the latest version.

    Please notice that examples in this section does not use ``dist-ver``
    options explicitly for simplicity.


.. _sppc_usecases_test_vhost_single:

Perfromance Test of Vhost in Single Node
----------------------------------------

First use case is a simple performance test of vhost PMDs as shown in
:numref:`figure_sppc_usecase_vhost`.
Two of containers of ``spp_nfv`` are connected with a ring PMD and
all of app container processes run on a single node.

.. _figure_sppc_usecase_vhost:

.. figure:: ../../images/tools/sppc/sppc_usecase_vhost.*
    :width: 58%

    Test of vhost PMD in a single node


You use three terminals in this example, first one is for ``spp-ctl``,
second one is for SPP CLI and third one is for managing app containers.
First of all, launch ``spp-ctl`` in terminal 1.

.. code-block:: console

    # Terminal 1
    $ cd /path/to/spp
    $ python3 src/spp-ctl/spp-ctl

Then, ``spp.py`` in terminal 2.

.. code-block:: console

    # Terminal 2
    $ cd /path/to/spp
    $ python src/spp.py

Move to terminal 3, launch app containers of ``spp_primary``
and ``spp_nfv`` step by step in background mode.
You notice that vhost device is attached with ``-dv 1`` which is not used
actually.
It is because that SPP primary requires at least one port even if
it is no need.
You can also assign a physical port instead of this vhost device.

.. code-block:: console

    # Terminal 3
    $ cd /path/to/spp/tools/sppc
    $ python app/spp-primary.py -l 0 -p 0x01 -dv 1
    $ python app/spp-nfv.py -i 1 -l 1-2
    $ python app/spp-nfv.py -i 2 -l 3-4

Then, add two vhost PMDs for pktgen app container from SPP CLI.

.. code-block:: console

    # Terminal 2
    spp > nfv 1; add vhost 1
    spp > nfv 2; add vhost 2

It is ready for launching pktgen app container. In this usecase,
use five lcores for pktgen. One lcore is used for master, and remaining
lcores are used for rx and tx evenly.
Device ID option ``-d 1,2`` is for refferring vhost 1 and 2.

.. code-block:: console

    # Terminal 3
    $ python app/pktgen.py -fg -l 5-9 -d 1,2

Finally, configure network path from SPP controller,

.. code-block:: console

    # Terminal 2
    spp > nfv 1; patch ring:0 vhost:1
    spp > nfv 2; patch vhost:2 ring:0
    spp > nfv 1; forward
    spp > nfv 2; forward

and start forwarding from pktgen.

.. code-block:: console

    # Terminal 2
    $ Pktgen:/> start 1

You find that packet count of rx of port 0 and tx of port 1
is increased rapidlly.


.. _sppc_usecases_test_ring:

Performance Test of Ring
------------------------

Ring PMD is a very fast path to communicate between DPDK processes.
It is a kind of zero-copy data passing via shared memory and better
performance than vhost PMD.
Currently, only ``spp_nfv`` provides ring PMD in SPP container.
It is also possible other DPDK applications to have ring PMD interface
for SPP technically,
but not implemented yet.

This use case is for testing performance of ring PMDs.
As described in :numref:`figure_sppc_usecase_ring`,
each of app containers on which ``spp_nfv`` is running are connected
with ring PMDs in serial.

.. _figure_sppc_usecase_ring:

.. figure:: ../../images/tools/sppc/sppc_usecase_ring.*
   :width: 100%

   Test of ring PMD

You use three terminals on host 1, first one is for ``spp-ctl``,
second one is for ``spp.py``, and third one is for ``spp_nfv`` app containers.
Pktgen on host 2 is started forwarding after setup on host 1 is finished.

First, launch ``spp-ctl`` in terminal 1.

.. code-block:: console

    # Terminal 1
    $ cd /path/to/spp
    $ python3 src/spp-ctl/spp-ctl

Then, launch ``spp.py`` in terminal 2.

.. code-block:: console

    # Terminal 2
    $ cd /path/to/spp
    $ python src/spp.py

In terminal 3, launch ``spp_primary`` and ``spp_nfv`` containers
in background mode.
In this case, you attach physical ports to ``spp_primary`` with
portmask option.

.. code-block:: console

    # Terminal 3
    $ cd /path/to/spp/tools/sppc
    $ python app/spp-primary.py -l 0 -p 0x03
    $ python app/spp-nfv.py -i 1 -l 1-2
    $ python app/spp-nfv.py -i 2 -l 3-4
    $ python app/spp-nfv.py -i 3 -l 5-6
    $ python app/spp-nfv.py -i 4 -l 7-8


.. note::

    It might happen an error to input if the number of SPP process is
    increased. It also might get bothered to launch several SPP
    processes if the number is large.

    You can use ``tools/spp-launcher.py`` to launch SPP processes
    at once. Here is an example for launching ``spp_primary`` and
    four ``spp_nfv`` processes. ``-n`` is for specifying the nubmer of
    ``spp_nfv``.

    .. code-block:: console

        $ python tools/spp-launcher.py -n 4

    You will find that lcore assignment is the same as below.
    Lcore is assigned from 0 for primary, and next two lcores for the
    first ``spp_nfv``.

    .. code-block:: console

        $ python app/spp-primary.py -l 0 -p 0x03
        $ python app/spp-nfv.py -i 1 -l 1,2
        $ python app/spp-nfv.py -i 2 -l 3,4
        $ python app/spp-nfv.py -i 3 -l 5,6
        $ python app/spp-nfv.py -i 4 -l 7,8

    You can also assign lcores with ``--shared`` to master lcore
    be shared among ``spp_nfv`` processes.
    It is useful to reduce the usage of lcores as explained in
    :ref:`sppc_usecases_pktgen_l2fwd_less_lcores`.

    .. code-block:: console

        $ python tools/spp-launcher.py -n 4 --shared

    The result of assignment of this command is the same as below.
    Master lcore 1 is shared among secondary processes.

    .. code-block:: console

        $ python app/spp-primary.py -l 0 -p 0x03
        $ python app/spp-nfv.py -i 1 -l 1,2
        $ python app/spp-nfv.py -i 2 -l 1,3
        $ python app/spp-nfv.py -i 3 -l 1,4
        $ python app/spp-nfv.py -i 4 -l 1,5

Add ring PMDs considering which of rings is shared between which of
containers.
You can use recipe scripts from ``playback`` command instead of
typing commands step by step.
For this usecase example, it is included in
``recipes/sppc/samples/test_ring.rcp``.

.. code-block:: console

    # Terminal 2
    spp > nfv 1; add ring:0
    spp > nfv 2; add ring:1
    spp > nfv 2; add ring:2
    spp > nfv 3; add ring:2
    spp > nfv 3; add ring:3
    spp > nfv 4; add ring:3

Then, patch all of ports to be configured containers are connected
in serial.

.. code-block:: console

    # Terminal 2
    spp > nfv 1; patch phy:0 ring:0
    spp > nfv 2; patch ring:0 ring:1
    spp > nfv 3; patch ring:1 ring:2
    spp > nfv 3; patch ring:2 ring:3
    spp > nfv 4; patch ring:3 phy:1
    spp > nfv 1; forward
    spp > nfv 2; forward
    spp > nfv 3; forward
    spp > nfv 4; forward

After setup on host 1 is finished, start forwarding from pktgen on host 2.
You can see the throughput of rx and tx ports on pktgen's terminal.
You also find that the throughput is almost not decreased and keeping wire
rate speed even after it through several chained containers.


.. _sppc_usecases_pktgen_l2fwd:

Pktgen and L2fwd
----------------

To consider more practical service function chaining like use case,
connect not only SPP processes, but also DPDK application to ``pktgen``.
In this example, use ``l2fwd`` app container as a DPDK application
for simplicity.
You can also use other DPDK applications as similar to this example
as described in next sections.

.. _figure_sppc_usecase_l2fwdpktgen:

.. figure:: ../../images/tools/sppc/sppc_usecase_l2fwdpktgen.*
    :width: 95%

    Chainning pktgen and l2fwd

This configuration requires more CPUs than previous example.
It is up to 14 lcores, but you can reduce lcores to do the trick.
It is a trade-off between usage and performance.
In this case, we focus on the usage of maximum lcores to get high
performance.

Here is a list of lcore assignment for each of app containers.

* One lcore for ``spp_primary`` container.
* Eight lcores for four ``spp_nfv`` containers.
* Three lcores for ``pktgen`` container.
* Two lcores for ``l2fwd`` container.

First of all, launch ``spp-ctl`` and ``spp.py``.

.. code-block:: console

    # Terminal 1
    $ cd /path/to/spp
    $ python3 src/spp-ctl/spp-ctl

    # Terminal 2
    $ cd /path/to/spp
    $ python src/spp.py

Then, launch ``spp_primary`` and ``spp_nfv`` containers in background.
It does not use physical NICs as similar to
:ref:`sppc_usecases_test_vhost_single`.
Use four of ``spp_nfv`` containers for using four vhost PMDs.

.. code-block:: console

    # Terminal 3
    $ cd /path/to/spp/tools/sppc
    $ python app/spp-primary.py -l 0 -p 0x01 -dv 9
    $ python app/spp-nfv.py -i 1 -l 1-2
    $ python app/spp-nfv.py -i 2 -l 3-4
    $ python app/spp-nfv.py -i 3 -l 5-6
    $ python app/spp-nfv.py -i 4 -l 7-8

Assign ring and vhost PMDs. Each of vhost IDs to be the same as
its secondary ID.

.. code-block:: console

    # Terminal 2
    spp > nfv 1; add vhost:1
    spp > nfv 2; add vhost:2
    spp > nfv 3; add vhost:3
    spp > nfv 4; add vhost:4
    spp > nfv 1; add ring:0
    spp > nfv 4; add ring:0
    spp > nfv 2; add ring:1
    spp > nfv 3; add ring:1


After vhost PMDs are created, you can launch containers
of ``pktgen`` and ``l2fwd``.

In this case, ``pktgen`` container owns vhost 1 and 2,

.. code-block:: console

    # Terminal 3
    $ cd /path/to/spp/tools/sppc
    $ python app/pktgen.py -l 9-11 -d 1,2

and ``l2fwd`` container owns vhost 3 and 4.

.. code-block:: console

    # Terminal 4
    $ cd /path/to/spp/tools/sppc
    $ python app/l2fwd.py -l 12-13 -d 3,4


Then, configure network path by pactching each of ports
and start forwarding from SPP controller.

.. code-block:: console

    # Terminal 2
    spp > nfv 1; patch ring:0 vhost:1
    spp > nfv 2; patch vhost:2 ring:1
    spp > nfv 3; patch ring:1 vhost:3
    spp > nfv 4; patch vhost:4 ring:0
    spp > nfv 1; forward
    spp > nfv 2; forward
    spp > nfv 3; forward
    spp > nfv 4; forward

Finally, start forwarding from ``pktgen`` container.
You can see that packet count is increased on both of
``pktgen`` and ``l2fwd``.

For this usecase example, recipe scripts are included in
``recipes/sppc/samples/pg_l2fwd.rcp``.

.. _sppc_usecases_pktgen_l2fwd_less_lcores:

Pktgen and L2fwd using less Lcores
----------------------------------

This section describes the effort of reducing the usage of lcore for
:ref:`sppc_usecases_pktgen_l2fwd`.

Here is a list of lcore assignment for each of app containers.
It is totally 7 lcores while the maximum number is 14.

* One lcore for ``spp_primary`` container.
* Three lcores for four ``spp_nfv`` containers.
* Two lcores for pktgen container.
* One lcores for l2fwd container.

.. _figure_sppc_usecase_l2fwdpktgen_less:

.. figure:: ../../images/tools/sppc/sppc_usecase_l2fwdpktgen_less.*
    :width: 95%

    Pktgen and l2fwd using less lcores

First of all, launch ``spp-ctl`` and ``spp.py``.

.. code-block:: console

    # Terminal 1
    $ cd /path/to/spp
    $ python3 src/spp-ctl/spp-ctl

    # Terminal 2
    $ cd /path/to/spp
    $ python src/spp.py

Launch ``spp_primary`` and ``spp_nfv`` containers in background.
It does not use physical NICs as similar to
:ref:`sppc_usecases_test_vhost_single`.
Use two of ``spp_nfv`` containers for using four vhost PMDs.

.. code-block:: console

    # Terminal 3
    $ cd /path/to/spp/tools/sppc
    $ python app/spp-primary.py -l 0 -p 0x01 -dv 9
    $ python app/spp-nfv.py -i 1 -l 1,2
    $ python app/spp-nfv.py -i 2 -l 1,3

The number of process and CPUs are fewer than previous example.
You can reduce the number of ``spp_nfv`` processes by assigning
several vhost PMDs to one process, although performance is decreased
possibly.
For the number of lcores, you can reduce it by sharing
the master lcore 1 which has no heavy tasks.

Assign each of two vhost PMDs to the processes.

.. code-block:: console

    # Terminal 2
    spp > nfv 1; add vhost:1
    spp > nfv 1; add vhost:2
    spp > nfv 2; add vhost:3
    spp > nfv 2; add vhost:4
    spp > nfv 1; add ring:0
    spp > nfv 1; add ring:1
    spp > nfv 2; add ring:0
    spp > nfv 2; add ring:1

After vhost PMDs are created, you can launch containers
of ``pktgen`` and ``l2fwd``.
These processes also share the master lcore 1 with others.

In this case, ``pktgen`` container uses vhost 1 and 2,

.. code-block:: console

    # Terminal 3
    $ python app/pktgen.py -l 1,4,5 -d 1,2

and ``l2fwd`` container uses vhost 3 and 4.

.. code-block:: console

    # Terminal 4
    $ cd /path/to/spp/tools/sppc
    $ python app/l2fwd.py -l 1,6 -d 3,4


Then, configure network path by pactching each of ports
and start forwarding from SPP controller.

.. code-block:: console

    # Terminal 2
    spp > nfv 1; patch ring:0 vhost:1
    spp > nfv 1; patch vhost:2 ring:1
    spp > nfv 3; patch ring:1 vhost:3
    spp > nfv 4; patch vhost:4 ring:0
    spp > nfv 1; forward
    spp > nfv 2; forward
    spp > nfv 3; forward
    spp > nfv 4; forward

Finally, start forwarding from ``pktgen`` container.
You can see that packet count is increased on both of
``pktgen`` and ``l2fwd``.

For this usecase example, recipe scripts are included in
``recipes/sppc/samples/pg_l2fwd_less.rcp``.

.. _sppc_usecases_lb_pktgen:

Load-Balancer and Pktgen
------------------------

Previous examples are all the single-path configurations and do not
have branches.
To explain how to setup a multi-path configuration, we use
`Load-Balancer
<https://dpdk.org/doc/guides/sample_app_ug/load_balancer.html>`_
application in this example.
It is an application distributes packet I/O task with several worker
lcores to share IP addressing.

.. _figure_sppc_usecase_lb_pktgen:

.. figure:: ../../images/tools/sppc/sppc_usecase_lb_pktgen.*
    :width: 100%

    Multi-path configuration with load_balancer and pktgen

Packets from tx of ``pktgen``, through ring:0, are received by rx
of ``load_balancer``.
Then, ``load_balancer`` classify the packets to decide the
destionations.
You can count received packets on rx ports of ``pktgen``.

There are six ``spp_nfv`` and two DPDK applications in this example.
To reduce the number of lcores, configure lcore assignment to share
the master lcore.
Do not assign several vhosts to a process to avoid the performance
degradation.
It is 15 lcores required to the configuration.

Here is a list of lcore assignment for each of app containers.

* One lcore for ``spp_primary`` container.
* Seven lcores for four ``spp_nfv`` containers.
* Three lcores for ``pktgen`` container.
* Four lcores for ``load_balancer`` container.

First of all, launch ``spp-ctl`` and ``spp.py``.

.. code-block:: console

    # Terminal 1
    $ cd /path/to/spp
    $ python3 src/spp-ctl/spp-ctl

    # Terminal 2
    $ cd /path/to/spp
    $ python src/spp.py

Launch ``spp_primary`` and ``spp_nfv`` containers in background.
It does not use physical NICs as similar to
:ref:`sppc_usecases_test_vhost_single`.
Use six ``spp_nfv`` containers for using six vhost PMDs.

.. code-block:: console

    # Terminal 3
    $ cd /path/to/spp/tools/sppc
    $ python app/spp-primary.py -l 0 -p 0x01 -dv 9
    $ python app/spp-nfv.py -i 1 -l 1,2
    $ python app/spp-nfv.py -i 2 -l 1,3
    $ python app/spp-nfv.py -i 3 -l 1,4
    $ python app/spp-nfv.py -i 4 -l 1,5
    $ python app/spp-nfv.py -i 5 -l 1,6
    $ python app/spp-nfv.py -i 6 -l 1,7

Assign ring and vhost PMDs. Each of vhost IDs to be the same as
its secondary ID.

.. code-block:: console

    # Terminal 2
    spp > nfv 1; add vhost:1
    spp > nfv 2; add vhost:2
    spp > nfv 3; add vhost:3
    spp > nfv 4; add vhost:4
    spp > nfv 5; add vhost:5
    spp > nfv 6; add vhost:6
    spp > nfv 1; add ring:0
    spp > nfv 2; add ring:1
    spp > nfv 3; add ring:2
    spp > nfv 4; add ring:0
    spp > nfv 5; add ring:1
    spp > nfv 6; add ring:2

And patch all of ports.

.. code-block:: console

    # Terminal 2
    spp > nfv 1; patch vhost:1 ring:0
    spp > nfv 2; patch ring:1 vhost:2
    spp > nfv 3; patch ring:2 vhost:3
    spp > nfv 4; patch ring:0 vhost:4
    spp > nfv 5; patch vhost:5 ring:1
    spp > nfv 6; patch vhost:6 ring:2

You had better to check that network path is configured properly.
``topo`` command is useful for checking it with a graphical image.
Define two groups of vhost PMDs as ``c1`` and ``c2`` with
``topo_subgraph`` command before.

.. code-block:: console

    # Terminal 2
    # define c1 and c2 to help your understanding
    spp > topo_subgraph add c1 vhost:1,vhost:2,vhost:3
    spp > topo_subgraph add c2 vhost:4,vhost:5,vhost:6

    # show network diagram
    spp > topo term


Finally, launch ``pktgen`` and ``load_balancer`` app containers
to start traffic monitoring.

For ``pktgen`` container, assign lcores 8-10 and vhost 1-3.
``-T`` options is required to enable color terminal output.

.. code-block:: console

    # Terminal 3
    $ cd /path/to/spp/tools/sppc
    $ python app/pktgen.py -l 8-10 -d 1-3 -T


For ``load_balancer`` container, assign lcores 12-15 and vhost 4-6.
Four lcores are assigned to rx, tx and two workers.
You should add ``-nq`` option because this example requires three
or more queues. In this case, assign 4 queues.

.. code-block:: console

    # Terminal 4
    $ cd /path/to/spp/tools/sppc
    $ python app/load_balancer.py -l 11-14 -d 4-6 -fg -nq 4
      -rx "(0,0,11),(0,1,11),(0,2,11)" \
      -tx "(0,12),(1,12),(2,12)" \
      -w 13,14 \
      --lpm "1.0.0.0/24=>0; 1.0.1.0/24=>1; 1.0.2.0/24=>2;"


Then, configure network path by pactching each of ports
and start forwarding from SPP controller.

.. code-block:: console

    # Terminal 2
    spp > nfv 1; forward
    spp > nfv 2; forward
    spp > nfv 3; forward
    spp > nfv 4; forward
    spp > nfv 5; forward
    spp > nfv 6; forward

You start forwarding from ``pktgen`` container.
The destination of ``load_balancer`` is decided by considering
LPM rules. Try to classify incoming packets to port 1 on the
``load_balancer`` application.

On ``pktgen``, change the destination IP address of port 0
to ``1.0.1.100``, and start.

.. code-block:: console

    # Terminal 3
    Pktgen:/> set 0 dst ip 1.0.1.100
    Pktgen:/> start 0

As forwarding on port 0 is started, you will find the packet count of
port 1 is increase rapidly.
You can change the destination IP address and send packets to port 2
by stopping to forward,
changing the destination IP address to ``1.0.2.100`` and restart
forwarding.

.. code-block:: console

    # Terminal 3
    Pktgen:/> stop 0
    Pktgen:/> set 0 dst ip 1.0.2.100
    Pktgen:/> start 0

You might not be able to stop ``load_balancer`` application with *Ctrl-C*.
In this case, terminate it with ``docker kill`` directly as explained in
:ref:`sppc_appl_load_balancer`.
You can find the name of container from ``docker ps``.

For this usecase example, recipe scripts are included in
``recipes/sppc/samples/lb_pg.rcp``.
