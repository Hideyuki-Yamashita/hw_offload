..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2010-2014 Intel Corporation


.. _gsg_performance_opt:

Performance Optimization
========================

Reduce Context Switches
-----------------------

Use the ``isolcpus`` Linux kernel parameter to isolate them
from Linux scheduler to reduce context switches.
It prevents workloads of other processes than DPDK running on
reserved cores with ``isolcpus`` parameter.

For Ubuntu 16.04, define ``isolcpus`` in ``/etc/default/grub``.

.. code-block:: console

    GRUB_CMDLINE_LINUX_DEFAULT=“isolcpus=0-3,5,7”

The value of this ``isolcpus`` depends on your environment and usage.
This example reserves six cores(0,1,2,3,5,7).


Optimizing QEMU Performance
---------------------------

QEMU process runs threads for vcpu emulation. It is effective strategy
for pinning vcpu threads to decicated cores.

To find vcpu threads, you use ``ps`` command to find PID of QEMU process
and ``pstree`` command for threads launched from QEMU process.

.. code-block:: console

    $ ps ea
       PID TTY     STAT  TIME COMMAND
    192606 pts/11  Sl+   4:42 ./x86_64-softmmu/qemu-system-x86_64 -cpu host ...

Run ``pstree`` with ``-p`` and this PID to find all threads launched from QEMU.

.. code-block:: console

    $ pstree -p 192606
    qemu-system-x86(192606)--+--{qemu-system-x8}(192607)
                             |--{qemu-system-x8}(192623)
                             |--{qemu-system-x8}(192624)
                             |--{qemu-system-x8}(192625)
                             |--{qemu-system-x8}(192626)

Update affinity by using ``taskset`` command to pin vcpu threads.
The vcpu threads is listed from the second entry and later.
In this example, assign PID 192623 to core 4, PID 192624 to core 5
and so on.

.. code-block:: console

    $ sudo taskset -pc 4 192623
    pid 192623's current affinity list: 0-31
    pid 192623's new affinity list: 4
    $ sudo taskset -pc 5 192624
    pid 192624's current affinity list: 0-31
    pid 192624's new affinity list: 5
    $ sudo taskset -pc 6 192625
    pid 192625's current affinity list: 0-31
    pid 192625's new affinity list: 6
    $ sudo taskset -pc 7 192626
    pid 192626's current affinity list: 0-31
    pid 192626's new affinity list: 7


Reference
---------

* [1] `Best pinning strategy for latency/performance trade-off
  <https://www.redhat.com/archives/vfio-users/2017-February/msg00010.html>`_
* [2] `PVP reference benchmark setup using testpmd
  <http://dpdk.org/doc/guides/howto/pvp_reference_benchmark.html>`_
* [3] `Enabling Additional Functionality
  <http://dpdk.org/doc/guides/linux_gsg/enable_func.html>`_
* [4] `How to get best performance with NICs on Intel platforms
  <http://dpdk.org/doc/guides/linux_gsg/nic_perf_intel_platform.html>`_
