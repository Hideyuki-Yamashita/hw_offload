..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2018-2019 Nippon Telegraph and Telephone Corporation


.. _spp_ctl_rest_api_proc_independ:

Independent of Process Type
===========================


GET /v1/processes
-----------------

Show SPP processes connected with ``spp-ctl``.


Response
~~~~~~~~

An array of dict of processes in which process type and secondary ID are
defined. So, primary process does not have this ID.

.. _table_spp_ctl_processes_codes:

.. table:: Response code of getting processes.

    +-------+-----------------------+
    | Value | Description           |
    |       |                       |
    +=======+=======================+
    | 200   | Normal response code. |
    +-------+-----------------------+

.. _table_spp_ctl_processes:

.. table:: Response params of getting processes.

    +-----------+---------+--------------------------------------------------+
    | Name      | Type    | Description                                      |
    |           |         |                                                  |
    +===========+=========+==================================================+
    | type      | string  | Process type such as ``primary``, ``nfv`` or so. |
    +-----------+---------+--------------------------------------------------+
    | client-id | integer | Secondary ID, so ``primary`` does not have it.   |
    +-----------+---------+--------------------------------------------------+


Examples
~~~~~~~~

Request
^^^^^^^

.. code-block:: console

    $ curl -X GET -H 'application/json' \
    http://127.0.0.1:7777/v1/processes

Response
^^^^^^^^

.. code-block:: json

    [
      {
        "type": "primary"
      },
      {
        "type": "vf",
        "client-id": 1
      },
      {
        "type": "nfv",
        "client-id": 2
      }
    ]


GET /v1/cpu_layout
------------------

Show CPU layout of a server on which ``spp-ctl`` running.


Response
~~~~~~~~

An array of CPU socket params which consists of each of physical core ID and
logical cores if hyper threading is enabled.

.. _table_spp_ctl_cpu_layout_codes:

.. table:: Response code of CPU layout.

    +-------+-----------------------+
    | Value | Description           |
    |       |                       |
    +=======+=======================+
    | 200   | Normal response code. |
    +-------+-----------------------+


.. _table_spp_ctl_cpu_layout_params:

.. table:: Response params of getting CPU layout.

    +-----------+---------+-------------------------------+
    | Name      | Type    | Description                   |
    |           |         |                               |
    +===========+=========+===============================+
    | cores     | array   | Set of cores on a socket.     |
    +-----------+---------+-------------------------------+
    | core_id   | integer | ID of physical core.          |
    +-----------+---------+-------------------------------+
    | lcores    | array   | Set of IDs of logical cores.  |
    +-----------+---------+-------------------------------+
    | socket_id | integer | Socket ID.                    |
    +-----------+---------+-------------------------------+

Examples
~~~~~~~~

Request
^^^^^^^

.. code-block:: console

    $ curl -X GET -H 'application/json' \
    http://127.0.0.1:7777/v1/cpu_layout

Response
^^^^^^^^

.. code-block:: json

    [
      {
        "cores": [
          {
            "core_id": 1,
            "lcores": [2, 3]
          },
          {
            "core_id": 0,
            "lcores": [0, 1]
          },
          {
            "core_id": 2,
            "lcores": [4, 5]
          }
          {
            "core_id": 3,
            "lcores": [6, 7]
          }
        ],
        "socket_id": 0
      }
    ]


GET /v1/cpu_usage
------------------

Show CPU usage of a server on which ``spp-ctl`` running.


Response
~~~~~~~~

An array of CPU usage of each of SPP processes. This usage consists of
two params, master lcore and lcore set including master and slaves.

.. _table_spp_ctl_cpu_usage_codes:

.. table:: Response code of CPU layout.

    +-------+-----------------------+
    | Value | Description           |
    |       |                       |
    +=======+=======================+
    | 200   | Normal response code. |
    +-------+-----------------------+


.. _table_spp_ctl_cpu_usage_params:

.. table:: Response params of getting CPU layout.

    +--------------+---------+-----------------------------------------------+
    | Name         | Type    | Description                                   |
    |              |         |                                               |
    +==============+=========+===============================================+
    | proc-type    | string  | Proc type such as ``primary``, ``nfv`` or so. |
    +--------------+---------+-----------------------------------------------+
    | master-lcore | integer | Lcore ID of master.                           |
    +--------------+---------+-----------------------------------------------+
    | lcores       | array   | All of Lcore IDs including master and slaves. |
    +--------------+---------+-----------------------------------------------+

Examples
~~~~~~~~

Request
^^^^^^^

.. code-block:: console

    $ curl -X GET -H 'application/json' \
    http://127.0.0.1:7777/v1/cpu_usage

Response
^^^^^^^^

.. code-block:: json

    [
      {
        "proc-type": "primary",
        "master-lcore": 0,
        "lcores": [
          0
        ]
      },
      {
        "proc-type": "nfv",
        "client-id": 2,
        "master-lcore": 1,
        "lcores": [1, 2]
      },
      {
        "proc-type": "vf",
        "client-id": 3,
        "master-lcore": 1,
        "lcores": [1, 3, 4, 5]
      }
    ]
