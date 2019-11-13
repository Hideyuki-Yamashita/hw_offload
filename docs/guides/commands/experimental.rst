..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2017-2019 Nippon Telegraph and Telephone Corporation


.. _commands_experimental:

Experimental Commands
=====================

There are experimental commands in SPP controller.
It might not work for some cases properly because it is not well tested
currently.


.. _commands_experimental_topo:

topo
----

Output network topology in several formats.
Support four types of output.

* Terminal
* Browser (websocket server is required)
* Text (dot, json, yaml)
* Image file (jpg, png, bmp)

This command uses `graphviz
<https://www.graphviz.org/>`_
for generating topology file.
You can also generate a dot formatted file or image files supported by
graphviz.

Here is an example for installing required tools for ``topo term`` command
to output in a terminal.

.. code-block:: console

    $ sudo apt install graphviz \
      imagemagick \
      libsixel-bin

MacOS is also supported optionally for using topo runs on a remote host.
In this case, iTerm2 and imgcat are required as described in the next
section.

To output in browser with ``topo http`` command, install required packages
by using ``requirements.txt`` as described in
:ref:`install SPP<setup_install_spp>`, or only for them as follwoing.

.. code-block:: console

    $ pip3 install tornado \
      websocket-client


Output to Terminal
~~~~~~~~~~~~~~~~~~

Output an image of network configuration in terminal.

.. code-block:: console

    spp > topo term

There are few terminal applications supporting to output image with ``topo``.
You can use mlterm, xterm or other terminals supported by `img2sixel
<https://github.com/saitoha/libsixel>`_.
You can also use `iTerm2
<https://iterm2.com/index.html>`_ on MacOS.
If you use iTerm2, you need to downloada a shell script
``imgcat`` from `iTerm2's displaying support site
<https://iterm2.com/documentation-images.html>`_
and save this script as
``src/controller/3rd_party/imgcat`` with permission ``775``.
``topo`` command tries to ``img2sixel`` first, then ``imgcat``
in the ``3rd_party`` directory.

.. _figure_topo_term_exp:

.. figure:: ../images/commands/expr/topo_term_exp.*
   :width: 67%

   topo term example


Output to Browser
~~~~~~~~~~~~~~~~~

Output an image of network configuration in browser.

.. code-block:: console

    spp > topo http

``topo term`` is useful to understand network configuration intuitively.
However, it should be executed on a node running SPP controller.
You cannnot see the image if you login remote node via ssh and running
SPP controller on remote.

Websocket server is launched from ``src/controller/websocket/spp_ws.py``
to accept client messages.
You should start it before using ``topo term`` command.
Then, open url shown in the terminal (default is
``http://127.0.0.1:8989``).

Browser and SPP controller behave as clients, but have different roles.
Browser behaves as a viwer and SPP controller behaves as a udpater.
If you update network configuration and run ``topo http`` command,
SPP controller sends a message containing network configuration
as DOT language format.
Once the message is accepted, websocket server sends it to viewer clients
immediately.


Output to File
~~~~~~~~~~~~~~

Output a text or image of network configuration to a file.

.. code-block:: console

    spp > topo [FILE_NAME] [FILE_TYPE]

You do not need to specify ``FILE_TYPE`` because ``topo`` is able to
decide file type from ``FILE_NAME``. It is optional.
This is a list of supported file type.

* dot
* js (or json)
* yml (or yaml)
* jpg
* png
* bmp

To generate a DOT file ``network.dot``, run ``topo`` command with
file name.

.. code-block:: console

    # generate DOT file
    spp > topo network.dot
    Create topology: 'network.dot'
    # show contents of the file
    spp > cat network.dot
    digraph spp{
    newrank=true;
    node[shape="rectangle", style="filled"];
    ...

To generate a jpg image, run ``topo`` with the name ``network.jpg``.

.. code-block:: console

    spp > topo network.jpg
    spp > ls
    ...  network.jpg  ...


.. _commands_experimental_topo_subgraph:

topo_subgraph
-------------

``topo_subgraph`` is a supplemental command for managing subgraphs
for ``topo``.

.. code-block:: console

    spp > topo_subgraph VERB LABEL RES_ID1,RES_ID2,...

Each of options are:

* VERB: ``add`` or ``del``
* LABEL: Arbitrary text, such as ``guest_vm1`` or ``container1``
* RES_ID: Series of Resource ID consists of type and ID such as
  ``vhost:1``. Each of resource IDs are separated with ``,`` or
  ``;``.

Subgraph is a group of object defined in dot language. Grouping objects
helps your understanding relationship or hierarchy of each of objects.
It is used for grouping resources on VM or container to be more
understandable.

For example, if you create two vhost interfaces for a guest VM and patch
them to physical ports, ``topo term`` shows a network configuration as
following.

.. _figure_topo_subg_before:

.. figure:: ../images/commands/expr/topo_subg_before.*
   :width: 67%

   Before using topo_subgraph

Two of vhost interfaces are placed outside of ``Host`` while the guest
VM runs on ``Host``.
However, ``vhost:1`` and ``vhost:2`` should be placed inside ``Host``
actually. It is required to use subgraph!

To include guest VM and its resources inside the ``Host``,
use ``topo_subgraph`` with options.
In this case, add subgraph ``guest_vm`` and includes resoures
``vhost:1`` and ``vhost:2`` into the subgraph.

.. code-block:: console

    spp > topo_subgraph add guest_vm vhost:1,vhost:2

.. _figure_topo_subg_after:

.. figure:: ../images/commands/expr/topo_subg_after.*
   :width: 67%

   After using topo_subgraph

All of registered subgraphs are listed by using ``topo_subgraph``
with no options.

.. code-block:: console

    spp > topo_subgraph
    label: guest_vm subgraph: "vhost:1,vhost:2"

If guest VM is shut down and subgraph is not needed anymore,
delete subgraph ``guest_vm``.

.. code-block:: console

    spp > topo_subgraph del guest_vm


.. _commands_experimental_load_cmd:

load_cmd
--------

Load command plugin dynamically while running SPP controller.

.. code-block:: console

    spp > load_cmd [CMD_NAME]

CLI of SPP controller is implemented with ``Shell`` class which is
derived from Python standard library ``Cmd``.
It means that subcommands of SPP controller must be implemented as
a member method named as ``do_xxx``.
For instance, ``status`` subcommand is implemented as ``do_status``
method.

``load_cmd`` is for providing a way to define user specific command
as a plugin.
Plugin file must be placed in ``spp/src/controller/command`` and
command name must be the same as file name.
In addition, ``do_xxx`` method must be defined which is called from
SPP controller.

For example, ``hello`` sample plugin is defined as
``spp/src/controller/command/hello.py`` and ``do_hello`` is defined
in this plugin.
Comment for ``do_hello`` is used as help message for ``hello`` command.

.. code-block:: python

    def do_hello(self, name):
        """Say hello to given user

        spp > hello alice
        Hello, alice!
        """

        if name == '':
            print('name is required!')
        else:
            hl = Hello(name)
            hl.say()

``hello`` is loaded and called as following.

.. code-block:: console

    spp > load_cmd hello
    Module 'command.hello' loaded.
    spp > hello alice
    Hello, alice!
