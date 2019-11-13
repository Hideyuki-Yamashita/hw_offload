..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2010-2014 Intel Corporation
    Copyright(c) 2018-2019 Nippon Telegraph and Telephone Corporation


.. _spp_overview_design_spp_primary:

SPP Primary
===========

SPP is originally derived from
`Client-Server Multi-process Example
<https://doc.dpdk.org/guides/sample_app_ug/multi_process.html#client-server-multi-process-example>`_
of
`Multi-process Sample Application
<https://doc.dpdk.org/guides/sample_app_ug/multi_process.html>`_
in DPDK's sample applications.
``spp_primary`` is a server for other secondary processes and
basically working same as described in
"How the Application Works" section of the sample application.

However, there are some differences between ``spp_primary`` and
the server process of the sample application.
``spp_primary`` has no limitation of the number of secondary processes.
It does not work for packet forwaring without in some usecases, but just
provide rings and memory pools for secondary processes.


Master and Worker Threads
-------------------------

In SPP, Both of primary and secondary processes consist of master thread and
worker thread as slave. Master thread is for accepting commands from a user
for doing task, and running on a master lcore. On the other hand,
slave thread is for packet forwarding or other process specific jobs
as worker, and running on slave lcore. Only slave thread requires
dedicated core for running in pole mode, and launched from
``rte_eal_remote_launch()`` or ``rte_eal_mp_remote_launch()``.

``spp_primary`` is able to run with or without worker thread selectively,
and requires at least one lcore for server process.
Using worker thread or not depends on your usecases.
``spp_primary`` provides two types of workers currently.


Worker Types
------------

There are two types of worker thread in ``spp_primary``. First one is
is forwarder thread, and second one is monitor thread.

As default, ``spp_primary`` runs packet forwarder if two or more lcores
are given while launching the process. Behavior of this forwarder is
same as ``spp_nfv`` described in the next section.
This forwarder provides features for managing ports, patching them and
forwarding packets between ports.
It is useful for very simple usecase in which only few ports are patched
and no need to do forwarding packets in parallel with several processes.

.. note::

    In DPDK v18.11 or later, some of PMDs, such as vhost, do not work for
    multi-process application. It means that packets cannot forwarded
    to a VM or container via secondary process in SPP.
    In this case, you use forwarder in ``spp_primary``.

Another type is monitor for displaying status of ``spp_primary`` in which
statistics of RX and TX packets on each of physical ports and ring ports
are shown periodically in terminal ``spp_primary`` is launched.
Although statistics can be referred in SPP CLI by using ``pri; status``
command, running monitor thread is useful if you always watch statistics.
