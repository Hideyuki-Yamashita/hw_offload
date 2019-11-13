..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2018-2019 Nippon Telegraph and Telephone Corporation

.. _spp_ctl_rest_api_spp_mirror:


spp_mirror
==========

GET /v1/mirrors/{client_id}
---------------------------

Get the information of the ``spp_mirror`` process.

* Normal response codes: 200
* Error response codes: 400, 404


Request (path)
~~~~~~~~~~~~~~

.. _table_spp_ctl_mirrors_get:

.. table:: Request parameter for getting spp_mirror.

    +-----------+---------+--------------------------+
    | Name      | Type    | Description              |
    |           |         |                          |
    +===========+=========+==========================+
    | client_id | integer | client id.               |
    +-----------+---------+--------------------------+


Request example
~~~~~~~~~~~~~~~

.. code-block:: console

    $ curl -X GET -H 'application/json' \
      http://127.0.0.1:7777/v1/mirrors/1


Response
~~~~~~~~

.. _table_spp_ctl_spp_mirror_res:

.. table:: Response params of getting spp_mirror.

    +------------------+---------+-----------------------------------------------+
    | Name             | Type    | Description                                   |
    |                  |         |                                               |
    +==================+=========+===============================================+
    | client-id        | integer | client id.                                    |
    +------------------+---------+-----------------------------------------------+
    | ports            | array   | an array of port ids used by the process.     |
    +------------------+---------+-----------------------------------------------+
    | components       | array   | an array of component objects in the process. |
    +------------------+---------+-----------------------------------------------+

Component objects:

.. _table_spp_ctl_spp_mirror_res_comp:

.. table:: Component objects of getting spp_mirror.

    +---------+---------+---------------------------------------------------------------------+
    | Name    | Type    | Description                                                         |
    |         |         |                                                                     |
    +=========+=========+=====================================================================+
    | core    | integer | core id running on the component                                    |
    +---------+---------+---------------------------------------------------------------------+
    | name    | string  | an array of port ids used by the process.                           |
    +---------+---------+---------------------------------------------------------------------+
    | type    | string  | an array of component objects in the process.                       |
    +---------+---------+---------------------------------------------------------------------+
    | rx_port | array   | an array of port objects connected to the rx side of the component. |
    +---------+---------+---------------------------------------------------------------------+
    | tx_port | array   | an array of port objects connected to the tx side of the component. |
    +---------+---------+---------------------------------------------------------------------+

Port objects:

.. _table_spp_ctl_spp_mirror_res_port:

.. table:: Port objects of getting spp_vf.

    +---------+---------+---------------------------------------------------------------+
    | Name    | Type    | Description                                                   |
    |         |         |                                                               |
    +=========+=========+===============================================================+
    | port    | string  | port id. port id is the form {interface_type}:{interface_id}. |
    +---------+---------+---------------------------------------------------------------+


Response example
~~~~~~~~~~~~~~~~

.. code-block:: json

    {
      "client-id": 1,
      "ports": [
        "phy:0", "phy:1", "ring:0", "ring:1", "ring:2"
      ],
      "components": [
        {
          "core": 2,
          "name": "mr0",
          "type": "mirror",
          "rx_port": [
            {
            "port": "ring:0"
            }
          ],
          "tx_port": [
            {
              "port": "ring:1"
            },
            {
              "port": "ring:2"
            }
          ]
        },
        {
          "core": 3,
          "type": "unuse"
        }
      ]
    }

The component which type is ``unused`` is to indicate unused core.


Equivalent CLI command
~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: none

    spp > mirror {client_id}; status


POST /v1/mirrors/{client_id}/components
---------------------------------------

Start component.

* Normal response codes: 204
* Error response codes: 400, 404


Request (path)
~~~~~~~~~~~~~~

.. _table_spp_ctl_spp_mirror_components:

.. table:: Request params of components of spp_mirror.

    +-----------+---------+-------------+
    | Name      | Type    | Description |
    +===========+=========+=============+
    | client_id | integer | client id.  |
    +-----------+---------+-------------+


Request (body)
~~~~~~~~~~~~~~

.. _table_spp_ctl_spp_mirror_components_res:

.. table:: Response params of components of spp_mirror.

    +-----------+---------+----------------------------------------------------------------------+
    | Name      | Type    | Description                                                          |
    |           |         |                                                                      |
    +===========+=========+======================================================================+
    | name      | string  | component name. must be unique in the process.                       |
    +-----------+---------+----------------------------------------------------------------------+
    | core      | integer | core id.                                                             |
    +-----------+---------+----------------------------------------------------------------------+
    | type      | string  | component type. only ``mirror`` is available.                        |
    +-----------+---------+----------------------------------------------------------------------+


Request example
~~~~~~~~~~~~~~~

.. code-block:: console

    $ curl -X POST -H 'application/json' \
      -d '{"name": "mr1", "core": 12, "type": "mirror"}' \
      http://127.0.0.1:7777/v1/mirrors/1/components


Response
~~~~~~~~

There is no body content for the response of a successful ``POST`` request.


Equivalent CLI command
~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: none

    spp > mirror {client_id}; component start {name} {core} {type}


DELETE /v1/mirrors/{client_id}/components/{name}
------------------------------------------------

Stop component.

* Normal response codes: 204
* Error response codes: 400, 404


Request (path)
~~~~~~~~~~~~~~

.. _table_spp_ctl_spp_mirror_del:

.. table:: Request params of deleting component of spp_mirror.

    +-----------+---------+---------------------------------+
    | Name      | Type    | Description                     |
    |           |         |                                 |
    +===========+=========+=================================+
    | client_id | integer | client id.                      |
    +-----------+---------+---------------------------------+
    | name      | string  | component name.                 |
    +-----------+---------+---------------------------------+


Request example
~~~~~~~~~~~~~~~

.. code-block:: console

    $ curl -X DELETE -H 'application/json' \
      http://127.0.0.1:7777/v1/mirrors/1/components/mr1


Response
~~~~~~~~

There is no body content for the response of a successful ``POST`` request.


Equivalent CLI command
~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: none

    spp > mirror {client_id}; component stop {name}


PUT /v1/mirrors/{client_id}/components/{name}/ports
---------------------------------------------------

Add or delete port to the component.

* Normal response codes: 204
* Error response codes: 400, 404


Request (path)
~~~~~~~~~~~~~~

.. _table_spp_ctl_spp_mirror_comp_port:

.. table:: Request params for ports of component of spp_mirror.

    +-----------+---------+---------------------------+
    | Name      | Type    | Description               |
    |           |         |                           |
    +===========+=========+===========================+
    | client_id | integer | client id.                |
    +-----------+---------+---------------------------+
    | name      | string  | component name.           |
    +-----------+---------+---------------------------+


Request (body)
~~~~~~~~~~~~~~

.. _table_spp_ctl_spp_mirror_comp_port_body:

.. table:: Request body params for ports of component of spp_mirror.

    +---------+---------+-----------------------------------------------------------------+
    | Name    | Type    | Description                                                     |
    |         |         |                                                                 |
    +=========+=========+=================================================================+
    | action  | string  | ``attach`` or ``detach``.                                       |
    +---------+---------+-----------------------------------------------------------------+
    | port    | string  | port id. port id is the form {interface_type}:{interface_id}.   |
    +---------+---------+-----------------------------------------------------------------+
    | dir     | string  | ``rx`` or ``tx``.                                               |
    +---------+---------+-----------------------------------------------------------------+


Request example
~~~~~~~~~~~~~~~

Attach rx port of ``ring:1`` to component named ``mr1``.

.. code-block:: console

    $ curl -X PUT -H 'application/json' \
      -d '{"action": "attach", "port": "ring:1", "dir": "rx"}' \
      http://127.0.0.1:7777/v1/mirrors/1/components/mr1/ports

Detach tx port of ``ring:1`` from component named ``mr1``.

.. code-block:: console

    $ curl -X PUT -H 'application/json' \
      -d '{"action": "detach", "port": "ring:0", "dir": "tx"}' \
      http://127.0.0.1:7777/v1/mirrors/1/components/mr1/ports


Response
~~~~~~~~

There is no body content for the response of a successful ``PUT`` request.


Equivalent CLI command
~~~~~~~~~~~~~~~~~~~~~~

Action is ``attach``.

.. code-block:: none

    spp > mirror {client_id}; port add {port} {dir} {name}

Action is ``detach``.

.. code-block:: none

    spp > mirror {client_id}; port del {port} {dir} {name}
