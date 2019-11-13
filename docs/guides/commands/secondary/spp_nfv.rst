..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2010-2014 Intel Corporation


.. _commands_spp_nfv:

spp_nfv
=======

Each of ``spp_nfv`` and ``spp_vm`` processes is managed with ``nfv`` command.
It is for sending sub commands to secondary with specific ID called
secondary ID.

``nfv`` command takes an secondary ID and a sub command. They must be
separated with delimiter ``;``.
Some of sub commands take additional arguments for speicfying resource
owned by secondary process.

.. code-block:: console

    spp > nfv SEC_ID; SUB_CMD

All of Sub commands are referred with ``help`` command.

.. code-block:: console

    spp > help nfv

    Send a command to secondary process specified with ID.

        SPP secondary process is specified with secondary ID and takes
        sub commands.

        spp > nfv 1; status
        spp > nfv 1; add ring:0
        spp > nfv 1; patch phy:0 ring:0

        You can refer all of sub commands by pressing TAB after
        'nfv 1;'.

        spp > nfv 1;  # press TAB
        add     del     exit    forward patch   status  stop


.. _commands_spp_nfv_status:

status
------

Show running status and ports assigned to the process. If a port is
patched to other port, source and destination ports are shown, or only
source if it is not patched.

.. code-block:: console

    spp > nfv 1; status
    - status: idling
    - lcores: [1, 2]
    - ports:
      - phy:0 -> ring:0
      - phy:1


.. _commands_spp_nfv_add:

add
---

Add a port to the secondary with resource ID.

For example, adding ``ring:0`` by

.. code-block:: console

    spp > nfv 1; add ring:0
    Add ring:0.

Or adding ``vhost:0`` by

.. code-block:: console

    spp > nfv 1; add vhost:0
    Add vhost:0.


.. _commands_spp_nfv_patch:

patch
------

Create a path between two ports, source and destination ports.
This command just creates a path and does not start forwarding.

.. code-block:: console

    spp > nfv 1; patch phy:0 ring:0
    Patch ports (phy:0 -> ring:0).


.. _commands_spp_nfv_forward:

forward
-------

Start forwarding.

.. code-block:: console

    spp > nfv 1; forward
    Start forwarding.

Running status is changed from ``idling`` to ``running`` by
executing it.

.. code-block:: console

    spp > nfv 1; status
    - status: running
    - ports:
      - phy:0
      - phy:1


.. _commands_spp_nfv_stop:

stop
----

Stop forwarding.

.. code-block:: console

    spp > nfv 1; stop
    Stop forwarding.

Running status is changed from ``running`` to ``idling`` by
executing it.

.. code-block:: console

    spp > nfv 1; status
    - status: idling
    - ports:
      - phy:0
      - phy:1


.. _commands_spp_nfv_del:

del
---

Delete a port from the secondary.

.. code-block:: console

    spp > nfv 1; del ring:0
    Delete ring:0.


.. _commands_spp_nfv_exit:

exit
----

Terminate the secondary. For terminating all secondaries,
use ``bye sec`` command instead of it.

.. code-block:: console

    spp > nfv 1; exit
