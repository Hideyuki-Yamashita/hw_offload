..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2018-2019 Nippon Telegraph and Telephone Corporation

.. _spp_overview_spp_controller:

SPP Controller
==============

SPP is controlled from python based management framework. It consists of
front-end CLI and back-end server process.
SPP's front-end CLI provides a patch panel like interface for users.
This CLI process parses user input and sends request to the back-end via REST
APIs. It means that the back-end server process accepts requests from other
than CLI. It enables developers to implement control interface such as GUI, or
plugin for other framework.
`networking-spp
<https://github.com/openstack/networking-spp>`_
is a Neutron ML2 plugin for using SPP with OpenStack.
By using networking-spp and doing some of extra tunings for optimization, you
can deploy high-performance NFV services on OpenStack
`[1]
<https://www.openstack.org/summit/vancouver-2018/summit-schedule/events/20826>`_.


spp-ctl
-------

``spp-ctl`` is designed for managing SPP from several controllers
via REST-like APIs for users or other applications.
It is implemented to be simple and it is stateless.
Basically, it only converts a request into a command of SPP process and
forward it to the process without any of syntax or lexical checking.

There are several usecases where SPP is managed from other process without
user inputs. For example, you need a intermediate process if you think of
using SPP from a framework, such as OpenStack.
`networking-spp
<https://github.com/openstack/networking-spp>`_
is a Neutron ML2 plugin for SPP and `spp-agent` works as a SPP controller.

As shown in :numref:`figure_spp_overview_design_spp_ctl`,
``spp-ctl`` behaves as a TCP server for SPP primary and secondary processes,
and REST API server for client applications.
It should be launched in advance to setup connections with other processes.
``spp-ctl``  uses three TCP ports for primary, secondaries and clients.
The default port numbers are ``5555``, ``6666`` and ``7777``.

.. _figure_spp_overview_design_spp_ctl:

.. figure:: ../images/overview/design/spp_overview_design_spp-ctl.*
   :width: 48%

   Spp-ctl as a REST API server

``spp-ctl`` accepts multiple requests at the same time and serializes them
by using
`bottle
<https://bottlepy.org/docs/dev/>`_
which is simple and well known as a web framework and
`eventlet
<http://eventlet.net/>`_
for parallel processing.


SPP CLI
-------

SPP CLI is a user interface for managing SPP and implemented as a client of
``spp-ctl``. It provides several kinds of command for inspecting SPP
processes, changing path configuration or showing statistics of packets.
However, you do not need to use SPP CLI if you use ``netowrking-spp`` or
other client applications of ``spp-ctl``. SPP CLI is one of them.

From SPP CLI, user is able to configure paths as similar as
patch panel like manner by sending commands to each of SPP secondary processes.
``patch phy:0 ring:0`` is to connect two ports, ``phy:0`` and ``ring:0``.

As described in :ref:`Getting Started<spp_setup_howto_use_spp_cli>` guide,
SPP CLI is able to communicate several ``spp-ctl`` to support multiple nodes
configuration.


Reference
---------

* [1] `Integrating OpenStack with DPDK for High Performance Applications
  <https://www.openstack.org/summit/vancouver-2018/summit-schedule/events/20826>`_
