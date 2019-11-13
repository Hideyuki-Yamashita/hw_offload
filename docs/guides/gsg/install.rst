..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2017-2019 Nippon Telegraph and Telephone Corporation


.. _setup_install_dpdk_spp:

Install DPDK and SPP
====================

Before setting up SPP, you need to install DPDK.
In this document, briefly described how to install and setup DPDK.
Refer to `DPDK documentation
<https://dpdk.org/doc/guides/>`_ for more details.
For Linux, see `Getting Started Guide for Linux
<http://www.dpdk.org/doc/guides/linux_gsg/index.html>`_ .


.. _setup_install_packages:

Required Packages
-----------------

Installing packages for DPDK and SPP is almost the on Ubunu and CentOS,
but names are different for some packages.


Ubuntu
~~~~~~

To compile DPDK, it is required to install following packages.

.. code-block:: console

    $ sudo apt install libnuma-dev \
      libarchive-dev \
      build-essential

You also need to install linux-headers of your kernel version.

.. code-block:: console

    $ sudo apt install linux-headers-$(uname -r)

Some of SPP secondary processes depend on other libraries and you fail to
compile SPP without installing them.

SPP provides libpcap-based PMD for dumping packet to a file or retrieve
it from the file.
``spp_nfv`` and ``spp_pcap`` use ``libpcap-dev`` for packet capture.
``spp_pcap`` uses ``liblz4-dev`` and ``liblz4-tool`` to compress PCAP file.

.. code-block:: console

   $ sudo apt install libpcap-dev \
     liblz4-dev \
     liblz4-tool

``text2pcap`` is also required for creating pcap file which
is included in ``wireshark``.

.. code-block:: console

    $ sudo apt install wireshark


CentOS
~~~~~~

Before installing packages for DPDK, you should add
`IUS Community repositories
<https://ius.io/GettingStarted/>`_
with yum command.

.. code-block:: console

    $ sudo yum install https://centos7.iuscommunity.org/ius-release.rpm

To compile DPDK, required to install following packages.

.. code-block:: console

    $ sudo yum install numactl-devel \
      libarchive-devel \
      kernel-headers \
      kernel-devel

As same as Ubuntu, you should install additional packages because
SPP provides libpcap-based PMD for dumping packet to a file or retrieve
it from the file.
``spp_nfv`` and ``spp_pcap`` use ``libpcap-dev`` for packet capture.
``spp_pcap`` uses ``liblz4-dev`` and ``liblz4-tool`` to compress PCAP file.
``text2pcap`` is also required for creating pcap file which is included in ``wireshark``.

.. code-block:: console

   $ sudo apt install libpcap-dev \
     libpcap \
     libpcap-devel \
     lz4 \
     lz4-devel \
     wireshark \
     wireshark-devel \
     libX11-devel


.. _setup_install_dpdk:

DPDK
----

Clone repository and compile DPDK in any directory.

.. code-block:: console

    $ cd /path/to/any
    $ git clone http://dpdk.org/git/dpdk

Installing on Ubuntu and CentOS are almost the same, but required packages
are just bit different.

PCAP is disabled by default in DPDK configuration.
``CONFIG_RTE_LIBRTE_PMD_PCAP`` and ``CONFIG_RTE_PORT_PCAP`` defined in
config file ``common_base`` should be changed to ``y`` to enable PCAP.

.. code-block:: console

    # dpdk/config/common_base
    CONFIG_RTE_LIBRTE_PMD_PCAP=y
    ...
    CONFIG_RTE_PORT_PCAP=y

Compile DPDK with target environment.

.. code-block:: console

    $ cd dpdk
    $ export RTE_SDK=$(pwd)
    $ export RTE_TARGET=x86_64-native-linuxapp-gcc  # depends on your env
    $ make install T=$RTE_TARGET


PCAP is disabled by default in DPDK configuration, so should be changed
if you use this feature.
``CONFIG_RTE_LIBRTE_PMD_PCAP`` and ``CONFIG_RTE_PORT_PCAP`` defined in
config file ``common_base`` should be changed to ``y`` to enable PCAP.

.. code-block:: console

    # dpdk/config/common_base
    CONFIG_RTE_LIBRTE_PMD_PCAP=y
    ...
    CONFIG_RTE_PORT_PCAP=y

Compile DPDK with options for target environment.

.. code-block:: console

    $ cd dpdk
    $ export RTE_SDK=$(pwd)
    $ export RTE_TARGET=x86_64-native-linuxapp-gcc  # depends on your env
    $ make install T=$RTE_TARGET


Pyhton
------

Python3 and pip3 are also required because SPP controller is implemented
in Pyhton3. Required packages can be installed from ``requirements.txt``.

.. code-block:: console

    # Ubuntu
    $ sudo apt install python3 \
      python3-pip

For CentOS, you need to specify minor version of python3.
Here is an example of installing python3.6.

.. code-block:: console

    # CentOS
    $ sudo yum install python36 \
      python36-pip

SPP provides ``requirements.txt`` for installing required packages of Python3.
You might fail to run ``pip3`` without sudo on some environments.

.. code-block:: console

    $ pip3 install -r requirements.txt

For some environments, ``pip3`` might install packages under your home
directory ``$HOME/.local/bin`` and you should add it to ``$PATH`` environment
variable.


.. _setup_install_spp:

SPP
---

Clone SPP repository and compile it in any directory.

.. code-block:: console

    $ cd /path/to/any
    $ git clone http://dpdk.org/git/apps/spp
    $ cd spp
    $ make  # Confirm that $RTE_SDK and $RTE_TARGET are set

If you use ``spp_mirror`` in deep copy mode,
which is used for cloning whole of packet data for modification,
you should change configuration of copy mode in Makefile of ``spp_mirror``
before.
It is for copying full payload into a new mbuf.
Default mode is shallow copy.

.. code-block:: console

    # src/mirror/Makefile
    #CFLAGS += -Dspp_mirror_SHALLOWCOPY

.. note::

    Before run make command, you might need to consider if using deep copy
    for cloning packets in ``spp_mirror``. Comparing with shallow copy, it
    clones entire packet payload into a new mbuf and it is modifiable,
    but lower performance. Which of copy mode should be chosen depends on
    your usage.


Binding Network Ports to DPDK
-----------------------------

Network ports must be bound to DPDK with a UIO (Userspace IO) driver.
UIO driver is for mapping device memory to userspace and registering
interrupts.

UIO Drivers
~~~~~~~~~~~

You usually use the standard ``uio_pci_generic`` for many use cases
or ``vfio-pci`` for more robust and secure cases.
Both of drivers are included by default in modern Linux kernel.

.. code-block:: console

    # Activate uio_pci_generic
    $ sudo modprobe uio_pci_generic

    # or vfio-pci
    $ sudo modprobe vfio-pci

You can also use kmod included in DPDK instead of ``uio_pci_generic``
or ``vfio-pci``.

.. code-block:: console

    $ sudo modprobe uio
    $ sudo insmod kmod/igb_uio.ko

Binding Network Ports
~~~~~~~~~~~~~~~~~~~~~

Once UIO driver is activated, bind network ports with the driver.
DPDK provides ``usertools/dpdk-devbind.py`` for managing devices.

Find ports for binding to DPDK by running the tool with ``-s`` option.

.. code-block:: console

    $ $RTE_SDK/usertools/dpdk-devbind.py --status

    Network devices using DPDK-compatible driver
    ============================================
    <none>

    Network devices using kernel driver
    ===================================
    0000:29:00.0 '82571EB ... 10bc' if=enp41s0f0 drv=e1000e unused=
    0000:29:00.1 '82571EB ... 10bc' if=enp41s0f1 drv=e1000e unused=
    0000:2a:00.0 '82571EB ... 10bc' if=enp42s0f0 drv=e1000e unused=
    0000:2a:00.1 '82571EB ... 10bc' if=enp42s0f1 drv=e1000e unused=

    Other Network devices
    =====================
    <none>
    ....

You can find network ports are bound to kernel driver and not to DPDK.
To bind a port to DPDK, run ``dpdk-devbind.py`` with specifying a driver
and a device ID.
Device ID is a PCI address of the device or more friendly style like
``eth0`` found by ``ifconfig`` or ``ip`` command..

.. code-block:: console

    # Bind a port with 2a:00.0 (PCI address)
    ./usertools/dpdk-devbind.py --bind=uio_pci_generic 2a:00.0

    # or eth0
    ./usertools/dpdk-devbind.py --bind=uio_pci_generic eth0


After binding two ports, you can find it is under the DPDK driver and
cannot find it by using ``ifconfig`` or ``ip``.

.. code-block:: console

    $ $RTE_SDK/usertools/dpdk-devbind.py -s

    Network devices using DPDK-compatible driver
    ============================================
    0000:2a:00.0 '82571EB ... 10bc' drv=uio_pci_generic unused=vfio-pci
    0000:2a:00.1 '82571EB ... 10bc' drv=uio_pci_generic unused=vfio-pci

    Network devices using kernel driver
    ===================================
    0000:29:00.0 '...' if=enp41s0f0 drv=e1000e unused=vfio-pci,uio_pci_generic
    0000:29:00.1 '...' if=enp41s0f1 drv=e1000e unused=vfio-pci,uio_pci_generic

    Other Network devices
    =====================
    <none>
    ....


Confirm DPDK is setup properly
------------------------------

For testing, you can confirm if you are ready to use DPDK by running
DPDK's sample application. ``l2fwd`` is good example to confirm it
before SPP because it is very similar to SPP's worker process for forwarding.

.. code-block:: console

   $ cd $RTE_SDK/examples/l2fwd
   $ make
     CC main.o
     LD l2fwd
     INSTALL-APP l2fwd
     INSTALL-MAP l2fwd.map

In this case, run this application simply with just two options
while DPDK has many kinds of options.

  * ``-l``: core list
  * ``-p``: port mask

.. code-block:: console

   $ sudo ./build/app/l2fwd \
     -l 1-2 \
     -- -p 0x3

It must be separated with ``--`` to specify which option is
for EAL or application.
Refer to `L2 Forwarding Sample Application
<https://dpdk.org/doc/guides/sample_app_ug/l2_forward_real_virtual.html>`_
for more details.


Build Documentation
-------------------

This documentation is able to be built as HTML and PDF formats from make
command. Before compiling the documentation, you need to install some of
packages required to compile.

For HTML documentation, install sphinx and additional theme.

.. code-block:: console

    $ pip3 install sphinx \
      sphinx-rtd-theme

For PDF, inkscape and latex packages are required.

.. code-block:: console

    # Ubuntu
    $ sudo apt install inkscape \
      texlive-latex-extra \
      texlive-latex-recommended

.. code-block:: console

    # CentOS
    $ sudo yum install inkscape \
      texlive-latex

You might also need to install ``latexmk`` in addition to if you use
Ubuntu 18.04 LTS.

.. code-block:: console

    $ sudo apt install latexmk

HTML documentation is compiled by running make with ``doc-html``. This
command launch sphinx for compiling HTML documents.
Compiled HTML files are created in ``docs/guides/_build/html/`` and
You can find the top page ``index.html`` in the directory.

.. code-block:: console

    $ make doc-html

PDF documentation is compiled with ``doc-pdf`` which runs latex for.
Compiled PDF file is created as ``docs/guides/_build/html/SoftPatchPanel.pdf``.

.. code-block:: console

    $ make doc-pdf

You can also compile both of HTML and PDF documentations with ``doc`` or
``doc-all``.

.. code-block:: console

    $ make doc
    # or
    $ make doc-all

.. note::

    For CentOS, compilation PDF document is not supported.
