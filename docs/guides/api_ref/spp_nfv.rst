..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2018-2019 Nippon Telegraph and Telephone Corporation


.. _spp_ctl_rest_api_spp_nfv:

spp_nfv
=======

GET /v1/nfvs/{client_id}
------------------------

Get the information of ``spp_nfv``.

* Normal response codes: 200
* Error response codes: 400, 404


Request (path)
~~~~~~~~~~~~~~

.. _table_spp_ctl_nfvs_get:

.. table:: Request parameter for getting info of ``spp_nfv``.

    +-----------+---------+-------------------------------------+
    | Name      | Type    | Description                         |
    |           |         |                                     |
    +===========+=========+=====================================+
    | client_id | integer | client id.                          |
    +-----------+---------+-------------------------------------+


Request example
~~~~~~~~~~~~~~~

.. code-block:: console

    $ curl -X GET -H 'application/json' \
      http://127.0.0.1:7777/v1/nfvs/1


Response
~~~~~~~~

.. _table_spp_ctl_spp_nfv_res:

.. table:: Response params of getting info of ``spp_nfv``.

    +-----------+---------+---------------------------------------------+
    | Name      | Type    | Description                                 |
    |           |         |                                             |
    +===========+=========+=============================================+
    | client-id | integer | client id.                                  |
    +-----------+---------+---------------------------------------------+
    | status    | string  | ``running`` or ``idling``.                  |
    +-----------+---------+---------------------------------------------+
    | ports     | array   | an array of port ids used by the process.   |
    +-----------+---------+---------------------------------------------+
    | patches   | array   | an array of patches.                        |
    +-----------+---------+---------------------------------------------+

Patch ports.

.. _table_spp_ctl_patch_spp_nfv:

.. table:: Attributes of patch command of ``spp_nfv``.

    +------+--------+----------------------------------------------+
    | Name | Type   | Description                                  |
    |      |        |                                              |
    +======+========+==============================================+
    | src  | string | source port id.                              |
    +------+--------+----------------------------------------------+
    | dst  | string | destination port id.                         |
    +------+--------+----------------------------------------------+


Response example
~~~~~~~~~~~~~~~~

.. code-block:: json

    {
      "client-id": 1,
      "status": "running",
      "ports": [
        "phy:0", "phy:1", "vhost:0", "vhost:1", "ring:0", "ring:1"
      ],
      "patches": [
        {
          "src": "vhost:0", "dst": "ring:0"
        },
        {
          "src": "ring:1", "dst": "vhost:1"
        }
      ]
    }


Equivalent CLI command
~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: none

    spp > nfv {client_id}; status


PUT /v1/nfvs/{client_id}/forward
--------------------------------

Start or Stop forwarding.

* Normal response codes: 204
* Error response codes: 400, 404


Request (path)
~~~~~~~~~~~~~~

.. _table_spp_ctl_spp_nfv_forward_get:

.. table:: Request params of forward command of ``spp_nfv``.

    +-----------+---------+---------------------------------+
    | Name      | Type    | Description                     |
    |           |         |                                 |
    +===========+=========+=================================+
    | client_id | integer | client id.                      |
    +-----------+---------+---------------------------------+


Request example
~~~~~~~~~~~~~~~

.. code-block:: console

    $ curl -X PUT -H 'application/json' \
      -d '{"action": "start"}' \
      http://127.0.0.1:7777/v1/nfvs/1/forward


Request (body)
~~~~~~~~~~~~~~

.. _table_spp_ctl_spp_nfv_forward_get_body:

.. table:: Request body params of forward of ``spp_nfv``.

    +--------+--------+-------------------------------------+
    | Name   | Type   | Description                         |
    |        |        |                                     |
    +========+========+=====================================+
    | action | string | ``start`` or ``stop``.              |
    +--------+--------+-------------------------------------+


Response
~~~~~~~~

There is no body content for the response of a successful ``PUT`` request.


Equivalent CLI command
~~~~~~~~~~~~~~~~~~~~~~

Action is ``start``.

.. code-block:: none

    spp > nfv {client_id}; forward

Action is ``stop``.

.. code-block:: none

    spp > nfv {client_id}; stop


PUT /v1/nfvs/{client_id}/ports
------------------------------

Add or delete port.

* Normal response codes: 204
* Error response codes: 400, 404


Request(path)
~~~~~~~~~~~~~

.. _table_spp_ctl_spp_nfv_ports_get:

.. table:: Request params of ports of ``spp_nfv``.

    +-----------+---------+--------------------------------+
    | Name      | Type    | Description                    |
    |           |         |                                |
    +===========+=========+================================+
    | client_id | integer | client id.                     |
    +-----------+---------+--------------------------------+


Request (body)
~~~~~~~~~~~~~~

.. _table_spp_ctl_spp_nfv_ports_get_body:

.. table:: Request body params of ports of ``spp_nfv``.

    +--------+--------+---------------------------------------------------------------+
    | Name   | Type   | Description                                                   |
    |        |        |                                                               |
    +========+========+===============================================================+
    | action | string | ``add`` or ``del``.                                           |
    +--------+--------+---------------------------------------------------------------+
    | port   | string | port id. port id is the form {interface_type}:{interface_id}. |
    +--------+--------+---------------------------------------------------------------+


Request example
~~~~~~~~~~~~~~~

.. code-block:: console

    $ curl -X PUT -H 'application/json' \
      -d '{"action": "add", "port": "ring:0"}' \
      http://127.0.0.1:7777/v1/nfvs/1/ports


Response
~~~~~~~~

There is no body content for the response of a successful ``PUT`` request.


Equivalent CLI command
~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: none

    spp > nfv {client_id}; {action} {if_type} {if_id}


PUT /v1/nfvs/{client_id}/patches
--------------------------------

Add a patch.

* Normal response codes: 204
* Error response codes: 400, 404


Request (path)
~~~~~~~~~~~~~~

.. _table_spp_ctl_spp_nfv_patches_get:

.. table:: Request params of patches of ``spp_nfv``.

    +-----------+---------+---------------------------------+
    | Name      | Type    | Description                     |
    |           |         |                                 |
    +===========+=========+=================================+
    | client_id | integer | client id.                      |
    +-----------+---------+---------------------------------+


Request (body)
~~~~~~~~~~~~~~

.. _table_spp_ctl_spp_nfv_ports_patches_body:

.. table:: Request body params of patches of ``spp_nfv``.

    +------+--------+------------------------------------+
    | Name | Type   | Description                        |
    |      |        |                                    |
    +======+========+====================================+
    | src  | string | source port id.                    |
    +------+--------+------------------------------------+
    | dst  | string | destination port id.               |
    +------+--------+------------------------------------+


Request example
~~~~~~~~~~~~~~~

.. code-block:: console

    $ curl -X PUT -H 'application/json' \
      -d '{"src": "ring:0", "dst": "ring:1"}' \
      http://127.0.0.1:7777/v1/nfvs/1/patches


Response
~~~~~~~~

There is no body content for the response of a successful ``PUT`` request.


Equivalent CLI command
~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: none

    spp > nfv {client_id}; patch {src} {dst}


DELETE /v1/nfvs/{client_id}/patches
-----------------------------------

Reset patches.

* Normal response codes: 204
* Error response codes: 400, 404


Request (path)
~~~~~~~~~~~~~~

.. _table_spp_ctl_spp_nfv_del_patches:

.. table:: Request params of deleting patches of ``spp_nfv``.

    +-----------+---------+---------------------------------------+
    | Name      | Type    | Description                           |
    |           |         |                                       |
    +===========+=========+=======================================+
    | client_id | integer | client id.                            |
    +-----------+---------+---------------------------------------+


Request example
~~~~~~~~~~~~~~~

.. code-block:: console

    $ curl -X DELETE -H 'application/json' \
      http://127.0.0.1:7777/v1/nfvs/1/patches


Response
~~~~~~~~

There is no body content for the response of a successful ``DELETE`` request.


Equivalent CLI command
~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: none

    spp > nfv {client_id}; patch reset


DELETE /v1/nfvs/{client_id}
---------------------------

Terminate ``spp_nfv``.

* Normal response codes: 204
* Error response codes: 400, 404


Request (path)
~~~~~~~~~~~~~~

.. _table_spp_ctl_nfvs_delete:

.. table:: Request parameter for terminating ``spp_nfv``.

    +-----------+---------+-------------------------------------+
    | Name      | Type    | Description                         |
    |           |         |                                     |
    +===========+=========+=====================================+
    | client_id | integer | client id.                          |
    +-----------+---------+-------------------------------------+


Request example
~~~~~~~~~~~~~~~

.. code-block:: console

    $ curl -X DELETE -H 'application/json' \
      http://127.0.0.1:7777/v1/nfvs/1


Response example
~~~~~~~~~~~~~~~~

There is no body content for the response of a successful ``DELETE`` request.


Equivalent CLI command
~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: none

    spp > nfv {client_id}; exit
