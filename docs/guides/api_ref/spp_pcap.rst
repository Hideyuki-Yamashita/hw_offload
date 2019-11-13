..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2018-2019 Nippon Telegraph and Telephone Corporation


.. _spp_ctl_rest_api_spp_pcap:

spp_pcap
========

GET /v1/pcaps/{client_id}
-------------------------

Get the information of the ``spp_pcap`` process.

* Normal response codes: 200
* Error response codes: 400, 404


Request (path)
~~~~~~~~~~~~~~

.. _table_spp_ctl_pcap_get:

.. table:: Request parameter for getting spp_pcap info.

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
      http://127.0.0.1:7777/v1/pcaps/1


Response
~~~~~~~~

.. _table_spp_ctl_spp_pcap_res:

.. table:: Response params of getting spp_pcap.

    +------------------+---------+-----------------------------------------------+
    | Name             | Type    | Description                                   |
    |                  |         |                                               |
    +==================+=========+===============================================+
    | client-id        | integer | client id.                                    |
    +------------------+---------+-----------------------------------------------+
    | status           | string  | status of the process. "running" or "idle".   |
    +------------------+---------+-----------------------------------------------+
    | core             | array   | an array of core objects in the process.      |
    +------------------+---------+-----------------------------------------------+

Core objects:

.. _table_spp_ctl_spp_pcap_res_core:

.. table:: Core objects of getting spp_pcap.

    +----------+---------+----------------------------------------------------------------------+
    | Name     | Type    | Description                                                          |
    |          |         |                                                                      |
    +==========+=========+======================================================================+
    | core     | integer | core id                                                              |
    +----------+---------+----------------------------------------------------------------------+
    | role     | string  | role of the task running on the core. "receive" or "write".          |
    +----------+---------+----------------------------------------------------------------------+
    | rx_port  | array   | an array of port object for caputure. This member exists if role is  |
    |          |         | "recieve". Note that there is only a port object in the array.       |
    +----------+---------+----------------------------------------------------------------------+
    | filename | string  | a path name of output file. This member exists if role is "write".   |
    +----------+---------+----------------------------------------------------------------------+

There is only a port object in the array.

Port object:

.. _table_spp_ctl_spp_pcap_res_port:

.. table:: Port objects of getting spp_pcap.

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
      "status": "running",
      "core": [
        {
          "core": 2,
          "role": "receive",
          "rx_port": [
            {
            "port": "phy:0"
            }
          ]
        },
        {
          "core": 3,
          "role": "write",
          "filename": "/tmp/spp_pcap.20181108110600.ring0.1.2.pcap"
        }
      ]
    }


Equivalent CLI command
~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: none

    spp > pcap {client_id}; status


PUT /v1/pcaps/{client_id}/capture
---------------------------------

Start or Stop capturing.

* Normal response codes: 204
* Error response codes: 400, 404


Request (path)
~~~~~~~~~~~~~~

.. _table_spp_ctl_spp_pcap_capture:

.. table:: Request params of capture of spp_pcap.

    +-----------+---------+---------------------------------+
    | Name      | Type    | Description                     |
    |           |         |                                 |
    +===========+=========+=================================+
    | client_id | integer | client id.                      |
    +-----------+---------+---------------------------------+


Request (body)
~~~~~~~~~~~~~~

.. _table_spp_ctl_spp_pcap_capture_body:

.. table:: Request body params of capture of spp_pcap.

    +--------+--------+-------------------------------------+
    | Name   | Type   | Description                         |
    |        |        |                                     |
    +========+========+=====================================+
    | action | string | ``start`` or ``stop``.              |
    +--------+--------+-------------------------------------+


Request example
~~~~~~~~~~~~~~~

.. code-block:: console

    $ curl -X PUT -H 'application/json' \
      -d '{"action": "start"}' \
      http://127.0.0.1:7777/v1/pcaps/1/capture


Response
~~~~~~~~

There is no body content for the response of a successful ``PUT`` request.


Equivalent CLI command
~~~~~~~~~~~~~~~~~~~~~~

Action is ``start``.

.. code-block:: none

    spp > pcap {client_id}; start

Action is ``stop``.

.. code-block:: none

    spp > pcap {client_id}; stop


DELETE /v1/pcaps/{client_id}
----------------------------

Terminate ``spp_pcap`` process.

* Normal response codes: 204
* Error response codes: 400, 404


Request (path)
~~~~~~~~~~~~~~

.. _table_spp_ctl_pcap_delete:

.. table:: Request parameter for terminating spp_pcap.

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
      http://127.0.0.1:7777/v1/pcaps/1


Response example
~~~~~~~~~~~~~~~~

There is no body content for the response of a successful ``DELETE`` request.


Equivalent CLI command
~~~~~~~~~~~~~~~~~~~~~~

.. code-block:: none

    spp > pcap {client_id}; exit
