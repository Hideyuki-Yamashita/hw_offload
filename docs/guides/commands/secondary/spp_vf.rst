..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2010-2014 Intel Corporation

.. _commands_spp_vf:

spp_vf
======

``spp_vf`` is a kind of SPP secondary process. It is introduced for
providing SR-IOV like features.

Each of ``spp_vf`` processes is managed with ``vf`` command. It is for
sending sub commands with specific ID called secondary ID for changing
configuration, assigning or releasing resources.

Secondary ID is referred as ``--client-id`` which is given as an argument
while launching ``spp_vf``. It should be unique among all of secondary
processes including ``spp_nfv`` and others.

``vf`` command takes an secondary ID and one of sub commands. Secondary ID
and sub command should be separated with delimiter ``;``, or failed to a
command error. Some of sub commands take additional arguments for
configuration of the process or its resource management.

.. code-block:: console

    spp > vf SEC_ID; SUB_CMD

In this example, ``SEC_ID`` is a secondary ID and ``SUB_CMD`` is one of the
following sub commands. Details of each of sub commands are described in the
next sections.

* status
* component
* port
* classifier_table

``spp_vf`` supports TAB completion. You can complete all of the name
of commands and its arguments. For instance, you find all of sub commands
by pressing TAB after ``vf SEC_ID;``.

.. code-block:: console

    spp > vf 1;  # press TAB key
    classifier_table  component      port        status

It tries to complete all of possible arguments. However, ``spp_vf`` takes
also an arbitrary parameter which cannot be predicted, for example, name of
component MAC address. In this cases, ``spp_vf`` shows capitalized keyword
for indicating it is an arbitrary parameter. Here is an exmaple of
``component`` command to initialize a worker thread. Keyword ``NAME`` should
be replaced with your favorite name for the worker of the role.

.. code-block:: console

    spp > vf 1; component st  # press TAB key to show args starting 'st'
    start  stop
    spp > vf 1; component start NAME  # 'NAME' is shown with TAB after start
    spp > vf 1; component start fw1   # replace 'NAME' with your favorite name
    spp > vf 1; component start fw1   # then, press TAB to show core IDs
    5  6  7  8

It is another example of replacing keyword. ``port`` is a sub command for
assigning a resource to a worker thread. ``RES_UID`` is replaced with
resource UID which is a combination of port type and its ID such as
``ring:0`` or ``vhost:1`` to assign it as RX port of forwarder ``fw1``.

.. code-block:: console

    spp > vf 1; port add RES_UID
    spp > vf 1; port add ring:0 rx fw1

If you are reached to the end of arguments, no candidate keyword is displayed.
It is a completed statement of ``component`` command, and TAB
completion does not work after ``forward`` because it is ready to run.

.. code-block:: console

    spp > vf 1; component start fw1 5 forward
    Succeeded to start component 'fw1' on core:5

It is also completed secondary IDs of ``spp_vf`` and it is helpful if you run
several ``spp_vf`` processes.

.. code-block:: console

    spp > vf  # press TAB after space following 'vf'
    1;  3;    # you find two spp_vf processes of sec ID 1, 3

By the way, it is also a case of no candidate keyword is displayed if your
command statement is wrong. You might be encountered an error if you run the
wrong command. Please take care.

.. code-block:: console

    spp > vf 1; compo  # no candidate shown for wrong command
    Invalid command "compo".


.. _commands_spp_vf_status:

status
------

Show the information of worker threads and its resources. Status information
consists of three parts.

.. code-block:: console

    spp > vf 1; status
    Basic Information:
      - client-id: 3
      - ports: [phy:0, phy:1, ring:0, ring:1, ring:2, ring:3, ring:4]
      - lcore_ids:
        - master: 2
        - slaves: [3, 4, 5, 6]
    Classifier Table:
      - C0:8E:CD:38:EA:A8, ring:4
      - C0:8E:CD:38:BC:E6, ring:3
    Components:
      - core:5 'fw1' (type: forward)
        - rx: ring:0
        - tx: ring:1
      - core:6 'mg' (type: merge)
      - core:7 'cls' (type: classifier)
        - rx: ring:2
        - tx: ring:3
        - tx: ring:4
      - core:8 '' (type: unuse)

``Basic Information`` is for describing attributes of ``spp_vf`` itself.
``client-id`` is a secondary ID of the process and ``ports`` is a list of
all of ports owned the process.

``Classifier Table`` is a list of entries of ``classifier`` worker thread.
Each of entry is a combination of MAC address and destination port which is
assigned to this thread.

``Components`` is a list of all of worker threads. Each of workers has a
core ID running on, type of the worker and a list of resources.
Entry of no name with ``unuse`` type means that no worker thread assigned to
the core. In other words, it is ready to be assigned.


.. _commands_spp_vf_component:

component
---------

Assign or release a role of forwarding to worker threads running on each of
cores which are reserved with ``-c`` or ``-l`` option while launching
``spp_vf``. The role of the worker is chosen from ``forward``, ``merge`` or
``classifier``.

``forward`` role is for simply forwarding from source port to destination port.
On the other hands, ``merge`` role is for receiving packets from multiple ports
as N:1 communication, or ``classifier`` role is for sending packet to
multiple ports by referring MAC address as 1:N communication.

You are required to give an arbitrary name with as an ID for specifying the role.
This name is also used while releasing the role.

.. code-block:: console

    # assign 'ROLE' to worker on 'CORE_ID' with a 'NAME'
    spp > vf SEC_ID; component start NAME CORE_ID ROLE

    # release worker 'NAME' from the role
    spp > vf SEC_ID; component stop NAME

Here are some examples of assigning roles with ``component`` command.

.. code-block:: console

    # assign 'forward' role with name 'fw1' on core 2
    spp > vf 2; component start fw1 2 forward

    # assign 'merge' role with name 'mgr1' on core 3
    spp > vf 2; component start mgr1 3 merge

    # assign 'classifier' role with name 'cls1' on core 4
    spp > vf 2; component start cls1 4 classifier

In the above examples, each different ``CORE-ID`` is specified to each role.
You can assign several components on the same core, but performance might be
decreased. This is an example for assigning two roles of ``forward`` and
``merge`` on the same ``core 2``.

.. code-block:: console

    # assign two roles on the same 'core 2'.
    spp > vf 2; component start fw1 2 forward
    spp > vf 2; component start mgr1 2 merge

Examples of releasing roles.

.. code-block:: console

    # release roles
    spp > vf 2; component stop fw1
    spp > vf 2; component stop mgr1
    spp > vf 2; component stop cls1


.. _commands_spp_vf_port:

port
----

Add or delete a port to a worker.

Adding port
~~~~~~~~~~~

.. code-block:: console

    spp > vf SEC_ID; port add RES_UID DIR NAME

``RES_UID`` is with replaced with resource UID such as ``ring:0`` or
``vhost:1``. ``spp_vf`` supports three types of port.

  * ``phy`` : Physical NIC
  * ``ring`` : Ring PMD
  * ``vhost`` : Vhost PMD

``DIR`` means the direction of forwarding and it should be ``rx`` or ``tx``.
``NAME`` is the same as for ``component`` command.

This is an example for adding ports to a classifer ``cls1``. In this case,
it is configured to receive packets from ``phy:0`` and send it to ``ring:0``
or ``ring:1``. The destination is decided with MAC address of the packets
by referring the table. How to configure the table is described in
:ref:`classifier_table<commands_spp_vf_classifier_table>` command.

.. code-block:: console

    # recieve from 'phy:0'
    spp > vf 2; port add phy:0 rx cls1

    # send to 'ring:0' and 'ring:1'
    spp > vf 2; port add ring:0 tx cls1
    spp > vf 2; port add ring:1 tx cls1

``spp_vf`` also supports VLAN features, adding or deleting VLAN tag.
It is used remove VLAN tags from incoming packets from outside of host
machine, or add VLAN tag to outgoing packets.

To configure VLAN features, use additional sub command ``add_vlantag``
or ``del_vlantag`` followed by ``port`` sub command.

To remove VLAN tag, simply add ``del_vlantag`` sub command without arguments.

.. code-block:: console

    spp > vf SEC_ID; port add RES_UID DIR NAME del_vlantag

On the other hand, use ``add_vlantag`` which takes two arguments,
``VID`` and ``PCP``, for adding VLAN tag to the packets.

.. code-block:: console

    spp > vf SEC_ID; port add RES_UID DIR NAME add_vlantag VID PCP

``VID`` is a VLAN ID and ``PCP`` is a Priority Code Point defined in
`IEEE 802.1p
<https://1.ieee802.org/>`_.
It is used for QoS by defining priority ranged from lowest prioroty
``0`` to the highest ``7``.

Here is an example of use of VLAN features considering a use case of
a forwarder removes VLAN tag from incoming packets and another forwarder
adds VLAN tag before sending packet outside.

.. code-block:: console

    # remove VLAN tag in forwarder 'fw1'
    spp > vf 2; port add phy:0 rx fw1 del_vlantag

    # add VLAN tag with VLAN ID and PCP in forwarder 'fw2'
    spp > vf 2; port add phy:1 tx fw2 add_vlantag 101 3

Adding port may cause component to start packet forwarding. Please see
detail in
:ref:`design spp_vf<spp_design_spp_sec_vf>`.

Until one rx port and one tx port are added, forwarder does not start packet
forwarding. If it is requested to add more than one rx and one tx port, it
replies an error message.
Until at least one rx port and two tx ports are added, classifier does not
start packet forwarding. If it is requested to add more than two rx ports, it
replies an error message.
Until at least two rx ports and one tx port are added, merger does not start
packet forwarding. If it is requested to add more than two tx ports, it replies
an error message.

Deleting port
~~~~~~~~~~~~~

Delete a port which is not used anymore.

.. code-block:: console

    spp > vf SEC_ID; port del RES_UID DIR NAME

It is same as the adding port, but no need to add additional sub command
for VLAN features.

Here is an example.

.. code-block:: console

    # delete rx port 'ring:0' from 'cls1'
    spp > vf 2; port del ring:0 rx cls1

    # delete tx port 'vhost:1' from 'mgr1'
    spp > vf 2; port del vhost:1 tx mgr1

.. note::

   Deleting port may cause component to stop packet forwarding.
   Please see detail in :ref:`design spp_vf<spp_design_spp_sec_vf>`.

.. _commands_spp_vf_classifier_table:

classifier_table
----------------

Register an entry of a combination of MAC address and port to
a table of classifier.

.. code-block:: console

    # add entry
    spp > vf SEC_ID; classifier_table add mac MAC_ADDR RES_UID

    # delete entry
    spp > vf SEC_ID; classifier_table del mac MAC_ADDRESS RES_ID

This is an example to register MAC address ``52:54:00:01:00:01``
with port ``ring:0``.

.. code-block:: console

    spp > vf 1; classifier_table add mac 52:54:00:01:00:01 ring:0

Classifier supports the ``default`` entry for packets which does not
match any of entries in the table. If you assign ``ring:1`` as default,
simply specify ``default`` instead of MAC address.

.. code-block:: console

    spp > vf 1; classifier_table add mac default ring:1

``classifier_table`` sub command also supports VLAN features as similar
to ``port``.

.. code-block:: console

    # add entry with VLAN features
    spp > vf SEC_ID; classifier_table add vlan VID MAC_ADDR RES_UID

    # delete entry of VLAN
    spp > vf SEC_ID; classifier_table del vlan VID MAC_ADDR RES_UID

Here is an example for adding entries.

.. code-block:: console

    # add entry with VLAN tag
    spp > vf 1; classifier_table add vlan 101 52:54:00:01:00:01 ring:0

    # add entry of default with VLAN tag
    spp > vf 1; classifier_table add vlan 101 default ring:1

Delete an entry. This is an example to delete an entry with VLAN tag 101.

.. code-block:: console

    # delete entry with VLAN tag
    spp > vf 1; classifier_table del vlan 101 52:54:00:01:00:01 ring:0

exit
----

Terminate the spp_vf.

.. code-block:: console

    spp > vf 1; exit
