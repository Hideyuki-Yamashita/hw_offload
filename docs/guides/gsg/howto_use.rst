..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2010-2014 Intel Corporation

.. _spp_gsg_howto_use:

How to Use
==========

As described in :ref:`Design<spp_overview_design>`, SPP consists of
primary process for managing resources, secondary processes for
forwarding packet, and SPP controller to accept user commands and
send it to SPP processes.

You should keep in mind the order of launching processes if you do it
manually, or you can use startup script. This start script is for launching
``spp-ctl``, ``spp_primary`` and SPP CLI.


.. _spp_gsg_howto_quick_start:

Quick Start
-----------

Run ``bin/start.sh`` with configuration file ``bin/config.sh``.
First time you run the script, it does not lanch processes but create a
template configuration file and asks you to edit this file.
After that, you can run the startup script for launching processes. All of
options for the processes are defined in the configuration.

.. code-block:: console

    # launch with default URL http://127.0.0.1:7777
    $ bin/start.sh
    Start spp-ctl
    Start spp_primary
    Waiting for spp-ctl is ready ...
    Welcome to the SPP CLI. Type `help` or `?` to list commands.

    spp >

Check status of ``spp_primary`` because it takes several seconds to be ready.
Confirm that the status is ``running``.

.. code-block:: none

    spp > status
    - spp-ctl:
      - address: 127.0.0.1:7777
    - primary:
      - status: running
    - secondary:
      - processes:

Now you are ready to launch secondary processes from ``pri; launch``
command, or another terminal. Here is an example for launching ``spp_nfv``
with options from ``pri; launch``. Log file of this process is created as
``log/spp_nfv1.log``.

.. code-block:: none

    spp > pri; launch nfv 1 -l 1,2 -m 512 -- -n 1 -s 127.0.0.1:6666

This ``launch`` command supports TAB completion. Parameters for ``spp_nfv``
are completed after secondary ID ``1``.

.. code-block:: none

    spp > pri; launch nfv 1

    # Press TAB
    spp > pri; launch nfv 1 -l 1,2 -m 512 -- -n 1 -s 127.0.0.1:6666


It is same as following options launching from terminal.

.. code-block:: console

    $ sudo ./src/nfv/x86_64-native-linuxapp-gcc/spp_nfv \
        -l 1,2 -n 4 -m 512 \
        --proc-type secondary \
        -- \
        -n 1 \
        -s 127.0.0.1:6666

Parameters for completion are defined in SPP CLI, and you can find
parameters with ``config`` command.

.. code-block:: none

    spp > config
    - max_secondary: "16"   # The maximum number of secondary processes
    - prompt: "spp > "  # Command prompt
    - topo_size: "60%"  # Percentage or ratio of topo
    - sec_mem: "-m 512" # Mem size
    ...

You can launch consequence secondary processes from CLI for your usage.
If you just patch two DPDK applications on host, it is enough to use one
``spp_nfv``, or use ``spp_vf`` if you need to classify packets.

.. code-block:: none

    spp > pri; launch nfv 2 -l 1,3 -m 512 -- -n 2 -s 127.0.0.1:6666
    spp > pri; launch vf 3 -l 1,4,5,6 -m 512 -- -n 3 -s 127.0.0.1:6666
    ...

If you launch processes by yourself, ``spp_primary`` must be launched
before secondary processes.
``spp-ctl`` need to be launched before SPP CLI, but no need to be launched
before other processes. SPP CLI is launched from ``spp.py``.
If ``spp-ctl`` is not running after primary and
secondary processes are launched, processes wait ``spp-ctl`` is launched.

In general, ``spp-ctl`` should be launched first, then SPP CLI and
``spp_primary`` in each of terminals without running as background process.
After ``spp_primary``, you launch secondary processes for your usage.

In the rest of this chapter is for explaining how to launch each of processes
options and usages for the all of processes.
How to connect to VMs is also described in this chapter.

How to use of these secondary processes is described as usecases
in the next chapter.


.. _spp_gsg_howto_controller:

SPP Controller
--------------

SPP Controller consists of ``spp-ctl`` and SPP CLI.

spp-ctl
~~~~~~~

``spp-ctl`` is a HTTP server for REST APIs for managing SPP
processes. In default, it is accessed with URL ``http://127.0.0.1:7777``
or ``http://localhost:7777``.
``spp-ctl`` shows no messages at first after launched, but shows
log messages for events such as receiving a request or terminating
a process.

.. code-block:: console

    # terminal 1
    $ cd /path/to/spp
    $ python3 src/spp-ctl/spp-ctl

It has a option ``-b`` for binding address explicitly to be accessed
from other than default, ``127.0.0.1`` or ``localhost``.
If you deploy SPP on multiple nodes, you might need to use ``-b`` option
it to be accessed from other processes running on other than local node.

.. code-block:: console

    # launch with URL http://192.168.1.100:7777
    $ python3 src/spp-ctl/spp-ctl -b 192.168.1.100

``spp-ctl`` is the most important process in SPP. For some usecases,
you might better to manage this process with ``systemd``.
Here is a simple example of service file for systemd.

.. code-block:: none

    [Unit]
    Description = SPP Controller

    [Service]
    ExecStart = /usr/bin/python3 /path/to/spp/src/spp-ctl/spp-ctl
    User = root

All of options can be referred with help option ``-h``.

.. code-block:: console

    python3 ./src/spp-ctl/spp-ctl -h
    usage: spp-ctl [-h] [-b BIND_ADDR] [-p PRI_PORT] [-s SEC_PORT] [-a API_PORT]

    SPP Controller

    optional arguments:
      -h, --help            show this help message and exit
      -b BIND_ADDR, --bind-addr BIND_ADDR
                            bind address, default=localhost
      -p PRI_PORT           primary port, default=5555
      -s SEC_PORT           secondary port, default=6666
      -a API_PORT           web api port, default=7777

.. _spp_setup_howto_use_spp_cli:

SPP CLI
~~~~~~~

If ``spp-ctl`` is launched, go to the next terminal and launch SPP CLI.

.. code-block:: console

    # terminal 2
    $ cd /path/to/spp
    $ python3 src/spp.py
    Welcome to the spp.   Type help or ? to list commands.

    spp >

If you launched ``spp-ctl`` with ``-b`` option, you also need to use the same
option for ``spp.py``, or failed to connect and to launch.

.. code-block:: console

    # terminal 2
    # bind to spp-ctl on http://192.168.1.100:7777
    $ python3 src/spp.py -b 192.168.1.100
    Welcome to the spp.   Type help or ? to list commands.

    spp >

One of the typical usecase of this option is to deploy multiple SPP nodes.
:numref:`figure_spp_howto_multi_spp` is an exmaple of multiple nodes case.
There are three nodes on each of which ``spp-ctl`` is running for accepting
requests for SPP. These ``spp-ctl`` processes are controlled from
``spp.py`` on host1 and all of paths are configured across the nodes.
It is also able to be configured between hosts by changing
soure or destination of phy ports.

.. _figure_spp_howto_multi_spp:

.. figure:: ../images/setup/howto_use/spp_howto_multi_spp.*
   :width: 80%

   Multiple SPP nodes

Launch SPP CLI with three entries of binding addresses with ``-b`` option
for specifying ``spp-ctl``.

.. code-block:: console

    # Launch SPP CLI with three nodes
    $ python3 src/spp.py -b 192.168.11.101 \
        -b 192.168.11.102 \
        -b 192.168.11.103 \

You can also add nodes after SPP CLI is launched.

.. code-block:: console

    # Launch SPP CLI with one node
    $ python3 src/spp.py -b 192.168.11.101
    Welcome to the SPP CLI. Type `help` or `?` to list commands.

    # Add the rest of nodes after
    spp > server add 192.168.11.102
    Registered spp-ctl "192.168.11.102:7777".
    spp > server add 192.168.11.103
    Registered spp-ctl "192.168.11.103:7777".

You find the host under the management of SPP CLI and switch with
``server`` command.

.. code-block:: none

    spp > server list
      1: 192.168.1.101:7777 *
      2: 192.168.1.102:7777
      3: 192.168.1.103:7777

To change the server, add an index number after ``server``.

.. code-block:: none

    # Launch SPP CLI
    spp > server 3
    Switch spp-ctl to "3: 192.168.1.103:7777".

All of options can be referred with help option ``-h``.

.. code-block:: console

    $ python3 src/spp.py -h
    usage: spp.py [-h] [-b BIND_ADDR] [-a API_PORT]

    SPP Controller

    optional arguments:
      -h, --help            show this help message and exit
      -b BIND_ADDR, --bind-addr BIND_ADDR
                            bind address, default=127.0.0.1:7777


All of SPP CLI commands are described in :doc:`../../commands/index`.


Default Configuration
^^^^^^^^^^^^^^^^^^^^^

SPP CLI imports several params from configuration file while launching.
Some of behaviours of SPP CLI depends on the params.
The default configuration is defined in
``src/controller/config/default.yml``.
You can change this params by editing the config file, or from ``config``
command after SPP CLI is launched.

All of config params are referred by ``config`` command.

.. code-block:: none

    # show list of config
    spp > config
    - max_secondary: "16"       # The maximum number of secondary processes
    - sec_nfv_nof_lcores: "1"   # Default num of lcores for workers of spp_nfv
    ....

To change the config, set a value for the param.
Here is an example for changing command prompt.

.. code-block:: none

    # set prompt to "$ spp "
    spp > config prompt "$ spp "
    Set prompt: "$ spp "
    $ spp


.. _spp_gsg_howto_pri:

SPP Primary
-----------

SPP primary is a resource manager and has a responsibility for
initializing EAL for secondary processes. It should be launched before
secondary.

To launch SPP primary, run ``spp_primary`` with specific options.

.. code-block:: console

    # terminal 3
    $ sudo ./src/primary/x86_64-native-linuxapp-gcc/spp_primary \
        -l 0 -n 4 \
        --socket-mem 512,512 \
        --huge-dir /dev/hugepages \
        --proc-type primary \
        --base-virtaddr 0x100000000
        -- \
        -p 0x03 \
        -n 10 \
        -s 192.168.1.100:5555

SPP primary takes EAL options and application specific options.

Core list option ``-l`` is for assigining cores and SPP primary requires just
one core. You can use core mask option ``-c`` instead of ``-l``.
For memory, this example is for reserving 512 MB on each of two NUMA nodes
hardware, so you use ``-m 1024`` simply, or ``--socket-mem 1024,0``
if you run the process on single NUMA node.

.. note::

   If you use DPDK v18.08 or before,
   you should consider give ``--base-virtaddr`` with 4 GiB or higher value
   because a secondary process is accidentally failed to mmap while init
   memory. The reason of the failure is secondary process tries to reserve
   the region which is already used by some of thread of primary.

   .. code-block:: console

      # Failed to secondary
      EAL: Could not mmap 17179869184 ... - please use '--base-virtaddr' option

   ``--base-virtaddr`` is to decide base address explicitly to avoid this
   overlapping. 4 GiB ``0x100000000`` is enough for the purpose.

   If you use DPDK v18.11 or later, ``--base-virtaddr 0x100000000`` is enabled
   in default. You need to use this option only for changing the default value.

If ``spp_primary`` is launched with two or more lcores, forwarder or monitor
is activated. The default is forwarder and monitor is optional in this case.
If you use monitor thread, additional option ``--disp-stat`` is required.
Here is an example for launching ``spp_primary`` with monitor thread.

.. code-block:: console

    # terminal 3
    $ sudo ./src/primary/x86_64-native-linuxapp-gcc/spp_primary \
        -l 0-1 -n 4 \   # two lcores
        --socket-mem 512,512 \
        --huge-dir /dev/hugepages \
        --proc-type primary \
        --base-virtaddr 0x100000000
        -- \
        -p 0x03 \
        -n 10 \
        -s 192.168.1.100:5555
        --disp-stats

Primary process sets up physical ports of given port mask with ``-p`` option
and ring ports of the number of ``-n`` option. Ports of  ``-p`` option is for
accepting incomming packets and ``-n`` option is for inter-process packet
forwarding. You can also add ports initialized with ``--vdev`` option to
physical ports. However, ports added with ``--vdev`` cannot referred from
secondary processes.

.. code-block:: console

    # terminal 3
    $ sudo ./src/primary/x86_64-native-linuxapp-gcc/spp_primary \
        -l 0 -n 4 \
        --socket-mem 512,512 \
        --huge-dir=/dev/hugepages \
        --vdev eth_vhost1,iface=/tmp/sock1  # used as 1st phy port
        --vdev eth_vhost2,iface=/tmp/sock2  # used as 2nd phy port
        --proc-type=primary \
        --base-virtaddr 0x100000000
        -- \
        -p 0x03 \
        -n 10 \
        -s 192.168.1.100:5555

- EAL options:

  - ``-l``: core list
  - ``--socket-mem``: Memory size on each of NUMA nodes.
  - ``--huge-dir``: Path of hugepage dir.
  - ``--proc-type``: Process type.
  - ``--base-virtaddr``: Specify base virtual address.
  - ``--disp-stats``: Show statistics periodically.

- Application options:

  - ``-p``: Port mask.
  - ``-n``: Number of ring PMD.
  - ``-s``: IP address of controller and port prepared for primary.


.. _spp_gsg_howto_sec:

SPP Secondary
-------------

Secondary process behaves as a client of primary process and a worker
for doing tasks for packet processing. There are several kinds of secondary
process, for example, simply forwarding between ports, classsifying packets
by referring its header or duplicate packets for redundancy.


spp_nfv
~~~~~~~

Run ``spp_nfv`` with options which simply forward packets as similar
as ``l2fwd``.

.. code-block:: console

    # terminal 4
    $ cd /path/to/spp
    $ sudo ./src/nfv/x86_64-native-linuxapp-gcc/spp_nfv \
        -l 2-3 -n 4 \
        --proc-type secondary \
        -- \
        -n 1 \
        -s 192.168.1.100:6666

EAL options are the same as primary process. Here is a list of application
options of ``spp_nfv``.

* ``-n``: Secondary ID.
* ``-s``: IP address and secondary port of spp-ctl.
* ``--vhost-client``: Enable vhost-user client mode.

Secondary ID is used to identify for sending messages and must be
unique among all of secondaries.
If you attempt to launch a secondary process with the same ID, it
is failed.

If ``--vhost-client`` option is specified, then ``vhost-user`` act as
the client, otherwise the server.
For reconnect feature from SPP to VM, ``--vhost-client`` option can be
used. This reconnect features requires QEMU 2.7 (or later).
See also `Vhost Sample Application
<http://dpdk.org/doc/guides/sample_app_ug/vhost.html>`_.


spp_vf
~~~~~~

``spp_vf`` is a kind of secondary process for classify or merge packets.

.. code-block:: console

    $ sudo ./src/vf/x86_64-native-linuxapp-gcc/spp_vf \
      -l 2-13 -n 4 \
      --proc-type secondary \
      -- \
      --client-id 1 \
      -s 192.168.1.100:6666 \
      --vhost-client

EAL options are the same as primary process. Here is a list of application
options of ``spp_vf``.

* ``--client-id``: Client ID unique among secondary processes.
* ``-s``: IPv4 address and secondary port of spp-ctl.
* ``--vhost-client``: Enable vhost-user client mode.


spp_mirror
~~~~~~~~~~

``spp_mirror`` is a kind of secondary process for duplicating packets,
and options are same as ``spp_vf``.

.. code-block:: console

    $ sudo ./src/mirror/x86_64-native-linuxapp-gcc/spp_mirror \
      -l 2,3 -n 4 \
      --proc-type secondary \
      -- \
      --client-id 1 \
      -s 192.168.1.100:6666 \
      --vhost-client

EAL options are the same as primary process. Here is a list of application
options of ``spp_mirror``.

* ``--client-id``: Client ID unique among secondary processes.
* ``-s``: IPv4 address and secondary port of spp-ctl.
* ``--vhost-client``: Enable vhost-user client mode.


.. _spp_vf_gsg_howto_use_spp_pcap:

spp_pcap
~~~~~~~~

Other than PCAP feature implemented as pcap port in ``spp_nfv``,
SPP provides ``spp_pcap`` for capturing comparatively heavy traffic.

.. code-block:: console

    $ sudo ./src/pcap/x86_64-native-linuxapp-gcc/spp_pcap \
      -l 2-5 -n 4 \
      --proc-type secondary \
      -- \
      --client-id 1 \
      -s 192.168.1.100:6666 \
      -c phy:0 \
      --out-dir /path/to/dir \
      --fsize 107374182

EAL options are the same as primary process. Here is a list of application
options of ``spp_pcap``.

* ``--client-id``: Client ID unique among secondary processes.
* ``-s``: IPv4 address and secondary port of spp-ctl.
* ``-c``: Captured port. Only ``phy`` and ``ring`` are supported.
* ``--out-dir``: Optional. Path of dir for captured file. Default is ``/tmp``.
* ``--fsize``: Optional. Maximum size of a capture file. Default is ``1GiB``.

Captured file of LZ4 is generated in ``/tmp`` by default.
The name of file is consists of timestamp, resource ID of captured port,
ID of ``writer`` threads and sequential number.
Timestamp is decided when capturing is started and formatted as
``YYYYMMDDhhmmss``.
Both of ``writer`` thread ID and sequential number are started from ``1``.
Sequential number is required for the case if the size of
captured file is reached to the maximum and another file is generated to
continue capturing.

This is an example of captured file. It consists of timestamp,
``20190214154925``, port ``phy0``, thread ID ``1`` and sequential number
``1``.

.. code-block:: none

    /tmp/spp_pcap.20190214154925.phy0.1.1.pcap.lz4

``spp_pcap`` also generates temporary files which are owned by each of
``writer`` threads until capturing is finished or the size of captured file
is reached to the maximum.
This temporary file has additional extension ``tmp`` at the end of file
name.

.. code-block:: none

    /tmp/spp_pcap.20190214154925.phy0.1.1.pcap.lz4.tmp


Launch from SPP CLI
~~~~~~~~~~~~~~~~~~~

You can launch SPP secondary processes from SPP CLI wihtout openning
other terminals. ``pri; launch`` command is for any of secondary processes
with specific options. It takes secondary type, ID and options of EAL
and application itself as similar to launching from terminal.
Here is an example of launching ``spp_nfv``. You notice that there is no
``--proc-type secondary`` which should be required for secondary.
It is added to the options by SPP CLI before launching the process.

.. code-block:: none

    # terminal 2
    # launch spp_nfv with sec ID 2
    spp > pri; launch nfv 2 -l 1,2 -m 512 -- -n 2 -s 192.168.1.100:6666
    Send request to launch nfv:2.

After running this command, you can find ``nfv:2`` is launched
successfully.

.. code-block:: none

    # terminal 2
    spp > status
    - spp-ctl:
      - address: 192.168.1.100:7777
    - primary:
      - status: running
    - secondary:
      - processes:
        1: nfv:2

Instead of displaying log messages in terminal, it outputs the messages
in a log file. All of log files of secondary processes launched with
``pri`` are located in ``log/`` directory under the project root.
The name of log file is found ``log/spp_nfv-2.log``.

.. code-block:: console

    # terminal 5
    $ tail -f log/spp_nfv-2.log
    SPP_NFV: Used lcores: 1 2
    SPP_NFV: entering main loop on lcore 2
    SPP_NFV: My ID 2 start handling message
    SPP_NFV: [Press Ctrl-C to quit ...]
    SPP_NFV: Creating socket...
    SPP_NFV: Trying to connect ... socket 24
    SPP_NFV: Connected
    SPP_NFV: Received string: _get_client_id
    SPP_NFV: token 0 = _get_client_id
    SPP_NFV: To Server: {"results":[{"result":"success"}],"client_id":2, ...


Launch SPP on VM
~~~~~~~~~~~~~~~~

To communicate DPDK application running on a VM,
it is required to create a virtual device for the VM.
In this instruction, launch a VM with qemu command and
create ``vhost-user`` and ``virtio-net-pci`` devices on the VM.

Before launching VM, you need to prepare a socket file for creating
``vhost-user`` device.
Run ``add`` command with resource UID ``vhost:0`` to create socket file.

.. code-block:: none

    # terminal 2
    spp > nfv 1; add vhost:0

In this example, it creates socket file with index 0 from ``spp_nfv`` of ID 1.
Socket file is created as ``/tmp/sock0``.
It is used as a qemu option to add vhost interface.

Launch VM with ``qemu-system-x86_64`` for x86 64bit architecture.
Qemu takes many options for defining resources including virtual
devices. You cannot use this example as it is because some options are
depend on your environment.
You should specify disk image with ``-hda``, sixth option in this
example, and ``qemu-ifup`` script for assigning an IP address for the VM
to be able to access as 12th line.

.. code-block:: console

    # terminal 5
    $ sudo qemu-system-x86_64 \
        -cpu host \
        -enable-kvm \
        -numa node,memdev=mem \
        -mem-prealloc \
        -hda /path/to/image.qcow2 \
        -m 4096 \
        -smp cores=4,threads=1,sockets=1 \
        -object \
        memory-backend-file,id=mem,size=4096M,mem-path=/dev/hugepages,share=on \
        -device e1000,netdev=net0,mac=00:AD:BE:B3:11:00 \
        -netdev tap,id=net0,ifname=net0,script=/path/to/qemu-ifup \
        -nographic \
        -chardev socket,id=chr0,path=/tmp/sock0 \  # /tmp/sock0
        -netdev vhost-user,id=net1,chardev=chr0,vhostforce \
        -device virtio-net-pci,netdev=net1,mac=00:AD:BE:B4:11:00 \
        -monitor telnet::44911,server,nowait

This VM has two network interfaces.
``-device e1000`` is a management network port
which requires ``qemu-ifup`` to activate while launching.
Management network port is used for login and setup the VM.
``-device virtio-net-pci`` is created for SPP or DPDK application
running on the VM.

``vhost-user`` is a backend of ``virtio-net-pci`` which requires
a socket file ``/tmp/sock0`` created from secondary with ``-chardev``
option.

For other options, please refer to
`QEMU User Documentation
<https://qemu.weilnetz.de/doc/qemu-doc.html>`_.

.. note::

    In general, you need to prepare several qemu images for launcing
    several VMs, but installing DPDK and SPP for several images is bother
    and time consuming.

    You can shortcut this tasks by creating a template image and copy it
    to the VMs. It is just one time for installing for template.

After VM is booted, you install DPDK and SPP in the VM as in the host.
IP address of the VM is assigned while it is created and you can find
the address in a file generated from libvirt if you use Ubuntu.

.. code-block:: console

    # terminal 5
    $ cat /var/lib/libvirt/dnsmasq/virbr0.status
    [
        {
            "ip-address": "192.168.122.100",
            ...

    # Login VM, install DPDK and SPP
    $ ssh user@192.168.122.100
    ...

It is recommended to configure ``/etc/default/grub`` for hugepages and
reboot the VM after installation.

Finally, login to the VM, bind ports to DPDK and launch ``spp-ctl``
and ``spp_primamry``.
You should add ``-b`` option to be accessed from SPP CLI on host.

.. code-block:: console

    # terminal 5
    $ ssh user@192.168.122.100
    $ cd /path/to/spp
    $ python3 src/spp-ctl/spp-ctl -b 192.168.122.100
    ...

Confirm that virtio interfaces are under the management of DPDK before
launching DPDK processes.

.. code-block:: console

    # terminal 6
    $ ssh user@192.168.122.100
    $ cd /path/to/spp
    $ sudo ./src/primary/x86_64-native-linuxapp-gcc/spp_primary \
        -l 1 -n 4 \
        -m 1024 \
        --huge-dir=/dev/hugepages \
        --proc-type=primary \
        --base-virtaddr 0x100000000
        -- \
        -p 0x03 \
        -n 6 \
        -s 192.168.122.100:5555

You can configure SPP running on the VM from SPP CLI.
Use ``server`` command to switch node under the management.

.. code-block:: none

    # terminal 2
    # show list of spp-ctl nodes
    spp > server
    1: 192.168.1.100:7777 *
    2: 192.168.122.100:7777

    # change node under the management
    spp > server 2
    Switch spp-ctl to "2: 192.168.122.100:7777".

    # confirm node is switched
    spp > server
    1: 192.168.1.100:7777
    2: 192.168.122.100:7777 *

    # configure SPP on VM
    spp > status
    ...

Now, you are ready to setup your network environment for DPDK and non-DPDK
applications with SPP.
SPP enables users to configure service function chaining between applications
running on host and VMs.
Usecases of network configuration are explained in the next chapter.


.. _spp_gsg_howto_virsh:

Using virsh
~~~~~~~~~~~

First of all, please check version of qemu.

.. code-block:: console

    $ qemu-system-x86_64 --version

You should install qemu 2.7 or higher for using vhost-user client mode.
Refer `instruction
<https://wiki.qemu.org/index.php/Hosts/Linux>`_
to install.

``virsh`` is a command line interface that can be used to create, destroy,
stop start and edit VMs and configure.

You also need to install following packages to run ``virt-install``.

* libvirt-bin
* virtinst
* bridge-utils

virt-install
^^^^^^^^^^^^

Install OS image with ``virt-install`` command.
``--location`` is a URL of installer. Use Ubuntu 16.04 for amd64 in this
case.

.. code-block:: none

    http://archive.ubuntu.com/ubuntu/dists/xenial/main/installer-amd64/

This is an example of ``virt-install``.

.. code-block:: console

   virt-install \
   --name VM_NAME \
   --ram 4096 \
   --disk path=/var/lib/libvirt/images/VM_NAME.img,size=30 \
   --vcpus 4 \
   --os-type linux \
   --os-variant ubuntu16.04 \
   --network network=default \
   --graphics none \
   --console pty,target_type=serial \
   --location 'http://archive.ubuntu.com/ubuntu/dists/xenial/main/...'
   --extra-args 'console=ttyS0,115200n8 serial'

You might need to enable serial console as following.

.. code-block:: console

    $sudo systemctl enable serial-getty@ttyS0.service
    $sudo systemctl start serial-getty@ttyS0.service


Edit Config
^^^^^^^^^^^

Edit configuration of VM with virsh command. The name of VMs are found from
``virsh list``.

.. code-block:: console

    # Find the name of VM
    $ sudo virsh list --all

    $ sudo virsh edit VM_NAME

You need to define namespace ``qemu`` to use tags such as
``<qemu:commandline>``.

.. code-block:: none

    xmlns:qemu='http://libvirt.org/schemas/domain/qemu/1.0'

In addition, you need to add attributes for specific resources for DPDK and SPP.

* ``<memoryBacking>``
* ``<qemu:commandline>``

Take care about the index numbers of devices should be the same value such as
``chr0`` or ``sock0`` in ``virtio-net-pci`` device. This index is referred as
ID of vhost port from SPP. MAC address defined in the attribute is used while
registering destinations for classifier's table.

.. code-block:: xml

    <qemu:arg value='virtio-net-pci,netdev=vhost-net0,mac=52:54:00:12:34:56'/>


Here is an example of XML config for using with SPP.

.. code-block:: xml

    <domain type='kvm' xmlns:qemu='http://libvirt.org/schemas/domain/qemu/1.0'>
      <name>spp-vm1</name>
      <uuid>d90f5420-861a-4479-8559-62d7a1545cb9</uuid>
      <memory unit='KiB'>4194304</memory>
      <currentMemory unit='KiB'>4194304</currentMemory>
      <memoryBacking>
        <hugepages/>
      </memoryBacking>
      <vcpu placement='static'>4</vcpu>
      <os>
        <type arch='x86_64' machine='pc-i440fx-2.3'>hvm</type>
        <boot dev='hd'/>
      </os>
      <features>
        <acpi/>
        <apic/>
        <pae/>
      </features>
      <clock offset='utc'/>
      <on_poweroff>destroy</on_poweroff>
      <on_reboot>restart</on_reboot>
      <on_crash>restart</on_crash>
      <devices>
        <emulator>/usr/local/bin/qemu-system-x86_64</emulator>
        <disk type='file' device='disk'>
          <driver name='qemu' type='raw'/>
          <source file='/var/lib/libvirt/images/spp-vm1.qcow2'/>
          <target dev='hda' bus='ide'/>
          <address type='drive' controller='0' bus='0' target='0' unit='0'/>
        </disk>
        <disk type='block' device='cdrom'>
          <driver name='qemu' type='raw'/>
          <target dev='hdc' bus='ide'/>
          <readonly/>
          <address type='drive' controller='0' bus='1' target='0' unit='0'/>
        </disk>
        <controller type='usb' index='0'>
          <address type='pci' domain='0x0000' bus='0x00' slot='0x01'
          function='0x2'/>
        </controller>
        <controller type='pci' index='0' model='pci-root'/>
        <controller type='ide' index='0'>
          <address type='pci' domain='0x0000' bus='0x00' slot='0x01'
          function='0x1'/>
        </controller>
        <interface type='network'>
          <mac address='52:54:00:99:aa:7f'/>
          <source network='default'/>
          <model type='rtl8139'/>
          <address type='pci' domain='0x0000' bus='0x00' slot='0x02'
          function='0x0'/>
        </interface>
        <serial type='pty'>
          <target type='isa-serial' port='0'/>
        </serial>
        <console type='pty'>
          <target type='serial' port='0'/>
        </console>
        <memballoon model='virtio'>
          <address type='pci' domain='0x0000' bus='0x00' slot='0x03'
          function='0x0'/>
        </memballoon>
      </devices>
      <qemu:commandline>
        <qemu:arg value='-cpu'/>
        <qemu:arg value='host'/>
        <qemu:arg value='-object'/>
        <qemu:arg
        value='memory-backend-file,id=mem,size=4096M,mem-path=/run/hugepages/kvm,share=on'/>
        <qemu:arg value='-numa'/>
        <qemu:arg value='node,memdev=mem'/>
        <qemu:arg value='-mem-prealloc'/>
        <qemu:arg value='-chardev'/>
        <qemu:arg value='socket,id=chr0,path=/tmp/sock0,server'/>
        <qemu:arg value='-device'/>
        <qemu:arg
        value='virtio-net-pci,netdev=vhost-net0,mac=52:54:00:12:34:56'/>
        <qemu:arg value='-netdev'/>
        <qemu:arg value='vhost-user,id=vhost-net0,chardev=chr0,vhostforce'/>
        <qemu:arg value='-chardev'/>
        <qemu:arg value='socket,id=chr1,path=/tmp/sock1,server'/>
        <qemu:arg value='-device'/>
        <qemu:arg
        value='virtio-net-pci,netdev=vhost-net1,mac=52:54:00:12:34:57'/>
        <qemu:arg value='-netdev'/>
        <qemu:arg value='vhost-user,id=vhost-net1,chardev=chr1,vhostforce'/>
      </qemu:commandline>
    </domain>


Launch VM
^^^^^^^^^

After updating XML configuration, launch VM with ``virsh start``.

.. code-block:: console

    $ virsh start VM_NAME

It is required to add network configurations for processes running on the VMs.
If this configuration is skipped, processes cannot communicate with others
via SPP.

On the VMs, add an interface and disable offload.

.. code-block:: console

    # Add interface
    $ sudo ifconfig IF_NAME inet IPADDR netmask NETMASK up

    # Disable offload
    $ sudo ethtool -K IF_NAME tx off

On host machine, it is also required to disable offload.

.. code-block:: console

    # Disable offload for VM
    $ sudo ethtool -K IF_NAME tx off
