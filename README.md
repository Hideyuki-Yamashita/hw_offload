# Soft Patch Panel


## Overview

[Soft Patch Panel](http://git.dpdk.org/apps/spp/)
(SPP) is a DPDK application for providing switching
functionality for Service Function Chaining in NFV
(Network Function Virtualization).
DPDK stands for Data Plane Development Kit.


## Project Goal

In general, implementation and configuration of DPDK application is
difficult because it requires deep understandings of system architecture
and networking technologies.

The goal of SPP is to easily inter-connect DPDK applications together
and assign resources dynamically to these applications
with patch panel like simple interface.


## Architecture Overview

SPP is a kind of DPDK
[multi-process application](https://doc.dpdk.org/guides/prog_guide/multi_proc_support.html).
It is composed of primary process which is responsible for resource
management, and secondary processes as workers for packet forwarding.
This primary process does not interact with any traffic. It is
responsible for creation and freeing of resources shared among other
secondary processes.

A Python based management interface called SPP controller consists of
`spp-ctl` and `SPP CLI`.
`SPP CLI` is a command line interface of SPP, and `spp-ctl` a backend
server for managing primary and secondary processes.
`spp-ctl` behaves as a REST API server and `SPP CLI` sends a command via
the REST API.


## Install

Before using SPP, you need to install DPDK. Briefly describ here how to
install and setup DPDK. Please refer to SPP's
[Getting Started](https://doc.dpdk.org/spp/setup/getting_started.html) guide
for more details. For DPDK, refer to
[Getting Started Guide for Linux](https://doc.dpdk.org/guides/linux_gsg/index.html).

It is required to install Python3 and libnuma-devel library before.
SPP does not support Python2 anymore.

```sh
$ sudo apt install python3 python3-pip
$ sudo apt install libnuma-dev
```

### DPDK

Clone repository and compile DPDK in any directory.

```
$ cd /path/to/any
$ git clone http://dpdk.org/git/dpdk
```

Compile DPDK with target environment.

```sh
$ cd dpdk
$ export RTE_SDK=$(pwd)
$ export RTE_TARGET=x86_64-native-linuxapp-gcc  # depends on your env
$ make install T=$RTE_TARGET
```

### SPP

Clone repository and compile SPP in any directory.

```sh
$ cd /path/to/any
$ git clone http://dpdk.org/git/apps/spp
$ cd spp
$ make  # Confirm that $RTE_SDK and $RTE_TARGET are set
```

### Binding Network Ports to DPDK

Network ports must be bound to DPDK with a UIO (Userspace IO) driver.
UIO driver is for mapping device memory to userspace and registering
interrupts.

You usually use the standard `uio_pci_generic` for many use cases or
`vfio-pci` for more robust and secure cases. Both of drivers are
included by default in modern Linux kernel.

```sh
# Activate uio_pci_generic
$ sudo modprobe uio_pci_generic

# or vfio-pci
$ sudo modprobe vfio-pci
```

Once UIO driver is activated, bind network ports with the driver.
DPDK provides `usertools/dpdk-devbind.py` for managing devices.
Here are some examples.

```
# Bind a port with 2a:00.0 (PCI address)
$ ./usertools/dpdk-devbind.py --bind=uio_pci_generic 2a:00.0

# or eth0
$ ./usertools/dpdk-devbind.py --bind=uio_pci_generic eth0
```

After binding a port, you can find it is under the DPDK driver,
and cannot find it by using `ifconfig` or `ip`.

```sh
$ $RTE_SDK/usertools/dpdk-devbind.py -s

Network devices using DPDK-compatible driver
============================================
0000:2a:00.0 '82571EB ... 10bc' drv=uio_pci_generic unused=vfio-pci
....
```

## How to Use

You can use SPP from `bin/start.sh` script or launching each of
processes manually. This startup script is provided for skipping to
input many options for multiple DPDK applications.

### Quick Start

Start with `bin/start.sh` with configuration file `bin/config.sh`.
First time you run the startup script, it generates the config file
and asks to edit the config without launchin processes.

Here is a part of config parameters. You do not need to change most of
params.
If you do not have physical NICs on your server, activate
`PRI_VHOST_IDS` which is for setting up vhost interfaces instead of
physical.

```sh
SPP_HOST_IP=127.0.0.1
SPP_HUGEPAGES=/dev/hugepages

# spp_primary options
LOGLEVEL=7  # change to 8 if you refer debug messages.
PRI_CORE_LIST=0  # required one lcore usually.
PRI_MEM=1024
PRI_MEMCHAN=4  # change for your memory channels.
NUM_RINGS=8
PRI_PORTMASK=0x03  # total num of ports of spp_primary.
#PRI_VHOST_IDS=(11 12)  # you use if you have no phy ports.
```

After you edit configuration, you can launch `spp-ctl`,
`spp_primary` and `SPP CLI` from startup script.

```sh
$ ./bin/start.sh
Start spp-ctl
Start spp_primary
Waiting for spp-ctl is ready ...
Welcome to the SPP CLI. Type `help` or `?` to list commands.

spp >
```

Check status of `spp_primary` because it takes several seconds to be
ready. Confirm that the status is `running`.

```sh
spp > status
- spp-ctl:
  - address: 127.0.0.1:7777
- primary:
  - status: running
- secondary:
  - processes:
```

Now you are ready to launch secondary processes from `pri; launch`
command, or another terminal.
Here is an example for launching `spp_nfv` with options from
`pri; launch`. Log file of this process is created as
`log/spp_nfv1.log`.

```sh
spp > pri; launch nfv 1 -l 1,2 -m 512 -- -n 1 -s 127.0.0.1:6666
```

This `launch` command supports TAB completion. Parameters for `spp_nfv`
are completed after secondary ID `1`.

```sh
spp > pri; launch nfv 1

# Press TAB
spp > pri; launch nfv 1 -l 1,2 -m 512 -- -n 1 -s 127.0.0.1:6666
```

It is same as following options launching from terminal.

```sh
$ sudo ./src/nfv/x86_64-native-linuxapp-gcc/spp_nfv \
    -l 1,2 -n 4 -m 512 \
    --proc-type secondary \
    -- \
    -n 1 \
    -s 127.0.0.1:6666
```

Parameters for completion are defined in `SPP CLI`, and you can find
parameters with `config` command.

```sh
spp > config
- max_secondary: "16"   # The maximum number of secondary processes
- prompt: "spp > "  # Command prompt
- topo_size: "60%"  # Percentage or ratio of topo
- sec_mem: "-m 512" # Mem size
...
```

You can launch consequence secondary processes from CLI.

```sh
spp > pri; launch nfv 2 -l 1,3 -m 512 -- -n 2 -s 127.0.0.1:6666
spp > pri; launch vf 3 -l 1,4,5,6 -m 512 -- -n 3 -s 127.0.0.1:6666
...
```

### Startup Manually

You should keep in mind the order of launching processes if you launch
processes without using startup script.
`spp-ctl` should be launched before all of other processes. Then,
primary process must be launched before secondary processes.
On the other hand, you can launch SPP CLI `spp.py` in any time
after `spp-ctl`. In general, `spp-ctl` is launched first,
then `spp.py`, `spp_primary` and secondary processes.

It has a option -b for binding address explicitly to be accessed from
other than default, `127.0.0.1` or `localhost`.

In the following example, processes are launched in different terminals
for describing options, although you can launch them with
`pri; launch` command.

#### SPP Controller

SPP controller consists of `spp-ctl` and SPP CLI.
`spp-ctl` is a HTTP server for REST APIs for managing SPP processes.

```sh
# terminal 1
$ python3 src/spp-ctl/spp-ctl -b 192.168.1.100
```

SPP CLI is a client of `spp-ctl` for providing simple user interface
without using REST APIs.

```sh
# terminal 2
$ python3 src/spp.py -b 192.168.1.100
```

#### SPP Processes

Launch SPP primary and secondary processes.
SPP primary is a resource manager and initializing EAL for secondary
processes. Secondary process behaves as a client of primary process
and a worker for doing tasks.

```sh
# terminal 3
$ sudo ./src/primary/x86_64-native-linuxapp-gcc/spp_primary \
    -l 1 -n 4 \
    --socket-mem 512,512 \
    --huge-dir=/dev/hugepages \
    --proc-type=primary \
    -- \
    -p 0x03 \
    -n 10 \
    -s 192.168.1.100:5555
```

There are several kinds of secondary process. Here is an example of the simplest
one.

```sh
# terminal 4
$ sudo ./src/nfv/x86_64-native-linuxapp-gcc/spp_nfv \
    -l 2-3 -n 4 \
    --proc-type=secondary \
    -- \
    -n 1 \
    -s 192.168.1.100:6666
```

After all of SPP processes are launched, configure network path from SPP CLI.
Please refer to SPP
[Use Cases](https://doc.dpdk.org/spp/setup/use_cases.html)
for the configuration.
