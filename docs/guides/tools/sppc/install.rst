..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2017-2018 Nippon Telegraph and Telephone Corporation

.. _spp_container_install:

Install
=======


.. _sppc_install_required:

Required Packages
-----------------

You need to install packages required for DPDK, and docker.

* DPDK 17.11 or later (supporting container)
* docker


.. _sppc_install_config:

Configurations
--------------

You might need some additional non-mandatory configurations.

Run docker without sudo
~~~~~~~~~~~~~~~~~~~~~~~

You should run docker as sudo in most of scripts provided in
SPP container because for running DPDK applications.

However, you can run docker without sudo if you do not run DPDK
applications.
It is useful if you run ``docker kill`` for terminating containerized
process, ``docker rm`` or ``docker rmi`` for cleaning resources.

.. code-block:: console

    # Terminate container from docker command
    $ docker kill xxxxxx_yyyyyyy

    # Remove all of containers
    $ docker rm `docker ps -aq`

    # Remove all of images
    $ docker rmi `docker images -aq`

The reason for running docker requires sudo is docker daemon
binds to a unix socket instead of a TCP port.
Unix socket is owned by root and other users can only access it using
sudo.
However, you can run if you add your account to docker group.

.. code-block:: console

    $ sudo groupadd docker
    $ sudo usermod -aG docker $USER

To activate it, you have to logout and re-login at once.


Network Proxy
~~~~~~~~~~~~~

If you are behind a proxy, you should configure proxy to build an image
or running container.
SPP container is supportng proxy configuration for getting
it from shell environments.
You confirm that ``http_proxy``, ``https_proxy``
and ``no_proxy`` of environmental variables are defined.

It also required to add proxy configurations for docker daemon.
Proxy for docker daemon is defined as ``[Service]`` entry in
``/etc/systemd/system/docker.service.d/http-proxy.conf``.

.. code-block:: console

    [Service]
    Environment="HTTP_PROXY=http:..." "HTTPS_PROXY=https..." "NO_PROXY=..."

To activate it, you should restart docker daemon.

.. code-block:: console

    $ systemctl daemon-reload
    $ systemctl restart docker

You can confirm that environments are updated by running
``docker info``.
