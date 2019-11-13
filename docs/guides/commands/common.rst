..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2010-2014 Intel Corporation
    Copyright(c) 2017-2019 Nippon Telegraph and Telephone Corporation


.. _commands_common:

Common Commands
===============

.. _commands_common_status:

status
------

Show the status of SPP processes.

.. code-block:: console

    spp > status
    - spp-ctl:
      - address: 172.30.202.151:7777
    - primary:
      - status: running
    - secondary:
      - processes:
        1: nfv:1
        2: vf:3


.. _commands_common_config:

config
------

Show or update config params.

Config params used for changing behaviour of SPP CLI. For instance, if you
change command prompt, you can set any of prompt with ``config`` command
other than default ``spp >``.

.. code-block:: none

    # set prompt to "$ spp "
    spp > config prompt "$ spp "
    Set prompt: "$ spp "
    $ spp


Show Config
~~~~~~~~~~~

To show the list of config all of params, simply run ``config``.

.. code-block:: none

    # show list of config
    spp > config
    - max_secondary: "16"       # The maximum number of secondary processes
    - sec_nfv_nof_lcores: "1"   # Default num of lcores for workers of spp_nfv
    - topo_size: "60%"  # Percentage or ratio of topo
    - sec_base_lcore: "1"       # Shared lcore among secondaries
    ....

Or show params only started from ``sec_``, add the keyword to the commnad.

.. code-block:: none

    # show config started from `sec_`
    spp > config sec_
    - sec_vhost_cli: "" # Vhost client mode, activated if set any of values
    - sec_mem: "-m 512" # Mem size
    - sec_nfv_nof_lcores: "1"   # Default num of lcores for workers of spp_nfv
    - sec_base_lcore: "1"       # Shared lcore among secondaryes
    ....

Set Config
~~~~~~~~~~

One of typical uses of ``config`` command is to change the default params of
other commands. ``pri; launch`` takes several options for launching secondary
process and it is completed by using default params started from ``sec_``.

.. code-block:: none

    spp > pri; launch nfv 2  # press TAB for completion
    spp > pri; launch nfv 2 -l 1,2 -m 512 -- -n 2 -s 192.168.11.59:6666

The default number of memory size is ``-m 512`` and the definition
``sec_mem`` can be changed with ``config`` command.
Here is an example of changing ``-m 512`` to ``--socket-mem 512,0``.

.. code-block:: none

    spp > config sec_mem "--socket-mem 512,0"
    Set sec_mem: "--socket-mem 512,0"

After updating the param, expanded options is also updated.

.. code-block:: none

    spp > pri; launch nfv 2  # press TAB for completion
    spp > pri; launch nfv 2 -l 1,2 --socket-mem 512,0 -- -n 2 -s ...


.. _commands_common_playback:

playback
--------

Restore network configuration from a recipe file which defines a set
of SPP commands.
You can prepare a recipe file by using ``record`` command or editing
file by hand.

It is recommended to use extension ``.rcp`` to be self-sxplanatory as
a recipe, although you can use any of extensions such as ``.txt`` or
``.log``.

.. code-block:: console

    spp > playback /path/to/my.rcp


.. _commands_common_record:

record
------

Start recording user's input and create a recipe file for loading
from ``playback`` commnad.
Recording recipe is stopped by executing ``exit`` or ``playback``
command.

.. code-block:: console

    spp > record /path/to/my.rcp

.. note::

    It is not supported to stop recording without ``exit`` or ``playback``
    command.
    It is planned to support ``stop`` command for stopping record in
    next relase.


.. _commands_common_history:

history
-------

Show command history. Command history is recorded in a file named
``$HOME/.spp_history``. It does not add some command which are no
meaning for history, ``bye``, ``exit``, ``history`` and ``redo``.

.. code-block:: console

    spp > history
      1  ls
      2  cat file.txt


.. _commands_common_redo:

redo
----

Execute command of index of history.

.. code-block:: console

    spp > redo 5  # exec 5th command in the history


.. _commands_common_server:

server
------

Switch SPP REST API server.

SPP CLI is able to manage several SPP nodes via REST API servers.
It is also abaivable to register new one, or unregister.

Show all of registered servers by running ``server list`` or simply
``server``. Notice that ``*`` means that the first node is under the
control of SPP CLI.

.. code-block:: console

    spp > server
      1: 192.168.1.101:7777 *
      2: 192.168.1.102:7777

    spp > server list  # same as above
      1: 192.168.1.101:7777 *
      2: 192.168.1.102:7777

Switch to other server by running ``server`` with index or address displayed
in the list. Port number can be omitted if it is default ``7777``.

.. code-block:: console

    # Switch to the second node
    spp > server 2
    Switch spp-ctl to "2: 192.168.1.102:7777".

    # Switch to firt one again with address
    spp > server 192.168.1.101  # no need for port num
    Switch spp-ctl to "1: 192.168.1.101:7777".

Register new one by using ``add`` command, or unregister by ``del`` command
with address. For unregistering, node is also specified with index.

.. code-block:: console

    # Register
    spp > server add 192.168.122.177
    Registered spp-ctl "192.168.122.177:7777".

    # Unregister second one
    spp > server del 2  # or 192.168.1.102
    Unregistered spp-ctl "192.168.1.102:7777".

You cannot unregister node under the control, or switch to other one before.

.. code-block:: console

    spp > server del 1
    Cannot del server "1" in use!


.. _commands_common_pwd:

pwd
---

Show current path.

.. code-block:: console

    spp> pwd
    /path/to/curdir


.. _commands_common_cd:

cd
--

Change current directory.

.. code-block:: console

    spp> cd /path/to/dir


.. _commands_common_ls:

ls
--

Show a list of directory contents.

.. code-block:: console

    spp> ls /path/to/dir


.. _commands_common_mkdir:

mkdir
-----

Make a directory.

.. code-block:: console

    spp> mkdir /path/to/dir


.. _commands_common_cat:

cat
---

Show contents of a file.

.. code-block:: console

    spp> cat /path/to/file


.. _commands_common_less:

less
----

Show contents of a file.

.. code-block:: console

    spp> less /path/to/file


.. _commands_common_bye:

bye
---

``bye`` command is for terminating SPP processes.
It supports two types of termination as sub commands.

  - sec
  - all

First one is for terminating only secondary processes at once.

.. code-block:: console

    spp > bye sec
    Closing secondary ...
    Exit nfv 1
    Exit vf 3.


Second one is for all SPP processes other than controller.

.. code-block:: console

    spp > bye all
    Closing secondary ...
    Exit nfv 1
    Exit vf 3.
    Closing primary ...
    Exit primary


.. _commands_common_exit:

exit
----

Same as ``bye`` command but just for terminating SPP controller and
not for other processes.

.. code-block:: console

    spp > exit
    Thank you for using Soft Patch Panel


.. _commands_common_help:

help
----

Show help message for SPP commands.

.. code-block:: console

    spp > help

    Documented commands (type help <topic>):
    ========================================
    bye  exit     inspect   ls      nfv       pwd     server  topo_resize
    cat  help     less      mirror  playback  record  status  topo_subgraph
    cd   history  load_cmd  mkdir   pri       redo    topo    vf

    spp > help status
    Display status info of SPP processes

        spp > status

    spp > help nfv
    Send a command to spp_nfv specified with ID.

        Spp_nfv is specified with secondary ID and takes sub commands.

        spp > nfv 1; status
        spp > nfv 1; add ring:0
        spp > nfv 1; patch phy:0 ring:0

        You can refer all of sub commands by pressing TAB after
        'nfv 1;'.

        spp > nfv 1;  # press TAB
        add     del     exit    forward patch   status  stop
