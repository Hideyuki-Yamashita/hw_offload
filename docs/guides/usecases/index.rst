..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2019 Nippon Telegraph and Telephone Corporation

.. _spp_usecases_index:

Use Cases
=========

As described in :ref:`Design<spp_design_index>`,
SPP has several kinds of secondary process for
usecases such as simple forwarding to network entities, capturing or
mirroring packets for monitoring, or connecting VMs or containers for
Service Function Chaining in NFV.

This chapter is focusing on explaining about each of secondary
processes with simple usecases.
Usecase of ``spp_primary`` is not covered here because it is almost
similar to ``spp_nfv`` and no need to explain both.

Details of usages of each process is not covered in this chapter.
You can refer the details of SPP processes via CLI from
:ref:`SPP Commands<spp_commands_index>`,
or via REST API from :ref:`API Reference<spp_api_ref_index>`.

.. toctree::
   :maxdepth: 1
   :numbered:

   spp_nfv
   spp_vf
   spp_mirror
   spp_pcap
   multi_nodes
