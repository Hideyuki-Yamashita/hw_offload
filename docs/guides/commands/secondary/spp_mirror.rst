..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2010-2014 Intel Corporation

.. _commands_spp_mirror:

spp_mirror
==========

``spp_mirror`` is an implementation of TaaS feature as a SPP secondary process
for port mirroring.

Each of ``spp_mirror`` processes is managed with ``mirror`` command. Because
basic design of ``spp_mirror`` is derived from ``spp_vf``, its commands are
almost similar to ``spp_vf``.

Secondary ID is referred as ``--client-id`` which is given as an argument
while launching ``spp_mirror``. It should be unique among all of secondary
processes including ``spp_nfv`` and others.

``mirror`` command takes an secondary ID and one of sub commands. Secondary ID
and sub command should be separated with delimiter ``;``, or failed to a
command error. Some of sub commands take additional arguments for
configuration of the process or its resource management.

.. code-block:: console

    spp > mirror SEC_ID; SUB_CMD

In this example, ``SEC_ID`` is a secondary ID and ``SUB_CMD`` is one of the
following sub commands. Details of each of sub commands are described in the
next sections.

* status
* component
* port

``spp_mirror`` supports TAB completion. You can complete all of the name
of commands and its arguments. For instance, you find all of sub commands
by pressing TAB after ``mirror SEC_ID;``.

.. code-block:: console

    spp > mirror 1;  # press TAB key
    component      port        status

It tries to complete all of possible arguments. However, ``spp_mirror`` takes
also an arbitrary parameter which cannot be predicted, for example, name of
component. In this cases, ``spp_mirror`` shows capitalized keyword for
indicating it is an arbitrary parameter. Here is an exmaple of ``component``
command to initialize a worker thread. Keyword ``NAME`` should be replaced with
your favorite name for the worker of the role.

.. code-block:: console

    spp > mirror 1; component st  # press TAB key to show args starting 'st'
    start  stop
    spp > mirror 1; component start NAME  # 'NAME' is shown with TAB after start
    spp > mirror 1; component start mr1   # replace 'NAME' with favorite name
    spp > mirror 1; component start mr1   # then, press TAB to show core IDs
    5  6  7

It is another example of replacing keyword. ``port`` is a sub command for
assigning a resource to a worker thread. ``RES_UID`` is replaced with
resource UID which is a combination of port type and its ID such as
``ring:0`` or ``vhost:1`` to assign it as RX port of forwarder ``mr1``.

.. code-block:: console

    spp > mirror 1; port add RES_UID
    spp > mirror 1; port add ring:0 rx mr1

If you are reached to the end of arguments, no candidate keyword is displayed.
It is a completed statement of ``component`` command, and TAB
completion does not work after ``mirror`` because it is ready to run.

.. code-block:: console

    spp > mirror 1; component start mr1 5 mirror
    Succeeded to start component 'mr1' on core:5

It is also completed secondary IDs of ``spp_mirror`` and it is helpful if you run
several ``spp_mirror`` processes.

.. code-block:: console

    spp > mirror  # press TAB after space following 'mirror'
    1;  3;    # you find two spp_mirror processes of sec ID 1, 3

By the way, it is also a case of no candidate keyword is displayed if your
command statement is wrong. You might be encountered an error if you run the
wrong command. Please take care.

.. code-block:: console

    spp > mirror 1; compa  # no candidate shown for wrong command
    Invalid command "compa".


.. _commands_spp_mirror_status:

status
------

Show the information of worker threads and its resources. Status information
consists of three parts.

.. code-block:: console

    spp > mirror 1; status
    Basic Information:
      - client-id: 3
      - ports: [phy:0, phy:1, ring:0, ring:1, ring:2, ring:3, ring:4]
     - lcore_ids:
       - master: 1
       - slaves: [2, 3, 4]
    Components:
      - core:5 'mr1' (type: mirror)
        - rx: ring:0
        - tx: [ring:1, ring:2]
      - core:6 'mr2' (type: mirror)
        - rx: ring:3
        - tx: [ring:4, ring:5]
      - core:7 '' (type: unuse)

``Basic Information`` is for describing attributes of ``spp_mirror`` itself.
``client-id`` is a secondary ID of the process and ``ports`` is a list of
all of ports owned the process.

``Components`` is a list of all of worker threads. Each of workers has a
core ID running on, type of the worker and a list of resources.
Entry of no name with ``unuse`` type means that no worker thread assigned to
the core. In other words, it is ready to be assinged.


.. _commands_spp_mirror_component:

component
---------

Assing or release a role of forwarding to worker threads running on each of
cores which are reserved with ``-c`` or ``-l`` option while launching
``spp_mirror``. Unlike ``spp_vf``, ``spp_mirror`` only has one role, ``mirror``.

.. code-block:: console

    # assign 'ROLE' to worker on 'CORE_ID' with a 'NAME'
    spp > mirror SEC_ID; component start NAME CORE_ID ROLE

    # release worker 'NAME' from the role
    spp > mirror SEC_ID; component stop NAME

Here is an example of assigning role with ``component`` command.

.. code-block:: console

    # assign 'mirror' role with name 'mr1' on core 2
    spp > mirror 2; component start mr1 2 mirror

And an examples of releasing role.

.. code-block:: console

    # release mirror role
    spp > mirror 2; component stop mr1


.. _commands_spp_mirror_port:

port
----

Add or delete a port to a worker.

Adding port
~~~~~~~~~~~

.. code-block:: console

    spp > mirror SEC_ID; port add RES_UID DIR NAME

``RES_UID`` is with replaced with resource UID such as ``ring:0`` or
``vhost:1``. ``spp_mirror`` supports three types of port.

  * ``phy`` : Physical NIC
  * ``ring`` : Ring PMD
  * ``vhost`` : Vhost PMD

``DIR`` means the direction of forwarding and it should be ``rx`` or ``tx``.
``NAME`` is the same as for ``component`` command.

This is an example for adding ports to ``mr1``. In this case, it is configured
to receive packets from ``ring:0`` and send it to ``vhost:0`` and ``vhost:1``
by duplicating the packets.

.. code-block:: console

    # recieve from 'phy:0'
    spp > mirror 2; port add ring:0 rx mr1

    # send to 'ring:0' and 'ring:1'
    spp > mirror 2; port add vhost:0 tx mr1
    spp > mirror 2; port add vhost:1 tx mr1

Adding port may cause component to start packet forwarding. Please see
details in
:ref:`design spp_mirror<spp_design_spp_sec_mirror>`.

Until one rx and two tx ports are registered, ``spp_mirror`` does not start
forwarding. If it is requested to add more than one rx and two tx ports, it
replies an error message.

Deleting port
~~~~~~~~~~~~~

Delete a port which is not be used anymore. It is almost same as adding port.

.. code-block:: console

    spp > mirror SEC_ID; port del RES_UID DIR NAME


Here is some examples.

.. code-block:: console

    # delete rx port 'ring:0' from 'mr1'
    spp > mirror 2; port del ring:0 rx mr1

    # delete tx port 'vhost:1' from 'mr1'
    spp > mirror 2; port del vhost:1 tx mr1


.. note::

  Deleting port may cause component to stop packet forwarding.
  Please see detail in :ref:`design spp_mirror<spp_design_spp_sec_mirror>`.

exit
----

Terminate ``spp_mirror`` process.

.. code-block:: console

    spp > mirror 2; exit
