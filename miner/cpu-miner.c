/*
 * Copyright 2010 Jeff Garzik
 * Copyright 2012-2017 pooler
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.  See COPYING for more details.
 */

#include "cpuminer-config.h"
#define _GNU_SOURCE
#include "config.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include <sys/time.h>
#include <math.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/resource.h>
#include <signal.h>
#include <errno.h>
#include <curl/curl.h>
#include <jansson.h>

#include "compat.h"
#include "miner.h"
#include "sha2.h"
#include "opus_hash.h"
#include "arith_uint256.h"

#define PROGRAM_NAME		"minerd"
#define LP_SCANTIME		60

#ifdef __linux /* Linux specific policy and affinity management */
#include <sched.h>
static inline void drop_policy(void)
{
	struct sched_param param;
	param.sched_priority = 0;

#ifdef SCHED_IDLE
	if (unlikely(sched_setscheduler(0, SCHED_IDLE, &param) == -1))
#endif
#ifdef SCHED_BATCH
		sched_setscheduler(0, SCHED_BATCH, &param);
#endif
}

static inline void affine_to_cpu(int id, int cpu)
{
	cpu_set_t set;

	CPU_ZERO(&set);
	CPU_SET(cpu, &set);
	sched_setaffinity(0, sizeof(set), &set);
}
#elif defined(__FreeBSD__) /* FreeBSD specific policy and affinity management */
#include <sys/cpuset.h>
static inline void drop_policy(void)
{
}

static inline void affine_to_cpu(int id, int cpu)
{
	cpuset_t set;
	CPU_ZERO(&set);
	CPU_SET(cpu, &set);
	cpuset_setaffinity(CPU_LEVEL_WHICH, CPU_WHICH_TID, -1, sizeof(cpuset_t), &set);
}
#else
static inline void drop_policy(void)
{
}

static inline void affine_to_cpu(int id, int cpu)
{
}
#endif
		
enum workio_commands {
	WC_GET_WORK,
	WC_SUBMIT_WORK,
};

struct workio_cmd {
	enum workio_commands	cmd;
	struct thr_info		*thr;
	union {
		struct work	*work;
	} u;
};

enum algos {
	ALGO_SCRYPT,		/* scrypt(1024,1,1) */
	ALGO_SHA256D,		/* SHA-256d */
};

static const char *algo_names[] = {
	[ALGO_SCRYPT]		= "scrypt",
	[ALGO_SHA256D]		= "sha256d",
};

bool opt_debug = false;
bool opt_debug_pow = false;
bool opt_protocol = false;
static bool opt_benchmark = false;
bool opt_redirect = true;
bool want_longpoll = true;
bool have_longpoll = false;
bool have_gbt = true;
bool allow_getwork = true;
bool want_stratum = true;
bool have_stratum = false;
bool use_syslog = false;
static bool opt_background = false;
static bool opt_quiet = false;
static int opt_retries = -1;
static int opt_fail_pause = 30;
int opt_timeout = 0;
static int opt_scantime = 5;
static enum algos opt_algo = ALGO_SCRYPT;
static int opt_scrypt_n = 1024;
static int opt_n_threads;
static int num_processors;
static char *rpc_url;
static char *rpc_userpass;
static char *rpc_user, *rpc_pass;
static int pk_script_size;
static unsigned char pk_script[42];
static char coinbase_sig[101] = "";
char *opt_cert;
char *opt_proxy;
long opt_proxy_type;
struct thr_info *thr_info;
static int work_thr_id;
int longpoll_thr_id = -1;
int stratum_thr_id = -1;
struct work_restart *work_restart = NULL;
static struct stratum_ctx stratum;

pthread_mutex_t applog_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_mutex_t stats_lock;

static unsigned long accepted_count = 0L;
static unsigned long rejected_count = 0L;
static double *thr_hashrates;

static bool opus_hash_selftest(void)
{
	/* Frozen vectors: core/src/test/pow_tests.cpp OpuHash_v3plus_reference_vectors */
	static const uint8_t base0[32] = {
		0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
	};
	static const uint8_t expect0[32] = {
		0x80, 0x57, 0x26, 0x6c, 0x60, 0x4b, 0xe4, 0xfb,
		0x54, 0x15, 0x5e, 0xfd, 0x9b, 0x79, 0xe8, 0xab,
		0xc6, 0xb7, 0xcd, 0x06, 0xbd, 0x97, 0x9b, 0x68,
		0x19, 0x8a, 0x59, 0xb1, 0x55, 0xff, 0xc4, 0x0c
	};
	static const uint8_t base1[32] = {
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
		0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff
	};
	static const uint8_t expect1[32] = {
		0xf7, 0xd4, 0x24, 0x71, 0x43, 0xde, 0x2f, 0x1a,
		0xd7, 0x93, 0x10, 0x85, 0x41, 0xfc, 0xcb, 0x25,
		0x7e, 0x1b, 0x26, 0x3a, 0x23, 0xa3, 0x51, 0x30,
		0x48, 0xb3, 0x48, 0x72, 0xde, 0xac, 0x96, 0x4e
	};
	uint8_t got[32];

	opus_hash_from_base_hash(base0, got);
	if (memcmp(got, expect0, sizeof(got)) != 0)
		return false;
	opus_hash_from_base_hash(base1, got);
	return memcmp(got, expect1, sizeof(got)) == 0;
}

#ifdef HAVE_GETOPT_LONG
#include <getopt.h>
#else
struct option {
	const char *name;
	int has_arg;
	int *flag;
	int val;
};
#endif

static char const usage[] = "\
Usage: " PROGRAM_NAME " [OPTIONS]\n\
Options:\n\
  -a, --algo=ALGO       specify the algorithm to use\n\
                          scrypt    scrypt(1024, 1, 1) (default)\n\
                          scrypt:N  scrypt(N, 1, 1)\n\
                          sha256d   SHA-256d\n\
  -o, --url=URL         URL of mining server\n\
  -O, --userpass=U:P    username:password pair for mining server\n\
  -u, --user=USERNAME   username for mining server\n\
  -p, --pass=PASSWORD   password for mining server\n\
      --cert=FILE       certificate for mining server using SSL\n\
  -x, --proxy=[PROTOCOL://]HOST[:PORT]  connect through a proxy\n\
  -t, --threads=N       number of miner threads (default: number of processors)\n\
  -r, --retries=N       number of times to retry if a network call fails\n\
                          (default: retry indefinitely)\n\
  -R, --retry-pause=N   time to pause between retries, in seconds (default: 30)\n\
  -T, --timeout=N       timeout for long polling, in seconds (default: none)\n\
  -s, --scantime=N      upper bound on time spent scanning current work when\n\
                          long polling is unavailable, in seconds (default: 5)\n\
      --coinbase-addr=ADDR  payout address for solo mining\n\
      --coinbase-sig=TEXT  data to insert in the coinbase when possible\n\
      --no-longpoll     disable long polling support\n\
      --no-getwork      disable getwork support\n\
      --no-gbt          disable getblocktemplate support\n\
      --no-stratum      disable X-Stratum support\n\
      --no-redirect     ignore requests to change the URL of the mining server\n\
  -q, --quiet           disable per-thread hashmeter output\n\
  -D, --debug           enable debug output\n\
      --debug-pow       log header hash and OpuHash digest for each found nonce (vs rigord -debugpowcompare)\n\
  -P, --protocol-dump   verbose dump of protocol-level activities\n"
#ifdef HAVE_SYSLOG_H
"\
  -S, --syslog          use system log for output messages\n"
#endif
#ifndef WIN32
"\
  -B, --background      run the miner in the background\n"
#endif
"\
      --benchmark       run in offline benchmark mode\n\
  -c, --config=FILE     load a JSON-format configuration file\n\
  -V, --version         display version information and exit\n\
  -h, --help            display this help text and exit\n\
";

static char const short_options[] =
#ifndef WIN32
	"B"
#endif
#ifdef HAVE_SYSLOG_H
	"S"
#endif
	"a:c:Dhp:Px:qr:R:s:t:T:o:u:O:V";

static struct option const options[] = {
	{ "algo", 1, NULL, 'a' },
#ifndef WIN32
	{ "background", 0, NULL, 'B' },
#endif
	{ "benchmark", 0, NULL, 1005 },
	{ "cert", 1, NULL, 1001 },
	{ "coinbase-addr", 1, NULL, 1013 },
	{ "coinbase-sig", 1, NULL, 1015 },
	{ "config", 1, NULL, 'c' },
	{ "debug", 0, NULL, 'D' },
	{ "debug-pow", 0, NULL, 1016 },
	{ "help", 0, NULL, 'h' },
	{ "no-gbt", 0, NULL, 1011 },
	{ "no-getwork", 0, NULL, 1010 },
	{ "no-longpoll", 0, NULL, 1003 },
	{ "no-redirect", 0, NULL, 1009 },
	{ "no-stratum", 0, NULL, 1007 },
	{ "pass", 1, NULL, 'p' },
	{ "protocol-dump", 0, NULL, 'P' },
	{ "proxy", 1, NULL, 'x' },
	{ "quiet", 0, NULL, 'q' },
	{ "retries", 1, NULL, 'r' },
	{ "retry-pause", 1, NULL, 'R' },
	{ "scantime", 1, NULL, 's' },
#ifdef HAVE_SYSLOG_H
	{ "syslog", 0, NULL, 'S' },
#endif
	{ "threads", 1, NULL, 't' },
	{ "timeout", 1, NULL, 'T' },
	{ "url", 1, NULL, 'o' },
	{ "user", 1, NULL, 'u' },
	{ "userpass", 1, NULL, 'O' },
	{ "version", 0, NULL, 'V' },
	{ 0, 0, 0, 0 }
};

struct work {
	uint32_t data[32];
	uint32_t target[8];

	int height;
	char *txs;
	char *workid;

	char *job_id;
	size_t xnonce2_len;
	unsigned char *xnonce2;

	/* Rigor: convergence_txids for computing convergence_root */
	char **convergence_txids;
	int convergence_txid_count;

	/* Rigor: previous block hash for querying nBits */
	char *prevblockhash;
	uint32_t prev_nbits;

	/* PoW: nBits from GBT (used to construct target via SetCompact) */
	uint32_t nbits;
};

static struct work g_work;
static time_t g_work_time;
static pthread_mutex_t g_work_lock;
static bool submit_old = false;
static char *lp_id;

static inline void work_free(struct work *w)
{
	free(w->txs);
	free(w->workid);
	free(w->job_id);
	free(w->xnonce2);
	if (w->convergence_txids) {
		for (int i = 0; i < w->convergence_txid_count; i++) {
			free(w->convergence_txids[i]);
		}
		free(w->convergence_txids);
	}
	free(w->prevblockhash);
}

static inline void work_copy(struct work *dest, const struct work *src)
{
	memcpy(dest, src, sizeof(struct work));
	if (src->txs)
		dest->txs = strdup(src->txs);
	if (src->workid)
		dest->workid = strdup(src->workid);
	if (src->job_id)
		dest->job_id = strdup(src->job_id);
	if (src->prevblockhash)
		dest->prevblockhash = strdup(src->prevblockhash);
	if (src->xnonce2 && src->xnonce2_len) {
		dest->xnonce2 = malloc(src->xnonce2_len);
		memcpy(dest->xnonce2, src->xnonce2, src->xnonce2_len);
	}
	if (src->convergence_txids && src->convergence_txid_count > 0) {
		dest->convergence_txids = calloc(src->convergence_txid_count, sizeof(char*));
		dest->convergence_txid_count = src->convergence_txid_count;
		for (int i = 0; i < src->convergence_txid_count; i++) {
			if (src->convergence_txids[i])
				dest->convergence_txids[i] = strdup(src->convergence_txids[i]);
		}
	}
}

static bool jobj_binary(const json_t *obj, const char *key,
			void *buf, size_t buflen)
{
	const char *hexstr;
	json_t *tmp;

	tmp = json_object_get(obj, key);
	if (unlikely(!tmp)) {
		applog(LOG_ERR, "JSON key '%s' not found", key);
		return false;
	}
	hexstr = json_string_value(tmp);
	if (unlikely(!hexstr)) {
		applog(LOG_ERR, "JSON key '%s' is not a string", key);
		return false;
	}
	if (!hex2bin(buf, hexstr, buflen))
		return false;

	return true;
}

static bool work_decode(const json_t *val, struct work *work)
{
	int i;

	if (unlikely(!jobj_binary(val, "data", work->data, sizeof(work->data)))) {
		applog(LOG_ERR, "JSON invalid data");
		goto err_out;
	}
	if (unlikely(!jobj_binary(val, "target", work->target, sizeof(work->target)))) {
		applog(LOG_ERR, "JSON invalid target");
		goto err_out;
	}

	for (i = 0; i < ARRAY_SIZE(work->data); i++)
		work->data[i] = le32dec(work->data + i);
	for (i = 0; i < ARRAY_SIZE(work->target); i++)
		work->target[i] = le32dec(work->target + i);

	return true;

err_out:
	return false;
}

/* Bitcoin compact-size reader (raw transaction encoding). */
static bool gbt_read_compact_size(const unsigned char *tx, int txlen, int *off, uint64_t *out)
{
	if (*off >= txlen)
		return false;
	const unsigned int ch = tx[(*off)++];
	if (ch < 253) {
		*out = ch;
		return true;
	}
	if (ch == 253) {
		if (*off + 2 > txlen)
			return false;
		*out = (uint64_t)tx[*off] | ((uint64_t)tx[*off + 1] << 8);
		*off += 2;
		return true;
	}
	if (ch == 254) {
		if (*off + 4 > txlen)
			return false;
		*out = (uint64_t)tx[*off]
		     | ((uint64_t)tx[*off + 1] << 8)
		     | ((uint64_t)tx[*off + 2] << 16)
		     | ((uint64_t)tx[*off + 3] << 24);
		*off += 4;
		return true;
	}
	if (*off + 8 > txlen)
		return false;
	*out = (uint64_t)tx[*off]
	     | ((uint64_t)tx[*off + 1] << 8)
	     | ((uint64_t)tx[*off + 2] << 16)
	     | ((uint64_t)tx[*off + 3] << 24)
	     | ((uint64_t)tx[*off + 4] << 32)
	     | ((uint64_t)tx[*off + 5] << 40)
	     | ((uint64_t)tx[*off + 6] << 48)
	     | ((uint64_t)tx[*off + 7] << 56);
	*off += 8;
	return true;
}

/**
 * Return length of vin[0].scriptSig copied into out, or -1 on parse error.
 * Used to mirror the node's template coinbase scriptSig (height + convergence_root).
 */
static int gbt_extract_coinbase_vin0_scriptsig(const unsigned char *tx, int txlen,
						unsigned char *out, int out_max)
{
	int off = 0;
	uint64_t vins, ss_len;

	if (txlen < 10)
		return -1;
	off += 4; /* nVersion */
	/* SegWit extended serialization: 0x00 0x01 marker after version (BIP141). */
	if (off + 2 <= txlen && tx[off] == 0 && tx[off + 1] == 1)
		off += 2;
	if (!gbt_read_compact_size(tx, txlen, &off, &vins))
		return -1;
	if (vins != 1)
		return -1;
	if (off + 32 + 4 > txlen)
		return -1;
	off += 32 + 4; /* prevout */
	if (!gbt_read_compact_size(tx, txlen, &off, &ss_len))
		return -1;
	if (ss_len > (uint64_t)out_max || off > txlen || (uint64_t)(txlen - off) < ss_len)
		return -1;
	memcpy(out, tx + off, (size_t)ss_len);
	return (int)ss_len;
}

/* Rigor: Compare two txid hex strings for sorting (uint256 memcmp order).
 * This implements the sorting order required by Rigor convergence_root calculation.
 * The hex string representation of uint256 matches memcmp order when compared lexicographically.
 */
static int txid_compare(const void *a, const void *b)
{
	const char *txid_a = *(const char **)a;
	const char *txid_b = *(const char **)b;
	unsigned char bin_a[32], bin_b[32];
	if (txid_a && strlen(txid_a) == 64)
		hex2bin(bin_a, txid_a, 32);
	else
		memset(bin_a, 0, 32);
	if (txid_b && strlen(txid_b) == 64)
		hex2bin(bin_b, txid_b, 32);
	else
		memset(bin_b, 0, 32);
	return memcmp(bin_a, bin_b, 32);
}

/* Rigor: Compute convergence_root from txids according to Rigor consensus rules.
 *
 * Algorithm (from consensus/convergence_root.cpp):
 * 1. If fewer than 2 txids, convergence_root is all-zero (32 bytes)
 * 2. Sort txids in ascending order (uint256 memcmp order)
 * 3. Concatenate all txids in their raw binary form (32 bytes each)
 * 4. Compute Hash256 (double SHA256) of the concatenated data
 *
 * This function implements the normative convergence_root calculation that miners
 * must perform to match the node's consensus validation.
 *
 * @param txids Array of hex-encoded txid strings (64 chars each)
 * @param count Number of txids in the array
 * @param root_out Output buffer (32 bytes) for the computed convergence_root
 */
static void compute_convergence_root(char **txids, int count, unsigned char *root_out)
{
	memset(root_out, 0, 32); /* Default to all-zero if < 2 txids */

	if (count < 2)
		return;

	/* Sort txids by hex string (which is uint256 memcmp order) */
	qsort(txids, count, sizeof(char*), txid_compare);

	/* Concatenate all txids and compute Hash256 */
	unsigned char *concat = malloc(count * 32);
	for (int i = 0; i < count; i++) {
		if (txids[i] && strlen(txids[i]) == 64) {
			hex2bin(concat + i * 32, txids[i], 32);
		}
	}

	sha256d(root_out, concat, count * 32);
	free(concat);
}

static bool gbt_work_decode(const json_t *val, struct work *work)
{
	int i, n;
	uint32_t version, curtime, bits;
	uint8_t prevhash[32];
	uint32_t target[8];
	int cbtx_size;
	unsigned char *cbtx = NULL;
	unsigned char *tx = NULL;
	int tx_count, tx_size;
	unsigned char txc_vi[9];
	unsigned char (*merkle_tree)[32] = NULL;
	bool coinbase_append = false;
	bool submit_coinbase = false;
	bool segwit = false;
	json_t *tmp, *txa;
	bool rc = false;
	unsigned char conv_script[512];
	int conv_script_len = 0;

	tmp = json_object_get(val, "rules");
	if (tmp && json_is_array(tmp)) {
		n = json_array_size(tmp);
		for (i = 0; i < n; i++) {
			const char *s = json_string_value(json_array_get(tmp, i));
			if (!s)
				continue;
			if (!strcmp(s, "segwit") || !strcmp(s, "!segwit"))
				segwit = true;
		}
	}

	tmp = json_object_get(val, "mutable");
	if (tmp && json_is_array(tmp)) {
		n = json_array_size(tmp);
		for (i = 0; i < n; i++) {
			const char *s = json_string_value(json_array_get(tmp, i));
			if (!s)
				continue;
			if (!strcmp(s, "coinbase/append"))
				coinbase_append = true;
			else if (!strcmp(s, "submit/coinbase"))
				submit_coinbase = true;
		}
	}

	tmp = json_object_get(val, "height");
	if (!tmp || !json_is_integer(tmp)) {
		applog(LOG_ERR, "JSON invalid height");
		goto out;
	}
	work->height = json_integer_value(tmp);

	/* Rigor GBT: CVGE commitment (RPC field name matches getblocktemplate). */
	tmp = json_object_get(val, "default_convergence_commitment");
	if (!tmp || !json_is_string(tmp))
		tmp = json_object_get(val, "default_forge_commitment"); /* legacy alias */
	if (tmp && json_is_string(tmp)) {
		const char *fs = json_string_value(tmp);
		int flen = fs ? (int)strlen(fs) / 2 : 0;
		if (flen > 0 && flen <= (int)sizeof(conv_script) && hex2bin(conv_script, fs, flen))
			conv_script_len = flen;
	}

	/* Rigor GBT: Parse convergence_txids for computing convergence_root */
	tmp = json_object_get(val, "convergence_txids");
	if (tmp && json_is_array(tmp)) {
		int n = json_array_size(tmp);
		work->convergence_txid_count = n;
		applog(LOG_INFO, "DEBUG: Received %d convergence_txids from GBT", n);
		if (n > 0) {
			work->convergence_txids = calloc(n, sizeof(char*));
			for (int i = 0; i < n; i++) {
				json_t *txid_obj = json_array_get(tmp, i);
				if (txid_obj && json_is_string(txid_obj)) {
					const char *txid_hex = json_string_value(txid_obj);
					if (txid_hex && strlen(txid_hex) == 64) {
						work->convergence_txids[i] = strdup(txid_hex);
					}
				}
			}
		} else {
			/* Empty array: set count to 0, txids remains NULL */
			work->convergence_txids = NULL;
			applog(LOG_INFO, "DEBUG: convergence_txids array is empty");
		}
	} else {
		applog(LOG_INFO, "DEBUG: convergence_txids field not found in GBT response");
	}

	/* Rigor GBT: Get previous block hash to query its nBits for RIGOR commitment */
	tmp = json_object_get(val, "previousblockhash");
	if (tmp && json_is_string(tmp)) {
		const char *prevhash_hex = json_string_value(tmp);
		if (prevhash_hex && strlen(prevhash_hex) == 64) {
			work->prevblockhash = strdup(prevhash_hex);
		}
	}

	/* Rigor GBT: parse prev_nbits (compact nBits of pindexPrev). Consensus requires the
	 * RIGOR commitment payload tail equal pindex->pprev->nBits; using the template's
	 * `bits` (next-block target) breaks at retarget boundaries (bad-rigor-target-mismatch). */
	tmp = json_object_get(val, "prev_nbits");
	if (tmp && json_is_string(tmp)) {
		const char *pnb_hex = json_string_value(tmp);
		if (pnb_hex && strlen(pnb_hex) == 8) {
			unsigned int pnb_val = 0;
			sscanf(pnb_hex, "%x", &pnb_val);
			work->prev_nbits = pnb_val;
		} else {
			applog(LOG_ERR, "JSON invalid prev_nbits length");
			goto out;
		}
	} else {
		applog(LOG_ERR, "JSON missing prev_nbits (node must expose pindexPrev->nBits via getblocktemplate)");
		goto out;
	}


	tmp = json_object_get(val, "version");
	if (!tmp || !json_is_integer(tmp)) {
		applog(LOG_ERR, "JSON invalid version");
		goto out;
	}
	version = json_integer_value(tmp);

	if (unlikely(!jobj_binary(val, "previousblockhash", prevhash, sizeof(prevhash)))) {
		applog(LOG_ERR, "JSON invalid previousblockhash");
		goto out;
	}

	/* Debug: print prevhash before reversal */
	if (opt_debug_pow) {
		char prevhash_hex[65];
		bin2hex(prevhash_hex, (unsigned char*)prevhash, 32);
		applog(LOG_INFO, "DEBUG: prevhash from GBT (before reversal): %s", prevhash_hex);
	}

	/* Reverse prevhash bytes for little-endian wire format */
	uint8_t prevhash_reversed[32];
	for (i = 0; i < 32; i++) {
		prevhash_reversed[i] = prevhash[31 - i];
	}
	memcpy(prevhash, prevhash_reversed, 32);

	/* Debug: print prevhash after reversal */
	if (opt_debug_pow) {
		char prevhash_hex[65];
		bin2hex(prevhash_hex, (unsigned char*)prevhash, 32);
		applog(LOG_INFO, "DEBUG: prevhash after reversal: %s", prevhash_hex);
	}

	tmp = json_object_get(val, "curtime");
	if (!tmp || !json_is_integer(tmp)) {
		applog(LOG_ERR, "JSON invalid curtime");
		goto out;
	}
	curtime = json_integer_value(tmp);

	/* Debug: print curtime */
	if (opt_debug_pow) {
		applog(LOG_INFO, "DEBUG: curtime from GBT: %u", curtime);
	}

	/* Rigor: Parse bits as hex string to uint32_t (no byte swap) */
	tmp = json_object_get(val, "bits");
	if (!tmp || !json_is_string(tmp)) {
		applog(LOG_ERR, "JSON invalid bits");
		goto out;
	}
	{
		const char *bits_hex = json_string_value(tmp);
		if (strlen(bits_hex) == 8) {
			unsigned int bits_val;
			sscanf(bits_hex, "%x", &bits_val);
			bits = bits_val;
			work->nbits = bits_val; /* Store nBits for SetCompact */
		} else {
			applog(LOG_ERR, "JSON invalid bits length");
			goto out;
		}
	}

	/* find count and size of transactions */
	txa = json_object_get(val, "transactions");
	if (!txa || !json_is_array(txa)) {
		applog(LOG_ERR, "JSON invalid transactions");
		goto out;
	}
	tx_count = json_array_size(txa);
	tx_size = 0;
	for (i = 0; i < tx_count; i++) {
		const json_t *tx = json_array_get(txa, i);
		const char *tx_hex = json_string_value(json_object_get(tx, "data"));
		if (!tx_hex) {
			applog(LOG_ERR, "JSON invalid transactions");
			goto out;
		}
		tx_size += strlen(tx_hex) / 2;
	}

	/*
	 * build coinbase transaction
	 *
	 * Rigor note:
	 * Prefer local coinbase assembly when a payout address is configured.
	 * This path appends default_convergence_commitment (CVGE) when provided by GBT.
	 * Some templates may include coinbasetxn that does not carry Rigor CVGE
	 * commitment, which would be rejected as bad-convergence-missing.
	 */
	const bool prefer_local_coinbase = (pk_script_size > 0);
	tmp = json_object_get(val, "coinbasetxn");
	if (tmp && !prefer_local_coinbase) {
		const char *cbtx_hex = json_string_value(json_object_get(tmp, "data"));
		cbtx_size = cbtx_hex ? strlen(cbtx_hex) / 2 : 0;
		cbtx = malloc(cbtx_size + 100);
		if (cbtx_size < 60 || !hex2bin(cbtx, cbtx_hex, cbtx_size)) {
			applog(LOG_ERR, "JSON invalid coinbasetxn");
			goto out;
		}
	} else {
		int64_t cbvalue;
		if (!pk_script_size) {
			if (allow_getwork) {
				applog(LOG_INFO, "No payout address provided, switching to getwork");
				have_gbt = false;
			} else
				applog(LOG_ERR, "No payout address provided");
			goto out;
		}
		tmp = json_object_get(val, "coinbasevalue");
		if (!tmp || !json_is_number(tmp)) {
			/*
			 * Fallback: if coinbasevalue is unavailable but a template coinbasetxn exists,
			 * use the template coinbase rather than failing hard.
			 */
			tmp = json_object_get(val, "coinbasetxn");
			if (tmp) {
				const char *cbtx_hex = json_string_value(json_object_get(tmp, "data"));
				cbtx_size = cbtx_hex ? strlen(cbtx_hex) / 2 : 0;
				cbtx = malloc(cbtx_size + 100);
				if (cbtx_size < 60 || !hex2bin(cbtx, cbtx_hex, cbtx_size)) {
					applog(LOG_ERR, "JSON invalid coinbasetxn");
					goto out;
				}
				goto coinbase_built;
			}
			applog(LOG_ERR, "JSON invalid coinbasevalue");
			goto out;
		}
		cbvalue = json_is_integer(tmp) ? json_integer_value(tmp) : json_number_value(tmp);
		cbtx = malloc(8192);
		le32enc((uint32_t *)cbtx, 1); /* version */
		cbtx[4] = 1; /* in-counter */
		memset(cbtx+5, 0x00, 32); /* prev txout hash */
		le32enc((uint32_t *)(cbtx+37), 0xffffffff); /* prev txout index */
		{
			unsigned char scriptsig_tpl[512];
			int scriptsig_tpl_len = -1;
			json_t *txnc = json_object_get(val, "coinbasetxn");
			if (txnc && json_is_object(txnc)) {
				const char *tpl_hex = json_string_value(json_object_get(txnc, "data"));
				const int tpl_len = tpl_hex ? (int)(strlen(tpl_hex) / 2) : 0;
				if (tpl_hex && tpl_len > 60 && tpl_len < 8000) {
					unsigned char *tpl = malloc((size_t)tpl_len);
					if (tpl && hex2bin(tpl, tpl_hex, tpl_len))
						scriptsig_tpl_len = gbt_extract_coinbase_vin0_scriptsig(
							tpl, tpl_len, scriptsig_tpl, (int)sizeof(scriptsig_tpl));
					free(tpl);
				}
			}
			if (scriptsig_tpl_len > 0) {
				cbtx[41] = (unsigned char)scriptsig_tpl_len;
				memcpy(cbtx + 42, scriptsig_tpl, (size_t)scriptsig_tpl_len);
				cbtx_size = 42 + scriptsig_tpl_len;
				/* Rigor: always append convergence_root even when using template */
				unsigned char convergence_root[32];
				compute_convergence_root(work->convergence_txids, work->convergence_txid_count, convergence_root);
				char conv_root_hex[65];
				bin2hex(conv_root_hex, convergence_root, 32);
				applog(LOG_INFO, "DEBUG: Appending convergence_root to coinbase: %s", conv_root_hex);
				cbtx[cbtx_size++] = 0x20; /* OP_PUSHBYTES_32 */
				memcpy(cbtx + cbtx_size, convergence_root, 32);
				cbtx_size += 32;
				cbtx[41] = cbtx_size - 42; /* update scriptsig length */
			} else {
				cbtx_size = 43;
				/* BIP 34: height in coinbase */
				if (work->height >= 1 && work->height <= 16) {
					/* Use OP_1-OP_16 to conform to Bitcoin's implementation. */
					cbtx[42] = work->height + 0x50;
					cbtx[cbtx_size++] = 0x00; /* OP_0; pads to 2 bytes */
				} else {
					for (n = work->height; n; n >>= 8) {
						cbtx[cbtx_size++] = n & 0xff;
						if (n < 0x100 && n >= 0x80)
							cbtx[cbtx_size++] = 0;
					}
					cbtx[42] = cbtx_size - 43;
				}
				/* Rigor: convergence root in scriptSig (computed from convergence_txids). */
				unsigned char convergence_root[32];
				compute_convergence_root(work->convergence_txids, work->convergence_txid_count, convergence_root);
				char conv_root_hex[65];
				bin2hex(conv_root_hex, convergence_root, 32);
				applog(LOG_INFO, "DEBUG: Appending convergence_root to coinbase (local): %s", conv_root_hex);
				cbtx[cbtx_size++] = 0x20; /* OP_PUSHBYTES_32 */
				memcpy(cbtx + cbtx_size, convergence_root, 32);
				cbtx_size += 32;
				cbtx[41] = cbtx_size - 42; /* scriptsig length */
			}
		}
		le32enc((uint32_t *)(cbtx+cbtx_size), 0xffffffff); /* sequence */
		cbtx_size += 4;
		{
			int out_n = 1 + (segwit ? 1 : 0) + (conv_script_len ? 1 : 0) + 1; /* +1 for RIGOR commitment */
			cbtx[cbtx_size++] = (unsigned char)out_n; /* out-counter */
		}
		le32enc((uint32_t *)(cbtx+cbtx_size), (uint32_t)cbvalue); /* value */
		le32enc((uint32_t *)(cbtx+cbtx_size+4), cbvalue >> 32);
		cbtx_size += 8;
		cbtx[cbtx_size++] = pk_script_size; /* txout-script length */
		memcpy(cbtx+cbtx_size, pk_script, pk_script_size);
		cbtx_size += pk_script_size;
		if (segwit) {
			unsigned char (*wtree)[32] = calloc(tx_count + 2, 32);
			memset(cbtx+cbtx_size, 0, 8); /* value */
			cbtx_size += 8;
			cbtx[cbtx_size++] = 38; /* txout-script length */
			cbtx[cbtx_size++] = 0x6a; /* txout-script */
			cbtx[cbtx_size++] = 0x24;
			cbtx[cbtx_size++] = 0xaa;
			cbtx[cbtx_size++] = 0x21;
			cbtx[cbtx_size++] = 0xa9;
			cbtx[cbtx_size++] = 0xed;
			for (i = 0; i < tx_count; i++) {
				const json_t *tx = json_array_get(txa, i);
				const json_t *hash = json_object_get(tx, "hash");
				if (!hash || !hex2bin(wtree[1+i], json_string_value(hash), 32)) {
					applog(LOG_ERR, "JSON invalid transaction hash");
					free(wtree);
					goto out;
				}
				memrev(wtree[1+i], 32);
			}
			n = tx_count + 1;
			while (n > 1) {
				if (n % 2)
					memcpy(wtree[n], wtree[n-1], 32);
				n = (n + 1) / 2;
				for (i = 0; i < n; i++)
					sha256d(wtree[i], wtree[2*i], 64);
			}
			memset(wtree[1], 0, 32);  /* witness reserved value = 0 */
			sha256d(cbtx+cbtx_size, wtree[0], 64);
			cbtx_size += 32;
			free(wtree);
		}
		if (conv_script_len > 0) {
			memset(cbtx + cbtx_size, 0, 8); /* value */
			cbtx_size += 8;
			cbtx[cbtx_size++] = (unsigned char)conv_script_len;
			memcpy(cbtx + cbtx_size, conv_script, conv_script_len);
			cbtx_size += conv_script_len;
		}
			/* Rigor: Add RIGOR draw-target commitment (OP_RETURN + RIGR tag + prev_nbits LE32).
			 * Consensus: core/src/script/standard.cpp IsRigorCommitment requires payload[0..4] == "RIGR"
			 *            (RigorTags::RIGOR) and payload[4..8] == LE32(pindex->pprev->nBits)
			 *            (validation.cpp CheckRigorDrawTargetCommitment). */
		{
			const uint32_t prev_nbits = work->prev_nbits;
			memset(cbtx + cbtx_size, 0, 8); /* value */
			cbtx_size += 8;
			cbtx[cbtx_size++] = 10; /* script length: OP_RETURN (1) + push opcode (1) + payload (8) */
			cbtx[cbtx_size++] = 0x6a; /* OP_RETURN */
			cbtx[cbtx_size++] = 0x08; /* OP_PUSHBYTES_8 */
			/* RIGOR tag: must match RigorTags::RIGOR = {'R','I','G','R'} byte-for-byte. */
			cbtx[cbtx_size++] = 'R';
			cbtx[cbtx_size++] = 'I';
			cbtx[cbtx_size++] = 'G';
			cbtx[cbtx_size++] = 'R';
			/* prev_nbits in little-endian (pindexPrev->nBits, NOT the template's next-block bits). */
			cbtx[cbtx_size++] = prev_nbits & 0xff;
			cbtx[cbtx_size++] = (prev_nbits >> 8) & 0xff;
			cbtx[cbtx_size++] = (prev_nbits >> 16) & 0xff;
			cbtx[cbtx_size++] = (prev_nbits >> 24) & 0xff;
			applog(LOG_INFO, "Rigor commitment: tag=RIGR prev_nbits=0x%08x (template bits=0x%08x)", prev_nbits, bits);
		}
		le32enc((uint32_t *)(cbtx+cbtx_size), 0); /* lock time */
		cbtx_size += 4;
		coinbase_append = true;
	}
coinbase_built:
	if (coinbase_append) {
		unsigned char xsig[100];
		int xsig_len = 0;
		if (*coinbase_sig) {
			n = strlen(coinbase_sig);
			if (cbtx[41] + xsig_len + n <= 100) {
				memcpy(xsig+xsig_len, coinbase_sig, n);
				xsig_len += n;
			} else {
				applog(LOG_WARNING, "Signature does not fit in coinbase, skipping");
			}
		}
		tmp = json_object_get(val, "coinbaseaux");
		if (tmp && json_is_object(tmp)) {
			void *iter = json_object_iter(tmp);
			while (iter) {
				unsigned char buf[100];
				const char *s = json_string_value(json_object_iter_value(iter));
				n = s ? strlen(s) / 2 : 0;
				if (!s || n > 100 || !hex2bin(buf, s, n)) {
					applog(LOG_ERR, "JSON invalid coinbaseaux");
					break;
				}
				if (cbtx[41] + xsig_len + n <= 100) {
					memcpy(xsig+xsig_len, buf, n);
					xsig_len += n;
				}
				iter = json_object_iter_next(tmp, iter);
			}
		}
		if (xsig_len) {
			unsigned char *ssig_end = cbtx + 42 + cbtx[41];
			int push_len = cbtx[41] + xsig_len < 76 ? 1 :
			               cbtx[41] + 2 + xsig_len > 100 ? 0 : 2;
			n = xsig_len + push_len;
			memmove(ssig_end + n, ssig_end, cbtx_size - 42 - cbtx[41]);
			cbtx[41] += n;
			if (push_len == 2)
				*(ssig_end++) = 0x4c; /* OP_PUSHDATA1 */
			if (push_len)
				*(ssig_end++) = xsig_len;
			memcpy(ssig_end, xsig, xsig_len);
			cbtx_size += n;
		}
	}

	n = varint_encode(txc_vi, 1 + tx_count);
	work->txs = malloc(2 * (n + cbtx_size + tx_size) + 1);
	bin2hex(work->txs, txc_vi, n);
	bin2hex(work->txs + 2*n, cbtx, cbtx_size);
	char *txs_end = work->txs + strlen(work->txs);

	/* generate merkle root */
	merkle_tree = malloc(32 * ((1 + tx_count + 1) & ~1));
	size_t tx_buf_size = 32 * 1024;
	tx = malloc(tx_buf_size);
	sha256d(merkle_tree[0], cbtx, cbtx_size);
	for (i = 0; i < tx_count; i++) {
		tmp = json_array_get(txa, i);
		const char *tx_hex = json_string_value(json_object_get(tmp, "data"));
		const size_t tx_hex_len = tx_hex ? strlen(tx_hex) : 0;
		const int tx_size = tx_hex_len / 2;
		if (segwit) {
			const char *txid = json_string_value(json_object_get(tmp, "txid"));
			if (!txid || !hex2bin(merkle_tree[1 + i], txid, 32)) {
				applog(LOG_ERR, "JSON invalid transaction txid");
				goto out;
			}
			memrev(merkle_tree[1 + i], 32);
		} else {
			if (tx_size > tx_buf_size) {
				free(tx);
				tx_buf_size = tx_size * 2;
				tx = malloc(tx_buf_size);
			}
			if (!tx_hex || !hex2bin(tx, tx_hex, tx_size)) {
				applog(LOG_ERR, "JSON invalid transactions");
				goto out;
			}
			sha256d(merkle_tree[1 + i], tx, tx_size);
		}
		if (!submit_coinbase) {
			strcpy(txs_end, tx_hex);
			txs_end += tx_hex_len;
		}
	}
	free(tx); tx = NULL;
	n = 1 + tx_count;
	while (n > 1) {
		if (n % 2) {
			memcpy(merkle_tree[n], merkle_tree[n-1], 32);
			++n;
		}
		n /= 2;
		for (i = 0; i < n; i++)
			sha256d(merkle_tree[i], merkle_tree[2*i], 64);
	}

	/* assemble block header */
	work->data[0] = swab32(version);
	/* prevhash is already in little-endian wire format after reversal - copy directly */
	memcpy(work->data + 1, prevhash, 32);

	/* Debug: print prevhash in work->data */
	if (opt_debug_pow) {
		char hexbuf[65];
		bin2hex(hexbuf, (unsigned char*)(work->data + 1), 32);
		applog(LOG_INFO, "DEBUG: prevhash in work->data: %s", hexbuf);
	}

	for (i = 0; i < 8; i++)
		work->data[9 + i] = le32dec((uint32_t *)merkle_tree[0] + i);
	work->data[17] = curtime;  /* curtime from GBT is already correct, no byte swap needed */
	work->data[18] = le32dec(&bits);
	memset(work->data + 19, 0x00, 52);
	work->data[20] = 0x80000000;
	work->data[31] = 0x00000280;

	/* === MINING DEBUG === */
	applog(LOG_INFO, "=== MINING DEBUG ===");
	applog(LOG_INFO, "Header.nBits = 0x%08x", work->data[18]);
	applog(LOG_INFO, "GBT bits = 0x%08x", bits);
	{
		char hexbuf[65];
		bin2hex(hexbuf, (unsigned char*)merkle_tree[0], 32);
		applog(LOG_INFO, "MerkleRoot = %s", hexbuf);
	}
	/* Debug: Print serialized 80-byte block header in hex */
	{
		unsigned char wire[80];
		int i;
		for (i = 0; i < 20; i++)
			le32enc(wire + 4 * i, work->data[i]);
		char header_hex[161];
		bin2hex(header_hex, wire, 80);
		applog(LOG_INFO, "Serialized 80-byte header: %s", header_hex);
	}

	/* === RIGOR MINER DEBUG === */
	if (opt_debug_pow) {
		unsigned char wire[80];
		char header_hex[161];
		char hash_hex[65];
		char target_hex[65];
		char merkle_hex[65];
		unsigned char hash[32];
		int i;

		/* Serialize header to wire format */
		for (i = 0; i < 20; i++)
			le32enc(wire + 4 * i, work->data[i]);
		bin2hex(header_hex, wire, 80);

		/* Compute hash */
		opus_hash(wire, hash);
		bin2hex(hash_hex, hash, 32);

		/* Target from SetCompact */
		bin2hex(target_hex, (unsigned char*)work->target, 32);

		/* Merkle root */
		bin2hex(merkle_hex, (unsigned char*)merkle_tree[0], 32);

		/* Coinbase TXID */
		unsigned char coinbase_txid[32];
		sha256d(coinbase_txid, cbtx, cbtx_size);
		char coinbase_hex[65];
		bin2hex(coinbase_hex, coinbase_txid, 32);

		/* Convergence root */
		unsigned char conv_root[32];
		compute_convergence_root(work->convergence_txids, work->convergence_txid_count, conv_root);
		char conv_root_hex[65];
		bin2hex(conv_root_hex, conv_root, 32);

		applog(LOG_INFO, "\n==== RIGOR MINER DEBUG ====");
		applog(LOG_INFO, "HEADER: %s", header_hex);
		applog(LOG_INFO, "HASH:   %s", hash_hex);
		applog(LOG_INFO, "NBITS:  %08x", work->nbits);
		applog(LOG_INFO, "TARGET: %s", target_hex);
		applog(LOG_INFO, "COINBASE TXID: %s", coinbase_hex);
		applog(LOG_INFO, "MERKLE ROOT: %s", merkle_hex);
		applog(LOG_INFO, "CONVERGENCE ROOT: %s", conv_root_hex);
		applog(LOG_INFO, "==== END RIGOR MINER DEBUG ====\n");
	}
	/* === END RIGOR MINER DEBUG === */

	/* PoW: Construct target from nBits using SetCompact (Bitcoin-compatible) */
	{
		bool fNegative, fOverflow;
		arith_uint256 arith_target;
		arith_uint256_set_compact(&arith_target, work->nbits, &fNegative, &fOverflow);
		if (fNegative || fOverflow) {
			applog(LOG_ERR, "Invalid nBits: 0x%08x (negative=%d, overflow=%d)",
			       work->nbits, fNegative, fOverflow);
			goto out;
		}
		/* Copy arith_uint256 to work->target (uint32_t[8]) */
		for (i = 0; i < 8; i++)
			work->target[i] = arith_target.data[i];
		/* Debug: Print target from SetCompact with raw array values */
		char target_hex[65];
		arith_uint256_to_hex(&arith_target, target_hex);
		applog(LOG_INFO, "Target (from SetCompact): %s", target_hex);
		applog(LOG_INFO, "Target raw array: [%08x %08x %08x %08x %08x %08x %08x %08x]",
		       work->target[0], work->target[1], work->target[2], work->target[3],
		       work->target[4], work->target[5], work->target[6], work->target[7]);
	}

	tmp = json_object_get(val, "workid");
	if (tmp) {
		if (!json_is_string(tmp)) {
			applog(LOG_ERR, "JSON invalid workid");
			goto out;
		}
		work->workid = strdup(json_string_value(tmp));
	}

	/* Long polling */
	tmp = json_object_get(val, "longpollid");
	if (want_longpoll && json_is_string(tmp)) {
		free(lp_id);
		lp_id = strdup(json_string_value(tmp));
		if (!have_longpoll) {
			char *lp_uri;
			tmp = json_object_get(val, "longpolluri");
			lp_uri = strdup(json_is_string(tmp) ? json_string_value(tmp) : rpc_url);
			have_longpoll = true;
			tq_push(thr_info[longpoll_thr_id].q, lp_uri);
		}
	}

	rc = true;

out:
	free(tx);
	free(cbtx);
	free(merkle_tree);
	return rc;
}

static void share_result(int result, const char *reason)
{
	char s[345];
	double hashrate;
	int i;

	hashrate = 0.;
	pthread_mutex_lock(&stats_lock);
	for (i = 0; i < opt_n_threads; i++)
		hashrate += thr_hashrates[i];
	result ? accepted_count++ : rejected_count++;
	pthread_mutex_unlock(&stats_lock);
	
	sprintf(s, hashrate >= 1e6 ? "%.0f" : "%.2f", 1e-3 * hashrate);
	applog(LOG_INFO, "accepted: %lu/%lu (%.2f%%), %s khash/s %s",
		   accepted_count,
		   accepted_count + rejected_count,
		   100. * accepted_count / (accepted_count + rejected_count),
		   s,
		   result ? "(yay!!!)" : "(booooo)");

	if (opt_debug && reason)
		applog(LOG_DEBUG, "DEBUG: reject reason: %s", reason);
}

static bool submit_upstream_work(CURL *curl, struct work *work)
{
	json_t *val, *res, *reason;
	char data_str[2 * sizeof(work->data) + 1];
	char target_str[65];
	uint32_t target_be[8];
	char s[345];
	int i;
	bool rc = false;

	/* pass if the previous hash is not the current previous hash */
	if (!submit_old && memcmp(work->data + 1, g_work.data + 1, 32)) {
		if (opt_debug)
			applog(LOG_DEBUG, "DEBUG: stale work detected, discarding");
		return true;
	}

	if (have_stratum) {
		uint32_t ntime, nonce;
		char ntimestr[9], noncestr[9], *xnonce2str, *req;

		le32enc(&ntime, work->data[17]);
		le32enc(&nonce, work->data[19]);
		bin2hex(ntimestr, (const unsigned char *)(&ntime), 4);
		bin2hex(noncestr, (const unsigned char *)(&nonce), 4);
		xnonce2str = abin2hex(work->xnonce2, work->xnonce2_len);
		req = malloc(256 + strlen(rpc_user) + strlen(work->job_id) + 2 * work->xnonce2_len);
		sprintf(req,
			"{\"method\": \"mining.submit\", \"params\": [\"%s\", \"%s\", \"%s\", \"%s\", \"%s\"], \"id\":4}",
			rpc_user, work->job_id, xnonce2str, ntimestr, noncestr);
		free(xnonce2str);

		rc = stratum_send_line(&stratum, req);
		free(req);
		if (unlikely(!rc)) {
			applog(LOG_ERR, "submit_upstream_work stratum_send_line failed");
			goto out;
		}
	} else if (work->txs) {
		char *req;
		uint32_t data_wire[20];

		/* Encode header to wire format without modifying work->data */
		for (i = 0; i < ARRAY_SIZE(work->data); i++)
			le32enc(data_wire + i, work->data[i]);
		bin2hex(data_str, (unsigned char *)data_wire, 80);
		for (i = 0; i < 8; i++)
			le32enc(target_be + i, work->target[7 - i]);
		bin2hex(target_str, (unsigned char *)target_be, 32);
		applog(LOG_INFO, "DEBUG: submitblock header=%s target=%s",
		       data_str, target_str);
		if (work->workid) {
			char *params;
			val = json_object();
			json_object_set_new(val, "workid", json_string(work->workid));
			params = json_dumps(val, 0);
			json_decref(val);
			req = malloc(128 + 2*80 + strlen(work->txs) + strlen(params));
			sprintf(req,
				"{\"method\": \"submitblock\", \"params\": [\"%s%s\", %s], \"id\":1}\r\n",
				data_str, work->txs, params);
			free(params);
		} else {
			req = malloc(128 + 2*80 + strlen(work->txs));
			sprintf(req,
				"{\"method\": \"submitblock\", \"params\": [\"%s%s\"], \"id\":1}\r\n",
				data_str, work->txs);
		}
		val = json_rpc_call(curl, rpc_url, rpc_userpass, req, NULL, 0);
		free(req);
		if (unlikely(!val)) {
			applog(LOG_ERR, "submit_upstream_work json_rpc_call failed");
			goto out;
		}

		/* Debug: Log RPC response */
		char *res_str = json_dumps(val, 0);
		applog(LOG_INFO, "RPC response: %s", res_str);
		free(res_str);

		res = json_object_get(val, "result");
		if (json_is_object(res)) {
			char *res_str;
			bool sumres = false;
			void *iter = json_object_iter(res);
			while (iter) {
				if (json_is_null(json_object_iter_value(iter))) {
					sumres = true;
					break;
				}
				iter = json_object_iter_next(res, iter);
			}
			res_str = json_dumps(res, 0);
			share_result(sumres, res_str);
			free(res_str);
		} else
			share_result(json_is_null(res), json_string_value(res));

		json_decref(val);
	} else {
		/* build hex string */
		uint32_t data_wire[20];
		for (i = 0; i < ARRAY_SIZE(work->data); i++)
			le32enc(data_wire + i, work->data[i]);
		bin2hex(data_str, (unsigned char *)data_wire, sizeof(data_wire));

		/* build JSON-RPC request */
		sprintf(s,
			"{\"method\": \"getwork\", \"params\": [ \"%s\" ], \"id\":1}\r\n",
			data_str);

		/* issue JSON-RPC request */
		val = json_rpc_call(curl, rpc_url, rpc_userpass, s, NULL, 0);
		if (unlikely(!val)) {
			applog(LOG_ERR, "submit_upstream_work json_rpc_call failed");
			goto out;
		}

		res = json_object_get(val, "result");
		reason = json_object_get(val, "reject-reason");
		share_result(json_is_true(res), reason ? json_string_value(reason) : NULL);

		json_decref(val);
	}

	rc = true;

out:
	return rc;
}

static const char *getwork_req =
	"{\"method\": \"getwork\", \"params\": [], \"id\":0}\r\n";

#define GBT_CAPABILITIES "[\"coinbasetxn\", \"coinbasevalue\", \"longpoll\", \"workid\"]"
#define GBT_RULES "[\"segwit\"]"

static const char *gbt_req =
	"{\"method\": \"getblocktemplate\", \"params\": [{\"capabilities\": "
	GBT_CAPABILITIES ", \"rules\": " GBT_RULES "}], \"id\":0}\r\n";
static const char *gbt_lp_req =
	"{\"method\": \"getblocktemplate\", \"params\": [{\"capabilities\": "
	GBT_CAPABILITIES ", \"rules\": " GBT_RULES ", \"longpollid\": \"%s\"}], \"id\":0}\r\n";

static bool get_upstream_work(CURL *curl, struct work *work)
{
	json_t *val;
	int err;
	bool rc;
	struct timeval tv_start, tv_end, diff;

start:
	gettimeofday(&tv_start, NULL);
	val = json_rpc_call(curl, rpc_url, rpc_userpass,
			    have_gbt ? gbt_req : getwork_req,
			    &err, have_gbt ? JSON_RPC_QUIET_404 : 0);
	gettimeofday(&tv_end, NULL);

	if (have_stratum) {
		if (val)
			json_decref(val);
		return true;
	}

	if (!have_gbt && !allow_getwork) {
		applog(LOG_ERR, "No usable protocol");
		if (val)
			json_decref(val);
		return false;
	}

	if (have_gbt && allow_getwork && !val && err == CURLE_OK) {
		applog(LOG_INFO, "getblocktemplate failed, falling back to getwork");
		have_gbt = false;
		goto start;
	}

	if (!val)
		return false;

	if (have_gbt) {
		rc = gbt_work_decode(json_object_get(val, "result"), work);
		if (!have_gbt) {
			json_decref(val);
			goto start;
		}
	} else
		rc = work_decode(json_object_get(val, "result"), work);

	if (opt_debug && rc) {
		timeval_subtract(&diff, &tv_end, &tv_start);
		applog(LOG_DEBUG, "DEBUG: got new work in %d ms",
		       diff.tv_sec * 1000 + diff.tv_usec / 1000);
	}

	json_decref(val);

	return rc;
}

static void workio_cmd_free(struct workio_cmd *wc)
{
	if (!wc)
		return;

	switch (wc->cmd) {
	case WC_SUBMIT_WORK:
		work_free(wc->u.work);
		free(wc->u.work);
		break;
	default: /* do nothing */
		break;
	}

	memset(wc, 0, sizeof(*wc));	/* poison */
	free(wc);
}

static bool workio_get_work(struct workio_cmd *wc, CURL *curl)
{
	struct work *ret_work;
	int failures = 0;

	ret_work = calloc(1, sizeof(*ret_work));
	if (!ret_work)
		return false;

	/* obtain new work from bitcoin via JSON-RPC */
	while (!get_upstream_work(curl, ret_work)) {
		if (unlikely((opt_retries >= 0) && (++failures > opt_retries))) {
			applog(LOG_ERR, "json_rpc_call failed, terminating workio thread");
			free(ret_work);
			return false;
		}

		/* pause, then restart work-request loop */
		applog(LOG_ERR, "json_rpc_call failed, retry after %d seconds",
			opt_fail_pause);
		sleep(opt_fail_pause);
	}

	/* send work to requesting thread */
	if (!tq_push(wc->thr->q, ret_work))
		free(ret_work);

	return true;
}

static bool workio_submit_work(struct workio_cmd *wc, CURL *curl)
{
	int failures = 0;

	/* submit solution to bitcoin via JSON-RPC */
	while (!submit_upstream_work(curl, wc->u.work)) {
		if (unlikely((opt_retries >= 0) && (++failures > opt_retries))) {
			applog(LOG_ERR, "...terminating workio thread");
			return false;
		}

		/* pause, then restart work-request loop */
		applog(LOG_ERR, "...retry after %d seconds",
			opt_fail_pause);
		sleep(opt_fail_pause);
	}

	return true;
}

static void *workio_thread(void *userdata)
{
	struct thr_info *mythr = userdata;
	CURL *curl;
	bool ok = true;

	curl = curl_easy_init();
	if (unlikely(!curl)) {
		applog(LOG_ERR, "CURL initialization failed");
		return NULL;
	}

	while (ok) {
		struct workio_cmd *wc;

		/* wait for workio_cmd sent to us, on our queue */
		wc = tq_pop(mythr->q, NULL);
		if (!wc) {
			ok = false;
			break;
		}

		/* process workio_cmd */
		switch (wc->cmd) {
		case WC_GET_WORK:
			ok = workio_get_work(wc, curl);
			break;
		case WC_SUBMIT_WORK:
			ok = workio_submit_work(wc, curl);
			break;

		default:		/* should never happen */
			ok = false;
			break;
		}

		workio_cmd_free(wc);
	}

	tq_freeze(mythr->q);
	curl_easy_cleanup(curl);

	return NULL;
}

static bool get_work(struct thr_info *thr, struct work *work)
{
	struct workio_cmd *wc;
	struct work *work_heap;

	if (opt_benchmark) {
		memset(work->data, 0x55, 76);
		work->data[17] = swab32(time(NULL));
		memset(work->data + 19, 0x00, 52);
		work->data[20] = 0x80000000;
		work->data[31] = 0x00000280;
		memset(work->target, 0x00, sizeof(work->target));
		return true;
	}

	/* fill out work request message */
	wc = calloc(1, sizeof(*wc));
	if (!wc)
		return false;

	wc->cmd = WC_GET_WORK;
	wc->thr = thr;

	/* send work request to workio thread */
	if (!tq_push(thr_info[work_thr_id].q, wc)) {
		workio_cmd_free(wc);
		return false;
	}

	/* wait for response, a unit of work */
	work_heap = tq_pop(thr->q, NULL);
	if (!work_heap)
		return false;

	/* copy returned work into storage provided by caller */
	memcpy(work, work_heap, sizeof(*work));
	free(work_heap);

	return true;
}

static bool submit_work(struct thr_info *thr, const struct work *work_in)
{
	struct workio_cmd *wc;
	
	/* fill out work request message */
	wc = calloc(1, sizeof(*wc));
	if (!wc)
		return false;

	wc->u.work = malloc(sizeof(*work_in));
	if (!wc->u.work)
		goto err_out;

	wc->cmd = WC_SUBMIT_WORK;
	wc->thr = thr;
	work_copy(wc->u.work, work_in);

	/* send solution to workio thread */
	if (!tq_push(thr_info[work_thr_id].q, wc))
		goto err_out;

	return true;

err_out:
	workio_cmd_free(wc);
	return false;
}

static void stratum_gen_work(struct stratum_ctx *sctx, struct work *work)
{
	unsigned char merkle_root[64];
	int i;

	pthread_mutex_lock(&sctx->work_lock);

	free(work->job_id);
	work->job_id = strdup(sctx->job.job_id);
	work->xnonce2_len = sctx->xnonce2_size;
	work->xnonce2 = realloc(work->xnonce2, sctx->xnonce2_size);
	memcpy(work->xnonce2, sctx->job.xnonce2, sctx->xnonce2_size);

	/* Generate merkle root */
	sha256d(merkle_root, sctx->job.coinbase, sctx->job.coinbase_size);
	for (i = 0; i < sctx->job.merkle_count; i++) {
		memcpy(merkle_root + 32, sctx->job.merkle[i], 32);
		sha256d(merkle_root, merkle_root, 64);
	}
	
	/* Increment extranonce2 */
	for (i = 0; i < sctx->xnonce2_size && !++sctx->job.xnonce2[i]; i++);

	/* Assemble block header */
	memset(work->data, 0, 128);
	work->data[0] = le32dec(sctx->job.version);
	for (i = 0; i < 8; i++)
		work->data[1 + i] = le32dec((uint32_t *)sctx->job.prevhash + i);
	for (i = 0; i < 8; i++)
		work->data[9 + i] = le32dec((uint32_t *)merkle_root + i);
	work->data[17] = le32dec(sctx->job.ntime);
	work->data[18] = le32dec(sctx->job.nbits);
	work->data[20] = 0x80000000;
	work->data[31] = 0x00000280;

	pthread_mutex_unlock(&sctx->work_lock);

	if (opt_debug) {
		char *xnonce2str = abin2hex(work->xnonce2, work->xnonce2_len);
		applog(LOG_DEBUG, "DEBUG: job_id='%s' extranonce2=%s ntime=%08x",
		       work->job_id, xnonce2str, swab32(work->data[17]));
		free(xnonce2str);
	}

	if (opt_algo == ALGO_SCRYPT)
		diff_to_target(work->target, sctx->job.diff / 65536.0);
	else
		diff_to_target(work->target, sctx->job.diff);
}

static void *miner_thread(void *userdata)
{
	struct thr_info *mythr = userdata;
	int thr_id = mythr->id;
	struct work work = {{0}};
	uint32_t max_nonce;
	uint32_t end_nonce = 0xffffffffU / opt_n_threads * (thr_id + 1) - 0x20;
	unsigned char *scratchbuf = NULL;
	char s[16];
	int i;

	/* Set worker threads to nice 19 and then preferentially to SCHED_IDLE
	 * and if that fails, then SCHED_BATCH. No need for this to be an
	 * error if it fails */
	if (!opt_benchmark) {
		setpriority(PRIO_PROCESS, 0, 19);
		drop_policy();
	}

	/* Cpu affinity only makes sense if the number of threads is a multiple
	 * of the number of CPUs */
	if (num_processors > 1 && opt_n_threads % num_processors == 0) {
		if (!opt_quiet)
			applog(LOG_INFO, "Binding thread %d to cpu %d",
			       thr_id, thr_id % num_processors);
		affine_to_cpu(thr_id, thr_id % num_processors);
	}
	
	if (opt_algo == ALGO_SCRYPT) {
		scratchbuf = scrypt_buffer_alloc(opt_scrypt_n);
		if (!scratchbuf) {
			applog(LOG_ERR, "scrypt buffer allocation failed");
			pthread_mutex_lock(&applog_lock);
			exit(1);
		}
	}

	while (1) {
		unsigned long hashes_done;
		struct timeval tv_start, tv_end, diff;
		int64_t max64;
		int rc;

		if (have_stratum) {
			while (time(NULL) >= g_work_time + 120)
				sleep(1);
			pthread_mutex_lock(&g_work_lock);
			if (work.data[19] >= end_nonce && !memcmp(work.data, g_work.data, 76))
				stratum_gen_work(&stratum, &g_work);
		} else {
			int min_scantime = have_longpoll ? LP_SCANTIME : opt_scantime;
			/* obtain new work from internal workio thread */
			pthread_mutex_lock(&g_work_lock);
			if (!have_stratum &&
			    (time(NULL) - g_work_time >= min_scantime ||
			     work.data[19] >= end_nonce)) {
				work_free(&g_work);
				if (unlikely(!get_work(mythr, &g_work))) {
					applog(LOG_ERR, "work retrieval failed, exiting "
						"mining thread %d", mythr->id);
					pthread_mutex_unlock(&g_work_lock);
					goto out;
				}
				g_work_time = have_stratum ? 0 : time(NULL);
			}
			if (have_stratum) {
				pthread_mutex_unlock(&g_work_lock);
				continue;
			}
		}
		if (memcmp(work.data, g_work.data, 76)) {
			work_free(&work);
			work_copy(&work, &g_work);
			work.data[19] = 0xffffffffU / opt_n_threads * thr_id;
		} else
			work.data[19]++;
		pthread_mutex_unlock(&g_work_lock);
		work_restart[thr_id].restart = 0;
		
		/* adjust max_nonce to meet target scan time */
		if (have_stratum)
			max64 = LP_SCANTIME;
		else
			max64 = g_work_time + (have_longpoll ? LP_SCANTIME : opt_scantime)
			      - time(NULL);
		max64 *= thr_hashrates[thr_id];
		if (max64 <= 0) {
			switch (opt_algo) {
			case ALGO_SCRYPT:
				max64 = opt_scrypt_n < 16 ? 0x3ffff : 0x3fffff / opt_scrypt_n;
				break;
			case ALGO_SHA256D:
				max64 = 0x1fffff;
				break;
			}
		}
		if (work.data[19] + max64 > end_nonce)
			max_nonce = end_nonce;
		else
			max_nonce = work.data[19] + max64;
		
		hashes_done = 0;
		gettimeofday(&tv_start, NULL);

		/* scan nonces for a proof-of-work hash */
		switch (opt_algo) {
		case ALGO_SCRYPT:
			rc = scanhash_scrypt(thr_id, work.data, scratchbuf, work.target,
			                     max_nonce, &hashes_done, opt_scrypt_n);
			break;

		case ALGO_SHA256D:
			rc = scanhash_sha256d(thr_id, work.data, work.target,
			                      max_nonce, &hashes_done);
			break;

		default:
			/* should never happen */
			goto out;
		}

		/* record scanhash elapsed time */
		gettimeofday(&tv_end, NULL);
		timeval_subtract(&diff, &tv_end, &tv_start);
		if (diff.tv_usec || diff.tv_sec) {
			pthread_mutex_lock(&stats_lock);
			thr_hashrates[thr_id] =
				hashes_done / (diff.tv_sec + 1e-6 * diff.tv_usec);
			pthread_mutex_unlock(&stats_lock);
		}
		if (!opt_quiet) {
			sprintf(s, thr_hashrates[thr_id] >= 1e6 ? "%.0f" : "%.2f",
				1e-3 * thr_hashrates[thr_id]);
			applog(LOG_INFO, "thread %d: %lu hashes, %s khash/s",
				thr_id, hashes_done, s);
		}
		if (opt_benchmark && thr_id == opt_n_threads - 1) {
			double hashrate = 0.;
			for (i = 0; i < opt_n_threads && thr_hashrates[i]; i++)
				hashrate += thr_hashrates[i];
			if (i == opt_n_threads) {
				sprintf(s, hashrate >= 1e6 ? "%.0f" : "%.2f", 1e-3 * hashrate);
				applog(LOG_INFO, "Total: %s khash/s", s);
			}
		}

		/* if nonce found, submit work */
		if (rc && !opt_benchmark && !submit_work(mythr, &work))
			break;
	}

out:
	tq_freeze(mythr->q);

	return NULL;
}

static void restart_threads(void)
{
	int i;

	for (i = 0; i < opt_n_threads; i++)
		work_restart[i].restart = 1;
}

static void *longpoll_thread(void *userdata)
{
	struct thr_info *mythr = userdata;
	CURL *curl = NULL;
	char *copy_start, *hdr_path = NULL, *lp_url = NULL;
	bool need_slash = false;

	curl = curl_easy_init();
	if (unlikely(!curl)) {
		applog(LOG_ERR, "CURL initialization failed");
		goto out;
	}

start:
	hdr_path = tq_pop(mythr->q, NULL);
	if (!hdr_path)
		goto out;

	/* full URL */
	if (strstr(hdr_path, "://")) {
		lp_url = hdr_path;
		hdr_path = NULL;
	}
	
	/* absolute path, on current server */
	else {
		copy_start = (*hdr_path == '/') ? (hdr_path + 1) : hdr_path;
		if (rpc_url[strlen(rpc_url) - 1] != '/')
			need_slash = true;

		lp_url = malloc(strlen(rpc_url) + strlen(copy_start) + 2);
		if (!lp_url)
			goto out;

		sprintf(lp_url, "%s%s%s", rpc_url, need_slash ? "/" : "", copy_start);
	}

	applog(LOG_INFO, "Long-polling activated for %s", lp_url);

	while (1) {
		json_t *val, *res, *soval;
		char *req = NULL;
		int err;

		if (have_gbt) {
			req = malloc(strlen(gbt_lp_req) + strlen(lp_id) + 1);
			sprintf(req, gbt_lp_req, lp_id);
		}
		val = json_rpc_call(curl, lp_url, rpc_userpass,
				    req ? req : getwork_req, &err,
				    JSON_RPC_LONGPOLL);
		free(req);
		if (have_stratum) {
			if (val)
				json_decref(val);
			goto out;
		}
		if (likely(val)) {
			bool rc;
			applog(LOG_INFO, "LONGPOLL pushed new work");
			res = json_object_get(val, "result");
			soval = json_object_get(res, "submitold");
			submit_old = soval ? json_is_true(soval) : false;
			pthread_mutex_lock(&g_work_lock);
			work_free(&g_work);
			if (have_gbt)
				rc = gbt_work_decode(res, &g_work);
			else
				rc = work_decode(res, &g_work);
			if (rc) {
				time(&g_work_time);
				restart_threads();
			}
			pthread_mutex_unlock(&g_work_lock);
			json_decref(val);
		} else {
			pthread_mutex_lock(&g_work_lock);
			g_work_time -= LP_SCANTIME;
			pthread_mutex_unlock(&g_work_lock);
			if (err == CURLE_OPERATION_TIMEDOUT) {
				restart_threads();
			} else {
				have_longpoll = false;
				restart_threads();
				free(hdr_path);
				free(lp_url);
				lp_url = NULL;
				sleep(opt_fail_pause);
				goto start;
			}
		}
	}

out:
	free(hdr_path);
	free(lp_url);
	tq_freeze(mythr->q);
	if (curl)
		curl_easy_cleanup(curl);

	return NULL;
}

static bool stratum_handle_response(char *buf)
{
	json_t *val, *err_val, *res_val, *id_val;
	json_error_t err;
	bool ret = false;

	val = JSON_LOADS(buf, &err);
	if (!val) {
		applog(LOG_INFO, "JSON decode failed(%d): %s", err.line, err.text);
		goto out;
	}

	res_val = json_object_get(val, "result");
	err_val = json_object_get(val, "error");
	id_val = json_object_get(val, "id");

	if (!id_val || json_is_null(id_val) || !res_val)
		goto out;

	share_result(json_is_true(res_val),
		err_val ? json_string_value(json_array_get(err_val, 1)) : NULL);

	ret = true;
out:
	if (val)
		json_decref(val);

	return ret;
}

static void *stratum_thread(void *userdata)
{
	struct thr_info *mythr = userdata;
	char *s;

	stratum.url = tq_pop(mythr->q, NULL);
	if (!stratum.url)
		goto out;
	applog(LOG_INFO, "Starting Stratum on %s", stratum.url);

	while (1) {
		int failures = 0;

		while (!stratum.curl) {
			pthread_mutex_lock(&g_work_lock);
			g_work_time = 0;
			pthread_mutex_unlock(&g_work_lock);
			restart_threads();

			if (!stratum_connect(&stratum, stratum.url) ||
			    !stratum_subscribe(&stratum) ||
			    !stratum_authorize(&stratum, rpc_user, rpc_pass)) {
				stratum_disconnect(&stratum);
				if (opt_retries >= 0 && ++failures > opt_retries) {
					applog(LOG_ERR, "...terminating workio thread");
					tq_push(thr_info[work_thr_id].q, NULL);
					goto out;
				}
				applog(LOG_ERR, "...retry after %d seconds", opt_fail_pause);
				sleep(opt_fail_pause);
			}
		}

		if (stratum.job.job_id &&
		    (!g_work_time || strcmp(stratum.job.job_id, g_work.job_id))) {
			pthread_mutex_lock(&g_work_lock);
			stratum_gen_work(&stratum, &g_work);
			time(&g_work_time);
			pthread_mutex_unlock(&g_work_lock);
			if (stratum.job.clean) {
				applog(LOG_INFO, "Stratum requested work restart");
				restart_threads();
			}
		}
		
		if (!stratum_socket_full(&stratum, 120)) {
			applog(LOG_ERR, "Stratum connection timed out");
			s = NULL;
		} else
			s = stratum_recv_line(&stratum);
		if (!s) {
			stratum_disconnect(&stratum);
			applog(LOG_ERR, "Stratum connection interrupted");
			continue;
		}
		if (!stratum_handle_method(&stratum, s))
			stratum_handle_response(s);
		free(s);
	}

out:
	return NULL;
}

static void show_version_and_exit(void)
{
	printf(PACKAGE_STRING "\n built on " __DATE__ "\n features:"
#if defined(USE_ASM) && defined(__i386__)
		" i386"
#endif
#if defined(USE_ASM) && defined(__x86_64__)
		" x86_64"
		" PHE"
#endif
#if defined(USE_ASM) && (defined(__i386__) || defined(__x86_64__))
		" SSE2"
#endif
#if defined(__x86_64__) && defined(USE_AVX)
		" AVX"
#endif
#if defined(__x86_64__) && defined(USE_AVX2)
		" AVX2"
#endif
#if defined(__x86_64__) && defined(USE_XOP)
		" XOP"
#endif
#if defined(USE_ASM) && defined(__arm__) && defined(__APCS_32__)
		" ARM"
#if defined(__ARM_ARCH_5E__) || defined(__ARM_ARCH_5TE__) || \
	defined(__ARM_ARCH_5TEJ__) || defined(__ARM_ARCH_6__) || \
	defined(__ARM_ARCH_6J__) || defined(__ARM_ARCH_6K__) || \
	defined(__ARM_ARCH_6M__) || defined(__ARM_ARCH_6T2__) || \
	defined(__ARM_ARCH_6Z__) || defined(__ARM_ARCH_6ZK__) || \
	defined(__ARM_ARCH_7__) || \
	defined(__ARM_ARCH_7A__) || defined(__ARM_ARCH_7R__) || \
	defined(__ARM_ARCH_7M__) || defined(__ARM_ARCH_7EM__)
		" ARMv5E"
#endif
#if defined(__ARM_NEON__)
		" NEON"
#endif
#endif
#if defined(USE_ASM) && (defined(__powerpc__) || defined(__ppc__) || defined(__PPC__))
		" PowerPC"
#if defined(__ALTIVEC__)
		" AltiVec"
#endif
#endif
		"\n");

	printf("%s\n", curl_version());
#ifdef JANSSON_VERSION
	printf("libjansson %s\n", JANSSON_VERSION);
#endif
	exit(0);
}

static void show_usage_and_exit(int status)
{
	if (status)
		stderr_msg("Try `" PROGRAM_NAME " --help' for more information.\n");
	else
		printf(usage);
	exit(status);
}

static void strhide(char *s)
{
	if (*s) *s++ = 'x';
	while (*s) *s++ = '\0';
}

static void parse_config(json_t *config, char *pname, char *ref);

static void parse_arg(int key, char *arg, char *pname)
{
	char *p;
	int v, i;

	switch(key) {
	case 'a':
		for (i = 0; i < ARRAY_SIZE(algo_names); i++) {
			v = strlen(algo_names[i]);
			if (!strncmp(arg, algo_names[i], v)) {
				if (arg[v] == '\0') {
					opt_algo = i;
					break;
				}
				if (arg[v] == ':' && i == ALGO_SCRYPT) {
					char *ep;
					v = strtol(arg+v+1, &ep, 10);
					if (*ep || v & (v-1) || v < 2)
						continue;
					opt_algo = i;
					opt_scrypt_n = v;
					break;
				}
			}
		}
		if (i == ARRAY_SIZE(algo_names)) {
			stderr_msg("%s: unknown algorithm -- '%s'\n",
				pname, arg);
			show_usage_and_exit(1);
		}
		break;
	case 'B':
		opt_background = true;
		break;
	case 'c': {
		json_error_t err;
		json_t *config = JSON_LOAD_FILE(arg, &err);
		if (!json_is_object(config)) {
			if (err.line < 0)
				stderr_msg("%s: %s\n", pname, err.text);
			else
				stderr_msg("%s: %s:%d: %s\n",
					pname, arg, err.line, err.text);
			exit(1);
		}
		parse_config(config, pname, arg);
		json_decref(config);
		break;
	}
	case 'q':
		opt_quiet = true;
		break;
	case 'D':
		opt_debug = true;
		break;
	case 1016:
		opt_debug_pow = true;
		break;
	case 'p':
		free(rpc_pass);
		rpc_pass = strdup(arg);
		strhide(arg);
		break;
	case 'P':
		opt_protocol = true;
		break;
	case 'r':
		v = atoi(arg);
		if (v < -1 || v > 9999)	/* sanity check */
			show_usage_and_exit(1);
		opt_retries = v;
		break;
	case 'R':
		v = atoi(arg);
		if (v < 1 || v > 9999)	/* sanity check */
			show_usage_and_exit(1);
		opt_fail_pause = v;
		break;
	case 's':
		v = atoi(arg);
		if (v < 1 || v > 9999)	/* sanity check */
			show_usage_and_exit(1);
		opt_scantime = v;
		break;
	case 'T':
		v = atoi(arg);
		if (v < 1 || v > 99999)	/* sanity check */
			show_usage_and_exit(1);
		opt_timeout = v;
		break;
	case 't':
		v = atoi(arg);
		if (v < 1 || v > 9999)	/* sanity check */
			show_usage_and_exit(1);
		opt_n_threads = v;
		break;
	case 'u':
		free(rpc_user);
		rpc_user = strdup(arg);
		break;
	case 'o': {			/* --url */
		char *ap, *hp;
		ap = strstr(arg, "://");
		ap = ap ? ap + 3 : arg;
		hp = strrchr(arg, '@');
		if (hp) {
			*hp = '\0';
			p = strchr(ap, ':');
			if (p) {
				free(rpc_userpass);
				rpc_userpass = strdup(ap);
				free(rpc_user);
				rpc_user = calloc(p - ap + 1, 1);
				strncpy(rpc_user, ap, p - ap);
				free(rpc_pass);
				rpc_pass = strdup(++p);
				if (*p) *p++ = 'x';
				v = strlen(hp + 1) + 1;
				memmove(p + 1, hp + 1, v);
				memset(p + v, 0, hp - p);
				hp = p;
			} else {
				free(rpc_user);
				rpc_user = strdup(ap);
			}
			*hp++ = '@';
		} else
			hp = ap;
		if (ap != arg) {
			if (strncasecmp(arg, "http://", 7) &&
			    strncasecmp(arg, "https://", 8) &&
			    strncasecmp(arg, "stratum+tcp://", 14) &&
			    strncasecmp(arg, "stratum+tcps://", 15)) {
				stderr_msg("%s: unknown protocol -- '%s'\n",
					pname, arg);
				show_usage_and_exit(1);
			}
			free(rpc_url);
			rpc_url = strdup(arg);
			strcpy(rpc_url + (ap - arg), hp);
		} else {
			if (*hp == '\0' || *hp == '/') {
				stderr_msg("%s: invalid URL -- '%s'\n",
					pname, arg);
				show_usage_and_exit(1);
			}
			free(rpc_url);
			rpc_url = malloc(strlen(hp) + 8);
			sprintf(rpc_url, "http://%s", hp);
		}
		have_stratum = !opt_benchmark && !strncasecmp(rpc_url, "stratum", 7);
		break;
	}
	case 'O':			/* --userpass */
		p = strchr(arg, ':');
		if (!p) {
			stderr_msg("%s: invalid username:password pair -- '%s'\n",
				pname, arg);
			show_usage_and_exit(1);
		}
		free(rpc_userpass);
		rpc_userpass = strdup(arg);
		free(rpc_user);
		rpc_user = calloc(p - arg + 1, 1);
		strncpy(rpc_user, arg, p - arg);
		free(rpc_pass);
		rpc_pass = strdup(++p);
		strhide(p);
		break;
	case 'x':			/* --proxy */
		if (!strncasecmp(arg, "socks4://", 9))
			opt_proxy_type = CURLPROXY_SOCKS4;
		else if (!strncasecmp(arg, "socks5://", 9))
			opt_proxy_type = CURLPROXY_SOCKS5;
#if LIBCURL_VERSION_NUM >= 0x071200
		else if (!strncasecmp(arg, "socks4a://", 10))
			opt_proxy_type = CURLPROXY_SOCKS4A;
		else if (!strncasecmp(arg, "socks5h://", 10))
			opt_proxy_type = CURLPROXY_SOCKS5_HOSTNAME;
#endif
		else
			opt_proxy_type = CURLPROXY_HTTP;
		free(opt_proxy);
		opt_proxy = strdup(arg);
		break;
	case 1001:
		free(opt_cert);
		opt_cert = strdup(arg);
		break;
	case 1005:
		opt_benchmark = true;
		want_longpoll = false;
		want_stratum = false;
		have_stratum = false;
		break;
	case 1003:
		want_longpoll = false;
		break;
	case 1007:
		want_stratum = false;
		break;
	case 1009:
		opt_redirect = false;
		break;
	case 1010:
		allow_getwork = false;
		break;
	case 1011:
		have_gbt = false;
		break;
	case 1013:			/* --coinbase-addr */
		pk_script_size = address_to_script(pk_script, sizeof(pk_script), arg);
		if (!pk_script_size) {
			stderr_msg("%s: invalid address -- '%s'\n",
				pname, arg);
			show_usage_and_exit(1);
		}
		break;
	case 1015:			/* --coinbase-sig */
		if (strlen(arg) + 1 > sizeof(coinbase_sig)) {
			stderr_msg("%s: coinbase signature too long\n", pname);
			show_usage_and_exit(1);
		}
		strcpy(coinbase_sig, arg);
		break;
	case 'S':
		use_syslog = true;
		break;
	case 'V':
		show_version_and_exit();
	case 'h':
		show_usage_and_exit(0);
	default:
		show_usage_and_exit(1);
	}
}

static void parse_config(json_t *config, char *pname, char *ref)
{
	int i;
	char *s;
	json_t *val;

	for (i = 0; i < ARRAY_SIZE(options); i++) {
		if (!options[i].name)
			break;

		val = json_object_get(config, options[i].name);
		if (!val)
			continue;

		if (options[i].has_arg && json_is_string(val)) {
			if (!strcmp(options[i].name, "config")) {
				stderr_msg("%s: %s: option '%s' not allowed here\n",
					pname, ref, options[i].name);
				exit(1);
			}
			s = strdup(json_string_value(val));
			if (!s)
				break;
			parse_arg(options[i].val, s, pname);
			free(s);
		} else if (!options[i].has_arg && json_is_true(val)) {
			parse_arg(options[i].val, "", pname);
		} else {
			stderr_msg("%s: invalid argument for option '%s'\n",
				pname, options[i].name);
			exit(1);
		}
	}
}

static void parse_cmdline(int argc, char *argv[])
{
	int key;

	while (1) {
#if HAVE_GETOPT_LONG
		key = getopt_long(argc, argv, short_options, options, NULL);
#else
		key = getopt(argc, argv, short_options);
#endif
		if (key < 0)
			break;

		parse_arg(key, optarg, argv[0]);
	}
	if (optind < argc) {
		stderr_msg("%s: unsupported non-option argument -- '%s'\n",
			argv[0], argv[optind]);
		show_usage_and_exit(1);
	}
}

#ifndef WIN32
static void signal_handler(int sig)
{
	switch (sig) {
	case SIGHUP:
		applog(LOG_INFO, "SIGHUP received");
		break;
	case SIGINT:
		applog(LOG_INFO, "SIGINT received, exiting");
		exit(0);
		break;
	case SIGTERM:
		applog(LOG_INFO, "SIGTERM received, exiting");
		exit(0);
		break;
	}
}
#endif

int main(int argc, char *argv[])
{
	struct thr_info *thr;
	long flags;
	int i;

	rpc_user = strdup("");
	rpc_pass = strdup("");

	/* parse command line */
	parse_cmdline(argc, argv);

	if (!opt_benchmark && !rpc_url) {
		stderr_msg("%s: no URL supplied\n", argv[0]);
		show_usage_and_exit(1);
	}

	if (!rpc_userpass) {
		rpc_userpass = malloc(strlen(rpc_user) + strlen(rpc_pass) + 2);
		if (!rpc_userpass)
			return 1;
		sprintf(rpc_userpass, "%s:%s", rpc_user, rpc_pass);
	}

	pthread_mutex_init(&stats_lock, NULL);
	pthread_mutex_init(&g_work_lock, NULL);
	pthread_mutex_init(&stratum.sock_lock, NULL);
	pthread_mutex_init(&stratum.work_lock, NULL);
	if (!opus_hash_selftest()) {
		applog(LOG_ERR, "OpusHash self-test failed. Ref hash mismatch.");
		return 1;
	}
	applog(LOG_INFO, "OpusHash self-test passed.");

	{
		unsigned char rigor_magic[4];
		rigor_testnet_message_start_fill(rigor_magic);
		applog(LOG_INFO,
		       "Rigor testnet P2P magic (pchMessageStart): %02x%02x%02x%02x — "
		       "minerd uses JSON-RPC only; use these bytes if you add wire P2P",
		       (unsigned)rigor_magic[0],
		       (unsigned)rigor_magic[1],
		       (unsigned)rigor_magic[2],
		       (unsigned)rigor_magic[3]);
		if (!rigor_testnet_message_start_eq(rigor_magic)) {
			applog(LOG_ERR, "Rigor testnet P2P magic self-check failed (internal error)");
			return 1;
		}
	}

	flags = opt_benchmark || (strncasecmp(rpc_url, "https://", 8) &&
	                          strncasecmp(rpc_url, "stratum+tcps://", 15))
	      ? (CURL_GLOBAL_ALL & ~CURL_GLOBAL_SSL)
	      : CURL_GLOBAL_ALL;
	if (curl_global_init(flags)) {
		applog(LOG_ERR, "CURL initialization failed");
		return 1;
	}

#ifndef WIN32
	if (opt_background) {
		i = fork();
		if (i < 0) exit(1);
		if (i > 0) exit(0);
		i = setsid();
		if (i < 0)
			applog(LOG_ERR, "setsid() failed (errno = %d)", errno);
		i = chdir("/");
		if (i < 0)
			applog(LOG_ERR, "chdir() failed (errno = %d)", errno);
		signal(SIGHUP, signal_handler);
		signal(SIGINT, signal_handler);
		signal(SIGTERM, signal_handler);
	}
#endif

#if defined(WIN32)
	SYSTEM_INFO sysinfo;
	GetSystemInfo(&sysinfo);
	num_processors = sysinfo.dwNumberOfProcessors;
#elif defined(_SC_NPROCESSORS_CONF)
	num_processors = sysconf(_SC_NPROCESSORS_CONF);
#elif defined(CTL_HW) && defined(HW_NCPU)
	int req[] = { CTL_HW, HW_NCPU };
	size_t len = sizeof(num_processors);
	sysctl(req, 2, &num_processors, &len, NULL, 0);
#else
	num_processors = 1;
#endif
	if (num_processors < 1)
		num_processors = 1;
	if (!opt_n_threads)
		opt_n_threads = num_processors;

#ifdef HAVE_SYSLOG_H
	if (use_syslog)
		openlog("cpuminer", LOG_PID, LOG_USER);
#endif

	work_restart = calloc(opt_n_threads, sizeof(*work_restart));
	if (!work_restart)
		return 1;

	thr_info = calloc(opt_n_threads + 3, sizeof(*thr));
	if (!thr_info)
		return 1;
	
	thr_hashrates = (double *) calloc(opt_n_threads, sizeof(double));
	if (!thr_hashrates)
		return 1;

	/* init workio thread info */
	work_thr_id = opt_n_threads;
	thr = &thr_info[work_thr_id];
	thr->id = work_thr_id;
	thr->q = tq_new();
	if (!thr->q)
		return 1;

	/* start work I/O thread */
	if (pthread_create(&thr->pth, NULL, workio_thread, thr)) {
		applog(LOG_ERR, "workio thread create failed");
		return 1;
	}

	if (want_longpoll && !have_stratum) {
		/* init longpoll thread info */
		longpoll_thr_id = opt_n_threads + 1;
		thr = &thr_info[longpoll_thr_id];
		thr->id = longpoll_thr_id;
		thr->q = tq_new();
		if (!thr->q)
			return 1;

		/* start longpoll thread */
		if (unlikely(pthread_create(&thr->pth, NULL, longpoll_thread, thr))) {
			applog(LOG_ERR, "longpoll thread create failed");
			return 1;
		}
	}
	if (want_stratum) {
		/* init stratum thread info */
		stratum_thr_id = opt_n_threads + 2;
		thr = &thr_info[stratum_thr_id];
		thr->id = stratum_thr_id;
		thr->q = tq_new();
		if (!thr->q)
			return 1;

		/* start stratum thread */
		if (unlikely(pthread_create(&thr->pth, NULL, stratum_thread, thr))) {
			applog(LOG_ERR, "stratum thread create failed");
			return 1;
		}

		if (have_stratum)
			tq_push(thr_info[stratum_thr_id].q, strdup(rpc_url));
	}

	/* start mining threads */
	for (i = 0; i < opt_n_threads; i++) {
		thr = &thr_info[i];

		thr->id = i;
		thr->q = tq_new();
		if (!thr->q)
			return 1;

		if (unlikely(pthread_create(&thr->pth, NULL, miner_thread, thr))) {
			applog(LOG_ERR, "thread %d create failed", i);
			return 1;
		}
	}

	applog(LOG_INFO, "%d miner threads started, "
		"using '%s' algorithm.",
		opt_n_threads,
		algo_names[opt_algo]);

	/* main loop - simply wait for workio thread to exit */
	pthread_join(thr_info[work_thr_id].pth, NULL);

	applog(LOG_INFO, "workio thread dead, exiting.");

	return 0;
}
