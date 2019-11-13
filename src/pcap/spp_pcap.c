/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Nippon Telegraph and Telephone Corporation
 */

#include <netinet/in.h>
#include <arpa/inet.h>
#include <getopt.h>
#include <sys/stat.h>

#include <rte_common.h>
#include <rte_cycles.h>

#include <lz4frame.h>

#include "shared/common.h"
#include "data_types.h"
#include "cmd_utils.h"
#include "spp_pcap.h"
#include "cmd_runner.h"
#include "cmd_parser.h"
#include "shared/secondary/common.h"
#include "shared/secondary/return_codes.h"
#include "shared/secondary/utils.h"
#include "shared/secondary/spp_worker_th/port_capability.h"

#ifdef SPP_RINGLATENCYSTATS_ENABLE
#include "shared/secondary/spp_worker_th/latency_stats.h"
#endif

/* Declare global variables */
#define RTE_LOGTYPE_SPP_PCAP RTE_LOGTYPE_USER2

/* Pcap file attributes */
#define PCAP_FPATH_STRLEN 128
#define PCAP_FNAME_STRLEN 64
#define PCAP_FDATE_STRLEN 16

/* Used to identify pcap files */
#define TCPDUMP_MAGIC 0xa1b2c3d4

/* Indicates major verions of libpcap file */
#define PCAP_VERSION_MAJOR 2
#define PCAP_VERSION_MINOR 4

#define PCAP_SNAPLEN_MAX 65535

#define PCAP_LINKTYPE 1  /* Link type 1 means LINKTYPE_ETHERNET */
#define IN_CHUNK_SIZE (16*1024)
#define DEFAULT_OUTPUT_DIR "/tmp"
#define DEFAULT_FILE_LIMIT 1073741824  /* 1GiB */
#define PORT_STR_SIZE 16
#define RING_SIZE 16384
#define MAX_PCAP_BURST 256  /* Num of received packets at once */

/* Ensure snaplen not to be over the maximum size */
#define TRANCATE_SNAPLEN(a, b) (((a) < (b))?(a):(b))

/* getopt_long return value for long option */
enum SPP_LONGOPT_RETVAL {
	SPP_LONGOPT_RETVAL__ = 127,

	/*
	 * Return value definition for getopt_long()
	 * Only for long option
	 */
	SPP_LONGOPT_RETVAL_CLIENT_ID,  /* --client-id */
	SPP_LONGOPT_RETVAL_OUT_DIR,    /* --out-dir */
	SPP_LONGOPT_RETVAL_FILE_SIZE   /* --fsize */
};

/* capture thread type */
enum worker_thread_type {
	PCAP_UNUSE,  /* Not used */
	PCAP_RECEIVE,/* thread type receive */
	PCAP_WRITE   /* thread type write */
};

/* compress file generate mode */
enum comp_file_generate_mode {
	INIT_MODE,  /** Initial gen mode while capture is starting. */
	UPDATE_MODE,  /** Update gen mode when cap file size reached max. */
	CLOSE_MODE   /* Close mode used when capture is stopped. */
};

/* lz4 preferences */
static const LZ4F_preferences_t g_kprefs = {
	{
		LZ4F_max256KB,
		LZ4F_blockLinked,
		LZ4F_noContentChecksum,
		LZ4F_frame,
		0,  /* unknown content size */
		{ 0, 0},  /* reserved, must be set to 0 */
	},
	0,  /* compression level; 0 == default */
	0,  /* autoflush */
	{ 0, 0, 0, 0},   /* reserved, must be set to 0 */
};

/* pcap file header */
struct __attribute__((__packed__)) pcap_header {
	uint32_t magic_number;  /* magic number */
	uint16_t major_ver;  /* major version */
	uint16_t minor_ver;  /* minor version */
	int32_t thiszone;  /* GMT to local correction */
	uint32_t sigfigs;  /* accuracy of timestamps */
	uint32_t snaplen;  /* max length of captured packets, in octets */
	uint32_t network;  /* data link type */
};

/* pcap packet header */
struct pcap_packet_header {
	uint32_t ts_sec;   /* time stamp seconds */
	uint32_t ts_usec;  /* time stamp micro seconds */
	uint32_t write_len;   /* write length */
	uint32_t packet_len;  /* packet length */
};

/* Option for pcap. */
struct pcap_option {
	struct timespec start_time;  /* start time */
	uint64_t fsize_limit;  /* file size limit */
	char compress_file_path[PCAP_FPATH_STRLEN];  /* file path */
	char compress_file_date[PCAP_FDATE_STRLEN];  /* file name date */
	struct sppwk_port_info port_cap;  /* capture port */
	struct rte_ring *cap_ring;  /* RTE ring structure */
};

/**
 * pcap management info which stores attributes.
 * (e.g. worker thread type, file number, pointer to writing file etc) per core
 */
struct pcap_mng_info {
	volatile enum worker_thread_type type;  /* thread type */
	enum sppwk_capture_status status;  /* ideling or running */
	int thread_no;  /* thread no */
	int file_no;    /* file no */
	char compress_file_name[PCAP_FNAME_STRLEN];  /* lz4 file name */
	LZ4F_compressionContext_t ctx;  /* lz4 file Ccontext */
	FILE *compress_fp;  /* lzf file pointer */
	size_t outbuf_capacity;  /* compress date buffer size */
	void *outbuff;  /* compress date buffer */
	uint64_t file_size;  /* file write size */
};

/* Pcap status info. */
struct pcap_status_info {
	int thread_cnt;  /* thread count */
	int start_up_cnt;  /* thread start up count */
};

/* Interface management information */
static struct iface_info g_iface_info;

/* Core management information */
static struct spp_pcap_core_mng_info g_core_info[RTE_MAX_LCORE];

/* Packet capture request information */
static int g_capture_request;

/* Packet capture status information */
static int g_capture_status;

/* pcap option */
static struct pcap_option g_pcap_option;

/* pcap managed info */
static struct pcap_mng_info g_pcap_info[RTE_MAX_LCORE];

/* pcap thread status info */
struct pcap_status_info g_pcap_thread_info;

/* pcap total write packet count */
static long long g_total_write[RTE_MAX_LCORE];

/* Print help message */
static void
usage(const char *progname)
{
	RTE_LOG(INFO, SPP_PCAP, "Usage: %s [EAL args] --"
		" --client-id CLIENT_ID"
		" -s IPADDR:PORT"
		" -c CAP_PORT"
		" [--out-dir OUTPUT_DIR]"
		" [--fsize MAX_FILE_SIZE]\n"
		" --client-id CLIENT_ID: My client ID\n"
		" -s IPADDR:PORT: IP addr and sec port for spp-ctl\n"
		" -c: Captured port (e.g. 'phy:0' or 'ring:1')\n"
		" --out-dir: Output dir (Default is /tmp)\n"
		" --fsize: Maximum captured file size (Default is 1GiB)\n"
		, progname);
}

/* Parse `--fsize` option and get the value */
static int
parse_fsize(const char *fsize_str, uint64_t *fsize)
{
	uint64_t fs = 0;
	char *endptr = NULL;

	fs = strtoull(fsize_str, &endptr, 10);
	if (unlikely(fsize_str == endptr) || unlikely(*endptr != '\0'))
		return SPPWK_RET_NG;

	*fsize = fs;
	RTE_LOG(DEBUG, SPP_PCAP, "Set fzise = %ld\n", *fsize);
	return SPPWK_RET_OK;
}

/* Parse `-c` option for captured port and get the port type and ID */
static int
parse_captured_port(const char *port_str, enum port_type *iface_type,
			int *iface_no)
{
	enum port_type type = UNDEF;
	const char *no_str = NULL;
	char *endptr = NULL;

	/* Find out which type of interface from resource UID */
	if (strncmp(port_str, SPPWK_PHY_STR ":",
			strlen(SPPWK_PHY_STR)+1) == 0) {
		/* NIC */
		type = PHY;
		no_str = &port_str[strlen(SPPWK_PHY_STR)+1];
	} else if (strncmp(port_str, SPPWK_RING_STR ":",
			strlen(SPPWK_RING_STR)+1) == 0) {
		/* RING */
		type = RING;
		no_str = &port_str[strlen(SPPWK_RING_STR)+1];
	} else {
		/* OTHER */
		RTE_LOG(ERR, SPP_PCAP, "The interface that does not suppor. "
					"(port = %s)\n", port_str);
		return SPPWK_RET_NG;
	}

	/* Convert from string to number */
	int ret_no = strtol(no_str, &endptr, 0);
	if (unlikely(no_str == endptr) || unlikely(*endptr != '\0')) {
		/* No IF number */
		RTE_LOG(ERR, SPP_PCAP, "No interface number. (port = %s)\n",
								port_str);
		return SPPWK_RET_NG;
	}

	*iface_type = type;
	*iface_no = ret_no;

	RTE_LOG(DEBUG, SPP_PCAP, "Port = %s => Type = %d No = %d\n",
					port_str, *iface_type, *iface_no);
	return SPPWK_RET_OK;
}

/* Parse options for client app */
static int
parse_app_args(int argc, char *argv[])
{
	int cli_id;  /* Client ID. */
	char *ctl_ip;  /* IP address of spp_ctl. */
	int ctl_port;  /* Port num to connect spp_ctl. */
	char cap_port_str[PORT_STR_SIZE];  /* Captured port. */
	int cnt;
	int ret;
	int option_index, opt;

	int proc_flg = 0;
	int server_flg = 0;
	int port_flg = 0;

	const int argcopt = argc;
	char *argvopt[argcopt];
	const char *progname = argv[0];

	static struct option lgopts[] = {
		{ "client-id", required_argument, NULL,
			SPP_LONGOPT_RETVAL_CLIENT_ID },
		{ "out-dir", required_argument, NULL,
			SPP_LONGOPT_RETVAL_OUT_DIR },
		{ "fsize", required_argument, NULL,
			SPP_LONGOPT_RETVAL_FILE_SIZE},
		{ 0 },
	};
	/**
	 * Save argv to argvopt to avoid losing the order of options
	 * by getopt_long()
	 */
	for (cnt = 0; cnt < argcopt; cnt++)
		argvopt[cnt] = argv[cnt];

	/* option parameters init */
	memset(&g_pcap_option, 0x00, sizeof(g_pcap_option));
	strcpy(g_pcap_option.compress_file_path, DEFAULT_OUTPUT_DIR);
	g_pcap_option.fsize_limit = DEFAULT_FILE_LIMIT;

	/* Check options of application */
	while ((opt = getopt_long(argc, argvopt, "c:s:", lgopts,
			&option_index)) != EOF) {
		switch (opt) {
		case SPP_LONGOPT_RETVAL_CLIENT_ID:
			if (parse_client_id(&cli_id, optarg) != SPPWK_RET_OK) {
				usage(progname);
				return SPPWK_RET_NG;
			}
			set_client_id(cli_id);
			proc_flg = 1;
			break;
		case SPP_LONGOPT_RETVAL_OUT_DIR:
			strcpy(g_pcap_option.compress_file_path, optarg);
			struct stat statBuf;
			if (g_pcap_option.compress_file_path[0] == '\0' ||
						stat(optarg, &statBuf) != 0) {
				usage(progname);
				return SPPWK_RET_NG;
			}
			break;
		case SPP_LONGOPT_RETVAL_FILE_SIZE:
			if (parse_fsize(optarg, &g_pcap_option.fsize_limit) !=
					SPPWK_RET_OK) {
				usage(progname);
				return SPPWK_RET_NG;
			}
			break;
		case 'c':  /* captured port */
			strcpy(cap_port_str, optarg);
			if (parse_captured_port(optarg,
					&g_pcap_option.port_cap.iface_type,
					&g_pcap_option.port_cap.iface_no) !=
					SPPWK_RET_OK) {
				usage(progname);
				return SPPWK_RET_NG;
			}
			port_flg = 1;
			break;
		case 's':  /* server addr */
			ret = parse_server(&ctl_ip, &ctl_port, optarg);
			set_spp_ctl_ip(ctl_ip);
			set_spp_ctl_port(ctl_port);
			if (ret != SPPWK_RET_OK) {
				usage(progname);
				return SPPWK_RET_NG;
			}
			server_flg = 1;
			break;
		default:
			usage(progname);
			return SPPWK_RET_NG;
		}
	}

	/* Check mandatory parameters */
	if ((proc_flg == 0) || (server_flg == 0) || (port_flg == 0)) {
		usage(progname);
		return SPPWK_RET_NG;
	}

	RTE_LOG(INFO, SPP_PCAP,
			"Parsed app args ('--client-id %d', '-s %s:%d', "
			"'-c %s', '--out-dir %s', '--fsize %ld')\n",
			cli_id, ctl_ip, ctl_port, cap_port_str,
			g_pcap_option.compress_file_path,
			g_pcap_option.fsize_limit);
	return SPPWK_RET_OK;
}

/* TODO(yasufum) refactor name of func and vars, and comments. */
/**
 * Get each of attrs such as name, type or nof ports of a thread on a lcore.
 * MEMO: This func is not for getting core status, but thread info actually.
 */
int
spp_pcap_get_core_status(
		unsigned int lcore_id,
		struct sppwk_lcore_params *params)
{
	char role_type[8];
	struct pcap_mng_info *info = &g_pcap_info[lcore_id];
	char name[PCAP_FPATH_STRLEN + PCAP_FDATE_STRLEN];
	struct sppwk_port_idx rx_ports[1];
	int rx_num = 0;
	int res;

	RTE_LOG(DEBUG, SPP_PCAP, "status core[%d]\n", lcore_id);
	if (info->type == PCAP_RECEIVE) {
		memset(rx_ports, 0x00, sizeof(rx_ports));
		rx_ports[0].iface_type = g_pcap_option.port_cap.iface_type;
		rx_ports[0].iface_no   = g_pcap_option.port_cap.iface_no;
		rx_num = 1;
		strcpy(role_type, "receive");
	}
	if (info->type == PCAP_WRITE) {
		memset(name, 0x00, sizeof(name));
		if (info->compress_fp != NULL)
			snprintf(name, sizeof(name) - 1, "%s/%s",
					g_pcap_option.compress_file_path,
					info->compress_file_name);
		strcpy(role_type, "write");
	}

	/* Set information with specified by the command. */
	res = (*params->lcore_proc)(params, lcore_id, name, role_type,
		rx_num, rx_ports, 0, NULL);
	if (unlikely(res != 0))
		return SPPWK_RET_NG;

	return SPPWK_RET_OK;
}

/* write compressed data into file  */
static int output_pcap_file(FILE *compress_fp, void *srcbuf, size_t write_len)
{
	size_t write_size;

	if (write_len == 0)
		return SPPWK_RET_OK;
	write_size = fwrite(srcbuf, write_len, 1, compress_fp);
	if (write_size != 1) {
		RTE_LOG(ERR, SPP_PCAP, "file write error len=%lu\n",
								write_len);
		return SPPWK_RET_NG;
	}
	return SPPWK_RET_OK;
}

/* compress data & write file */
static int output_lz4_pcap_file(struct pcap_mng_info *info,
			       void *srcbuf,
			       int src_len)
{
	size_t compress_len;

	compress_len = LZ4F_compressUpdate(info->ctx, info->outbuff,
				info->outbuf_capacity, srcbuf, src_len, NULL);
	if (LZ4F_isError(compress_len)) {
		RTE_LOG(ERR, SPP_PCAP, "Compression failed: error %zd\n",
							compress_len);
		return SPPWK_RET_NG;
	}
	RTE_LOG(DEBUG, SPP_PCAP, "src len=%d\n", src_len);
	if (output_pcap_file(info->compress_fp, info->outbuff,
						compress_len) != 0)
		return SPPWK_RET_NG;

	return SPPWK_RET_OK;
}

/**
 * File compression operation. There are three mode.
 * Open and update and close.
 */
static int file_compression_operation(struct pcap_mng_info *info,
				   enum comp_file_generate_mode mode)
{
	struct pcap_header pcap_h;
	size_t ctxCreation;
	size_t headerSize;
	size_t compress_len;
	char temp_file[PCAP_FPATH_STRLEN + PCAP_FNAME_STRLEN];
	char save_file[PCAP_FPATH_STRLEN + PCAP_FNAME_STRLEN];
	const char *iface_type_str;

	if (mode == INIT_MODE) { /* initial generation mode */
		/* write buffer size get */
		info->outbuf_capacity = LZ4F_compressBound(IN_CHUNK_SIZE,
								&g_kprefs);
		/* write buff allocation */
		info->outbuff = malloc(info->outbuf_capacity);

		/* Initialize pcap file name */
		info->file_size = 0;
		info->file_no = 1;
		if (g_pcap_option.port_cap.iface_type == PHY)
			iface_type_str = SPPWK_PHY_STR;
		else
			iface_type_str = SPPWK_RING_STR;
		snprintf(info->compress_file_name,
					PCAP_FNAME_STRLEN - 1,
					"spp_pcap.%s.%s%d.%u.%u.pcap.lz4",
					g_pcap_option.compress_file_date,
					iface_type_str,
					g_pcap_option.port_cap.iface_no,
					info->thread_no,
					info->file_no);
	} else if (mode == UPDATE_MODE) { /* update generation mode */
		/* old compress file close */
		/* flush whatever remains within internal buffers */
		compress_len = LZ4F_compressEnd(info->ctx, info->outbuff,
					info->outbuf_capacity, NULL);
		if (LZ4F_isError(compress_len)) {
			RTE_LOG(ERR, SPP_PCAP, "Failed to end compression: "
					"error %zd\n", compress_len);
			fclose(info->compress_fp);
			info->compress_fp = NULL;
			free(info->outbuff);
			return SPPWK_RET_NG;
		}
		if (output_pcap_file(info->compress_fp, info->outbuff,
						compress_len) != SPPWK_RET_OK) {
			fclose(info->compress_fp);
			info->compress_fp = NULL;
			free(info->outbuff);
			return SPPWK_RET_NG;
		}

		/* flush remained data */
		fclose(info->compress_fp);
		info->compress_fp = NULL;

		/* rename temporary file */
		memset(temp_file, 0,
			PCAP_FPATH_STRLEN + PCAP_FNAME_STRLEN);
		memset(save_file, 0,
			PCAP_FPATH_STRLEN + PCAP_FNAME_STRLEN);
		snprintf(temp_file,
		    (PCAP_FPATH_STRLEN + PCAP_FNAME_STRLEN) - 1,
		    "%s/%s.tmp", g_pcap_option.compress_file_path,
		    info->compress_file_name);
		snprintf(save_file,
		    (PCAP_FPATH_STRLEN + PCAP_FNAME_STRLEN) - 1,
		    "%s/%s", g_pcap_option.compress_file_path,
		    info->compress_file_name);
		rename(temp_file, save_file);

		/* Initialize pcap file name */
		info->file_size = 0;
		info->file_no++;
		if (g_pcap_option.port_cap.iface_type == PHY)
			iface_type_str = SPPWK_PHY_STR;
		else
			iface_type_str = SPPWK_RING_STR;
		snprintf(info->compress_file_name,
					PCAP_FNAME_STRLEN - 1,
					"spp_pcap.%s.%s%d.%u.%u.pcap.lz4",
					g_pcap_option.compress_file_date,
					iface_type_str,
					g_pcap_option.port_cap.iface_no,
					info->thread_no,
					info->file_no);
	} else { /* close mode */
		/* Close temporary file and rename to persistent */
		if (info->compress_fp == NULL)
			return SPPWK_RET_OK;
		compress_len = LZ4F_compressEnd(info->ctx, info->outbuff,
					info->outbuf_capacity, NULL);
		if (LZ4F_isError(compress_len)) {
			RTE_LOG(ERR, SPP_PCAP, "Failed to end compression: "
					"error %zd\n", compress_len);
		} else {
			output_pcap_file(info->compress_fp, info->outbuff,
								compress_len);
			info->file_size += compress_len;
		}
		/* flush remained data */
		fclose(info->compress_fp);

		/* rename temporary file */
		memset(temp_file, 0,
			PCAP_FPATH_STRLEN + PCAP_FNAME_STRLEN);
		memset(save_file, 0,
			PCAP_FPATH_STRLEN + PCAP_FNAME_STRLEN);
		snprintf(temp_file,
		    (PCAP_FPATH_STRLEN + PCAP_FNAME_STRLEN) - 1,
		    "%s/%s.tmp", g_pcap_option.compress_file_path,
		    info->compress_file_name);
		snprintf(save_file,
		    (PCAP_FPATH_STRLEN + PCAP_FNAME_STRLEN) - 1,
		    "%s/%s", g_pcap_option.compress_file_path,
		    info->compress_file_name);
		rename(temp_file, save_file);

		info->compress_fp = NULL;
		free(info->outbuff);
		return SPPWK_RET_OK;
	}

	/* file open */
	memset(temp_file, 0,
		PCAP_FPATH_STRLEN + PCAP_FNAME_STRLEN);
	snprintf(temp_file,
		(PCAP_FPATH_STRLEN + PCAP_FNAME_STRLEN) - 1,
		"%s/%s.tmp", g_pcap_option.compress_file_path,
		info->compress_file_name);
	RTE_LOG(INFO, SPP_PCAP, "open compress filename=%s\n", temp_file);
	info->compress_fp = fopen(temp_file, "wb");
	if (info->compress_fp == NULL) {
		RTE_LOG(ERR, SPP_PCAP, "file open error! filename=%s\n",
						info->compress_file_name);
		free(info->outbuff);
		return SPPWK_RET_NG;
	}

	/* init lz4 stream */
	ctxCreation = LZ4F_createCompressionContext(&info->ctx, LZ4F_VERSION);
	if (LZ4F_isError(ctxCreation)) {
		RTE_LOG(ERR, SPP_PCAP, "LZ4F_createCompressionContext error "
						"(%zd)\n", ctxCreation);
		fclose(info->compress_fp);
		info->compress_fp = NULL;
		free(info->outbuff);
		return SPPWK_RET_NG;
	}

	/* write compress frame header */
	headerSize = LZ4F_compressBegin(info->ctx, info->outbuff,
					info->outbuf_capacity, &g_kprefs);
	if (LZ4F_isError(headerSize)) {
		RTE_LOG(ERR, SPP_PCAP, "Failed to start compression: "
					"error %zd\n", headerSize);
		fclose(info->compress_fp);
		info->compress_fp = NULL;
		free(info->outbuff);
		return SPPWK_RET_NG;
	}
	RTE_LOG(DEBUG, SPP_PCAP, "Buffer size is %zd bytes, header size %zd "
			"bytes\n", info->outbuf_capacity, headerSize);
	if (output_pcap_file(info->compress_fp, info->outbuff,
						headerSize) != 0) {
		fclose(info->compress_fp);
		info->compress_fp = NULL;
		free(info->outbuff);
		return SPPWK_RET_NG;
	}
	info->file_size = headerSize;

	/* init the common pcap header */
	pcap_h.magic_number = TCPDUMP_MAGIC;
	pcap_h.major_ver = PCAP_VERSION_MAJOR;
	pcap_h.minor_ver = PCAP_VERSION_MINOR;
	pcap_h.thiszone = 0;
	pcap_h.sigfigs = 0;
	pcap_h.snaplen = PCAP_SNAPLEN_MAX;
	pcap_h.network = PCAP_LINKTYPE;

	/* pcap header write */
	if (output_lz4_pcap_file(info, &pcap_h, sizeof(struct pcap_header))
							!= SPPWK_RET_OK) {
		RTE_LOG(ERR, SPP_PCAP, "pcap header write  error!\n");
		fclose(info->compress_fp);
		info->compress_fp = NULL;
		free(info->outbuff);
		return SPPWK_RET_NG;
	}

	return SPPWK_RET_OK;
}

/* compress packet data */
static int compress_file_packet(struct pcap_mng_info *info,
				struct rte_mbuf *cap_pkt)
{
	unsigned int write_packet_length;
	unsigned int packet_length;
	struct timespec cap_time;
	struct pcap_packet_header pcap_packet_h;
	unsigned int remaining_bytes;
	int bytes_to_write;

	if (info->compress_fp == NULL)
		return SPPWK_RET_OK;

	/* capture file rool */
	if (info->file_size > g_pcap_option.fsize_limit) {
		if (file_compression_operation(info, UPDATE_MODE)
							!= SPPWK_RET_OK)
			return SPPWK_RET_NG;
	}

	/* cast to packet */
	packet_length = rte_pktmbuf_pkt_len(cap_pkt);

	/* truncate packet over the maximum length */
	write_packet_length = TRANCATE_SNAPLEN(PCAP_SNAPLEN_MAX,
							packet_length);

	/* get time */
	clock_gettime(CLOCK_REALTIME, &cap_time);

	/* write block header */
	pcap_packet_h.ts_sec = (int32_t)cap_time.tv_sec;
	pcap_packet_h.ts_usec = (int32_t)(cap_time.tv_nsec / 1000);
	pcap_packet_h.write_len = write_packet_length;
	pcap_packet_h.packet_len = packet_length;

	/* output to lz4_pcap_file */
	if (output_lz4_pcap_file(info, &pcap_packet_h.ts_sec,
			sizeof(struct pcap_packet_header)) != SPPWK_RET_OK) {
		file_compression_operation(info, CLOSE_MODE);
		return SPPWK_RET_NG;
	}
	info->file_size += sizeof(struct pcap_packet_header);

	/* write content */
	remaining_bytes = write_packet_length;
	while (cap_pkt != NULL && remaining_bytes > 0) {
		/* write file */
		bytes_to_write = TRANCATE_SNAPLEN(
					rte_pktmbuf_data_len(cap_pkt),
					remaining_bytes);

		/* output to lz4_pcap_file */
		if (output_lz4_pcap_file(info,
				rte_pktmbuf_mtod(cap_pkt, void*),
						bytes_to_write) != 0) {
			file_compression_operation(info, CLOSE_MODE);
			return SPPWK_RET_NG;
		}
		cap_pkt = cap_pkt->next;
		remaining_bytes -= bytes_to_write;
		info->file_size += bytes_to_write;
	}

	return SPPWK_RET_OK;
}

/* Receive packets from shared ring buffer */
static int pcap_proc_receive(int lcore_id)
{
	struct timespec cur_time;  /* Used as timestamp for the file name */
	struct tm l_time;
	int buf;
	int nb_rx = 0;
	int nb_tx = 0;
	struct sppwk_port_info *rx;
	struct rte_mbuf *bufs[MAX_PCAP_BURST];
	struct pcap_mng_info *info = &g_pcap_info[lcore_id];
	struct rte_ring *write_ring = g_pcap_option.cap_ring;
	static long long total_rx;
	static long long total_drop;

	if (g_capture_request == SPP_CAPTURE_IDLE) {
		if (info->status == SPP_CAPTURE_RUNNING) {
			RTE_LOG(DEBUG, SPP_PCAP,
					"Recive on lcore %d, run->idle\n",
					lcore_id);
			RTE_LOG(INFO, SPP_PCAP,
					"Recive on lcore %d, total_rx=%llu, "
					"total_drop=%llu\n", lcore_id,
					total_rx, total_drop);

			info->status = SPP_CAPTURE_IDLE;
			g_capture_status = SPP_CAPTURE_IDLE;
			if (g_pcap_thread_info.start_up_cnt != 0)
				g_pcap_thread_info.start_up_cnt -= 1;
		}
		return SPPWK_RET_OK;
	}
	if (info->status == SPP_CAPTURE_IDLE) {
		/* Get time for output file name */
		clock_gettime(CLOCK_REALTIME, &cur_time);
		memset(g_pcap_option.compress_file_date, 0, PCAP_FDATE_STRLEN);
		localtime_r(&cur_time.tv_sec, &l_time);
		strftime(g_pcap_option.compress_file_date, PCAP_FDATE_STRLEN,
					"%Y%m%d%H%M%S", &l_time);
		info->status = SPP_CAPTURE_RUNNING;
		g_capture_status = SPP_CAPTURE_RUNNING;

		RTE_LOG(DEBUG, SPP_PCAP,
				"Recive on lcore %d, idle->run\n", lcore_id);
		RTE_LOG(DEBUG, SPP_PCAP,
				"Recive on lcore %d, start time=%s\n",
				lcore_id, g_pcap_option.compress_file_date);
		g_pcap_thread_info.start_up_cnt += 1;
		total_rx = 0;
		total_drop = 0;
	}

	/* Write thread start up wait. */
	if (g_pcap_thread_info.thread_cnt > g_pcap_thread_info.start_up_cnt)
		return SPPWK_RET_OK;

	/* Receive packets */
	rx = &g_pcap_option.port_cap;
#ifdef SPP_RINGLATENCYSTATS_ENABLE
	nb_rx = sppwk_eth_ring_stats_rx_burst(rx->ethdev_port_id,
			rx->iface_type, rx->iface_no, 0, bufs, MAX_PCAP_BURST);
#else
	nb_rx = rte_eth_rx_burst(rx->ethdev_port_id, 0, bufs, MAX_PCAP_BURST);
#endif
	if (unlikely(nb_rx == 0))
		return SPPWK_RET_OK;

	/* Forward to ring for writer thread */
	nb_tx = rte_ring_enqueue_burst(write_ring, (void *)bufs, nb_rx, NULL);

	/* Discard remained packets to release mbuf */
	if (unlikely(nb_tx < nb_rx)) {
		RTE_LOG(ERR, SPP_PCAP, "drop packets(receve) %d\n",
							(nb_rx - nb_tx));
		for (buf = nb_tx; buf < nb_rx; buf++)
			rte_pktmbuf_free(bufs[buf]);
	}

	total_rx += nb_rx;
	total_drop += nb_rx - nb_tx;

	return SPPWK_RET_OK;
}

/* Output packets to file on writer thread */
static int pcap_proc_write(int lcore_id)
{
	int ret = SPPWK_RET_OK;
	int buf;
	int nb_rx = 0;
	struct rte_mbuf *bufs[MAX_PCAP_BURST];
	struct rte_mbuf *mbuf = NULL;
	struct pcap_mng_info *info = &g_pcap_info[lcore_id];
	struct rte_ring *read_ring = g_pcap_option.cap_ring;

	if (g_capture_status == SPP_CAPTURE_IDLE) {
		if (info->status == SPP_CAPTURE_IDLE)
			return SPPWK_RET_OK;
	}
	if (info->status == SPP_CAPTURE_IDLE) {
		RTE_LOG(DEBUG, SPP_PCAP, "write[%d] idle->run\n", lcore_id);
		info->status = SPP_CAPTURE_RUNNING;
		if (file_compression_operation(info, INIT_MODE)
						!= SPPWK_RET_OK) {
			info->status = SPP_CAPTURE_IDLE;
			return SPPWK_RET_NG;
		}
		g_pcap_thread_info.start_up_cnt += 1;
		g_total_write[lcore_id] = 0;
	}

	/* Read packets from shared ring */
	nb_rx =  rte_ring_mc_dequeue_burst(read_ring, (void *)bufs,
					   MAX_PCAP_BURST, NULL);
	if (unlikely(nb_rx == 0)) {
		if (g_capture_status == SPP_CAPTURE_IDLE) {
			RTE_LOG(DEBUG, SPP_PCAP,
					"Write on lcore %d, run->idle\n",
					lcore_id);
			RTE_LOG(INFO, SPP_PCAP,
					"Write on lcore %d, total_write=%llu\n",
					lcore_id, g_total_write[lcore_id]);

			info->status = SPP_CAPTURE_IDLE;
			if (g_pcap_thread_info.start_up_cnt != 0)
				g_pcap_thread_info.start_up_cnt -= 1;
			if (file_compression_operation(info, CLOSE_MODE)
							!= SPPWK_RET_OK)
				return SPPWK_RET_NG;
		}
		return SPPWK_RET_OK;
	}

	for (buf = 0; buf < nb_rx; buf++) {
		mbuf = bufs[buf];
		rte_prefetch0(rte_pktmbuf_mtod(mbuf, void *));
		if (compress_file_packet(&g_pcap_info[lcore_id], mbuf)
							!= SPPWK_RET_OK) {
			RTE_LOG(ERR, SPP_PCAP,
					"Failed compress_file_packet(), "
					"errno=%d (%s)\n",
					errno, strerror(errno));
			ret = SPPWK_RET_NG;
			info->status = SPP_CAPTURE_IDLE;
			file_compression_operation(info, CLOSE_MODE);
			break;
		}
	}

	/* Free mbuf */
	for (buf = 0; buf < nb_rx; buf++)
		rte_pktmbuf_free(bufs[buf]);

	g_total_write[lcore_id] += nb_rx;
	return ret;
}

/* Main process of slave core */
static int
slave_main(void *arg __attribute__ ((unused)))
{
	int ret = SPPWK_RET_OK;
	unsigned int lcore_id = rte_lcore_id();
	struct pcap_mng_info *pcap_info = &g_pcap_info[lcore_id];

	if (pcap_info->thread_no == 0) {
		RTE_LOG(INFO, SPP_PCAP, "Receiver started on lcore %d.\n",
				lcore_id);
		pcap_info->type = PCAP_RECEIVE;
	} else {
		RTE_LOG(INFO, SPP_PCAP, "Writer %d started on lcore %d.\n",
					pcap_info->thread_no, lcore_id);
		pcap_info->type = PCAP_WRITE;
	}
	set_core_status(lcore_id, SPPWK_LCORE_IDLING);

	while (1) {
		if (sppwk_get_lcore_status(lcore_id) == SPPWK_LCORE_REQ_STOP) {
			if (pcap_info->status == SPP_CAPTURE_IDLE)
				break;
			if (pcap_info->type == PCAP_RECEIVE)
				g_capture_request = SPP_CAPTURE_IDLE;
		}

		if (pcap_info->type == PCAP_RECEIVE)
			ret = pcap_proc_receive(lcore_id);
		else
			ret = pcap_proc_write(lcore_id);
		if (unlikely(ret != SPPWK_RET_OK)) {
			RTE_LOG(ERR, SPP_PCAP,
					"Failed to capture on lcore %d.\n",
					lcore_id);
			break;
		}
	}

	set_core_status(lcore_id, SPPWK_LCORE_STOPPED);
	RTE_LOG(INFO, SPP_PCAP,
			"Terminated slave on lcore %d.\n", lcore_id);
	return ret;
}

/**
 * Main function
 *
 * Return SPPWK_RET_NG explicitly if error is occurred.
 */
int
main(int argc, char *argv[])
{
	int ret;
	char ctl_ip[IPADDR_LEN] = { 0 };
	int ctl_port;
	int ret_cmd_init;
	unsigned int master_lcore;
	unsigned int lcore_id;
	unsigned int thread_no;

#ifdef SPP_DEMONIZE
	/* Daemonize process */
	int ret_daemon = daemon(0, 0);
	if (unlikely(ret_daemon != 0)) {
		RTE_LOG(ERR, SPP_PCAP, "daemonize is failed. (ret = %d)\n",
				ret_daemon);
		return ret_daemon;
	}
#endif

	/* Signal handler registration (SIGTERM/SIGINT) */
	signal(SIGTERM, stop_process);
	signal(SIGINT,  stop_process);

	ret = rte_eal_init(argc, argv);
	if (unlikely(ret < 0))
		rte_exit(EXIT_FAILURE, "Invalid EAL arguments.\n");

	argc -= ret;
	argv += ret;

	/**
	 * It should be initialized outside of while loop, or failed to
	 * compile because it is referred when finalize `g_core_info`.
	 */
	master_lcore = rte_get_master_lcore();

	/**
	 * If any failure is occured in the while block, break to go the end
	 * of the block to finalize.
	 */
	while (1) {
		/* Parse spp_pcap specific parameters */
		int ret_parse = parse_app_args(argc, argv);
		if (unlikely(ret_parse != 0))
			break;

		/* set manage address */
		if (spp_set_mng_data_addr(&g_iface_info, g_core_info,
					&g_capture_request,
					&g_capture_status) < 0) {
			RTE_LOG(ERR, SPP_PCAP,
				"manage address set is failed.\n");
			break;
		}

		int ret_mng = init_mng_data();
		if (unlikely(ret_mng != 0))
			break;

		sppwk_port_capability_init();

		/* Setup connection for accepting commands from controller */
		get_spp_ctl_ip(ctl_ip);
		ctl_port = get_spp_ctl_port();
		ret_cmd_init = spp_command_proc_init(ctl_ip, ctl_port);
		if (unlikely(ret_cmd_init != SPPWK_RET_OK))
			break;

		/* capture port setup */
		struct sppwk_port_info *port_cap = &g_pcap_option.port_cap;
		struct sppwk_port_info *port_info = get_iface_info(
						port_cap->iface_type,
						port_cap->iface_no);
		if (port_info == NULL) {
			RTE_LOG(ERR, SPP_PCAP, "caputre port undefined.\n");
			break;
		}
		if (port_cap->iface_type == PHY) {
			if (port_info->iface_type != UNDEF)
				port_cap->ethdev_port_id =
					port_info->ethdev_port_id;
			else {
				RTE_LOG(ERR, SPP_PCAP,
					"caputre port undefined.(phy:%d)\n",
							port_cap->iface_no);
				break;
			}
		} else {
			if (port_info->iface_type == UNDEF) {
				ret = add_ring_pmd(port_info->iface_no);
				if (ret == SPPWK_RET_NG) {
					RTE_LOG(ERR, SPP_PCAP, "caputre port "
						"undefined.(ring:%d)\n",
						port_cap->iface_no);
					break;
				}
				port_cap->ethdev_port_id = ret;
			} else {
				RTE_LOG(ERR, SPP_PCAP, "caputre port "
						"undefined.(ring:%d)\n",
						port_cap->iface_no);
				break;
			}
		}
		RTE_LOG(DEBUG, SPP_PCAP,
				"Recv port type=%d, no=%d, port_id=%d\n",
				port_cap->iface_type, port_cap->iface_no,
				port_cap->ethdev_port_id);

		/* create ring */
		char ring_name[PORT_STR_SIZE];
		memset(ring_name, 0x00, PORT_STR_SIZE);
		snprintf(ring_name, PORT_STR_SIZE, "cap_ring_%d",
				get_client_id());
		g_pcap_option.cap_ring = rte_ring_create(ring_name,
					rte_align32pow2(RING_SIZE),
					rte_socket_id(), 0);
		if (g_pcap_option.cap_ring == NULL) {
			RTE_LOG(ERR, SPP_PCAP, "ring create error(%s).\n",
						rte_strerror(rte_errno));
			break;
		}
		RTE_LOG(DEBUG, SPP_PCAP, "Ring port name=%s, flags=0x%x\n",
				g_pcap_option.cap_ring->name,
				g_pcap_option.cap_ring->flags);

		/* Start worker threads of recive or write */
		g_pcap_thread_info.thread_cnt = 0;
		g_pcap_thread_info.start_up_cnt = 0;
		lcore_id = 0;
		thread_no = 0;
		RTE_LCORE_FOREACH_SLAVE(lcore_id) {
			g_pcap_thread_info.thread_cnt += 1;
			g_pcap_info[lcore_id].thread_no = thread_no++;
			rte_eal_remote_launch(slave_main, NULL, lcore_id);
		}

		/* Set the status of main thread to idle */
		g_core_info[master_lcore].status = SPPWK_LCORE_IDLING;
		int ret_wait = check_core_status_wait(SPPWK_LCORE_IDLING);
		if (unlikely(ret_wait != 0))
			break;

		/* Start secondary */
		set_all_core_status(SPPWK_LCORE_RUNNING);
		RTE_LOG(INFO, SPP_PCAP, "[Press Ctrl-C to quit ...]\n");

		/* Enter loop for accepting commands */
		int ret_do = 0;
		while (likely(g_core_info[master_lcore].status !=
				SPPWK_LCORE_REQ_STOP)) {
			/* Receive command */
			ret_do = sppwk_run_cmd();
			if (unlikely(ret_do != SPPWK_RET_OK))
				break;

			/*
			 * Wait to avoid CPU overloaded.
			 */
			usleep(100);
		}

		if (unlikely(ret_do != SPPWK_RET_OK)) {
			set_all_core_status(SPPWK_LCORE_REQ_STOP);
			break;
		}

		ret = SPPWK_RET_OK;
		break;
	}

	/* Finalize to exit */
	g_core_info[master_lcore].status = SPPWK_LCORE_STOPPED;
	int ret_core_end = check_core_status_wait(SPPWK_LCORE_STOPPED);
	if (unlikely(ret_core_end != 0))
		RTE_LOG(ERR, SPP_PCAP, "Failed to terminate master thread.\n");

	/* capture write ring free */
	if (g_pcap_option.cap_ring != NULL)
		rte_ring_free(g_pcap_option.cap_ring);


	RTE_LOG(INFO, SPP_PCAP, "Exit spp_pcap.\n");
	return ret;
}
