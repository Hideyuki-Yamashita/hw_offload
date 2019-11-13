..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2018-2019 Nippon Telegraph and Telephone Corporation


.. _spp_ctl_rest_api_ref:

spp-ctl REST API
================

Overview
--------

``spp-ctl`` provides simple REST like API. It supports http only, not https.

Request and Response
~~~~~~~~~~~~~~~~~~~~

Request body is JSON format.
It is accepted both ``text/plain`` and ``application/json``
for the content-type header.

A response of ``GET`` is JSON format and the content-type is
``application/json`` if the request success.

.. code-block:: console

    $ curl http://127.0.0.1:7777/v1/processes
    [{"type": "primary"}, ..., {"client-id": 2, "type": "vf"}]

    $ curl -X POST http://localhost:7777/v1/vfs/1/components \
      -d '{"core": 2, "name": "fwd0_tx", "type": "forward"}'

If a request is failed, the response is a text which shows error reason
and the content-type is ``text/plain``.


Error code
~~~~~~~~~~

``spp-ctl`` does basic syntax and lexical check of a request.

.. _table_spp_ctl_error_codes:

.. table:: Error codes in spp-ctl.

    +-------+----------------------------------------------------------------+
    | Error | Description                                                    |
    |       |                                                                |
    +=======+================================================================+
    | 400   | Syntax or lexical error, or SPP returns error for the request. |
    +-------+----------------------------------------------------------------+
    | 404   | URL is not supported, or no SPP process of client-id in a URL. |
    +-------+----------------------------------------------------------------+
    | 500   | System error occured in ``spp-ctl``.                           |
    +-------+----------------------------------------------------------------+
