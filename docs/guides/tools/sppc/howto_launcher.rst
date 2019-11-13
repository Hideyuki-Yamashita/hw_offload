..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2017-2018 Nippon Telegraph and Telephone Corporation

.. _sppc_howto_define_appc:

How to Define Your App Launcher
===============================

SPP container is a set of python script for launching DPDK application
on a container with docker command. You can launch your own application
by preparing a container image and install your application in
the container.
In this chapter, you will understand how to define application container
for your application.


.. _sppc_howto_build_img:

Build Image
-----------

SPP container provides a build tool with version specific Dockerfiles.
You should read the Dockerfiles to understand environmental variable
or command path are defined.
Build tool refer ``conf/env.py`` for the definitions before running
docker build.

Dockerfiles of pktgen or SPP can help your understanding for building
app container in which your application is placed outside of DPDK's
directory.
On the other hand, if you build an app container of DPDK sample
application, you do not need to prepare your Dockerfile because all of
examples are compiled while building DPDK's image.


.. _sppc_howto_create_appc:

Create App Container Script
---------------------------

As explained in :ref:`spp_container_app_launcher`, app container script
shold be prepared for each of applications.
Application of SPP container is roughly categorized as DPDK sample apps
or not. The former case is like that you change an existing DPDK sample
application and run as a app container.

For DPDK sample apps, it is easy to build image and create app container
script.
On the other hand, it is a bit complex because you should you should
define environmental variables, command path and compilation process by
your own.

This section describes how to define app container script,
first for DPDK sample applications,
and then second for other than them.

.. _sppc_howto_dpdk_sample_appc:

DPDK Sample App Container
-------------------------

Procedure of App container script is defined in main() and
consists of three steps of
(1)parsing options, (2)building docker command and
(3)application command run inside the container.

Here is a sample code of :ref:`sppc_appl_helloworld`.
``parse_args()`` is defined in each
of app container scripts to parse all of EAL, docker and application
specific options.
It returns a result of ``parse_args()`` method of
``argparse.ArgumentParser`` class.
App container script uses standard library module ``argparse``
for parsing the arguments.

.. code-block:: python

    def main():
        args = parse_args()

        # Check for other mandatory opitons.
        if args.dev_ids is None:
            common.error_exit('--dev-ids')

        # Setup for vhost devices with given device IDs.
        dev_ids_list = app_helper.dev_ids_to_list(args.dev_ids)
        sock_files = app_helper.sock_files(dev_ids_list)

Each of options is accessible as ``args.dev_ids`` or
``args.core_list``.
Before step (2) and (3), you had better to check given option,
expecially mandatory options.
In this case, ``--dev-ids`` is the mandatory and you should terminate
the application if it is not given.
``common.error_exit()`` is a helper method to print an error message
for given option and do ``exit()``.

Setup of ``dev_ids_list`` and ``sock_files`` is required for launching
container.
``lib/app_helper.py`` defines helper functions commonly used
for app containers.

Then, setup docker command and its options as step (2).
Docker options are setup by using helper method
``setup_docker_opts()`` which generates commonly used options for app
containers.
This methods returns a list of a part of options to give it to
``subprocess.call()``.

.. code-block:: python

    # Setup docker command.
    docker_cmd = ['sudo', 'docker', 'run', '\\']
    docker_opts = app_helper.setup_docker_opts(
        args, target_name, sock_files)

You should notice a option ``target_name``.
It is used as a label to choose which of container image you use.
The name of container image is defined as a combination of basename,
distribution name and version.
Basename is defined as a member of ``CONTAINER_IMG_NAME`` in
``conf/env.py``.

.. code-block:: python

    # defined in conf/env.py
    CONTAINER_IMG_NAME = {
        'dpdk': 'sppc/dpdk',
        'pktgen': 'sppc/pktgen',
        'spp': 'sppc/spp'}

This usecase is for DPDK sample app, so you should define target as
``dpdk``.
You do not need to change for using DPDK sample apps in general.
But it can be changed by using other target name.
For example, if you give target ``pktgen`` and
use default dist name and verion of ``ubuntu`` and ``latest``,
The name of image is ``sppc/pktgen-ubuntu:latest``.

For using images other than defined above, you can override it with
``--container-image`` option.
It enables to use any of container images and applications.

You also notice that ``docker_cmd`` has ``\\`` at the end of the list.
It is only used to format the printed command on the terminal.
If you do no care about formatting, you do not need to add it.

Next step is (3), to setup the application command.
You should change ``cmd_path`` and ``file_prefix`` to specify
the application.
For ``cmd_path``, ``helloworld`` should be changed to other name of
application, for example,

.. code-block:: python

    # Setup helloworld run on container.
    cmd_path = '%s/examples/helloworld/%s/helloworld' % (
        env.RTE_SDK, env.RTE_TARGET)

    hello_cmd = [cmd_path, '\\']

    file_prefix = 'spp-hello-container%d' % dev_ids_list[0]
    eal_opts = app_helper.setup_eal_opts(args, file_prefix)

    # No application specific options for helloworld
    hello_opts = []

``file_prefix`` for EAL option should be unique on the system
because it is used as the name of hugepage file.
In SPP container, it is a combination of fixed text and vhost device ID
because this ID is unique in SPP container and cannot be overlapped,
at least among app containers in SPP container.
EAL options are also generated by helper method.

Finally, combine all of commands and its options and launch
from ``subprocess.call()``.

.. code-block:: python

    cmds = docker_cmd + docker_opts + hello_cmd + eal_opts + hello_opts
    if cmds[-1] == '\\':
        cmds.pop()
    common.print_pretty_commands(cmds)

    if args.dry_run is True:
        exit()

    # Remove delimiters for print_pretty_commands().
    while '\\' in cmds:
        cmds.remove('\\')
    subprocess.call(cmds)

All of commands and options are combined in to a list ``cmds``
to give it to ``subprocess.call()``.
You can ignore procedures for ``\\`` and
``common.print_pretty_commands()``
if you do not care about printing commands in the terminal.
However, you should not to shortcut for ``args.dry_run`` because
it is very important for users to check the command syntax
before running it.


.. _sppc_howto_dpdk_appc_nots:

App Container not for DPDK Sample
---------------------------------

There are several application using DPDK but not included in
`sample applications
<https://dpdk.org/doc/guides/sample_app_ug/index.html>`_.
``pktgen.py`` is an example of this type of app container.
As described in :ref:`sppc_howto_dpdk_sample_appc`,
app container consists of three steps and it is the same for
this case.

First of all, you define parsing option for EAL, docker and
your application.

.. code-block:: python

    def parse_args():
        parser = argparse.ArgumentParser(
            description="Launcher for pktgen-dpdk application container")

        parser = app_helper.add_eal_args(parser)
        parser = app_helper.add_appc_args(parser)

        parser.add_argument(
            '-s', '--pcap-file',
            type=str,
            help="PCAP packet flow file of port, defined as 'N:filename'")
        parser.add_argument(
            '-f', '--script-file',
            type=str,
            help="Pktgen script (.pkt) to or a Lua script (.lua)")
        ...

        parser = app_helper.add_sppc_args(parser)
        return parser.parse_args()

It is almost the same as :ref:`sppc_howto_dpdk_sample_appc`,
but it has options for ``pktgen`` itself.
For your application, you can simply add options to ``parser`` object.

.. code-block:: python

    def main():
        args = parse_args()

        # Setup for vhost devices with given device IDs.
        dev_ids_list = app_helper.dev_ids_to_list(args.dev_ids)
        sock_files = app_helper.sock_files(dev_ids_list)

        # Setup docker command.
        docker_cmd = ['sudo', 'docker', 'run', '\\']
        docker_opts = app_helper.setup_docker_opts(
            args, target_name, sock_files,
            '%s/../pktgen-dpdk' % env.RTE_SDK)

        cmd_path = '%s/../pktgen-dpdk/app/%s/pktgen' % (
            env.RTE_SDK, env.RTE_TARGET)

Setup for docker command is the same as the example.
The ``terget_name`` might be different from the image you will use,
but you do not need to care about which of container image is used
because it is overriden with given image with ``--container-image``
option.
However, you should care about the path of application ``cmd_path``
which is run in the container.

Then, you should decide ``file_prefix`` to your application container
be unique on the system.
The ``file_prefix`` of SPP container is named as
``spp-[APP_NAME]-container[VHOST_ID]`` convensionally to it be unique.

.. code-block:: python

    # Setup pktgen command
    pktgen_cmd = [cmd_path, '\\']

    file_prefix = 'spp-pktgen-container%d' % dev_ids_list[0]
    eal_opts = app_helper.setup_eal_opts(args, file_prefix)

You should check the arguments for the application.

.. code-block:: python

    ...
    if args.pcap_file is not None:
        pktgen_opts += ['-s', args.pcap_file, '\\']

    if args.script_file is not None:
        pktgen_opts += ['-f', args.script_file, '\\']

    if args.log_file is not None:
        pktgen_opts += ['-l', args.log_file, '\\']
    ...

Finally, combine all of commands and its options and launch
from ``subprocess.call()``.

.. code-block:: python

    cmds = docker_cmd + docker_opts + pktgen_cmd + eal_opts + pktgen_opts
    if cmds[-1] == '\\':
        cmds.pop()
    common.print_pretty_commands(cmds)

    if args.dry_run is True:
        exit()

    # Remove delimiters for print_pretty_commands().
    while '\\' in cmds:
        cmds.remove('\\')
    subprocess.call(cmds)

As you can see, it is almost the same as DPDK sample app container
without application path and options of application specific.
