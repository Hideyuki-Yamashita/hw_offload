..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2018-2019 Nippon Telegraph and Telephone Corporation

.. _spp_overview_design:

Soft Patch Panel
================

SPP is composed of several DPDK processes and controller processes for
connecting each of client processes with high-throughput path of DPDK.
:numref:`figure_spp_overview_design_all` shows SPP processes and client apps
for describing overview of design of SPP. In this diagram, solid line arrows
describe a data path for packet forwarding and it can be configured from
controller via command messaging of blue dashed line arrows.

.. _figure_spp_overview_design_all:

.. figure:: ../images/overview/design/spp_overview_design_all.*
   :width: 85%

   Overview of design of SPP

In terms of DPDK processes, SPP is derived from DPDK's multi-process sample
application and it consists of a primary process and multiple secondary
processes.
SPP primary process is responsible for resource management, for example,
initializing ports, mbufs or shared memory. On the other hand,
secondary processes of ``spp_nfv`` are working for forwarding
`[1]
<https://dpdksummit.com/Archive/pdf/2017USA/Implementation%20and%20Testing%20of%20Soft%20Patch%20Panel.pdf>`_.


Reference
---------

* [1] `Implementation and Testing of Soft Patch Panel
  <https://dpdksummit.com/Archive/pdf/2017USA/Implementation%20and%20Testing%20of%20Soft%20Patch%20Panel.pdf>`_
