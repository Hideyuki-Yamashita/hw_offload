..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2017-2018 Nippon Telegraph and Telephone Corporation

.. _spp_container_app_launcher:

App Container Launchers
=======================

App container launcher is a python script for running SPP or DPDK
application on a container.
As described by name, for instance, ``pktgen.py`` launches pktgen-dpdk
inside a container.


.. code-block:: console

    $ tree app/
    app/
    ...
    |--- helloworld.py
    |--- l2fwd.py
    |--- l3fwd.py
    |--- l3fwd-acl.py
    |--- load-balancer.py
    |--- pktgen.py
    |--- spp-nfv.py
    |--- spp-primary.py
    ---- testpmd.py


.. note::

    As described in
    :ref:`sppc_gs_build_docker_imgs`
    section, you had better to use Ubuntu 16.04 with
    ``--dist-ver`` option because SPP container is not stable for running
    on the latest version.

    Please notice that examples in this section does not use ``dist-ver``
    options explicitly for simplicity.


.. _sppc_appl_setup:

Setup
-----

You should define ``SPP_CTL_IP`` environment variable to SPP controller
be accessed from other SPP processes inside containers.
SPP controller is a CLI tool for accepting user's commands.

You cannot use ``127.0.0.1`` or ``localhost`` for ``SPP_CTL_IP``
because SPP processes try to find SPP controller inside each of
containers and fail to.
From inside of the container, SPP processes should be known IP address
other than ``127.0.0.1`` or ``localhost``
of host on which SPP controller running.

SPP controller should be launched before other SPP processes.

.. code-block:: console

    $ cd /path/to/spp
    $ python src/spp.py


.. _sppc_appl_spp_primary:

SPP Primary Container
---------------------

SPP primary process is launched from ``app/spp-primary.py`` as an
app container.
It manages resources on host from inside the container.
``app/spp-primary.py`` calls ``docker run`` with
``-v`` option to mount hugepages and other devices in the container
to share them between host and containers.

SPP primary process is launched as foreground or background mode.
You can show statistics of packet forwarding if you launch it with
two cores and in foreground mode.
In this case, SPP primary uses one for resource management and
another one for showing statistics.
If you need to minimize the usage of cores, or are not interested in
the statistics,
you should give just one core and run in background mode.
If you run SPP primary in foreground mode with one core,
it shows log messages which is also referred in syslog.

Here is an example for launching SPP primary with core list 0-1 in
foreground mode.
You should give portmask opiton ``-p`` because SPP primary requires
at least one port, or failed to launch.
This example is assumed that host machine has two or more
physical ports.

.. code-block:: console

    $ cd /path/to/spp/tools/sppc
    $ python app/spp-primary -l 0-1 -p 0x03 -fg

It is another example with one core and two ports in background mode.

.. code-block:: console

    $ python app/spp-primary -l 0 -p 0x03

SPP primary is able to run with virtual devices instead of
physical NICs for a case
you do not have dedicated NICs for DPDK.
SPP container supports two types of virtual device with options.

* ``--dev-tap-ids`` or ``-dt``:  Add TAP devices
* ``--dev-vhost-ids`` or ``-dv``: Add vhost devices

.. code-block:: console

    $ python app/spp-primary -l 0 -dt 1,2 -p 0x03



If you need to inspect a docker command without launching
a container, use ``--dry-run`` option.
It composes docker command and just display it without running the
docker command.

You refer all of options with ``-h`` option.
Options of app container scripts are categorized four types.
First one is EAL option, for example ``-l``, ``-c`` or ``-m``.
Second one is app container option which is a common set of options for
app containers connected with SPP. So, containers of SPP processes do
not have app container option.
Third one is application specific option. In this case,
``-n``, ``-p`` or ``-ip``.
Final one is container option, for example ``--dist-name`` or
``--ci``.
EAL options and container options are common for all of app container
scripts.
On the other hand, application specific options are different each other.

.. code-block:: console

    $ python app/spp-primary.py -h
    usage: spp-primary.py [-h] [-l CORE_LIST] [-c CORE_MASK] [-m MEM]
                          [--socket-mem SOCKET_MEM]
                          [-b [PCI_BLACKLIST [PCI_BLACKLIST ...]]]
                          [-w [PCI_WHITELIST [PCI_WHITELIST ...]]]
                          [--single-file-segment]
                          [--nof-memchan NOF_MEMCHAN] [-n NOF_RING]
                          [-p PORT_MASK]
                          [-dv DEV_VHOST_IDS] [-dt DEV_TAP_IDS] [-ip CTRL_IP]
                          [--ctrl-port CTRL_PORT] [--dist-name DIST_NAME]
                          [--dist-ver DIST_VER] [--workdir WORKDIR]
                          [-ci CONTAINER_IMAGE] [-fg] [--dry-run]

    Launcher for spp-primary application container

    optional arguments:
      -h, --help            show this help message and exit
      -l CORE_LIST, --core-list CORE_LIST
                            Core list
      -c CORE_MASK, --core-mask CORE_MASK
                            Core mask
      -m MEM, --mem MEM     Memory size (default is 1024)
      --socket-mem SOCKET_MEM
                            Memory size
      -b [PCI_BLACKLIST [PCI_BLACKLIST ...]], --pci-blacklist [PCI_BLACKLIST..
                            PCI blacklist for excluding devices
      -w [PCI_WHITELIST [PCI_WHITELIST ...]], --pci-whitelist [PCI_WHITELIST..
                            PCI whitelist for including devices
      --single-file-segments
                            Create fewer files in hugetlbfs (non-legacy mode
                            only).
      --nof-memchan NOF_MEMCHAN
                            Number of memory channels (default is 4)
      -n NOF_RING, --nof-ring NOF_RING
                            Maximum number of Ring PMD
      -p PORT_MASK, --port-mask PORT_MASK
                            Port mask
      -dv DEV_VHOST_IDS, --dev-vhost-ids DEV_VHOST_IDS
                            vhost device IDs
      -dt DEV_TAP_IDS, --dev-tap-ids DEV_TAP_IDS
                            TAP device IDs
      -ip CTRL_IP, --ctrl-ip CTRL_IP
                            IP address of SPP controller
      --ctrl-port CTRL_PORT
                            Port of SPP controller
      --dist-name DIST_NAME
                            Name of Linux distribution
      --dist-ver DIST_VER   Version of Linux distribution
      --workdir WORKDIR     Path of directory in which the command is launched
      -ci CONTAINER_IMAGE, --container-image CONTAINER_IMAGE
                            Name of container image
      -fg, --foreground     Run container as foreground mode
      --dry-run             Only print matrix, do not run, and exit


.. _sppc_appl_spp_secondary:

SPP Secondary Container
-----------------------

In SPP, there are three types of secondary process, ``spp_nfv``,
``spp_vf`` or so.
However, SPP container does only support ``spp_nfv`` currently.

``spp-nfv.py`` launches ``spp_nfv`` as an app container and requires
options for secondary ID and core list (or core mask).

.. code-block:: console

    $ cd /path/to/spp/tools/sppc
    $ python app/spp-nfv.py -i 1 -l 2-3

Refer help for all of options and usges.
It shows only application specific options for simplicity.


.. code-block:: console

    $ python app/spp-nfv.py -h
    usage: spp-nfv.py [-h] [-l CORE_LIST] [-c CORE_MASK] [-m MEM]
                      [--socket-mem SOCKET_MEM] [--nof-memchan NOF_MEMCHAN]
                      [-b [PCI_BLACKLIST [PCI_BLACKLIST ...]]]
                      [-w [PCI_WHITELIST [PCI_WHITELIST ...]]]
                      [--single-file-segment]
                      [-i SEC_ID] [-ip CTRL_IP] [--ctrl-port CTRL_PORT]
                      [--dist-name DIST_NAME] [--dist-ver DIST_VER]
                      [-ci CONTAINER_IMAGE] [-fg] [--dry-run]

    Launcher for spp-nfv application container

    optional arguments:
      ...
      -i SEC_ID, --sec-id SEC_ID
                            Secondary ID
      -ip CTRL_IP, --ctrl-ip CTRL_IP
                            IP address of SPP controller
      --ctrl-port CTRL_PORT
                            Port of SPP controller
      ...


.. _sppc_appl_l2fwd:

L2fwd Container
---------------

``app/l2fwd.py`` is a launcher script for DPDK ``l2fwd`` sample
application.
It launches ``l2fwd`` on a container with specified
vhost interfaces.

This is an example for launching with two cores (6-7th cores) with
``-l`` and two vhost interfaces with ``-d``.
``l2fwd`` requires ``--port-mask`` or ``-p`` option and the number of
ports should be even number.

.. code-block:: console

    $ cd /path/to/spp/tools/sppc
    $ python app/l2fwd.py -l 6-7 -d 1,2 -p 0x03 -fg
    ...

Refer help for all of options and usges.
It includes app container options, for example ``-d`` for vhost devices
and ``-nq`` for the number of queues of virtio, because ``l2fwd`` is not
a SPP process.
It shows options without of EAL and container for simplicity.

.. code-block:: console

    $ python app/l2fwd.py -h
    usage: l2fwd.py [-h] [-l CORE_LIST] [-c CORE_MASK] [-m MEM]
                    [--socket-mem SOCKET_MEM] [--nof-memchan NOF_MEMCHAN]
                    [-b [PCI_BLACKLIST [PCI_BLACKLIST ...]]]
                    [--single-file-segment]
                    [-w [PCI_WHITELIST [PCI_WHITELIST ...]]]
                    [-d DEV_IDS] [-nq NOF_QUEUES] [--no-privileged]
                    [-p PORT_MASK]
                    [--dist-name DIST_NAME] [--dist-ver DIST_VER]
                    [-ci CONTAINER_IMAGE] [-fg] [--dry-run]

    Launcher for l2fwd application container

    optional arguments:
      ...
      -d DEV_IDS, --dev-ids DEV_IDS
                            two or more even vhost device IDs
      -nq NOF_QUEUES, --nof-queues NOF_QUEUES
                            Number of queues of virtio (default is 1)
      --no-privileged       Disable docker's privileged mode if it's needed
      -p PORT_MASK, --port-mask PORT_MASK
                            Port mask
      ...


.. _sppc_appl_l3fwd:

L3fwd Container
---------------

`L3fwd
<https://dpdk.org/doc/guides/sample_app_ug/l3_forward.html>`_
application is a simple example of packet processing
using the DPDK.
Differed from l2fwd, the forwarding decision is made based on
information read from input packet.
This application provides LPM (longest prefix match) or
EM (exact match) methods for packet classification.

``app/l3fwd.py`` launches l3fwd on a container.
As similar to ``l3fwd`` application, this python script takes several
options other than EAL for port configurations and classification methods.
The mandatory options for the application are ``-p`` for portmask
and ``--config`` for rx as a set of combination of
``(port, queue, locre)``.

Here is an example for launching l3fwd app container with two vhost
interfaces and printed log messages.
There are two rx ports. ``(0,0,1)`` is for queue of port 0 for which
lcore 1 is assigned, and ``(1,0,2)`` is for port 1.
In this case, you should add ``-nq`` option because the number of both
of rx and tx queues are two while the default number of virtio device
is one.
The number of tx queues, is two in this case, is decided to be the same
value as the number of lcores.
In ``--vdev`` option setup in the script, the number of queues is
defined as ``virtio_...,queues=2,...``.

.. code-block:: console

    $ cd /path/to/spp/tools/sppc
    $ python app/l3fwd.py -l 1-2 -nq 2 -d 1,2 \
      -p 0x03 --config="(0,0,1),(1,0,2)" -fg
      sudo docker run \
      -it \
      ...
      --vdev virtio_user1,queues=2,path=/var/run/usvhost1 \
      --vdev virtio_user2,queues=2,path=/var/run/usvhost2 \
      --file-prefix spp-l3fwd-container1 \
      -- \
      -p 0x03 \
      --config "(0,0,8),(1,0,9)" \
      --parse-ptype ipv4
      EAL: Detected 16 lcore(s)
      EAL: Auto-detected process type: PRIMARY
      EAL: Multi-process socket /var/run/.spp-l3fwd-container1_unix
      EAL: Probing VFIO support...
      soft parse-ptype is enabled
      LPM or EM none selected, default LPM on
      Initializing port 0 ... Creating queues: nb_rxq=1 nb_txq=2...
      LPM: Adding route 0x01010100 / 24 (0)
      LPM: Adding route 0x02010100 / 24 (1)
      LPM: Adding route IPV6 / 48 (0)
      LPM: Adding route IPV6 / 48 (1)
      txq=8,0,0 txq=9,1,0
      Initializing port 1 ... Creating queues: nb_rxq=1 nb_txq=2...

      Initializing rx queues on lcore 8 ... rxq=0,0,0
      Initializing rx queues on lcore 9 ... rxq=1,0,0
      ...

You can increase lcores more than the number of ports, for instance,
four lcores for two ports.
However, remaining 3rd and 4th lcores do nothing and require
``-nq 4`` for tx queues.

Default classification rule is ``LPM`` and the routing table is defined
in ``dpdk/examples/l3fwd/l3fwd_lpm.c`` as below.

.. code-block:: c

    static struct ipv4_l3fwd_lpm_route ipv4_l3fwd_lpm_route_array[] = {
            {IPv4(1, 1, 1, 0), 24, 0},
            {IPv4(2, 1, 1, 0), 24, 1},
            {IPv4(3, 1, 1, 0), 24, 2},
            {IPv4(4, 1, 1, 0), 24, 3},
            {IPv4(5, 1, 1, 0), 24, 4},
            {IPv4(6, 1, 1, 0), 24, 5},
            {IPv4(7, 1, 1, 0), 24, 6},
            {IPv4(8, 1, 1, 0), 24, 7},
    };


Refer help for all of options and usges.
It shows options without of EAL and container for simplicity.

.. code-block:: console

    $ python app/l3fwd.py -h
    usage: l3fwd.py [-h] [-l CORE_LIST] [-c CORE_MASK] [-m MEM]
                    [--socket-mem SOCKET_MEM] [--nof-memchan NOF_MEMCHAN]
                    [-b [PCI_BLACKLIST [PCI_BLACKLIST ...]]]
                    [-w [PCI_WHITELIST [PCI_WHITELIST ...]]]
                    [--single-file-segment]
                    [-d DEV_IDS] [-nq NOF_QUEUES] [--no-privileged]
                    [-p PORT_MASK] [--config CONFIG] [-P] [-E] [-L]
                    [-dst [ETH_DEST [ETH_DEST ...]]] [--enable-jumbo]
                    [--max-pkt-len MAX_PKT_LEN] [--no-numa]
                    [--hash-entry-num] [--ipv6] [--parse-ptype PARSE_PTYPE]
                    [--dist-name DIST_NAME] [--dist-ver DIST_VER]
                    [-ci CONTAINER_IMAGE] [-fg] [--dry-run]

    Launcher for l3fwd application container

    optional arguments:
      ...
      -d DEV_IDS, --dev-ids DEV_IDS
                            two or more even vhost device IDs
      -nq NOF_QUEUES, --nof-queues NOF_QUEUES
                            Number of queues of virtio (default is 1)
      --no-privileged       Disable docker's privileged mode if it's needed
      -p PORT_MASK, --port-mask PORT_MASK
                            (Mandatory) Port mask
      --config CONFIG       (Mandatory) Define set of port, queue, lcore for
                            ports
      -P, --promiscous      Set all ports to promiscous mode (default is None)
      -E, --exact-match     Enable exact match (default is None)
      -L, --longest-prefix-match
                            Enable longest prefix match (default is None)
      -dst [ETH_DEST [ETH_DEST ...]], --eth-dest [ETH_DEST [ETH_DEST ...]]
                            Ethernet dest for port X (X,MM:MM:MM:MM:MM:MM)
      --enable-jumbo        Enable jumbo frames, [--enable-jumbo [--max-pkt-len
                            PKTLEN]]
      --max-pkt-len MAX_PKT_LEN
                            Max packet length (64-9600) if jumbo is enabled.
      --no-numa             Disable NUMA awareness (default is None)
      --hash-entry-num      Specify the hash entry number in hexadecimal
                            (default is None)
      --ipv6                Specify the hash entry number in hexadecimal
                            (default is None)
      --parse-ptype PARSE_PTYPE
                            Set analyze packet type, ipv4 or ipv6 (default is
                            ipv4)
      ...


.. _sppc_appl_l3fwd_acl:

L3fwd-acl Container
-------------------

`L3 Forwarding with Access Control
<https://doc.dpdk.org/guides/sample_app_ug/l3_forward_access_ctrl.html>`_
application is a simple example of packet processing using the DPDK.
The application performs a security check on received packets.
Packets that are in the Access Control List (ACL), which is loaded
during initialization, are dropped. Others are forwarded to the correct
port.

``app/l3fwd-acl.py`` launches l3fwd-acl on a container.
As similar to ``l3fwd-acl``, this python script takes several options
other than EAL for port configurations and rules.
The mandatory options for the application are ``-p`` for portmask
and ``--config`` for rx as a set of combination of
``(port, queue, locre)``.

Here is an example for launching l3fwd app container with two vhost
interfaces and printed log messages.
There are two rx ports. ``(0,0,1)`` is for queue of port 0 for which
lcore 1 is assigned, and ``(1,0,2)`` is for port 1.
In this case, you should add ``-nq`` option because the number of both
of rx and tx queues are two while the default number of virtio device
is one.
The number of tx queues, is two in this case, is decided to be the same
value as the number of lcores.
In ``--vdev`` option setup in the script, the number of queues is
defined as ``virtio_...,queues=2,...``.

.. code-block:: console

    $ cd /path/to/spp/tools/sppc
    $ python app/l3fwd-acl.py -l 1-2 -nq 2 -d 1,2 \
      --rule_ipv4="./rule_ipv4.db" -- rule_ipv6="./rule_ipv6.db" --scalar \
      -p 0x03 --config="(0,0,1),(1,0,2)" -fg
      sudo docker run \
      -it \
      ...
      --vdev virtio_user1,queues=2,path=/var/run/usvhost1 \
      --vdev virtio_user2,queues=2,path=/var/run/usvhost2 \
      --file-prefix spp-l3fwd-container1 \
      -- \
      -p 0x03 \
      --config "(0,0,8),(1,0,9)" \
      --rule_ipv4="./rule_ipv4.db" \
      --rule_ipv6="./rule_ipv6.db" \
      --scalar
      EAL: Detected 16 lcore(s)
      EAL: Auto-detected process type: PRIMARY
      EAL: Multi-process socket /var/run/.spp-l3fwd-container1_unix
      EAL: Probing VFIO support...
      soft parse-ptype is enabled
      LPM or EM none selected, default LPM on
      Initializing port 0 ... Creating queues: nb_rxq=1 nb_txq=2...
      LPM: Adding route 0x01010100 / 24 (0)
      LPM: Adding route 0x02010100 / 24 (1)
      LPM: Adding route IPV6 / 48 (0)
      LPM: Adding route IPV6 / 48 (1)
      txq=8,0,0 txq=9,1,0
      Initializing port 1 ... Creating queues: nb_rxq=1 nb_txq=2...

      Initializing rx queues on lcore 8 ... rxq=0,0,0
      Initializing rx queues on lcore 9 ... rxq=1,0,0
      ...

You can increase lcores more than the number of ports, for instance,
four lcores for two ports.
However, remaining 3rd and 4th lcores do nothing and require
``-nq 4`` for tx queues.

Refer help for all of options and usges.
It shows options without of EAL and container for simplicity.

.. code-block:: console

    $ python app/l3fwd-acl.py -h
    usage: l3fwd-acl.py [-h] [-l CORE_LIST] [-c CORE_MASK] [-m MEM]
                        [--socket-mem SOCKET_MEM]
                        [-b [PCI_BLACKLIST [PCI_BLACKLIST ...]]]
                        [-w [PCI_WHITELIST [PCI_WHITELIST ...]]]
                        [--single-file-segment] [--nof-memchan NOF_MEMCHAN]
                        [-d DEV_IDS] [-nq NOF_QUEUES] [--no-privileged]
                        [-p PORT_MASK] [--config CONFIG] [-P]
                        [--rule_ipv4 RULE_IPV4] [--rule_ipv6 RULE_IPV6]
                        [--scalar] [--enable-jumbo]
                        [--max-pkt-len MAX_PKT_LEN] [--no-numa]
                        [--dist-name DIST_NAME] [--dist-ver DIST_VER]
                        [--workdir WORKDIR] [-ci CONTAINER_IMAGE] [-fg]
                        [--dry-run]

    Launcher for l3fwd-acl application container

    optional arguments:
      ...
      -d DEV_IDS, --dev-ids DEV_IDS
                            two or more even vhost device IDs
      -nq NOF_QUEUES, --nof-queues NOF_QUEUES
                            Number of queues of virtio (default is 1)
      --no-privileged       Disable docker's privileged mode if it's needed
      -p PORT_MASK, --port-mask PORT_MASK
                            (Mandatory) Port mask
      --config CONFIG       (Mandatory) Define set of port, queue, lcore for
                            ports
      -P, --promiscous      Set all ports to promiscous mode (default is None)
      --rule_ipv4 RULE_IPV4
                            Specifies the IPv4 ACL and route rules file
      --rule_ipv6 RULE_IPV6
                            Specifies the IPv6 ACL and route rules file
      --scalar              Use a scalar function to perform rule lookup
      --enable-jumbo        Enable jumbo frames, [--enable-jumbo [--max-pkt-len
                            PKTLEN]]
      --max-pkt-len MAX_PKT_LEN
                            Max packet length (64-9600) if jumbo is enabled.
      --no-numa             Disable NUMA awareness (default is None)
      ...


.. _sppc_appl_testpmd:

Testpmd Container
-----------------

``testpmd.py`` is a launcher script for DPDK's
`testpmd
<https://dpdk.org/doc/guides/testpmd_app_ug/index.html>`_
application.

It launches ``testpmd`` inside a container with specified
vhost interfaces.

This is an example for launching with three cores (6-8th cores)
and two vhost interfaces.
This example is for launching ``testpmd`` in interactive mode.

.. code-block:: console

    $ cd /path/to/spp/tools/sppc
    $ python app/testpmd.py -l 6-8 -d 1,2 -fg -i
    sudo docker run \
     ...
     -- \
     --interactive
     ...
     Checking link statuses...
     Done
     testpmd>

Testpmd has many useful options. Please refer to
`Running the Application
<https://dpdk.org/doc/guides/testpmd_app_ug/run_app.html>`_
section for instructions.

.. note::
    ``testpmd.py`` does not support all of options of testpmd currently.
    You can find all of options with ``-h`` option, but some of them
    is not implemented. If you run testpmd with not supported option,
    It just prints an error message to mention.

    .. code-block:: console

        $ python app/testpmd.py -l 1,2 -d 1,2 --port-topology=chained
        Error: '--port-topology' is not supported yet


Refer help for all of options and usges.
It shows options without of EAL and container.

.. code-block:: console

    $ python app/testpmd.py -h
    usage: testpmd.py [-h] [-l CORE_LIST] [-c CORE_MASK] [-m MEM]
                      [--socket-mem SOCKET_MEM] [--nof-memchan NOF_MEMCHAN]
                      [-b [PCI_BLACKLIST [PCI_BLACKLIST ...]]]
                      [-w [PCI_WHITELIST [PCI_WHITELIST ...]]]
                      [--single-file-segment]
                      [-d DEV_IDS] [-nq NOF_QUEUES] [--no-privileged]
                      [--pci] [-i] [-a] [--tx-first]
                      [--stats-period STATS_PERIOD]
                      [--nb-cores NB_CORES] [--coremask COREMASK]
                      [--portmask PORTMASK] [--no-numa]
                      [--port-numa-config PORT_NUMA_CONFIG]
                      [--ring-numa-config RING_NUMA_CONFIG]
                      [--socket-num SOCKET_NUM] [--mbuf-size MBUF_SIZE]
                      [--total-num-mbufs TOTAL_NUM_MBUFS]
                      [--max-pkt-len MAX_PKT_LEN]
                      [--eth-peers-configfile ETH_PEERS_CONFIGFILE]
                      [--eth-peer ETH_PEER] [--pkt-filter-mode PKT_FILTER_MODE]
                      [--pkt-filter-report-hash PKT_FILTER_REPORT_HASH]
                      [--pkt-filter-size PKT_FILTER_SIZE]
                      [--pkt-filter-flexbytes-offset PKT_FILTER_FLEXBYTES...]
                      [--pkt-filter-drop-queue PKT_FILTER_DROP_QUEUE]
                      [--disable-crc-strip] [--enable-lro] [--enable-rx-cksum]
                      [--enable-scatter] [--enable-hw-vlan]
                      [--enable-hw-vlan-filter] [--enable-hw-vlan-strip]
                      [--enable-hw-vlan-extend] [--enable-drop-en]
                      [--disable-rss] [--port-topology PORT_TOPOLOGY]
                      [--forward-mode FORWARD_MODE] [--rss-ip] [--rss-udp]
                      [--rxq RXQ] [--rxd RXD] [--txq TXQ] [--txd TXD]
                      [--burst BURST] [--mbcache MBCACHE] [--rxpt RXPT]
                      [--rxht RXHT] [--rxfreet RXFREET] [--rxwt RXWT]
                      [--txpt TXPT] [--txht TXHT] [--txwt TXWT]
                      [--txfreet TXFREET] [--txrst TXRST]
                      [--rx-queue-stats-mapping RX_QUEUE_STATS_MAPPING]
                      [--tx-queue-stats-mapping TX_QUEUE_STATS_MAPPING]
                      [--no-flush-rx] [--txpkts TXPKTS] [--disable-link-check]
                      [--no-lsc-interrupt] [--no-rmv-interrupt]
                      [--bitrate-stats [BITRATE_STATS [BITRATE_STATS ...]]]
                      [--print-event PRINT_EVENT] [--mask-event MASK_EVENT]
                      [--flow-isolate-all] [--tx-offloads TX_OFFLOADS]
                      [--hot-plug] [--vxlan-gpe-port VXLAN_GPE_PORT]
                      [--mlockall] [--no-mlockall] [--dist-name DIST_NAME]
                      [--dist-ver DIST_VER] [-ci CONTAINER_IMAGE] [-fg]
                      [--dry-run]

    Launcher for testpmd application container

    optional arguments:
      ...
      -d DEV_IDS, --dev-ids DEV_IDS
                            two or more even vhost device IDs
      -nq NOF_QUEUES, --nof-queues NOF_QUEUES
                            Number of queues of virtio (default is 1)
      --no-privileged       Disable docker's privileged mode if it's needed
      --pci                 Enable PCI (default is None)
      -i, --interactive     Run in interactive mode (default is None)
      -a, --auto-start      Start forwarding on initialization (default ...)
      --tx-first            Start forwarding, after sending a burst of packets
                            first
      --stats-period STATS_PERIOD
                            Period of displaying stats, if interactive is
                            disabled
      --nb-cores NB_CORES   Number of forwarding cores
      --coremask COREMASK   Hexadecimal bitmask of the cores, do not include
                            master lcore
      --portmask PORTMASK   Hexadecimal bitmask of the ports
      --no-numa             Disable NUMA-aware allocation of RX/TX rings and RX
                            mbuf
      --port-numa-config PORT_NUMA_CONFIG
                            Specify port allocation as
                            (port,socket)[,(port,socket)]
      --ring-numa-config RING_NUMA_CONFIG
                            Specify ring allocation as
                            (port,flag,socket)[,(port,flag,socket)]
      --socket-num SOCKET_NUM
                            Socket from which all memory is allocated in NUMA
                            mode
      --mbuf-size MBUF_SIZE
                            Size of mbufs used to N (< 65536) bytes (default is
                            2048)
      --total-num-mbufs TOTAL_NUM_MBUFS
                            Number of mbufs allocated in mbuf pools, N > 1024.
      --max-pkt-len MAX_PKT_LEN
                            Maximum packet size to N (>= 64) bytes (default is
                            1518)
      --eth-peers-configfile ETH_PEERS_CONFIGFILE
                            Config file of Ether addrs of the peer ports
      --eth-peer ETH_PEER   Set MAC addr of port N as 'N,XX:XX:XX:XX:XX:XX'
      --pkt-filter-mode PKT_FILTER_MODE
                            Flow Director mode, 'none'(default), 'signature' or
                            'perfect'
      --pkt-filter-report-hash PKT_FILTER_REPORT_HASH
                            Flow Director hash match mode, 'none',
                            'match'(default) or 'always'
      --pkt-filter-size PKT_FILTER_SIZE
                            Flow Director memory size ('64K', '128K', '256K').
                            The default is 64K.
      --pkt-filter-flexbytes-offset PKT_FILTER_FLEXBYTES_OFFSET
                            Flexbytes offset (0-32, default is 0x6) defined in
                            words counted from the first byte of the dest MAC
                            address
      --pkt-filter-drop-queue PKT_FILTER_DROP_QUEUE
                            Set the drop-queue (default is 127)
      --disable-crc-strip   Disable hardware CRC stripping
      --enable-lro          Enable large receive offload
      --enable-rx-cksum     Enable hardware RX checksum offload
      --enable-scatter      Enable scatter (multi-segment) RX
      --enable-hw-vlan      Enable hardware vlan (default is None)
      --enable-hw-vlan-filter
                            Enable hardware VLAN filter
      --enable-hw-vlan-strip
                            Enable hardware VLAN strip
      --enable-hw-vlan-extend
                            Enable hardware VLAN extend
      --enable-drop-en      Enable per-queue packet drop if no descriptors
      --disable-rss         Disable RSS (Receive Side Scaling
      --port-topology PORT_TOPOLOGY
                            Port topology, 'paired' (the default) or 'chained'
      --forward-mode FORWARD_MODE
                            Forwarding mode, 'io' (default), 'mac', 'mac_swap',
                            'flowgen', 'rxonly', 'txonly', 'csum', 'icmpecho',
                            'ieee1588', 'tm'
      --rss-ip              Set RSS functions for IPv4/IPv6 only
      --rss-udp             Set RSS functions for IPv4/IPv6 and UDP
      --rxq RXQ             Number of RX queues per port, 1-65535 (default ...)
      --rxd RXD             Number of descriptors in the RX rings
                            (default is 128)
      --txq TXQ             Number of TX queues per port, 1-65535 (default ...)
      --txd TXD             Number of descriptors in the TX rings
                            (default is 512)
      --burst BURST         Number of packets per burst, 1-512 (default is 32)
      --mbcache MBCACHE     Cache of mbuf memory pools, 0-512 (default is 16)
      --rxpt RXPT           Prefetch threshold register of RX rings
                            (default is 8)
      --rxht RXHT           Host threshold register of RX rings (default is 8)
      --rxfreet RXFREET     Free threshold of RX descriptors,0-'rxd' (...)
      --rxwt RXWT           Write-back threshold register of RX rings
                            (default is 4)
      --txpt TXPT           Prefetch threshold register of TX rings (...)
      --txht TXHT           Host threshold register of TX rings (default is 0)
      --txwt TXWT           Write-back threshold register of TX rings (...)
      --txfreet TXFREET     Free threshold of RX descriptors, 0-'txd' (...)
      --txrst TXRST         Transmit RS bit threshold of TX rings, 0-'txd'
                            (default is 0)
      --rx-queue-stats-mapping RX_QUEUE_STATS_MAPPING
                            RX queues statistics counters mapping 0-15 as
                            '(port,queue,mapping)[,(port,queue,mapping)]'
      --tx-queue-stats-mapping TX_QUEUE_STATS_MAPPING
                            TX queues statistics counters mapping 0-15 as
                            '(port,queue,mapping)[,(port,queue,mapping)]'
      --no-flush-rx         Don’t flush the RX streams before starting
                            forwarding, Used mainly with the PCAP PMD
      --txpkts TXPKTS       TX segment sizes or total packet length, Valid for
                            tx-only and flowgen
      --disable-link-check  Disable check on link status when starting/stopping
                            ports
      --no-lsc-interrupt    Disable LSC interrupts for all ports
      --no-rmv-interrupt    Disable RMV interrupts for all ports
      --bitrate-stats [BITRATE_STATS [BITRATE_STATS ...]]
                            Logical core N to perform bitrate calculation
      --print-event PRINT_EVENT
                            Enable printing the occurrence of the designated
                            event, <unknown|intr_lsc|queue_state|intr_reset|
                            vf_mbox|macsec|intr_rmv|dev_probed|dev_released|
                            all>
      --mask-event MASK_EVENT
                            Disable printing the occurrence of the designated
                            event, <unknown|intr_lsc|queue_state|intr_reset|
                            vf_mbox|macsec|intr_rmv|dev_probed|dev_released|
                            all>
      --flow-isolate-all    Providing this parameter requests flow API isolated
                            mode on all ports at initialization time
      --tx-offloads TX_OFFLOADS
                            Hexadecimal bitmask of TX queue offloads (default
                            is 0)
      --hot-plug            Enable device event monitor machenism for hotplug
      --vxlan-gpe-port VXLAN_GPE_PORT
                            UDP port number of tunnel VXLAN-GPE (default is
                            4790)
      --mlockall            Enable locking all memory
      --no-mlockall         Disable locking all memory
      ...


.. _sppc_appl_pktgen:

Pktgen-dpdk Container
---------------------

``pktgen.py`` is a launcher script for
`pktgen-dpdk
<http://pktgen-dpdk.readthedocs.io/en/latest/index.html>`_.
Pktgen is a software based traffic generator powered by the DPDK
fast packet processing framework.
It is not only high-performance for generating 10GB traffic with
64 byte frames, but also very configurable to handle packets with
UDP, TCP, ARP, ICMP, GRE, MPLS and Queue-in-Queue.
It also supports
`Lua
<https://www.lua.org/>`_
for detailed configurations.

This ``pktgen.py`` script launches ``pktgen`` app container
with specified vhost interfaces.
Here is an example for launching with seven lcores (8-14th)
and three vhost interfaces.

.. code-block:: console

    $ cd /path/to/spp/tools/sppc
    $ python app/pktgen.py -l 8-14 -d 1-3 -fg --dist-ver 16.04
    sudo docker run \
     ...
     sppc/pktgen-ubuntu:16.04 \
     /root/dpdk/../pktgen-dpdk/app/x86_64-native-linuxapp-gcc/pktgen \
     -l 8-14 \
     ...
     -- \
     -m [9:10].0,[11:12].1,[13:14].2
     ...

You notice that given lcores ``-l 8-14`` are assigned appropriately.
Lcore 8 is used as master and remaining six lcores are use to worker
threads for three ports as ``-m [9:10].0,[11:12].1,[13:14].2`` equally.
If the number of given lcores is larger than required,
remained lcores are simply not used.

Calculation of core assignment of ``pktgen.py`` currently is supporting
up to four lcores for each of ports.
If you assign fire or more lcores to a port, ``pktgen.py`` terminates
to launch app container.
It is because a usecase more than four lcores is rare and
calculation is to be complicated.

.. code-block:: console

    # Assign five lcores for a slave is failed to launch
    $ python app/pktgen.py -l 6-11 -d 1
    Error: Too many cores for calculation for port assignment!
    Please consider to use '--matrix' for assigning directly

Here are other examples of lcore assignment of ``pktgen.py`` to help
your understanding.

**1. Three lcores for two ports**

Assign one lcore to master and two lcores two slaves for two ports.

.. code-block:: console

    $ python app/pktgen.py -l 6-8 -d 1,2
     ...
     -m 7.0,8.1 \


**2. Seven lcores for three ports**

Assign one lcore for master and each of two lcores to
three slaves for three ports.

.. code-block:: console

    $ python app/pktgen.py -l 6-12 -d 1,2,3
     ...
     -m [7:8].0,[9:10].1,[11:12].2 \


**3. Seven lcores for two ports**

Assign one lcore for master and each of three lcores to
two slaves for two ports.
In this case, each of three lcores cannot be assigned rx and tx port
equally, so given two lcores to rx and one core to tx.

.. code-block:: console

    $ python app/pktgen.py -l 6-12 -d 1,2
     ...
     -m [7-8:9].0,[10-11:12].1 \


Refer help for all of options and usges.
It shows options without of EAL and container for simplicity.

.. code-block:: console

    $ python app/pktgen.py -h
    usage: pktgen.py [-h] [-l CORE_LIST] [-c CORE_MASK] [-m MEM]
                     [--socket-mem SOCKET_MEM] [--nof-memchan NOF_MEMCHAN]
                     [-b [PCI_BLACKLIST [PCI_BLACKLIST ...]]]
                     [-w [PCI_WHITELIST [PCI_WHITELIST ...]]]
                     [--single-file-segment]
                     [-d DEV_IDS] [-nq NOF_QUEUES] [--no-privileged]
                     [--matrix MATRIX] [--log-level LOG_LEVEL]
                     [--dist-name DIST_NAME] [--dist-ver DIST_VER]
                     [-ci CONTAINER_IMAGE] [-fg] [--dry-run]

    Launcher for pktgen-dpdk application container

    optional arguments:
      ...
      -d DEV_IDS, --dev-ids DEV_IDS
                            two or more even vhost device IDs
      -nq NOF_QUEUES, --nof-queues NOF_QUEUES
                            Number of queues of virtio (default is 1)
      --no-privileged       Disable docker's privileged mode if it's needed
      -s PCAP_FILE, --pcap-file PCAP_FILE
                            PCAP packet flow file of port, defined as
                            'N:filename'
      -f SCRIPT_FILE, --script-file SCRIPT_FILE
                            Pktgen script (.pkt) to or a Lua script (.lua)
      -lf LOG_FILE, --log-file LOG_FILE
                            Filename to write a log, as '-l' of pktgen
      -P, --promiscuous     Enable PROMISCUOUS mode on all ports
      -G, --sock-default    Enable socket support using default server values
                            of localhost:0x5606
      -g SOCK_ADDRESS, --sock-address SOCK_ADDRESS
                            Same as -G but with an optional IP address and port
                            number
      -T, --term-color      Enable color terminal output in VT100
      -N, --numa            Enable NUMA support
      --matrix MATRIX       Matrix of cores and port as '-m' of pktgen, such as
                            [1:2].0 or 1.0
      ...


.. _sppc_appl_load_balancer:

Load-Balancer Container
-----------------------

`Load-Balancer
<https://dpdk.org/doc/guides/sample_app_ug/load_balancer.html>`_
is an application distributes packet I/O task with several worker
lcores to share IP addressing.

There are three types of lcore roles in this application, rx, tx and
worker lcores. Rx lcores retrieve packets from NICs and Tx lcores
send it to the destinations.
Worker lcores intermediate them, receive packets from rx lcores,
classify by looking up the address and send it to each of destination
tx lcores.
Each of lcores has a set of references of lcore ID and queue
as described in `Application Configuration
<https://dpdk.org/doc/guides/sample_app_ug/load_balancer.html#explanation>`_.

``load-balancer.py`` expects four mandatory options.

  * -rx: "(PORT, QUEUE, LCORE), ...", list of NIC RX ports and
    queues handled by the I/O RX lcores. This parameter also implicitly
    defines the list of I/O RX lcores.
  * -tx: "(PORT, LCORE), ...", list of NIC TX ports handled by
    the I/O TX lcores. This parameter also implicitly defines the list
    of I/O TX lcores.
  * -w: The list of the worker lcores.
  * --lpm: "IP / PREFIX => PORT", list of LPM rules used by the worker
    lcores for packet forwarding.

Here is an example for one rx, one tx and two worker on lcores 8-10.
Both of rx and rx is assinged to the same lcore 8.
It receives packets from port 0 and forwards it port 0 or 1.
The destination port is defined as ``--lpm`` option.

.. code-block:: console

    $ cd /path/to/spp/tools/sppc
    $ python app/load-balancer.py -fg -l 8-10  -d 1,2 \
    -rx "(0,0,8)" -tx "(0,8),(1,8)" -w 9,10 \
    --lpm "1.0.0.0/24=>0; 1.0.1.0/24=>1;"

If you are succeeded to launch the app container,
it shows details of rx, tx, worker lcores and LPM rules
, and starts forwarding.

.. code-block:: console

    ...
    Checking link statusdone
    Port0 Link Up - speed 10000Mbps - full-duplex
    Port1 Link Up - speed 10000Mbps - full-duplex
    Initialization completed.
    NIC RX ports: 0 (0 )  ;
    I/O lcore 8 (socket 0): RX ports  (0, 0)  ; Output rings  0x7f9af7347...
    Worker lcore 9 (socket 0) ID 0: Input rings  0x7f9af7347880  ;
    Worker lcore 10 (socket 0) ID 1: Input rings  0x7f9af7345680  ;

    NIC TX ports:  0  1  ;
    I/O lcore 8 (socket 0): Input rings per TX port  0 (0x7f9af7343480 ...
    Worker lcore 9 (socket 0) ID 0:
    Output rings per TX port  0 (0x7f9af7343480)  1 (0x7f9af7341280)  ;
    Worker lcore 10 (socket 0) ID 1:
    Output rings per TX port  0 (0x7f9af733f080)  1 (0x7f9af733ce80)  ;
    LPM rules:
    	0: 1.0.0.0/24 => 0;
    	1: 1.0.1.0/24 => 1;
    Ring sizes: NIC RX = 1024; Worker in = 1024; Worker out = 1024; NIC TX...
    Burst sizes: I/O RX (rd = 144, wr = 144); Worker (rd = 144, wr = 144);...
    Logical core 9 (worker 0) main loop.
    Logical core 10 (worker 1) main loop.
    Logical core 8 (I/O) main loop.


To stop forwarding, you need to terminate the application
but might not able to with *Ctrl-C*.
In this case, you can use ``docker kill`` command to terminate it.
Find the name of container on which ``load_balancer`` is running
and kill it.

.. code-block:: console

    $ docker ps
    CONTAINER ID  IMAGE                   ...  NAMES
    80ce3711b85e  sppc/dpdk-ubuntu:16.04  ...  competent_galileo  # kill it
    281aa8f236ef  sppc/spp-ubuntu:16.04   ...  youthful_mcnulty
    $ docker kill competent_galileo


.. note::

    You shold care about the number of worker lcores. If you add lcore 11
    and assign it for third worker thread,
    it is failed to lauhch the application.

    .. code-block:: console

        ...
        EAL: Probing VFIO support...
        Incorrect value for --w argument (-8)

            load_balancer <EAL PARAMS> -- <APP PARAMS>

        Application manadatory parameters:
            --rx "(PORT, QUEUE, LCORE), ..." : List of NIC RX ports and queues
                   handled by the I/O RX lcores
        ...


    The reason is the number of lcore is considered as invalid in
    ``parse_arg_w()`` as below.
    ``n_tuples`` is the number of lcores and it should be
    `2^n`, or returned with error code.

    .. code-block:: c

        // Defined in dpdk/examples/load_balancer/config.c
        static int
        parse_arg_w(const char *arg)
        {
                const char *p = arg;
                uint32_t n_tuples;
                ...
                if ((n_tuples & (n_tuples - 1)) != 0) {
                        return -8;
                }
                ...


Here are other examples.

**1. Separate rx and tx lcores**

Use four lcores 8-11 for rx, tx and two worker threads.
The number of ports is same as the previous example.
You notice that rx and tx have different lcore number, 8 and 9.

.. code-block:: console

    $ python app/load-balancer.py -fg -l 8-11 -d 1,2 \
    -rx "(0,0,8)" \
    -tx "(0,9),(1,9)" \
    -w 10,11 \
    --lpm "1.0.0.0/24=>0; 1.0.1.0/24=>1;"

**2. Assign multiple queues for rx**

To classify for three destination ports, use one rx lcore,
three tx lcores and four worker lcores.
In this case, rx has two queues and using ``-nq 2``.
You should start queue ID from 0 and to be in serial as `0,1,2,...`,
or failed to launch.

.. code-block:: console

    $ python app/load-balancer.py -fg -l 8-13 -d 1,2,3 -nq 2 \
    -rx "(0,0,8),(0,1,8)" \
    -tx "(0,9),(1,9),(2,9)" \
    -w 10,11,12,13 \
    --lpm "1.0.0.0/24=>0; 1.0.1.0/24=>1; 1.0.2.0/24=>2;"


Refer options and usages by ``load-balancer.py -h``.

.. code-block:: console

    $ python app/load-balancer.py -h
    usage: load-balancer.py [-h] [-l CORE_LIST] [-c CORE_MASK] [-m MEM]
                            [--socket-mem SOCKET_MEM]
                            [-b [PCI_BLACKLIST [PCI_BLACKLIST ...]]]
                            [-w [PCI_WHITELIST [PCI_WHITELIST ...]]]
                            [--single-file-segment]
                            [--nof-memchan NOF_MEMCHAN]
                            [-d DEV_IDS] [-nq NOF_QUEUES] [--no-privileged]
                            [-rx RX_PORTS] [-tx TX_PORTS] [-w WORKER_LCORES]
                            [-rsz RING_SIZES] [-bsz BURST_SIZES] [--lpm LPM]
                            [--pos-lb POS_LB] [--dist-name DIST_NAME]
                            [--dist-ver DIST_VER] [-ci CONTAINER_IMAGE]
                            [-fg] [--dry-run]

    Launcher for load-balancer application container

    optional arguments:
      ...
      -d DEV_IDS, --dev-ids DEV_IDS
                            two or more even vhost device IDs
      -nq NOF_QUEUES, --nof-queues NOF_QUEUES
                            Number of queues of virtio (default is 1)
      --no-privileged       Disable docker's privileged mode if it's needed
      -rx RX_PORTS, --rx-ports RX_PORTS
                            List of rx ports and queues handled by the I/O rx
                            lcores
      -tx TX_PORTS, --tx-ports TX_PORTS
                            List of tx ports and queues handled by the I/O tx
                            lcores
      -w WORKER_LCORES, --worker-lcores WORKER_LCORES
                            List of worker lcores
      -rsz RING_SIZES, --ring-sizes RING_SIZES
                            Ring sizes of 'rx_read,rx_send,w_send,tx_written'
      -bsz BURST_SIZES, --burst-sizes BURST_SIZES
                            Burst sizes of rx, worker or tx
      --lpm LPM             List of LPM rules
      --pos-lb POS_LB       Position of the 1-byte field used for identify
                            worker
      ...


.. _sppc_appl_suricata:

Suricata Container
------------------

`Suricata
<https://suricata.readthedocs.io/en/suricata-4.1.2/index.html>`_
is a sophisticated IDS/IPS application.
SPP container supports suricata 4.1.4 hosted this
`repository
<https://github.com/vipinpv85/DPDK_SURICATA-4_1_1>`_.

Unlike other scripts, ``app/suricata.py`` does not launch appliation
directly but bash to enable to edit config file on the container.
Suricata accepts options from config file specified with
``--dpdk`` option.
You can copy your config to the container by using ``docker cp``.
Sample config ``mysuricata.cfg`` is included under ``suricata-4.1.4``.

Here is an example of launching suricata with image
``sppc/suricata-ubuntu2:latest``
which is built as described in
:ref:`sppc_build_img_suricata`.

.. code-block:: console

    $ docker cp your.cnf CONTAINER_ID:/path/to/conf/your.conf
    $ ./suricata.py -d 1,2 -fg -ci sppc/suricata-ubuntu2:latest
    # suricata --dpdk=/path/to/config


Refer options and usages by ``load-balancer.py -h``.

.. code-block:: console

    $ python app/suricata.py -h
    usage: suricata.py [-h] [-l CORE_LIST] [-c CORE_MASK] [-m MEM]
                       [--socket-mem SOCKET_MEM]
                       [-b [PCI_BLACKLIST [PCI_BLACKLIST ...]]]
                       [-w [PCI_WHITELIST [PCI_WHITELIST ...]]]
                       [--single-file-segments] [--nof-memchan NOF_MEMCHAN]
                       [-d DEV_IDS] [-nq NOF_QUEUES] [--no-privileged]
                       [--dist-name DIST_NAME] [--dist-ver DIST_VER]
                       [--workdir WORKDIR] [-ci CONTAINER_IMAGE] [-fg] [--dry-run]

    Launcher for suricata container

    optional arguments:
      ...
      -d DEV_IDS, --dev-ids DEV_IDS
                            two or more even vhost device IDs
      -nq NOF_QUEUES, --nof-queues NOF_QUEUES
                            Number of queues of virtio (default is 1)
      --no-privileged       Disable docker's privileged mode if it's needed
      --dist-name DIST_NAME
                            Name of Linux distribution
      ...


.. _sppc_appl_helloworld:

Helloworld Container
--------------------

The `helloworld
<https://dpdk.org/doc/guides/sample_app_ug/hello_world.html>`_
sample application is an example of the simplest DPDK application
that can be written.

Unlike from the other applications, it does not work as a network
function actually.
This app container script ``helloworld.py`` is intended to be used
as a template for an user defined app container script.
You can use it as a template for developing your app container script.
An instruction for developing app container script is described in
:ref:`sppc_howto_define_appc`.

Helloworld app container has no application specific options. There are
only EAL and app container options.
You should give ``-l``  and ``-d`` options for the simplest app
container.
Helloworld application does not use vhost and ``-d`` options is not
required for the app, but required to setup continer itself.

.. code-block:: console

    $ cd /path/to/spp/tools/sppc
    $ python app/helloworld.py -l 4-6 -d 1 -fg
    ...
