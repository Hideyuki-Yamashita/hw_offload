.. SPDX-License-Identifier: BSD-3-Clause
   Copyright(c) 2019 Nippon Telegraph and Telephone Corporation

.. _design_impl_spp_nfv:

spp_nfv
=======

``spp_nfv`` is a DPDK secondary process and communicates with primary and
other peer processes via TCP sockets or shared memory.
``spp_nfv`` consists of several threads, main thread for maanging behavior of
``spp_nfv`` and worker threads for packet forwarding.

As initialization of the process, it calls ``rte_eal_init()``, then specific
initialization functions for resources of ``spp_nfv`` itself.

After initialization, main thread launches worker threads on each of given
slave lcores with ``rte_eal_remote_launch()``. It means that ``spp_nfv``
requires two lcores at least.
Main thread starts to accept user command after all of worker threads are
launched.


Initialization
--------------

In main funciton, ``spp_nfv`` calls ``rte_eal_init()`` first as other
DPDK applications, ``forward_array_init()`` and ``port_map_init()``
for initializing port forward array which is a kind of forwarding table.

.. code-block:: c

        int
        main(int argc, char *argv[])
        {
                ....

                ret = rte_eal_init(argc, argv);
                if (ret < 0)
                        return -1;
                ....

                /* initialize port forward array*/
                forward_array_init();
                port_map_init();
                ....

Port forward array is implemented as an array of ``port`` structure.
It consists of RX, TX ports and its forwarding functions,
``rte_rx_burst()`` and ``rte_tx_burst()`` actually.
Each of ports are identified with unique port ID.
Worker thread iterates this array and forward packets from RX port to
TX port.

.. code-block:: c

    /* src/shared/common.h */

    struct port {
        uint16_t in_port_id;
        uint16_t out_port_id;
        uint16_t (*rx_func)(uint16_t, uint16_t, struct rte_mbuf **, uint16_t);
        uint16_t (*tx_func)(uint16_t, uint16_t, struct rte_mbuf **, uint16_t);
    };

Port map is another kind of structure for managing its type and statistics.
Port type for indicating PMD type, for example, ring, vhost or so.
Statistics is used as a counter of packet forwarding.

.. code-block:: c

    /* src/shared/common.h */

    struct port_map {
            int id;
            enum port_type port_type;
            struct stats *stats;
            struct stats default_stats;
    };

Final step of initialization is setting up memzone.
In this step, ``spp_nfv`` just looks up memzone of primary process as a
secondary.

.. code-block:: c

                /* set up array for port data */
                if (rte_eal_process_type() == RTE_PROC_SECONDARY) {
                        mz = rte_memzone_lookup(MZ_PORT_INFO);
                        if (mz == NULL)
                                rte_exit(EXIT_FAILURE,
                                        "Cannot get port info structure\n");
                        ports = mz->addr;


Launch Worker Threads
---------------------

Worker threads are launched with ``rte_eal_remote_launch()`` from main thread.
``RTE_LCORE_FOREACH_SLAVE`` is a macro for traversing slave lcores while
incrementing ``lcore_id`` and ``rte_eal_remote_launch()`` is a function
for running a function on worker thread.

.. code-block:: c

        lcore_id = 0;
        RTE_LCORE_FOREACH_SLAVE(lcore_id) {
                rte_eal_remote_launch(main_loop, NULL, lcore_id);
        }

In this case, ``main_loop`` is a starting point for calling task of worker
thread ``nfv_loop()``.

.. code-block:: c

    static int
    main_loop(void *dummy __rte_unused)
    {
            nfv_loop();
            return 0;
    }


Parsing User Command
--------------------

After all of worker threads are launched, main threads goes into while
loop for waiting user command from SPP controller via TCP connection.
If receiving a user command, it simply parses the command and make a response.
It terminates the while loop if it receives ``exit`` command.

.. code-block:: c

        while (on) {
                ret = do_connection(&connected, &sock);
                ....
                ret = do_receive(&connected, &sock, str);
                ....
                flg_exit = parse_command(str);
                ....
                ret = do_send(&connected, &sock, str);
                ....
        }

``parse_command()`` is a function for parsing user command as named.
There are several commnads for ``spp_nfv`` as described in
:ref:`Secondary Commands<commands_spp_nfv>`.
Command from controller is a simple plain text and action for the command
is decided with the first token of the command.

.. code-block:: c

    static int
    parse_command(char *str)
    {
            ....

            if (!strcmp(token_list[0], "status")) {
                    RTE_LOG(DEBUG, SPP_NFV, "status\n");
                    memset(str, '\0', MSG_SIZE);
            ....

                    } else if (!strcmp(token_list[0], "add")) {
                    RTE_LOG(DEBUG, SPP_NFV, "Received add command\n");
                    if (do_add(token_list[1]) < 0)
                            RTE_LOG(ERR, SPP_NFV, "Failed to do_add()\n");

            } else if (!strcmp(token_list[0], "patch")) {
                    RTE_LOG(DEBUG, SPP_NFV, "patch\n");
            ....
    }

For instance, if the first token is ``add``, it calls ``do_add()`` with
given tokens and adds port to the process.
