..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2019 Nippon Telegraph and Telephone Corporation


.. _spp_usecases_pcap:

spp_pcap
========

Packet Capture
--------------

This section describes a usecase for capturing packets with ``spp_pcap``.
See inside of the captured file with ``tcpdump`` command.
:numref:`figure_simple_capture` shows the overview of scenario in which
incoming packets via ``phy:0`` are dumped as compressed pcap files by using
``spp_pcap``.

.. _figure_simple_capture:

.. figure:: ../images/design/spp_pcap_overview.*
    :width: 50%

    Packet capture with spp_pcap


.. _spp_pcap_use_case_launch_pcap:

Launch spp_pcap
~~~~~~~~~~~~~~~

Change directory if you are not in SPP's directory,
and compile if not done yet.

.. code-block:: console

    $ cd /path/to/spp

Launch spp-ctl and SPP CLI in different terminals.

.. code-block:: console

    # terminal 1
    $ python3 ./src/spp-ctl/spp-ctl -b 192.168.1.100

.. code-block:: console

    # terminal 2
    $ python3 ./src/spp.py -b 192.168.1.100


Then, run ``spp_primary`` with one physical port.

.. code-block:: console

    # terminal 3
    $ sudo ./src/primary/x86_64-native-linuxapp-gcc/spp_primary \
        -l 0 -n 4 \
        --socket-mem 512,512 \
        --huge-dir /run/hugepages/kvm \
        --proc-type primary \
        -- \
        -p 0x01 \
        -n 8 -s 192.168.1.100:5555

After ``spp_primary`` is launched successfully, run ``spp_pcap`` in other
terminal. In this usecase, you use default values for optional arguments.
Output directory of captured file is ``/tmp`` and the size of file is
``1GiB``.
You notice that six lcores are assigned with ``-l 1-6``.
It means that you use one locre for master, one for receiver, and four for
writer threads.

.. code-block:: console

    # terminal 4
    $ sudo ./src/pcap/x86_64-native-linuxapp-gcc/spp_pcap \
       -l 1-6 -n 4 --proc-type=secondary \
       -- \
       --client-id 1 -s 192.168.1.100:6666 \
       -c phy:0

You can confirm lcores and worker threads running on from ``status`` command.

.. code-block:: none

    # terminal 2
    spp > pcap 1; status
    Basic Information:
      - client-id: 1
      - status: idle
      - lcore_ids:
        - master: 1
        - slaves: [2, 3, 4, 5, 6]
    Components:
      - core:2 receive
        - rx: phy:0
      - core:3 write
        - filename:
      - core:4 write
        - filename:
      - core:5 write
        - filename:
      - core:6 write
        - filename:


.. _spp_pcap_use_case_start_capture:

Start Capture
~~~~~~~~~~~~~

If you already started to send packets to ``phy:0`` from outside,
you are ready to start capturing packets.

.. code-block:: none

    # terminal 2
    spp > pcap 1; start
    Start packet capture.

As you run ``start`` command, PCAP files are generated for each of
``writer`` threads for capturing.

.. code-block:: none

    # terminal 2
    spp > pcap 1; status
    Basic Information:
      - client-id: 1
      - status: running
      - lcore_ids:
        - master: 1
        - slaves: [2, 3, 4, 5, 6]
    Components:
      - core:2 receive
        - rx: phy:0
      - core:3 write
        - filename: /tmp/spp_pcap.20190214161550.phy0.1.1.pcap.lz4
      - core:4 write
        - filename: /tmp/spp_pcap.20190214161550.phy0.2.1.pcap.lz4
      - core:5 write
        - filename: /tmp/spp_pcap.20190214161550.phy0.3.1.pcap.lz4
      - core:6 write
        - filename: /tmp/spp_pcap.20190214161550.phy0.4.1.pcap.lz4


.. _spp_pcap_use_case_stop_capture:

Stop Capture
~~~~~~~~~~~~

Stop capturing and confirm that compressed PCAP files are generated.

.. code-block:: none

    # terminal 2
    spp > pcap 1; stop
    spp > ls /tmp
    ....
    spp_pcap.20190214175446.phy0.1.1.pcap.lz4
    spp_pcap.20190214175446.phy0.1.2.pcap.lz4
    spp_pcap.20190214175446.phy0.1.3.pcap.lz4
    spp_pcap.20190214175446.phy0.2.1.pcap.lz4
    spp_pcap.20190214175446.phy0.2.2.pcap.lz4
    spp_pcap.20190214175446.phy0.2.3.pcap.lz4
    ....

Index in the filename, such as ``1.1`` or ``1.2``, is a combination of
``writer`` thread ID and sequenceal number.
In this case, it means each of four threads generate three files.


.. _spp_pcap_use_case_shutdown:

Shutdown spp_pcap
~~~~~~~~~~~~~~~~~

Run ``exit`` or ``bye sec`` command to terminate ``spp_pcap``.

.. code-block:: none

    # terminal 2
    spp > pcap 1; exit


.. _spp_pcap_use_case_inspect_file:

Inspect PCAP Files
~~~~~~~~~~~~~~~~~~

You can inspect captured PCAP files by using utilities.

Merge PCAP Files
^^^^^^^^^^^^^^^^

Extract and merge compressed PCAP files.

For extract several LZ4 files at once, use ``-d`` and ``-m`` options.
``-d`` is for decompression and ``-m`` is for multiple files.

You had better not to merge divided files into single file, but still
several files because the size of merged file might be huge.
Each of extracted PCAP file is 1GiB in default, so total size of extracted
files is 12GiB in this case. To avoid the situation, merge files for each of
threads and generate four PCAP files of 3GiB.

First, extract LZ4 files of writer thread ID 1.

.. code-block:: console

    # terminal 4
    $ lz4 -d -m /tmp/spp_pcap.20190214175446.phy0.1.*

And confirm that the files are extracted.

.. code-block:: console

    # terminal 4
    $ ls /tmp | grep pcap$
    spp_pcap.20190214175446.phy0.1.1.pcap
    spp_pcap.20190214175446.phy0.1.2.pcap
    spp_pcap.20190214175446.phy0.1.3.pcap

Run ``mergecap`` command to merge extracted files to current directory
as ``spp_pcap1.pcap``.

.. code-block:: console

    # terminal 4
    $ mergecap /tmp/spp_pcap.20190214175446.phy0.1.*.pcap -w spp_pcap1.pcap

Inspect PCAP file
^^^^^^^^^^^^^^^^^

You can use any of applications, for instance ``wireshark`` or ``tcpdump``,
for inspecting PCAP file.
To inspect the merged PCAP file, read packet data from ``tcpdump`` command
in this usecase. ``-r`` option is to dump packet data in human readable format.

.. code-block:: console

    # terminal 4
    $ tcpdump -r spp_pcap1.pcap | less
    17:54:52.559783 IP 192.168.0.100.1234 > 192.168.1.1.5678: Flags [.], ...
    17:54:52.559784 IP 192.168.0.100.1234 > 192.168.1.1.5678: Flags [.], ...
    17:54:52.559785 IP 192.168.0.100.1234 > 192.168.1.1.5678: Flags [.], ...
    17:54:52.559785 IP 192.168.0.100.1234 > 192.168.1.1.5678: Flags [.], ...
