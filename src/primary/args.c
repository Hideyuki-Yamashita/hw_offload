/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015-2016 Intel Corporation
 * Copyright(c) 2019 Nippon Telegraph and Telephone Corporation
 */

#include <getopt.h>

#include <rte_memory.h>

#include "shared/common.h"
#include "args.h"
#include "init.h"
#include "primary.h"

/* global var for number of rings - extern in header */
uint16_t num_rings;
char *server_ip;
int server_port;

/* Flag for deciding to forward */
int do_forwarding;

/*
 * Long options mapped to a short option.
 *
 * First long only option value must be >= 256, so that we won't
 * conflict with short options.
 */
enum {
	CMD_LINE_OPT_MIN_NUM = 256,
	CMD_OPT_DISP_STATS,
};

struct option lgopts[] = {
	{"disp-stats", no_argument, NULL, CMD_OPT_DISP_STATS},
	{0}
};

static const char *progname;

/**
 * Prints out usage information to stdout
 */
static void
usage(void)
{
	RTE_LOG(INFO, PRIMARY,
	    "%s [EAL options] -- -p PORTMASK -n NUM_CLIENTS [-s NUM_SOCKETS]\n"
	    " -p PORTMASK: hexadecimal bitmask of ports to use\n"
	    " -n NUM_RINGS: number of ring ports used from secondaries\n"
	    , progname);
}

int set_forwarding_flg(int flg)
{
	if (flg == 0 || flg == 1)
		do_forwarding = flg;
	else {
		RTE_LOG(ERR, PRIMARY, "Invalid value for forwarding flg.\n");
		return -1;
	}
	return 0;
}

int get_forwarding_flg(void)
{
	if (do_forwarding < 0) {
		RTE_LOG(ERR, PRIMARY, "Forwarding flg is not initialized.\n");
		return -1;
	}
	return do_forwarding;
}

/**
 * The ports to be used by the application are passed in
 * the form of a bitmask. This function parses the bitmask
 * and places the port numbers to be used into the port[]
 * array variable
 */
int
parse_portmask(struct port_info *ports, uint16_t max_ports,
		const char *portmask)
{
	char *end = NULL;
	unsigned long pm;
	uint16_t count = 0;

	if (portmask == NULL || *portmask == '\0')
		return -1;

	/* convert parameter to a number and verify */
	pm = strtoul(portmask, &end, 16);
	if (end == NULL || *end != '\0' || pm == 0)
		return -1;

	/* loop through bits of the mask and mark ports */
	while (pm != 0) {
		if (pm & 0x01) { /* bit is set in mask, use port */
			if (count >= max_ports)
				RTE_LOG(WARNING, PRIMARY,
					"port %u not present - ignoring\n",
					count);
			else
				ports->id[ports->num_ports++] = count;
		}
		pm = (pm >> 1);
		count++;
	}

	return 0;
}

/**
 * Take the number of clients passed with `-n` option and convert to
 * to a number to store in the num_clients variable.
 */
static int
parse_nof_rings(uint16_t *num_clients, const char *clients)
{
	char *end = NULL;
	unsigned long temp;

	if (clients == NULL || *clients == '\0')
		return -1;

	temp = strtoul(clients, &end, 10);
	if (end == NULL || *end != '\0' || temp == 0)
		return -1;

	*num_clients = (uint16_t)temp;
	return 0;
}

/**
 * The application specific arguments follow the DPDK-specific
 * arguments which are stripped by the DPDK init. This function
 * processes these application arguments, printing usage info
 * on error.
 */
int
parse_app_args(uint16_t max_ports, int argc, char *argv[])
{
	int option_index, opt;
	char **argvopt = argv;
	int ret;

	progname = argv[0];

	while ((opt = getopt_long(argc, argvopt, "n:p:s:", lgopts,
		&option_index)) != EOF) {
		switch (opt) {
		case CMD_OPT_DISP_STATS:
			set_forwarding_flg(0);
			break;
		case 'p':
			if (parse_portmask(ports, max_ports, optarg) != 0) {
				usage();
				return -1;
			}
			break;
		case 'n':
			if (parse_nof_rings(&num_rings, optarg) != 0) {
				usage();
				return -1;
			}
			break;
		case 's':
			ret = parse_server(&server_ip, &server_port, optarg);
			if (ret != 0) {
				usage();
				return -1;
			}
			break;
		default:
			RTE_LOG(ERR,
				PRIMARY, "ERROR: Unknown option '%c'\n", opt);
			usage();
			return -1;
		}
	}

	if (ports->num_ports == 0 || num_rings == 0) {
		usage();
		return -1;
	}

	return 0;
}
