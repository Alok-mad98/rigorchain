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
#ifndef WIN32
#include <sys/resource.h>
#include <sys/stat.h>
#else
#include <direct.h>
#endif
#include <signal.h>
#include <errno.h>
#include <curl/curl.h>
#include <jansson.h>

#include "compat.h"
#include "miner.h"
#include "rgrpow.h"
#include "arith_uint256.h"
#include "gbt_solo.h"

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
	ALGO_SHA256D,		/* CLI sha256d/rgrpow01 = SHA3-256 RGRPOW01 */
};

static const char *algo_names[] = {
	[ALGO_SCRYPT]		= "scrypt",
	[ALGO_SHA256D]		= "sha256d",
};

bool opt_debug = false;
bool opt_debug_pow = false;
bool opt_verbose = false;
bool opt_protocol = false;
static bool opt_benchmark = false;
bool opt_redirect = true;
bool want_longpoll = true;
bool have_longpoll = false;
bool have_gbt = true;
bool allow_getwork = false; /* P041: GBT→submitblock is the only SOLO path */
bool want_stratum = false;  /* Stratum is P042-quarantined; not SOLO */
bool have_stratum = false;
bool use_syslog = false;
static bool opt_background = false;
static bool opt_quiet = false;
static int opt_retries = -1;
static int opt_fail_pause = 30;
/** Pause this many seconds after each accepted block (keeps Qt wallet responsive). */
static int opt_submit_delay = 0;
int opt_timeout = 0;
static int opt_scantime = 5;
static enum algos opt_algo = ALGO_SHA256D; /* CLI sha256d/rgrpow01 = SHA3-256 RGRPOW01 */
static int opt_scrypt_n = 1024;
static int opt_n_threads;
static int num_processors;
static char *rpc_url;
static char *rpc_userpass;
static char *rpc_user, *rpc_pass;
static int pk_script_size;
static unsigned char pk_script[42];
static char *coinbase_addr;
static char coinbase_sig[101] = "";

/* User-mode solo mining dashboard state (hidden behind --verbose for developers). */
static bool ux_node_connected = false;
static bool ux_mining_running = false;
static int ux_height;
static int64_t ux_reward_sat;
static char ux_last_block[32] = "-";
static char ux_status_line[96] = "Starting";
static time_t ux_last_status_print;
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

static pthread_mutex_t ux_print_lock = PTHREAD_MUTEX_INITIALIZER;

static bool rgrpow_selftest(void)
{
	/* core/src/test/rigor_pow_digest_tests.cpp zero-header vector */
	static const uint8_t expect[32] = {
		0xdd, 0x05, 0x7f, 0xc1, 0xdf, 0x8f, 0x5b, 0x94, 0x66, 0xe8, 0x34,
		0xcc, 0x9d, 0x7b, 0x81, 0x9e, 0x2d, 0x51, 0x1c, 0x75, 0xb4, 0x36,
		0x75, 0xb5, 0x77, 0xf6, 0xd9, 0xf5, 0xd6, 0xde, 0x04, 0xfc
	};
	uint8_t header[80];
	uint8_t got[32];
	memset(header, 0, sizeof(header));
	rgrpow_hash80(header, got);
	return memcmp(got, expect, sizeof(got)) == 0;
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
                          rgrpow01  SHA3-256(RGRPOW01 || header_80) (default)\n\
                          sha256d   alias for rgrpow01\n\
                          scrypt    disabled for RIGOR SOLO\n\
  -o, --url=URL         URL of mining server\n\
  -O, --userpass=U:P    username:password pair for mining server\n\
  -u, --user=USERNAME   username for mining server\n\
  -p, --pass=PASSWORD   password for mining server\n\
      --cert=FILE       certificate for mining server using SSL\n\
  -x, --proxy=[PROTOCOL://]HOST[:PORT]  connect through a proxy\n\
  -t, --threads=N       number of miner threads (default: processors minus 1,\n\
                          or minus 2 from 5 processors up, so the wallet keeps\n\
                          a core; pass -t explicitly to use every processor)\n\
  -r, --retries=N       number of times to retry if a network call fails\n\
                          (default: retry indefinitely)\n\
  -R, --retry-pause=N   time to pause between retries, in seconds (default: 30)\n\
  -T, --timeout=N       timeout for long polling, in seconds (default: none)\n\
  -s, --scantime=N      upper bound on time spent scanning current work when\n\
                          long polling is unavailable, in seconds (default: 5)\n\
      --coinbase-addr=ADDR  sent as GBT coinbase_address; node builds canonical coinbase\n\
      --coinbase-sig=TEXT  ignored (coinbase scriptSig is node-immutable)\n\
      --no-longpoll     disable long polling support\n\
      --no-getwork      (default) getwork is not a RIGOR SOLO path\n\
      --no-gbt          refused: GBT is required\n\
      --no-stratum      (default) Stratum is not the SOLO path\n\
      --no-redirect     ignore requests to change the URL of the mining server\n\
  -q, --quiet           disable per-thread hashmeter output\n\
      --submit-delay=N  sleep N seconds after each accepted block (eases wallet UI under easy PoW)\n\
  -v, --verbose         developer verbose output (GBT, LONGPOLL, RPC, self-test)\n\
  -D, --debug           enable debug output (implies --verbose)\n\
      --debug-pow       log header SHA256d and RGRPOW01 digest for each found nonce\n\
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
	"a:c:Dhp:Px:qr:R:s:t:T:o:u:O:Vv";

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
	{ "submit-delay", 1, NULL, 1017 },
	{ "scantime", 1, NULL, 's' },
#ifdef HAVE_SYSLOG_H
	{ "syslog", 0, NULL, 'S' },
#endif
	{ "threads", 1, NULL, 't' },
	{ "timeout", 1, NULL, 'T' },
	{ "url", 1, NULL, 'o' },
	{ "user", 1, NULL, 'u' },
	{ "userpass", 1, NULL, 'O' },
	{ "verbose", 0, NULL, 'v' },
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
	/* Prefer GBT "convergence_root" (exact bytes node expects) when present. */
	unsigned char convergence_root[32];
	bool has_convergence_root;

	/* Rigor: previous block hash for querying nBits */
	char *prevblockhash;
	uint32_t prev_nbits;

	/* PoW: nBits from GBT (used to construct target via SetCompact) */
	uint32_t nbits;

	/* Coinbase ORU subsidy+fees from GBT (atoms). Display only.
	 * Consensus subsidy is RFC-020: 0 at height 0, 128 ORU for every
	 * h ≥ 1, no halving. The miner copies GBT coinbasevalue; it does
	 * not recompute GetBlockSubsidy. */
	int64_t coinbasevalue;
};

static struct work g_work;
static time_t g_work_time;
static pthread_mutex_t g_work_lock;
static void restart_threads(void);
static void solo_invalidate_gbt(void);

static int hex_is_even(const char *s)
{
	size_t n;

	if (!s)
		return 0;
	for (n = 0; s[n]; n++) {
		char c = s[n];
		if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') ||
		      (c >= 'A' && c <= 'F')))
			return 0;
	}
	return n > 0 && (n % 2) == 0;
}
static bool submit_old = false;
static char *lp_id;
static pthread_mutex_t lp_id_lock = PTHREAD_MUTEX_INITIALIZER;

static double ux_total_hashrate(void)
{
	double hashrate = 0.;
	int i;

	if (!thr_hashrates || opt_n_threads <= 0)
		return 0.;
	pthread_mutex_lock(&stats_lock);
	for (i = 0; i < opt_n_threads; i++)
		hashrate += thr_hashrates[i];
	pthread_mutex_unlock(&stats_lock);
	return hashrate;
}

static void ux_format_hashrate(char *buf, size_t buflen, double hashrate)
{
	if (hashrate >= 1e6)
		snprintf(buf, buflen, "%.0f KH/s", 1e-3 * hashrate);
	else
		snprintf(buf, buflen, "%.2f KH/s", 1e-3 * hashrate);
}

static void ux_format_reward(char *buf, size_t buflen, int64_t sat)
{
	double oru = sat / 100000000.0;
	if (fabs(oru - floor(oru + 0.5)) < 1e-6)
		snprintf(buf, buflen, "%.0f ORU", oru);
	else
		snprintf(buf, buflen, "%.8f ORU", oru);
}

static void ux_emit(const char *text)
{
#ifdef CPUMINER_STDERR_LOG_LOCK
	pthread_mutex_lock(&applog_lock);
#endif
	fputs(text, stderr);
	fflush(stderr);
#ifdef CPUMINER_STDERR_LOG_LOCK
	pthread_mutex_unlock(&applog_lock);
#endif
}

static void ux_ensure_parent_dir(const char *path)
{
	char dir[1024];
	char *slash;

	if (!path || !path[0])
		return;
	snprintf(dir, sizeof(dir), "%s", path);
	slash = strrchr(dir, '/');
#ifdef WIN32
	{
		char *bslash = strrchr(dir, '\\');
		if (bslash && (!slash || bslash > slash))
			slash = bslash;
	}
#endif
	if (!slash || slash == dir)
		return;
	*slash = '\0';
#ifdef WIN32
	_mkdir(dir);
#else
	mkdir(dir, 0755);
#endif
}

/* Local dashboard telemetry. No RPC secrets. */
static void ux_write_status_file(void)
{
	char path[1024];
	char hr[32];
	const char *env = getenv("RIGOR_MINER_STATUS_FILE");
	const char *payout = (coinbase_addr && coinbase_addr[0]) ? coinbase_addr : "";
	FILE *fp;
	double hs;
	time_t now = time(NULL);

	if (env && env[0]) {
		snprintf(path, sizeof(path), "%s", env);
	} else {
#ifdef WIN32
		const char *appdata = getenv("APPDATA");
		if (appdata && appdata[0])
			snprintf(path, sizeof(path), "%s\\RIGOR\\miner-status.json", appdata);
		else
			snprintf(path, sizeof(path), "miner-status.json");
#else
		const char *home = getenv("HOME");
		if (home && home[0])
			snprintf(path, sizeof(path), "%s/.rigor/miner-status.json", home);
		else
			snprintf(path, sizeof(path), "miner-status.json");
#endif
	}
	ux_ensure_parent_dir(path);
	fp = fopen(path, "w");
	if (!fp)
		return;
	hs = ux_total_hashrate();
	ux_format_hashrate(hr, sizeof(hr), hs);
	fprintf(fp,
		"{\n"
		"  \"generated_at\": %ld,\n"
		"  \"path\": \"SOLO GBT\",\n"
		"  \"hashrate_hs\": %.6f,\n"
		"  \"hashrate\": \"%s\",\n"
		"  \"gbt_height\": %d,\n"
		"  \"blocks_accepted\": %lu,\n"
		"  \"blocks_rejected\": %lu,\n"
		"  \"last_submit\": \"%s\",\n"
		"  \"payout\": \"%s\",\n"
		"  \"node_connected\": %s,\n"
		"  \"mining\": %s\n"
		"}\n",
		(long)now,
		hs,
		hr,
		ux_height,
		accepted_count,
		rejected_count,
		ux_last_block[0] ? ux_last_block : "-",
		payout,
		ux_node_connected ? "true" : "false",
		ux_mining_running ? "true" : "false");
	fclose(fp);
}

/* Default alpha UX: one atomic status board (aligned, no developer internals). */
static void user_print_status(void)
{
	char hr[32];
	char height_buf[32];
	char buf[1024];
	const char *mode;
	const char *node;
	const char *wallet;
	const char *mining;
	const char *payout;

	if (opt_verbose)
		return;

	pthread_mutex_lock(&ux_print_lock);

	ux_format_hashrate(hr, sizeof(hr), ux_total_hashrate());
	if (ux_height > 0)
		snprintf(height_buf, sizeof(height_buf), "%d", ux_height);
	else
		snprintf(height_buf, sizeof(height_buf), "-");

	mode = have_stratum ? "Pool Mining" : "Solo Mining";
	node = ux_node_connected ? "Connected" : "Connecting...";
	if (have_stratum)
		wallet = "N/A (pool)";
	else
		wallet = ux_node_connected ? "Connected" : "Connecting...";
	mining = ux_mining_running ? "Running" : "Starting";
	payout = (coinbase_addr && coinbase_addr[0]) ? coinbase_addr : "(from node template)";

	snprintf(buf, sizeof(buf),
		"\n"
		"--------------------------------\n"
		"RIGOR Miner\n"
		"--------------------------------\n"
		"%-16s %s\n"
		"%-16s %s\n"
		"%-16s %s\n"
		"%-16s %s\n"
		"%-16s %s\n"
		"%-16s %s\n"
		"%-16s %lu\n"
		"%-16s %s\n"
		"%-16s %s\n"
		"%-16s %s\n"
		"--------------------------------\n",
		"Mode", mode,
		"Node", node,
		"Wallet", wallet,
		"Mining", mining,
		"Height", height_buf,
		"Hashrate", hr,
		"Blocks found", accepted_count,
		"Last block", ux_last_block,
		"Payout", payout,
		"Status", ux_status_line);
	ux_emit(buf);
	time(&ux_last_status_print);

	pthread_mutex_unlock(&ux_print_lock);
	ux_write_status_file();
}

/* Periodic heartbeat — one line, not a full board reprint. */
static void user_print_heartbeat(void)
{
	char hr[32];
	char buf[256];

	if (opt_verbose)
		return;

	pthread_mutex_lock(&ux_print_lock);
	ux_format_hashrate(hr, sizeof(hr), ux_total_hashrate());
	snprintf(buf, sizeof(buf),
		"[mining] height %d | %s | blocks %lu | %s\n",
		ux_height > 0 ? ux_height : 0,
		hr,
		accepted_count,
		ux_status_line);
	ux_emit(buf);
	time(&ux_last_status_print);
	pthread_mutex_unlock(&ux_print_lock);
	ux_write_status_file();
}

static void user_print_status_throttled(int interval_sec)
{
	time_t now;

	if (opt_verbose)
		return;
	time(&now);
	if (ux_last_status_print && (now - ux_last_status_print) < interval_sec)
		return;
	user_print_heartbeat();
}

static void user_print_block_found(int height, int64_t reward)
{
	char reward_buf[48];
	char height_buf[32];
	char buf[384];

	if (opt_verbose)
		return;

	if (reward > 0)
		ux_format_reward(reward_buf, sizeof(reward_buf), reward);
	else
		snprintf(reward_buf, sizeof(reward_buf), "-");
	if (height > 0)
		snprintf(height_buf, sizeof(height_buf), "%d", height);
	else
		snprintf(height_buf, sizeof(height_buf), "-");

	pthread_mutex_lock(&ux_print_lock);
	snprintf(buf, sizeof(buf),
		"\n"
		"--------------------------------\n"
		"Block found!\n"
		"%-16s %s\n"
		"%-16s %s\n"
		"%-16s Confirmed by node\n"
		"--------------------------------\n",
		"Height", height_buf,
		"Reward", reward_buf,
		"Status");
	ux_emit(buf);
	time(&ux_last_status_print);
	pthread_mutex_unlock(&ux_print_lock);
}

static void user_print_block_rejected(const char *reason)
{
	char buf[512];

	if (opt_verbose)
		return;

	pthread_mutex_lock(&ux_print_lock);
	snprintf(buf, sizeof(buf),
		"\n"
		"--------------------------------\n"
		"Block rejected\n"
		"%-16s %s\n"
		"%-16s Mining continues\n"
		"--------------------------------\n",
		"Reason", reason ? reason : "unknown",
		"Status");
	ux_emit(buf);
	time(&ux_last_status_print);
	pthread_mutex_unlock(&ux_print_lock);
}

static void ux_note_work(const struct work *work)
{
	if (!work)
		return;
	ux_node_connected = true;
	ux_mining_running = true;
	if (work->height > 0)
		ux_height = work->height;
	if (work->coinbasevalue > 0)
		ux_reward_sat = work->coinbasevalue;
	snprintf(ux_status_line, sizeof(ux_status_line), "Mining normally");
}

static void work_free(struct work *w)
{
	if (!w)
		return;
	free(w->txs);
	w->txs = NULL;
	free(w->workid);
	w->workid = NULL;
	free(w->job_id);
	w->job_id = NULL;
	free(w->xnonce2);
	w->xnonce2 = NULL;
	if (w->convergence_txids) {
		for (int i = 0; i < w->convergence_txid_count; i++)
			free(w->convergence_txids[i]);
		free(w->convergence_txids);
		w->convergence_txids = NULL;
		w->convergence_txid_count = 0;
	}
	free(w->prevblockhash);
	w->prevblockhash = NULL;
}

static void work_discard(struct work *w)
{
	if (!w)
		return;
	work_free(w);
	free(w);
}

/*
 * Deep-copy heap fields into a temporary, then swap into dest.
 * dest must be zeroed or previously work_free'd (calloc, not raw malloc).
 * On allocation failure dest is left unchanged and no pointer aliases src.
 * The old memcpy-then-strdup path aliased src on strdup failure and later
 * double-freed it (Windows STATUS_HEAP_CORRUPTION / 0xC0000374).
 */
static bool work_copy(struct work *dest, const struct work *src)
{
	struct work tmp;

	if (!dest || !src)
		return false;

	memcpy(&tmp, src, sizeof(tmp));
	tmp.txs = NULL;
	tmp.workid = NULL;
	tmp.job_id = NULL;
	tmp.xnonce2 = NULL;
	tmp.convergence_txids = NULL;
	tmp.convergence_txid_count = 0;
	tmp.prevblockhash = NULL;

	if (src->txs) {
		tmp.txs = strdup(src->txs);
		if (!tmp.txs)
			goto fail;
	}
	if (src->workid) {
		tmp.workid = strdup(src->workid);
		if (!tmp.workid)
			goto fail;
	}
	if (src->job_id) {
		tmp.job_id = strdup(src->job_id);
		if (!tmp.job_id)
			goto fail;
	}
	if (src->prevblockhash) {
		tmp.prevblockhash = strdup(src->prevblockhash);
		if (!tmp.prevblockhash)
			goto fail;
	}
	if (src->xnonce2 && src->xnonce2_len) {
		tmp.xnonce2 = malloc(src->xnonce2_len);
		if (!tmp.xnonce2)
			goto fail;
		memcpy(tmp.xnonce2, src->xnonce2, src->xnonce2_len);
	}
	if (src->convergence_txids && src->convergence_txid_count > 0) {
		int i;

		tmp.convergence_txids = calloc((size_t)src->convergence_txid_count,
					       sizeof(char *));
		if (!tmp.convergence_txids)
			goto fail;
		tmp.convergence_txid_count = src->convergence_txid_count;
		for (i = 0; i < src->convergence_txid_count; i++) {
			if (!src->convergence_txids[i])
				continue;
			tmp.convergence_txids[i] = strdup(src->convergence_txids[i]);
			if (!tmp.convergence_txids[i])
				goto fail;
		}
	}

	work_free(dest);
	memcpy(dest, &tmp, sizeof(tmp));
	return true;

fail:
	work_free(&tmp);
	return false;
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

static bool __attribute__((unused)) work_decode(const json_t *val, struct work *work)
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

static bool gbt_work_decode(const json_t *val, struct work *work)
{
	struct gbt_solo_out assembled;
	char err[256];

	if (!pk_script_size || !gbt_solo_payout_is_oru_v0(pk_script, pk_script_size)) {
		applog(LOG_ERR, "SOLO requires --coinbase-addr as ORU witness v0 (rg1q... / rgrt1q...)");
		return false;
	}
	if (!gbt_solo_assemble(val, pk_script, pk_script_size, &assembled, err, sizeof(err))) {
		applog(LOG_ERR, "GBT refuse: %s", err);
		gbt_solo_out_free(&assembled);
		return false;
	}

	work_free(work);
	memcpy(work->data, assembled.header_words, sizeof(assembled.header_words));
	work->data[20] = 0x80000000;
	work->data[31] = 0x00000280;
	memcpy(work->target, assembled.target, sizeof(assembled.target));
	work->height = assembled.height;
	work->coinbasevalue = assembled.coinbasevalue;
	work->nbits = assembled.nbits;
	work->txs = assembled.txs_hex;
	assembled.txs_hex = NULL;
	work->workid = assembled.workid;
	assembled.workid = NULL;
	gbt_solo_out_free(&assembled);

	{
		json_t *lp = json_object_get(val, "longpollid");
		const char *s = json_string_value(lp);
		bool start_lp = false;

		if (want_longpoll && s && longpoll_thr_id >= 0) {
			pthread_mutex_lock(&lp_id_lock);
			if (!lp_id || strcmp(lp_id, s)) {
				char *dup = strdup(s);
				if (dup) {
					free(lp_id);
					lp_id = dup;
					if (!have_longpoll) {
						have_longpoll = true;
						start_lp = true;
					}
				}
			}
			pthread_mutex_unlock(&lp_id_lock);
			if (start_lp)
				tq_push(thr_info[longpoll_thr_id].q, strdup(rpc_url));
		}
	}

	if (opt_debug || opt_debug_pow) {
		char hexbuf[65];
		bin2hex(hexbuf, (unsigned char *)(work->data + 9), 32);
		applog(LOG_DEBUG, "SOLO copy-bytes height=%d nBits=0x%08x merkle=%s RES_mint=none",
		       work->height, work->nbits, hexbuf);
	}
	return true;
}

static void share_result(int result, const char *reason, int height, int64_t reward)
{
	char s[345];
	double hashrate;
	int i;
	/* Stale multi-thread races — not a consensus failure; keep stats quiet. */
	const bool soft = !result && reason &&
		(!strcmp(reason, "inconclusive") ||
		 !strcmp(reason, "duplicate") ||
		 !strcmp(reason, "duplicate-inconclusive") ||
		 !strcmp(reason, "inconclusive-not-best-prevblk"));

	hashrate = 0.;
	pthread_mutex_lock(&stats_lock);
	for (i = 0; i < opt_n_threads; i++)
		hashrate += thr_hashrates[i];
	if (result)
		accepted_count++;
	else if (!soft)
		rejected_count++;
	pthread_mutex_unlock(&stats_lock);

	if (soft) {
		if (opt_debug)
			applog(LOG_DEBUG, "stale/race submit ignored: %s", reason);
		return;
	}

	sprintf(s, hashrate >= 1e6 ? "%.0f" : "%.2f", 1e-3 * hashrate);
	if (result) {
		if (!opt_verbose) {
			int show_height = height > 0 ? height : ux_height;
			int64_t show_reward = reward > 0 ? reward : ux_reward_sat;

			snprintf(ux_last_block, sizeof(ux_last_block), "Accepted");
			snprintf(ux_status_line, sizeof(ux_status_line), "Mining normally");
			user_print_block_found(show_height, show_reward);
			user_print_status();
		} else {
			applog(LOG_INFO, "block accepted: %lu ok / %lu hard-fail (%.2f%%), %s khash/s",
				   accepted_count,
				   accepted_count + rejected_count,
				   (accepted_count + rejected_count)
					   ? 100. * accepted_count / (accepted_count + rejected_count)
					   : 0.,
				   s);
		}
	} else {
		snprintf(ux_last_block, sizeof(ux_last_block), "Rejected");
		snprintf(ux_status_line, sizeof(ux_status_line), "Block rejected — mining continues");
		if (!opt_verbose) {
			user_print_block_rejected(reason);
		} else {
			applog(LOG_ERR, "block rejected: %s | %lu ok / %lu hard-fail, %s khash/s",
				   reason ? reason : "unknown",
				   accepted_count,
				   accepted_count + rejected_count,
				   s);
		}
	}
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
	if (!submit_old) {
		int stale;

		pthread_mutex_lock(&g_work_lock);
		stale = memcmp(work->data + 1, g_work.data + 1, 32) != 0;
		pthread_mutex_unlock(&g_work_lock);
		if (stale) {
			if (opt_debug)
				applog(LOG_DEBUG, "DEBUG: stale work detected, discarding");
			return true;
		}
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
		char *blockhex;
		json_t *rpc, *params;
		uint32_t data_wire[20];
		size_t txs_len;

		/* Encode 80-byte header only (do not walk work->data[32] into a 20-word buffer). */
		for (i = 0; i < 20; i++)
			le32enc(data_wire + i, work->data[i]);
		bin2hex(data_str, (unsigned char *)data_wire, 80);
		if (!hex_is_even(work->txs)) {
			applog(LOG_ERR, "submitblock: txs is not even hex (len=%zu)",
			       work->txs ? strlen(work->txs) : 0);
			solo_invalidate_gbt();
			rc = true;
			goto out;
		}
		if (opt_debug) {
			applog(LOG_DEBUG, "submit nTime=%u nBits=0x%08x nonce=0x%08x",
			       work->data[17], work->data[18], work->data[19]);
		}
		for (i = 0; i < 8; i++)
			le32enc(target_be + i, work->target[7 - i]);
		bin2hex(target_str, (unsigned char *)target_be, 32);
		if (opt_debug)
			applog(LOG_DEBUG, "submitblock header=%s target=%s", data_str, target_str);
		txs_len = strlen(work->txs);
		blockhex = malloc(160 + txs_len + 1);
		if (!blockhex)
			goto out;
		memcpy(blockhex, data_str, 160);
		memcpy(blockhex + 160, work->txs, txs_len + 1);
		rpc = json_object();
		params = json_array();
		json_array_append_new(params, json_string(blockhex));
		free(blockhex);
		if (work->workid) {
			json_t *extra = json_object();
			json_object_set_new(extra, "workid", json_string(work->workid));
			json_array_append_new(params, extra);
		}
		json_object_set_new(rpc, "method", json_string("submitblock"));
		json_object_set_new(rpc, "params", params);
		json_object_set_new(rpc, "id", json_integer(1));
		req = json_dumps(rpc, JSON_COMPACT);
		json_decref(rpc);
		if (!req)
			goto out;
		val = json_rpc_call(curl, rpc_url, rpc_userpass, req, NULL, 0);
		free(req);
		if (unlikely(!val)) {
			applog(LOG_ERR, "submit_upstream_work json_rpc_call failed");
			solo_invalidate_gbt();
			rc = true;
			goto out;
		}

		if (opt_debug) {
			char *dbg = json_dumps(val, 0);
			applog(LOG_DEBUG, "RPC response: %s", dbg);
			free(dbg);
		}

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
			share_result(sumres, res_str, work->height, work->coinbasevalue);
			free(res_str);
		} else
			share_result(json_is_null(res), json_string_value(res),
				     work->height, work->coinbasevalue);

		json_decref(val);

		solo_invalidate_gbt();
	} else {
		applog(LOG_ERR, "submitblock requires GBT copy-bytes work; getwork is disabled");
		goto out;
	}

	rc = true;

out:
	return rc;
}

/* Address selection goes to the node. Miner copies default_coinbase_tx. */
static char *gbt_request_json(const char *longpollid)
{
	json_t *root, *params, *req, *caps, *rules;
	char *body, *out;

	caps = json_array();
	json_array_append_new(caps, json_string("coinbasevalue"));
	json_array_append_new(caps, json_string("longpoll"));
	json_array_append_new(caps, json_string("workid"));
	rules = json_array();
	json_array_append_new(rules, json_string("segwit"));
	req = json_object();
	json_object_set_new(req, "capabilities", caps);
	json_object_set_new(req, "rules", rules);
	if (coinbase_addr && coinbase_addr[0])
		json_object_set_new(req, "coinbase_address", json_string(coinbase_addr));
	if (longpollid && longpollid[0])
		json_object_set_new(req, "longpollid", json_string(longpollid));
	params = json_array();
	json_array_append_new(params, req);
	root = json_object();
	json_object_set_new(root, "method", json_string("getblocktemplate"));
	json_object_set_new(root, "params", params);
	json_object_set_new(root, "id", json_integer(0));
	body = json_dumps(root, JSON_COMPACT);
	json_decref(root);
	if (!body)
		return NULL;
	out = malloc(strlen(body) + 3);
	if (!out) {
		free(body);
		return NULL;
	}
	sprintf(out, "%s\r\n", body);
	free(body);
	return out;
}

static bool get_upstream_work(CURL *curl, struct work *work)
{
	json_t *val;
	int err;
	bool rc;
	struct timeval tv_start, tv_end, diff;
	char *req;

	gettimeofday(&tv_start, NULL);
	req = gbt_request_json(NULL);
	if (!req)
		return false;
	val = json_rpc_call(curl, rpc_url, rpc_userpass, req, &err, 0);
	free(req);
	gettimeofday(&tv_end, NULL);

	if (have_stratum) {
		if (val)
			json_decref(val);
		return true;
	}

	if (!val)
		return false;

	rc = gbt_work_decode(json_object_get(val, "result"), work);

	if (rc) {
		bool first = !ux_node_connected;
		ux_note_work(work);
		if (!opt_verbose && first)
			user_print_status();
	}

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
		work_discard(wc->u.work);
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
			if (!opt_verbose)
				applog(LOG_ERR,
				       "Cannot connect to local RIGOR node.\n"
				       "Please start rigor-wallet or rigord first.");
			else
				applog(LOG_ERR, "json_rpc_call failed, terminating workio thread");
			work_discard(ret_work);
			return false;
		}

		/* pause, then restart work-request loop */
		if (!opt_verbose) {
			snprintf(ux_status_line, sizeof(ux_status_line),
				 "Waiting to reconnect to node");
			applog(LOG_ERR,
			       "Cannot connect to local RIGOR node.\n"
			       "Please start rigor-wallet or rigord first.\n"
			       "Retrying in %d seconds...",
			       opt_fail_pause);
		} else {
			applog(LOG_ERR, "json_rpc_call failed, retry after %d seconds",
				opt_fail_pause);
		}
		sleep(opt_fail_pause);
	}

	/* send work to requesting thread */
	if (!tq_push(wc->thr->q, ret_work))
		work_discard(ret_work);

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

	/* Move returned work into caller storage. dest must be zeroed. */
	work_free(work);
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

	wc->u.work = calloc(1, sizeof(*work_in));
	if (!wc->u.work)
		goto err_out;

	wc->cmd = WC_SUBMIT_WORK;
	wc->thr = thr;
	if (!work_copy(wc->u.work, work_in))
		goto err_out;

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
		time_t scan_ref_time;
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
				struct work fresh;
				memset(&fresh, 0, sizeof(fresh));
				/* Do not hold g_work_lock across get_work: workio submit
				 * may take the same lock to invalidate the stale template. */
				pthread_mutex_unlock(&g_work_lock);
				if (unlikely(!get_work(mythr, &fresh))) {
					applog(LOG_ERR, "work retrieval failed, exiting "
						"mining thread %d", mythr->id);
					goto out;
				}
				pthread_mutex_lock(&g_work_lock);
				work_free(&g_work);
				memcpy(&g_work, &fresh, sizeof(fresh));
				memset(&fresh, 0, sizeof(fresh));
				g_work_time = have_stratum ? 0 : time(NULL);
			}
			if (have_stratum) {
				pthread_mutex_unlock(&g_work_lock);
				continue;
			}
		}
		if (memcmp(work.data, g_work.data, 76)) {
			work_free(&work);
			if (!work_copy(&work, &g_work)) {
				pthread_mutex_unlock(&g_work_lock);
				applog(LOG_ERR, "work_copy failed, retrying");
				sleep(1);
				continue;
			}
			work.data[19] = 0xffffffffU / opt_n_threads * thr_id;
		} else
			work.data[19]++;
		scan_ref_time = g_work_time;
		pthread_mutex_unlock(&g_work_lock);
		work_restart[thr_id].restart = 0;
		
		/* adjust max_nonce to meet target scan time */
		if (have_stratum)
			max64 = LP_SCANTIME;
		else
			max64 = scan_ref_time + (have_longpoll ? LP_SCANTIME : opt_scantime)
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
		if (opt_verbose && !opt_quiet) {
			sprintf(s, thr_hashrates[thr_id] >= 1e6 ? "%.0f" : "%.2f",
				1e-3 * thr_hashrates[thr_id]);
			applog(LOG_INFO, "thread %d: %lu hashes, %s khash/s",
				thr_id, hashes_done, s);
		} else if (!opt_verbose && thr_id == 0) {
			user_print_status_throttled(15);
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
		if (rc && !opt_benchmark) {
			if (!submit_work(mythr, &work))
				break;
			/* Pace solo mining so the GUI node is not flooded (easy Alpha PoW). */
			if (opt_submit_delay > 0)
				sleep(opt_submit_delay);
			/* Stop grinding this header. workio may also invalidate, but
			 * this thread would otherwise clear work_restart and scan ~2M
			 * leftover hashes before the next GBT. */
			if (!have_stratum) {
				solo_invalidate_gbt();
				memset(work.data, 0, 76);
			}
		}
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

static void solo_invalidate_gbt(void)
{
	pthread_mutex_lock(&g_work_lock);
	g_work_time = 0;
	pthread_mutex_unlock(&g_work_lock);
	restart_threads();
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

	if (opt_verbose)
		applog(LOG_INFO, "Long-polling activated for %s", lp_url);

	while (1) {
		json_t *val, *res, *soval;
		char *req = NULL;
		int err;

		{
			char *lp_copy;

			pthread_mutex_lock(&lp_id_lock);
			if (!lp_id) {
				pthread_mutex_unlock(&lp_id_lock);
				applog(LOG_ERR, "GBT longpoll missing longpollid");
				goto out;
			}
			lp_copy = strdup(lp_id);
			pthread_mutex_unlock(&lp_id_lock);
			if (!lp_copy)
				goto out;
			req = gbt_request_json(lp_copy);
			free(lp_copy);
			if (!req)
				goto out;
		}
		val = json_rpc_call(curl, lp_url, rpc_userpass, req, &err,
				    JSON_RPC_LONGPOLL);
		free(req);
		if (have_stratum) {
			if (val)
				json_decref(val);
			goto out;
		}
		if (likely(val)) {
			bool rc;
			static uint32_t s_lp_prev[8];
			static int s_lp_height = -1;
			static int s_lp_same;
			res = json_object_get(val, "result");
			soval = json_object_get(res, "submitold");
			submit_old = soval ? json_is_true(soval) : false;
			pthread_mutex_lock(&g_work_lock);
			work_free(&g_work);
			rc = gbt_work_decode(res, &g_work);
			if (rc) {
				int same = (s_lp_height == g_work.height &&
					    !memcmp(s_lp_prev, g_work.data, 32));
				memcpy(s_lp_prev, g_work.data, 32);
				s_lp_height = g_work.height;
				if (same) {
					/*
					 * Same tip+height: node longpoll woke for template refresh
					 * (ASERT/UpdateTime nBits, mempool tick) — not a new block.
					 * Brief backoff avoids busy-spin; mining continues on current work.
					 * Unrelated to ORU Convergence; expected between accepted blocks.
					 */
					if (++s_lp_same == 1) {
						if (opt_verbose)
							applog(LOG_INFO, "LONGPOLL same tip (nBits/time refresh); backing off 2s");
						else {
							snprintf(ux_status_line, sizeof(ux_status_line),
								 "Waiting for new block template");
							/* Status text updates on next heartbeat; avoid spam. */
						}
					} else if ((s_lp_same % 30) == 0) {
						if (opt_verbose)
							applog(LOG_INFO, "LONGPOLL still same tip x%d (normal while waiting for next block)", s_lp_same);
					}
					pthread_mutex_unlock(&g_work_lock);
					json_decref(val);
					sleep(2);
					continue;
				}
				s_lp_same = 0;
				if (opt_verbose)
					applog(LOG_INFO, "LONGPOLL pushed new work");
				else {
					ux_note_work(&g_work);
					snprintf(ux_status_line, sizeof(ux_status_line), "Mining normally");
				}
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
		err_val ? json_string_value(json_array_get(err_val, 1)) : NULL,
		0, 0);

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
	if (opt_verbose)
		applog(LOG_INFO, "Starting Stratum on %s", stratum.url);
	else {
		snprintf(ux_status_line, sizeof(ux_status_line), "Connecting to pool");
		user_print_status_throttled(5);
	}

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
		if (!strcmp(arg, "rgrpow01") || !strcmp(arg, "sha3") || !strcmp(arg, "sha3-256")) {
			opt_algo = ALGO_SHA256D;
			break;
		}
		for (i = 0; i < ARRAY_SIZE(algo_names); i++) {
			v = strlen(algo_names[i]);
			if (!strncmp(arg, algo_names[i], v)) {
				if (arg[v] == '\0') {
					if (i == ALGO_SCRYPT) {
						stderr_msg("%s: scrypt is not a RIGOR SOLO algorithm; use -a rgrpow01\n",
							pname);
						show_usage_and_exit(1);
					}
					opt_algo = i;
					break;
				}
				if (arg[v] == ':' && i == ALGO_SCRYPT) {
					stderr_msg("%s: scrypt is not a RIGOR SOLO algorithm; use -a rgrpow01\n",
						pname);
					show_usage_and_exit(1);
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
	case 'v':
		opt_verbose = true;
		break;
	case 'D':
		opt_debug = true;
		opt_verbose = true;
		break;
	case 1016:
		opt_debug_pow = true;
		break;
	case 1017:			/* --submit-delay */
		v = atoi(arg);
		if (v < 0 || v > 3600)
			show_usage_and_exit(1);
		opt_submit_delay = v;
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
		if (have_stratum) {
			stderr_msg("%s: Stratum is not the RIGOR SOLO path (see P042). Use HTTP getblocktemplate.\n",
				pname);
			show_usage_and_exit(1);
		}
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
		stderr_msg("%s: --no-gbt is refused; GBT is the only RIGOR SOLO path\n", pname);
		show_usage_and_exit(1);
		break;
	case 1013:			/* --coinbase-addr */
		pk_script_size = address_to_script(pk_script, sizeof(pk_script), arg);
		if (!pk_script_size || !gbt_solo_payout_is_oru_v0(pk_script, pk_script_size)) {
			stderr_msg("%s: invalid ORU witness-v0 address -- '%s'\n",
				pname, arg);
			show_usage_and_exit(1);
		}
		free(coinbase_addr);
		coinbase_addr = strdup(arg);
		break;
	case 1015:			/* --coinbase-sig */
		applog(LOG_WARNING, "ignoring --coinbase-sig: coinbase scriptSig is node-immutable");
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
	if (!opt_benchmark &&
	    (!pk_script_size || !gbt_solo_payout_is_oru_v0(pk_script, pk_script_size))) {
		stderr_msg("%s: SOLO requires --coinbase-addr=<ORU witness v0 (rg1q... / rgrt1q...)>\n",
			argv[0]);
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
	if (!rgrpow_selftest()) {
		applog(LOG_ERR, "RGRPOW01 self-test failed. SHA3-256 digest mismatch.");
		return 1;
	}
	if (opt_verbose)
		applog(LOG_INFO, "RGRPOW01 self-test passed.");

	{
		unsigned char rigor_magic[4];
		rigor_testnet_message_start_fill(rigor_magic);
		if (opt_verbose)
			applog(LOG_INFO,
			       "Rigor testnet P2P magic (pchMessageStart): %02x%02x%02x%02x - "
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

	if (!opt_verbose) {
		snprintf(ux_status_line, sizeof(ux_status_line), "Connecting to node");
		user_print_status();
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
	int auto_thread_reserve = 0;
	if (!opt_n_threads) {
		/* Taking every core starves the rigord instance this miner depends on:
		 * its RPC and P2P threads share the machine, so the symptom is a frozen
		 * wallet and dropped peers rather than a slow miner. Leave headroom by
		 * default; -t still takes the whole machine when that is wanted. */
		auto_thread_reserve = num_processors >= 5 ? 2 : (num_processors >= 2 ? 1 : 0);
		opt_n_threads = num_processors - auto_thread_reserve;
		if (opt_n_threads < 1)
			opt_n_threads = 1;
	}

#ifdef HAVE_SYSLOG_H
	if (use_syslog)
		openlog("cpuminer", LOG_PID, LOG_USER);
#endif

	if (auto_thread_reserve)
		applog(LOG_INFO, "Mining with %d of %d processors, %d reserved for the wallet (use -t to override)",
		       opt_n_threads, num_processors, auto_thread_reserve);

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

	ux_mining_running = true;
	if (opt_verbose) {
		if (opt_algo == ALGO_SHA256D) {
			applog(LOG_INFO, "%d miner threads started, using SHA3-256 RGRPOW01 "
				"(CLI -a rgrpow01 / sha256d).",
				opt_n_threads);
		} else {
			applog(LOG_INFO, "%d miner threads started, using '%s' algorithm.",
				opt_n_threads, algo_names[opt_algo]);
		}
	} else {
		snprintf(ux_status_line, sizeof(ux_status_line),
			 ux_node_connected ? "Mining normally" : "Connecting to node");
		/* Full board already printed at startup / first work; avoid duplicate. */
	}

	/* main loop - simply wait for workio thread to exit */
	pthread_join(thr_info[work_thr_id].pth, NULL);

	if (opt_verbose)
		applog(LOG_INFO, "workio thread dead, exiting.");
	else
		stderr_msg("Miner stopped.\n");

	return 0;
}
