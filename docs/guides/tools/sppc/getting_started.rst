..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2017-2018 Nippon Telegraph and Telephone Corporation

.. _spp_container_gs:

Getting Started
===============

In this section, learn how to use SPP container with a simple
usecase.
You use four of terminals for running SPP processes and applications.

.. _sppc_gs_setup:

Setup DPDK and SPP
------------------

First of all, you need to clone DPDK and setup hugepages for running
DPDK application as described in
:doc:`../../gsg/setup`
or DPDK's
`Gettting Started Guide
<https://dpdk.org/doc/guides/linux_gsg/sys_reqs.html>`_.
You also need to load kernel modules and bind network ports as in
`Linux Drivers
<https://dpdk.org/doc/guides/linux_gsg/linux_drivers.html>`_.

Then, as described in
:doc:`../../gsg/install`
, clone and compile SPP in any directory.

.. code-block:: console

    # Terminal 1
    $ git clone http://dpdk.org/git/apps/spp
    $ cd spp


.. _sppc_gs_build_docker_imgs:

Build Docker Images
-------------------

Build tool is a python script for creating a docker image and
currently supporting three types of images for
DPDK sample applications, pktgen-dpdk, or SPP.

Run build tool for creating three type of docker images.
It starts to download the latest Ubuntu docker image and installation
for the latest DPDK, pktgen or SPP.

.. code-block:: console

    # Terminal 1
    $ cd /path/to/spp/tools/sppc
    $ python build/main.py -t dpdk
    $ python build/main.py -t pktgen
    $ python build/main.py -t spp

Of course DPDK is required from pktgen and SPP, and it causes a
problem of compatibility between them sometimes.
At the time writing this document, SPP v18.02 is not compatible with
the latest DPDK v18.05 and it is failed to compile.
In this case, you should build SPP with ``--dpdk-branch`` option to tell
the version of DPDK explicitly.

.. code-block:: console

    # Terminal 1
    $ python build/main.py -t spp --dpdk-branch v18.02

You can find all of options by ``build/main.py -h``.

Waiting for a minutes, then you are ready to launch app containers.
All of images are referred from ``docker images`` command.

.. code-block:: console

    $ docker images
    REPOSITORY           TAG       IMAGE ID         CREATED         SIZE
    sppc/spp-ubuntu      latest    3ec39adb460f     2 days ago      862MB
    sppc/pktgen-ubuntu   latest    ffe65cc70e65     2 days ago      845MB
    sppc/dpdk-ubuntu     latest    0d5910d10e3f     2 days ago      1.66GB
    <none>               <none>    d52d2f86a3c0     2 days ago      551MB
    ubuntu               latest    452a96d81c30     5 weeks ago     79.6MB

.. note::

    The Name of containers is defined as a set of target, name and
    version of Linux distoribution.
    For example, container image targetting dpdk apps on Ubuntu 16.04
    is named as ``sppc/dpdk-ubuntu:16.04``.

    Build script understands which of Dockerfile should be used based
    on the given options.
    If you run build script with options for dpdk and Ubuntu 16.04 as
    below, it finds ``build/ubuntu/dpdk/Dockerfile.16.04`` and runs
    ``docker build``.
    Options for Linux distribution have default value, ``ubuntu`` and
    ``latest``. So, you do not need to specify them if you use default.


    .. code-block:: console

        # latest DPDK on latest Ubuntu
        $ python build/main.py -t dpdk --dist-name ubuntu --dist-ver latest

        # it is also the same
        $ python build/main.py -t dpdk

        # or use Ubuntu 16.04
        $ python build/main.py -t dpdk --dist-ver 16.04


.. warning::

    Currently, the latest version of Ubuntu is 18.04 and DPDK is 18.05.
    However, SPP is not stable on the latest versions, especially
    running on containers.

    It is better to use Ubuntu 16.04 and DPDK 18.02 for SPP containers
    until be stabled.

    .. code-block:: console

        $ python build/main.py -t dpdk --dist-ver 16.04 --dpdk-branch v18.02
        $ python build/main.py -t pktgen --dist-ver 16.04 \
          --dpdk-branch v18.02 --pktgen-branch pktgen-3.4.9
        $ python build/main.py -t spp --dist-ver 16.04 --dpdk-branch v18.02


.. _sppc_gs_launch_containers:

Launch SPP and App Containers
-----------------------------

Before launch containers, you should set IP address of host machine
as ``SPP_CTL_IP`` environment variable
for controller to be accessed from inside containers.
It is better to define this variable in ``$HOME/.bashrc``.

.. code-block:: console

    # Set your host IP address
    export SPP_CTL_IP=HOST_IPADDR


SPP Controller
~~~~~~~~~~~~~~

Launch ``spp-ctl`` and ``spp.py`` to be ready before primary and secondary
processes.

.. note::

    SPP controller provides ``topo term`` which shows network
    topology in a terminal.

    However, there are a few terminals supporing this feature.
    ``mlterm`` is the most useful and easy to customize.
    Refer :doc:`../../commands/experimental` for ``topo`` command.

``spp-ctl`` is launched in the termina l.

.. code-block:: console

    # Terminal 1
    $ cd /path/to/spp
    $ python3 src/spp-ctl/spp-ctl

``spp.py`` is launched in the terminal 2.

.. code-block:: console

    # Terminal 2
    $ cd /path/to/spp
    $ python src/spp.py


SPP Primary Container
~~~~~~~~~~~~~~~~~~~~~

As ``SPP_CTL_IP`` is activated, you are enalbed to run
``app/spp-primary.py`` with options of EAL and SPP primary
in terminal 3.
In this case, launch spp-primary in background mode using one core
and two ports.

.. code-block:: console

    # Terminal 3
    $ cd /path/to/spp/tools/sppc
    $ python app/spp-primary.py -l 0 -p 0x03


SPP Secondary Container
~~~~~~~~~~~~~~~~~~~~~~~

For secondary process, ``spp_nfv`` is only supported for running on container
currently.

Launch ``spp_nfv`` in terminal 3
with options for secondary ID is ``1`` and
core list is ``1-2`` for using 2nd and 3rd cores.

.. code-block:: console

    # Terminal 3
    $ python app/spp-nfv.py -i 1 -l 1-2

If it is succeeded, container is running in background.
You can find it with ``docker -ps`` command.


App Container
~~~~~~~~~~~~~

Launch DPDK's ``testpmd`` as an example of app container.

Currently, most of app containers do not support ring PMD.
It means that you should create vhost PMDs from SPP controller
before launching the app container.

.. code-block:: console

    # Terminal 2
    spp > nfv 1; add vhost 1
    spp > nfv 1; add vhost 2

``spp_nfv`` of ID 1 running inside container creates
``vhost:1`` and ``vhost:2``.
Vhost PMDs are referred as an option ``-d 1,2`` from the
app container launcher.

.. code-block:: console

    # Terminal 3
    $ cd /path/to/spp/tools/sppc
    $ app/testpmd.py -l 3-4 -d 1,2
    sudo docker run -it \
    ...
    EAL: Detected 16 lcore(s)
    EAL: Auto-detected process type: PRIMARY
    EAL: Multi-process socket /var/run/.testpmd1_unix
    EAL: Probing VFIO support...
    EAL: VFIO support initialized
    Interactive-mode selected
    Warning: NUMA should be configured manually by using --port-numa-...
    testpmd: create a new mbuf pool <mbuf_pool_socket_0>: n=155456,...
    testpmd: preferred mempool ops selected: ring_mp_mc
    Configuring Port 0 (socket 0)
    Port 0: 32:CB:1D:72:68:B9
    Configuring Port 1 (socket 0)
    Port 1: 52:73:C3:5B:94:F1
    Checking link statuses...
    Done
    testpmd>

It launches ``testpmd`` in foreground mode.

.. note::

    DPDK app container tries to own ports on host which are shared with host
    and containers by default. It causes a confliction between SPP running on
    host and containers and unexpected behavior.

    To avoid this situation, it is required to use ``-b`` or
    ``--pci-blacklist`` EAL option to exclude ports on host. PCI address of
    port can be inspected by using ``dpdk-devbind.py -s``.

If you have ports on host and assign them to SPP, you should to exclude them
from the app container by specifying PCI addresses of the ports with ``-b``
or ``--pci-blacklist``.

You can find PCI addresses from ``dpdk-devbind.py -s``.

.. code-block:: console

    # Check the status of the available devices.
    dpdk-devbind --status
    Network devices using DPDK-compatible driver
    ============================================
    0000:0a:00.0 '82599ES 10-Gigabit' drv=igb_uio unused=ixgbe
    0000:0a:00.1 '82599ES 10-Gigabit' drv=igb_uio unused=ixgbe

    Network devices using kernel driver
    ===================================
    ...

In this case, you should exclude ``0000:0a:00.0`` and ``0000:0a:00.1``
with ``-b`` option.

.. code-block:: console

    # Terminal 3
    $ cd /path/to/spp/tools/sppc
    $ app/testpmd.py -l 3-4 -d 1,2 \
      -b 0000:0a:00.0 0000:0a:00.1
    sudo docker run -it \
    ...
    -b 0000:0a:00.0 \
    -b 0000:0a:00.1 \
    ...


.. _sppc_gs_run_apps:

Run Applications
----------------

At the end of this getting started guide, configure network paths
as described in
:numref:`figure_sppc_gsg_testpmd`
and start forwarding from testpmd.

.. _figure_sppc_gsg_testpmd:

.. figure:: ../../images/tools/sppc/sppc_gsg_testpmd.*
   :width: 58%

   SPP and testpmd on containers

In terminal 2, add ``ring:0``, connect ``vhost:1`` and ``vhost:2``
with it.

.. code-block:: console

    # Terminal 2
    spp > nfv 1; add ring 0
    spp > nfv 1; patch vhost:1 ring:0
    spp > nfv 1; patch ring:0 vhost:2
    spp > nfv 1; forward
    spp > nfv 1; status
    status: running
    ports:
      - 'ring:0 -> vhost:2'
      - 'vhost:1 -> ring:0'
      - 'vhost:2'

Start forwarding on port 0 by ``start tx_first``.

.. code-block:: console

    # Terminal 3
    testpmd> start tx_first
    io packet forwarding - ports=2 - cores=1 - streams=2 - NUMA support...
    Logical Core 4 (socket 0) forwards packets on 2 streams:
      RX P=0/Q=0 (socket 0) -> TX P=1/Q=0 (socket 0) peer=02:00:00:00:00:01
      RX P=1/Q=0 (socket 0) -> TX P=0/Q=0 (socket 0) peer=02:00:00:00:00:00
    ...

Finally, stop forwarding to show statistics as the result.
In this case, about 35 million packets are forwarded.

.. code-block:: console

    # Terminal 3
    testpmd> stop
    Telling cores to stop...
    Waiting for lcores to finish...

      ---------------------- Forward statistics for port 0  ------------------
      RX-packets: 0              RX-dropped: 0             RX-total: 0
      TX-packets: 35077664       TX-dropped: 0             TX-total: 35077664
      ------------------------------------------------------------------------

      ---------------------- Forward statistics for port 1  ------------------
      RX-packets: 35077632       RX-dropped: 0             RX-total: 35077632
      TX-packets: 32             TX-dropped: 0             TX-total: 32
      ------------------------------------------------------------------------

      +++++++++++++++ Accumulated forward statistics for all ports++++++++++++
      RX-packets: 35077632       RX-dropped: 0             RX-total: 35077632
      TX-packets: 35077696       TX-dropped: 0             TX-total: 35077696
      ++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
