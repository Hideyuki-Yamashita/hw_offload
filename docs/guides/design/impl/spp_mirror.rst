.. SPDX-License-Identifier: BSD-3-Clause
   Copyright(c) 2018 Nippon Telegraph and Telephone Corporation

.. _spp_vf_explain_spp_mirror:

spp_mirror
==========

This section describes implementation of ``spp_mirror``.
It consists of master thread and several worker threads for duplicating
packets.


Slave Main
----------

Main function of worker thread is defined as ``slave_main()`` in which
for duplicating packets is ``mirror_proc()`` on each of lcores.

.. code-block:: c

    for (cnt = 0; cnt < core->num; cnt++) {

        ret = mirror_proc(core->id[cnt]);
        if (unlikely(ret != 0))
            break;
    }


Mirroring Packets
-----------------

Worker thread receives and duplicate packets. There are two modes of copying
packets, ``shallowcopy`` and ``deepcopy``.
Deep copy is for duplicating whole of packet data, but less performance than
shallow copy. Shallow copy duplicates only packet header and body is not shared
among original packet and duplicated packet. So, changing packet data affects
both of original and copied packet.

You can configure using which of modes in Makefile. Default mode is
``shallowcopy``. If you change the mode to ``deepcopy``, comment out this
line of CFLAGS.

.. code-block:: makefile

    # Default mode is shallow copy.
    CFLAGS += -DSPP_MIRROR_SHALLOWCOPY

This code is a part of ``mirror_proc()``. In this function,
``rte_pktmbuf_clone()`` is just called if in shallow copy
mode, or create a new packet with ``rte_pktmbuf_alloc()`` for duplicated
packet if in deep copy mode.

.. code-block:: c

                for (cnt = 0; cnt < nb_rx; cnt++) {
                        org_mbuf = bufs[cnt];
                        rte_prefetch0(rte_pktmbuf_mtod(org_mbuf, void *));
   #ifdef SPP_MIRROR_SHALLOWCOPY
                        /* Shallow Copy */
            copybufs[cnt] = rte_pktmbuf_clone(org_mbuf,
                                                        g_mirror_pool);

   #else
                        struct rte_mbuf *mirror_mbuf = NULL;
                        struct rte_mbuf **mirror_mbufs = &mirror_mbuf;
                        struct rte_mbuf *copy_mbuf = NULL;
                        /* Deep Copy */
                        do {
                                copy_mbuf = rte_pktmbuf_alloc(g_mirror_pool);
                                if (unlikely(copy_mbuf == NULL)) {
                                        rte_pktmbuf_free(mirror_mbuf);
                                        mirror_mbuf = NULL;
                                        RTE_LOG(INFO, MIRROR,
                                                "copy mbuf alloc NG!\n");
                                        break;
                                }

                                copy_mbuf->data_off = org_mbuf->data_off;
                                ...
                                copy_mbuf->packet_type = org_mbuf->packet_type;

                                rte_memcpy(rte_pktmbuf_mtod(copy_mbuf, char *),
                                        rte_pktmbuf_mtod(org_mbuf, char *),
                                        org_mbuf->data_len);

                                *mirror_mbufs = copy_mbuf;
                                mirror_mbufs = &copy_mbuf->next;
                        } while ((org_mbuf = org_mbuf->next) != NULL);
            copybufs[cnt] = mirror_mbuf;

   #endif /* SPP_MIRROR_SHALLOWCOPY */
                }
        if (cnt != 0)
                        nb_tx2 = spp_eth_tx_burst(tx->dpdk_port, 0,
                                copybufs, cnt);
