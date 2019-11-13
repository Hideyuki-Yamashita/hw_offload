..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2019 Nippon Telegraph and Telephone Corporation

.. _spp_pcap_explain:

spp_pcap
========

This section describes implementation of ``spp_pcap``.


Slave Main
----------

In ``slave_main()``, it calls ``pcap_proc_receive()`` if thread type is
receiver, or ``pcap_proc_write()`` if the type is writer.

.. code-block:: c

    /* spp_pcap.c */

    while ((status = spp_get_core_status(lcore_id)) !=
                    SPP_CORE_STOP_REQUEST) {

            if (pcap_info->type == TYPE_RECIVE)
                    ret = pcap_proc_receive(lcore_id);
            else
                    ret = pcap_proc_write(lcore_id);
            }
    }

Receiving Pakcets
-----------------

``pcap_proc_receive()`` is for receiving packets with ``rte_eth_rx_burst``
and sending the packets to the writer thread via ring memory by using
``rte_ring_enqueue_bulk()``.

.. code-block:: c

    /* spp_pcap.c */

    rx = &g_pcap_option.port_cap;
    nb_rx = rte_eth_rx_burst(rx->ethdev_port_id, 0, bufs, MAX_PCAP_BURST);

    /* Forward to ring for writer thread */
    nb_tx = rte_ring_enqueue_burst(write_ring, (void *)bufs, nb_rx, NULL);


Writing Packet
--------------

``pcap_proc_write()`` is for capturing packets to a file. The captured file
is compressed with
`LZ4
<https://github.com/lz4/lz4>`_
which is a lossless compression algorithm and providing compression
speed > 500 MB/s per core.

.. code-block:: c

    nb_rx =  rte_ring_dequeue_bulk(read_ring, (void *)bufs, MAX_PKT_BURST,
                                                                    NULL);
    for (buf = 0; buf < nb_rx; buf++) {
            mbuf = bufs[buf];
            rte_prefetch0(rte_pktmbuf_mtod(mbuf, void *));
            if (compress_file_packet(&g_pcap_info[lcore_id], mbuf)
                                                    != SPPWK_RET_OK) {
                    RTE_LOG(ERR, PCAP, "capture file write error: "
                            "%d (%s)\n", errno, strerror(errno));
                    ret = SPPWK_RET_NG;
                    info->status = SPP_CAPTURE_IDLE;
                    compress_file_operation(info, CLOSE_MODE);
                    break;
            }
    }
    for (buf = nb_rx; buf < nb_rx; buf++)
            rte_pktmbuf_free(bufs[buf]);
