..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2017-2018 Nippon Telegraph and Telephone Corporation

.. _spp_container_overview:

Overview
========

SPP container is a set of tools for running SPP and DPDK applications
with docker.
It consists of shell or python scripts
for building container images and launching app containers
with docker commands.

As shown in :numref:`figure_sppc_overview`, all of
DPDK applications, including SPP primary and secondary processes,
run inside containers.
SPP controller is just a python script and does not need to be run as
an app container.


.. _figure_sppc_overview:

.. figure:: ../../images/tools/sppc/sppc_overview.*
   :width: 95%

   SPP container overview
