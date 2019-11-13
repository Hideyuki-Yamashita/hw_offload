..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2010-2014 Intel Corporation
    Copyright(c) 2017-2019 Nippon Telegraph and Telephone Corporation


.. _gsg_setup:

Setup
=====

This documentation is described for following distributions.

- Ubuntu 16.04 and 18.04
- CentOS 7.6 (not fully supported)


.. _gsg_reserve_hugep:

Reserving Hugepages
-------------------

Hugepages should be enabled for running DPDK application.
Hugepage support is to reserve large amount size of pages,
2MB or 1GB per page, to less TLB (Translation Lookaside Buffers) and
to reduce cache miss.
Less TLB means that it reduce the time for translating virtual address
to physical.

How to configure reserving hugepages is different between 2MB or 1GB.
In general, 1GB is better for getting high performance,
but 2MB is easier for configuration than 1GB.


1GB Hugepage
~~~~~~~~~~~~

For using 1GB page, hugepage setting is activated while booting system.
It must be defined in boot loader configuration, usually it is
``/etc/default/grub``.
Add an entry of configuration of the size and the number of pages.

Here is an example for Ubuntu, and almost the same as CentOS. The points are
that ``hugepagesz`` is for the size and ``hugepages`` is for the number of
pages.
You can also configure ``isolcpus`` in grub setting for improving performance
as described in
:ref:`Performance Optimizing<gsg_performance_opt>`.

.. code-block:: none

    # /etc/default/grub
    GRUB_CMDLINE_LINUX="default_hugepagesz=1G hugepagesz=1G hugepages=8"

For Ubuntu, you should run ``update-grub`` for updating
``/boot/grub/grub.cfg`` after editing to update grub's
config file, or this configuration is not activated.

.. code-block:: console

    # Ubuntu
    $ sudo update-grub
    Generating grub configuration file ...

Or for CentOS7, you use ``grub2-mkconfig`` instead of ``update-grub``.
In this case, you should give the output file with ``-o`` option.
The output path might be different, so you should find your correct
``grub.cfg`` by yourself.

.. code-block:: console

    # CentOS
    $ sudo grub2-mkconfig -o /boot/efi/EFI/centos/grub.cfg

.. note::

    1GB hugepages might not be supported on your hardware.
    It depends on that CPUs support 1GB pages or not. You can check it
    by referring ``/proc/cpuinfo``. If it is supported, you can find
    ``pdpe1gb`` in the ``flags`` attribute.

    .. code-block:: console

        $ cat /proc/cpuinfo | grep pdpe1gb
        flags           : fpu vme ... pdpe1gb ...


2MB Hugepage
~~~~~~~~~~~~

For using 2MB page, you can activate hugepages while booting or at anytime
after system is booted.
Define hugepages setting in ``/etc/default/grub`` to activate it while
booting, or overwrite the number of 2MB hugepages as following.

.. code-block:: console

    $ echo 1024 > /sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages

In this case, 1024 pages of 2MB, totally 2048 MB, are reserved.


Mount hugepages
---------------

Make the memory available for using hugepages from DPDK.

.. code-block:: console

    $ mkdir /mnt/huge
    $ mount -t hugetlbfs nodev /mnt/huge

It is also available while booting by adding a configuration of mount
point in ``/etc/fstab``.
The mount point for 2MB or 1GB can be made permanently accross reboot.
For 2MB, it is no need to declare the size of hugepages explicity.

.. code-block:: none

    # /etc/fstab
    nodev /mnt/huge hugetlbfs defaults 0 0

For 1GB, the size of hugepage ``pagesize`` must be specified.

.. code-block:: none

    # /etc/fstab
    nodev /mnt/huge_1GB hugetlbfs pagesize=1GB 0 0


Disable ASLR
------------

SPP is a DPDK multi-process application and there are a number of
`limitations
<https://dpdk.org/doc/guides/prog_guide/multi_proc_support.html#multi-process-limitations>`_
.

Address-Space Layout Randomization (ASLR) is a security feature for
memory protection, but may cause a failure of memory
mapping while starting multi-process application as discussed in
`dpdk-dev
<http://dpdk.org/ml/archives/dev/2014-September/005236.html>`_
.

ASLR can be disabled by assigning ``kernel.randomize_va_space`` to
``0``, or be enabled by assigning it to ``2``.

.. code-block:: console

    # disable ASLR
    $ sudo sysctl -w kernel.randomize_va_space=0

    # enable ASLR
    $ sudo sysctl -w kernel.randomize_va_space=2

You can check the value as following.

.. code-block:: console

    $ sysctl -n kernel.randomize_va_space


Using Virtual Machine
---------------------

SPP provides vhost interface for inter VM communication.
You can use any of DPDK supported hypervisors, but this document describes
usecases of qemu and libvirt.


Server mode v.s. Client mode
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

For using vhost, vhost port should be created before VM is launched in
server mode, or SPP is launched in client mode to be able to create
vhost port after VM is launched.

Client mode is optional and supported in qemu 2.7 or later.
For using this mode, launch secondary process with ``--vhost-client``.
Qemu creates socket file instead of secondary process.
It means that you can launch a VM before secondary process create vhost port.


Libvirt
~~~~~~~

If you use libvirt for managing virtual machines, you might need some
additional configurations.

To have access to resources with your account, update and
activate user and group parameters in ``/etc/libvirt/qemu.conf``.
Here is an example.

.. code-block:: none

    # /etc/libvirt/qemu.conf

    user = "root"
    group = "root"

For using hugepages with libvirt, change ``KVM_HUGEPAGES`` from 0 to 1
in ``/etc/default/qemu-kvm``.

.. code-block:: none

    # /etc/default/qemu-kvm

    KVM_HUGEPAGES=1

Change grub config as similar to
:ref:`Reserving Hugepages<gsg_reserve_hugep>`.
You can check hugepage settings as following.

.. code-block:: console

    $ cat /proc/meminfo | grep -i huge
    AnonHugePages:      2048 kB
    HugePages_Total:      36		#	/etc/default/grub
    HugePages_Free:       36
    HugePages_Rsvd:        0
    HugePages_Surp:        0
    Hugepagesize:    1048576 kB		#	/etc/default/grub

    $ mount | grep -i huge
    cgroup on /sys/fs/cgroup/hugetlb type cgroup (rw,...,nsroot=/)
    hugetlbfs on /dev/hugepages type hugetlbfs (rw,relatime)
    hugetlbfs-kvm on /run/hugepages/kvm type hugetlbfs (rw,...,gid=117)
    hugetlb on /run/lxcfs/controllers/hugetlb type cgroup (rw,...,nsroot=/)

Finally, you umount default hugepages.

.. code-block:: console

    $ sudo umount /dev/hugepages


Trouble Shooting
~~~~~~~~~~~~~~~~

You might encounter a permission error while creating a resource,
such as a socket file under ``tmp/``, because of AppArmor.

You can avoid this error by editing ``/etc/libvirt/qemu.conf``.

.. code-block:: console

    # Set security_driver to "none"
    $sudo vi /etc/libvirt/qemu.conf
    ...
    security_driver = "none"
    ...

Restart libvirtd to activate this configuration.

.. code-block:: console

    $sudo systemctl restart libvirtd.service

Or, you can also avoid by simply removing AppArmor itself.

.. code-block:: console

    $ sudo apt-get remove apparmor

If you use CentOS, confirm that SELinux doesn't prevent
for permission.
SELinux is disabled simply by changing the configuration to ``disabled``.

.. code-block:: console

    # /etc/selinux/config
    SELINUX=disabled

Check your SELinux configuration.

.. code-block:: console

    $ getenforce
    Disabled


Python 2 or 3 ?
---------------

Without SPP container tools, Python2 is not supported anymore.
SPP container will also be updated to Python3.


Reference
---------

* [1] `Use of Hugepages in the Linux Environment
  <http://dpdk.org/doc/guides/linux_gsg/sys_reqs.html#running-dpdk-applications>`_

* [2] `Using Linux Core Isolation to Reduce Context Switches
  <http://dpdk.org/doc/guides/linux_gsg/enable_func.html#using-linux-core-isolation-to-reduce-context-switches>`_

* [3] `Linux boot command line
  <http://dpdk.org/doc/guides/linux_gsg/nic_perf_intel_platform.html#linux-boot-command-line>`_
