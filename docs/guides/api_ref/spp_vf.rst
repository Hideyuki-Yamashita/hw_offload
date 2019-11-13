..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2018-2019 Nippon Telegraph and Telephone Corporation


.. _spp_ctl_rest_api__spp_vf:

spp_vf
======

GET /v1/vfs/{client_id}
-----------------------

Get the information of the ``spp_vf`` process.

* Normal response codes: 200
* Error response codes: 400, 404


Request (path)
~~~~~~~~~~~~~~

.. _table_spp_ctl_vfs_get:

.. table:: Request parameter for getting spp_vf.

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
      http://127.0.0.1:7777/v1/vfs/1


Response
~~~~~~~~

.. _table_spp_ctl_spp_vf_res:

.. table:: Response params of getting spp_vf.

    +------------------+---------+--------------------------------------------+
    | Name             | Type    | Description                                |
    |                  |         |                                            |
    +==================+=========+============================================+
    | client-id        | integer | Client id.                                 |
    +------------------+---------+--------------------------------------------+
    | ports            | array   | Array of port ids used by the process.     |
    +------------------+---------+--------------------------------------------+
    | components       | array   | Array of component objects in the process. |
    +------------------+---------+--------------------------------------------+
    | classifier_table | array   | Array of classifier tables in the process. |
    +------------------+---------+--------------------------------------------+

Component objects:

.. _table_spp_ctl_spp_vf_res_comp:

.. table:: Component objects of getting spp_vf.

    +---------+---------+--------------------------------------------------+
    | Name    | Type    | Description                                      |
    |         |         |                                                  |
    +=========+=========+==================================================+
    | core    | integer | Core id running on the component                 |
    +---------+---------+--------------------------------------------------+
    | name    | string  | Array of port ids used by the process.           |
    +---------+---------+--------------------------------------------------+
    | type    | string  | Array of component objects in the process.       |
    +---------+---------+--------------------------------------------------+
    | rx_port | array   | Array of port objs connected to rx of component. |
    +---------+---------+--------------------------------------------------+
    | tx_port | array   | Array of port objs connected to tx of component. |
    +---------+---------+--------------------------------------------------+

Port objects:

.. _table_spp_ctl_spp_vf_res_port:

.. table:: Port objects of getting spp_vf.

    +---------+---------+----------------------------------------------+
    | Name    | Type    | Description                                  |
    |         |         |                                              |
    +=========+=========+==============================================+
    | port    | string  | port id of {interface_type}:{interface_id}.  |
    +---------+---------+----------------------------------------------+
    | vlan    | object  | vlan operation which is applied to the port. |
    +---------+---------+----------------------------------------------+

Vlan objects:

.. _table_spp_ctl_spp_vf_res_vlan:

.. table:: Vlan objects of getting spp_vf.

    +-----------+---------+-------------------------------+
    | Name      | Type    | Description                   |
    |           |         |                               |
    +===========+=========+===============================+
    | operation | string  | ``add``, ``del`` or ``none``. |
    +-----------+---------+-------------------------------+
    | id        | integer | vlan id.                      |
    +-----------+---------+-------------------------------+
    | pcp       | integer | vlan pcp.                     |
    +-----------+---------+-------------------------------+

Classifier table:

.. _table_spp_ctl_spp_vf_res_cls:

.. table:: Vlan objects of getting spp_vf.

    +-----------+--------+-------------------------------------+
    | Name      | Type   | Description                         |
    |           |        |                                     |
    +===========+========+=====================================+
    | type      | string | ``mac`` or ``vlan``.                |
    +-----------+--------+-------------------------------------+
    | value     | string | mac_address or vlan_id/mac_address. |
    +-----------+--------+-------------------------------------+
    | port      | string | port id applied to classify.        |
    +-----------+--------+-------------------------------------+


Response example
~~~~~~~~~~~~~~~~

.. code-block:: json

    {
      "client-id": 1,
      "ports": [
        "phy:0", "phy:1", "vhost:0", "vhost:1", "ring:0", "ring:1"
      ],
      "components": [
        {
          "core": 2,
          "name": "fwd0_tx",
          "type": "forward",
          "rx_port": [
            {
            "port": "ring:0",
            "vlan": { "operation": "none", "id": 0, "pcp": 0 }
            }
          ],
          "tx_port": [
            {
              "port": "vhost:0",
              "vlan": { "operation": "none", "id": 0, "pcp": 0 }
            }
          ]
        },
        {
          "core": 3,
          "type": "unuse"
        },
        {
          "core": 4,
          "type": "unuse"
        },
        {
          "core": 5,
          "name": "fwd1_rx",
          "type": "forward",
          "rx_port": [
            {
            "port": "vhost:1",
            "vlan": { "operation": "none", "id": 0, "pcp": 0 }
            }
          ],
          "tx_port": [
            {
              "port": "ring:3",
              "vlan": { "operation": "none", "id": 0, "pcp": 0 }
            }
          ]
        },
        {
          "core": 6,
          "name": "cls",
          "type": "classifier",
          "rx_port": [
            {
              "port": "phy:0",
              "vlan": { "operation": "none", "id": 0, "pcp": 0 }
            }
          ],
          "tx_port": [
            {
              "port": "ring:0",
              "vlan": { "operation": "none", "id": 0, "pcp": 0 }
            },
            {
              "port": "ring:2",
              "vlan": { "operation": "none", "id": 0, "pcp": 0 }
            }
          ]
        },
        {
          "core": 7,
          "name": "mgr1",
          "type": "merge",
          "rx_port": [
            {
              "port": "ring:1",
              "vlan": { "operation": "none", "id": 0, "pcp": 0 }
            },
            {
              "port": "ring:3",
              "vlan": { "operation": "none", "id": 0, "pcp": 0 }
            }
          ],
          "tx_port": [
            {
              "port": "phy:0",
              "vlan": { "operation": "none", "id": 0, "pcp": 0 }
            }
          ]
        },
      ],
      "classifier_table": [
        {
          "type": "mac",
          "value": "FA:16:3E:7D:CC:35",
          "port": "ring:0"
        }
      ]
    }

The component which type is ``unused`` is to indicate unused core.


Equivalent CLI command
~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: none

    spp > vf {client_id}; status


POST /v1/vfs/{client_id}/components
-----------------------------------

Start component.

* Normal response codes: 204
* Error response codes: 400, 404


Request (path)
~~~~~~~~~~~~~~

.. _table_spp_ctl_spp_vf_components:

.. table:: Request params of components of spp_vf.

    +-----------+---------+-------------+
    | Name      | Type    | Description |
    +===========+=========+=============+
    | client_id | integer | client id.  |
    +-----------+---------+-------------+


Request (body)
~~~~~~~~~~~~~~

``type`` param is oen of ``forward``, ``merge`` or ``classifier``.

.. _table_spp_ctl_spp_vf_components_res:

.. table:: Response params of components of spp_vf.

    +-----------+---------+--------------------------------------------------+
    | Name      | Type    | Description                                      |
    |           |         |                                                  |
    +===========+=========+==================================================+
    | name      | string  | component name should be unique among processes. |
    +-----------+---------+--------------------------------------------------+
    | core      | integer | core id.                                         |
    +-----------+---------+--------------------------------------------------+
    | type      | string  | component type.                                  |
    +-----------+---------+--------------------------------------------------+

Request example
~~~~~~~~~~~~~~~

.. code-block:: console

    $ curl -X POST -H 'application/json' \
      -d '{"name": "fwd1", "core": 12, "type": "forward"}' \
      http://127.0.0.1:7777/v1/vfs/1/components


Response
~~~~~~~~

There is no body content for the response of a successful ``POST`` request.


Equivalent CLI command
~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: none

    spp > vf {client_id}; component start {name} {core} {type}


DELETE /v1/vfs/{sec id}/components/{name}
-----------------------------------------

Stop component.

* Normal response codes: 204
* Error response codes: 400, 404


Request (path)
~~~~~~~~~~~~~~

.. _table_spp_ctl_spp_vf_del:

.. table:: Request params of deleting component of spp_vf.

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
      http://127.0.0.1:7777/v1/vfs/1/components/fwd1


Response
~~~~~~~~

There is no body content for the response of a successful ``POST`` request.


Equivalent CLI command
~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: none

    spp > vf {client_id}; component stop {name}


PUT /v1/vfs/{client_id}/components/{name}/ports
-----------------------------------------------

Add or delete port to the component.

* Normal response codes: 204
* Error response codes: 400, 404


Request (path)
~~~~~~~~~~~~~~

.. _table_spp_ctl_spp_vf_comp_port:

.. table:: Request params for ports of component of spp_vf.

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

.. _table_spp_ctl_spp_vf_comp_port_body:

.. table:: Request body params for ports of component of spp_vf.

    +---------+---------+----------------------------------------------------+
    | Name    | Type    | Description                                        |
    |         |         |                                                    |
    +=========+=========+====================================================+
    | action  | string  | ``attach`` or ``detach``.                          |
    +---------+---------+----------------------------------------------------+
    | port    | string  | port id of {interface_type}:{interface_id}.        |
    +---------+---------+----------------------------------------------------+
    | dir     | string  | ``rx`` or ``tx``.                                  |
    +---------+---------+----------------------------------------------------+
    | vlan    | object  | vlan operation applied to port. it can be omitted. |
    +---------+---------+----------------------------------------------------+

Vlan object:

.. _table_spp_ctl_spp_vf_comp_port_body_vlan:

.. table:: Request body params for vlan ports of component of spp_vf.

    +-----------+---------+---------------------------------------------------+
    | Name      | Type    | Description                                       |
    |           |         |                                                   |
    +===========+=========+===================================================+
    | operation | string  | ``add``, ``del`` or ``none``.                     |
    +-----------+---------+---------------------------------------------------+
    | id        | integer | vid. ignored if operation is ``del`` or ``none``. |
    +-----------+---------+---------------------------------------------------+
    | pcp       | integer | pcp. ignored if operation is ``del`` or ``none``. |
    +-----------+---------+---------------------------------------------------+


Request example
~~~~~~~~~~~~~~~

.. code-block:: console

    $ curl -X PUT -H 'application/json' \
      -d '{"action": "attach", "port": "vhost:1", "dir": "rx", \
           "vlan": {"operation": "add", "id": 677, "pcp": 0}}' \
      http://127.0.0.1:7777/v1/vfs/1/components/fwd1/ports

.. code-block:: console

    $ curl -X PUT -H 'application/json' \
      -d '{"action": "detach", "port": "vhost:0", "dir": "tx"}' \
      http://127.0.0.1:7777/v1/vfs/1/components/fwd1/ports

Response
~~~~~~~~

There is no body content for the response of a successful ``PUT`` request.


Equivalent CLI command
~~~~~~~~~~~~~~~~~~~~~~

Action is ``attach``.

.. code-block:: none

    spp > vf {client_id}; port add {port} {dir} {name}

Action is ``attach`` with vlan tag feature.

.. code-block:: none

    # Add vlan tag
    spp > vf {client_id}; port add {port} {dir} {name} add_vlantag {id} {pcp}

    # Delete vlan tag
    spp > vf {client_id}; port add {port} {dir} {name} del_vlantag

Action is ``detach``.

.. code-block:: none

    spp > vf {client_id}; port del {port} {dir} {name}


PUT /v1/vfs/{sec id}/classifier_table
-------------------------------------

Set or Unset classifier table.

* Normal response codes: 204
* Error response codes: 400, 404


Request (path)
~~~~~~~~~~~~~~

.. _table_spp_ctl_spp_vf_cls_table:

.. table:: Request params for classifier_table of spp_vf.

    +-----------+---------+---------------------------+
    | Name      | Type    | Description               |
    |           |         |                           |
    +===========+=========+===========================+
    | client_id | integer | client id.                |
    +-----------+---------+---------------------------+


Request (body)
~~~~~~~~~~~~~~

For ``vlan`` param, it can be omitted if it is for ``mac``.

.. _table_spp_ctl_spp_vf_cls_table_body:

.. table:: Request body params for classifier_table of spp_vf.

    +-------------+-----------------+-----------------------------------------+
    | Name        | Type            | Description                             |
    |             |                 |                                         |
    +=============+=================+=========================================+
    | action      | string          | ``add`` or ``del``.                     |
    +-------------+-----------------+-----------------------------------------+
    | type        | string          | ``mac`` or ``vlan``.                    |
    +-------------+-----------------+-----------------------------------------+
    | vlan        | integer or null | vlan id for ``vlan``. null for ``mac``. |
    +-------------+-----------------+-----------------------------------------+
    | mac_address | string          | mac address.                            |
    +-------------+-----------------+-----------------------------------------+
    | port        | string          | port id.                                |
    +-------------+-----------------+-----------------------------------------+


Request example
~~~~~~~~~~~~~~~

Add an entry of port ``ring:0`` with MAC address ``FA:16:3E:7D:CC:35`` to
the table.

.. code-block:: console

    $ curl -X PUT -H 'application/json' \
      -d '{"action": "add", "type": "mac", \
         "mac_address": "FA:16:3E:7D:CC:35", \
         "port": "ring:0"}' \
      http://127.0.0.1:7777/v1/vfs/1/classifier_table

Delete an entry of port ``ring:0`` with MAC address ``FA:16:3E:7D:CC:35`` from
the table.

.. code-block:: console

    $ curl -X PUT -H 'application/json' \
      -d '{"action": "del", "type": "vlan", "vlan": 475, \
         "mac_address": "FA:16:3E:7D:CC:35", "port": "ring:0"}' \
      http://127.0.0.1:7777/v1/vfs/1/classifier_table


Response
~~~~~~~~

There is no body content for the response of a successful ``PUT`` request.


Equivalent CLI command
~~~~~~~~~~~~~~~~~~~~~~

Type is ``mac``.

.. code-block:: none

    spp > vf {cli_id}; classifier_table {action} mac {mac_addr} {port}

Type is ``vlan``.

.. code-block:: none

    spp > vf {cli_id}; classifier_table {action} vlan {vlan} {mac_addr} {port}
