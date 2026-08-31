#include "gbt_solo.h"

#include "arith_uint256.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void sha256d(unsigned char *hash, const unsigned char *data, int len);

static uint32_t le32dec_local(const void *pp)
{
	const uint8_t *p = (const uint8_t *)pp;
	return ((uint32_t)p[0]) | ((uint32_t)p[1] << 8) |
	       ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static void bin2hex_local(char *s, const uint8_t *p, int len)
{
	static const char hex[16] = "0123456789abcdef";
	int i;
	for (i = 0; i < len; i++) {
		s[2 * i] = hex[(p[i] >> 4) & 0xf];
		s[2 * i + 1] = hex[p[i] & 0xf];
	}
	s[2 * len] = 0;
}

static void fail_msg(char *err, size_t errlen, const char *msg)
{
	if (err && errlen)
		snprintf(err, errlen, "%s", msg);
}

static int hex_nibble(int c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'a' && c <= 'f')
		return c - 'a' + 10;
	if (c >= 'A' && c <= 'F')
		return c - 'A' + 10;
	return -1;
}

static bool decode_hex(uint8_t *out, int out_len, const char *hex)
{
	int i;

	if (!hex || (int)strlen(hex) != out_len * 2)
		return false;
	for (i = 0; i < out_len; i++) {
		int hi = hex_nibble((unsigned char)hex[2 * i]);
		int lo = hex_nibble((unsigned char)hex[2 * i + 1]);
		if (hi < 0 || lo < 0)
			return false;
		out[i] = (uint8_t)((hi << 4) | lo);
	}
	return true;
}

static bool decode_hex_alloc(uint8_t **out, int *out_len, const char *hex)
{
	int n;

	if (!hex)
		return false;
	n = (int)strlen(hex);
	if (n < 2 || (n & 1))
		return false;
	*out_len = n / 2;
	*out = malloc((size_t)*out_len);
	if (!*out)
		return false;
	if (!decode_hex(*out, *out_len, hex)) {
		free(*out);
		*out = NULL;
		return false;
	}
	return true;
}

static void memrev_local(uint8_t *p, int len)
{
	int i;
	for (i = 0; i < len / 2; i++) {
		uint8_t t = p[i];
		p[i] = p[len - 1 - i];
		p[len - 1 - i] = t;
	}
}

static bool all_zero(const uint8_t *p, int len)
{
	int i;
	for (i = 0; i < len; i++) {
		if (p[i])
			return false;
	}
	return true;
}

static int write_compact_size(uint8_t *buf, int cap, int off, uint64_t n)
{
	if (n < 0xfd) {
		if (off + 1 > cap)
			return -1;
		buf[off] = (uint8_t)n;
		return off + 1;
	}
	if (n <= 0xffff) {
		if (off + 3 > cap)
			return -1;
		buf[off] = 0xfd;
		buf[off + 1] = (uint8_t)n;
		buf[off + 2] = (uint8_t)(n >> 8);
		return off + 3;
	}
	return -1;
}

/*
 * Integer-only. Amounts must never round-trip through double, and the node
 * emits every one of these fields as a JSON integer.
 */
static int64_t json_int64_field(const json_t *obj, const char *key, bool *ok)
{
	json_t *v = json_object_get(obj, key);

	*ok = false;
	if (!v || !json_is_integer(v))
		return 0;
	*ok = true;
	return (int64_t)json_integer_value(v);
}

bool gbt_solo_payout_is_oru_v0(const uint8_t *spk, int len)
{
	return spk && len == GBT_SOLO_PAYOUT_V0_LEN && spk[0] == 0x00 && spk[1] == 0x14;
}

void gbt_solo_out_free(struct gbt_solo_out *out)
{
	if (!out)
		return;
	free(out->txs_hex);
	free(out->workid);
	out->txs_hex = NULL;
	out->workid = NULL;
}

static bool parse_scriptsig_three_pushes(const uint8_t *s, int len,
					 uint8_t root[32], uint8_t winner[32],
					 char *err, size_t errlen)
{
	int off = 0;
	int push_i = 0;
	uint8_t pushes[3][32];
	int push_len[3] = {0, 0, 0};

	if (len < 1 || len > GBT_SOLO_SCRIPTSIG_MAX) {
		fail_msg(err, errlen, "default_coinbase_scriptsig length invalid");
		return false;
	}

	while (off < len) {
		uint8_t op;
		int n;
		const uint8_t *payload;
		int payload_len;

		if (push_i >= 3) {
			fail_msg(err, errlen, "default_coinbase_scriptsig trailing bytes");
			return false;
		}
		op = s[off++];
		if (op == 0x00) {
			payload = NULL;
			payload_len = 0;
		} else if (op >= 0x51 && op <= 0x60) {
			pushes[push_i][0] = (uint8_t)(op - 0x50);
			push_len[push_i] = 1;
			push_i++;
			continue;
		} else if (op <= 75) {
			n = op;
			if (off + n > len) {
				fail_msg(err, errlen, "default_coinbase_scriptsig truncated push");
				return false;
			}
			payload = s + off;
			payload_len = n;
			off += n;
		} else {
			fail_msg(err, errlen, "default_coinbase_scriptsig non-canonical opcode");
			return false;
		}
		if (payload_len > 32) {
			fail_msg(err, errlen, "default_coinbase_scriptsig push too large");
			return false;
		}
		if (payload_len)
			memcpy(pushes[push_i], payload, (size_t)payload_len);
		push_len[push_i] = payload_len;
		push_i++;
	}

	if (push_i != 3) {
		fail_msg(err, errlen, "default_coinbase_scriptsig must be height|C|paid-winner");
		return false;
	}
	if (push_len[1] != 32 || push_len[2] != 32) {
		fail_msg(err, errlen, "default_coinbase_scriptsig C/paid-winner not 32 bytes");
		return false;
	}
	memcpy(root, pushes[1], 32);
	memcpy(winner, pushes[2], 32);
	(void)root;
	return true;
}

static void merkle_hash_level(uint8_t (*tree)[32], int *n)
{
	int i;
	int nn = *n;

	if (nn % 2) {
		memcpy(tree[nn], tree[nn - 1], 32);
		nn++;
	}
	nn /= 2;
	for (i = 0; i < nn; i++) {
		uint8_t cat[64];
		memcpy(cat, tree[2 * i], 32);
		memcpy(cat + 32, tree[2 * i + 1], 32);
		sha256d(tree[i], cat, 64);
	}
	*n = nn;
}

static void merkle_root_from_leaves(uint8_t (*leaves)[32], int count, uint8_t out[32])
{
	int n = count;
	if (n <= 0) {
		memset(out, 0, 32);
		return;
	}
	while (n > 1)
		merkle_hash_level(leaves, &n);
	memcpy(out, leaves[0], 32);
}

static int read_compact_size(const uint8_t *p, int len, int off, uint64_t *n)
{
	if (off < 0 || off >= len)
		return -1;
	if (p[off] < 0xfd) {
		*n = p[off];
		return off + 1;
	}
	if (p[off] == 0xfd && off + 3 <= len) {
		*n = (uint64_t)p[off + 1] | ((uint64_t)p[off + 2] << 8);
		return off + 3;
	}
	return -1;
}

/* Structural bounds for a RIGOR coinbase. Anything outside is not a SOLO template. */
#define GBT_SOLO_MAX_VOUT 8
#define GBT_SOLO_SPK_MAX 256
#define GBT_SOLO_MAX_WITNESS_ITEMS 4

struct gbt_cb_span {
	int off;
	int len;
};

struct gbt_cb_parsed {
	uint32_t version;
	uint32_t sequence;
	uint32_t locktime;
	struct gbt_cb_span scriptsig;
	int vout_count;
	int64_t vout_value[GBT_SOLO_MAX_VOUT];
	struct gbt_cb_span vout_spk[GBT_SOLO_MAX_VOUT];
	int witness_items;
	struct gbt_cb_span witness_item[GBT_SOLO_MAX_WITNESS_ITEMS];
	uint8_t txid[32];
};

/* Compact size that must be minimally encoded and within a declared bound. */
static int read_compact_size_checked(const uint8_t *p, int len, int off,
				     uint64_t max, uint64_t *n)
{
	int after = read_compact_size(p, len, off, n);

	if (after < 0 || *n > max)
		return -1;
	if (p[off] == 0xfd && *n < 0xfd)
		return -1;
	return after;
}

static int64_t read_i64_le(const uint8_t *p)
{
	uint64_t v = 0;
	int i;

	for (i = 7; i >= 0; i--)
		v = (v << 8) | p[i];
	return (int64_t)v;
}

/*
 * Structural parse of the node's canonical witness coinbase.
 *
 * Records byte spans rather than re-encoding, so the txid is SHA256d over an
 * exact slice of the node's own bytes. The miner locates consensus fields only
 * so that ownership and amounts can be verified; it never rebuilds them.
 */
static bool parse_coinbase(const uint8_t *tx, int len, struct gbt_cb_parsed *p,
			   char *err, size_t errlen)
{
	uint8_t base[GBT_SOLO_COINBASE_MAX];
	uint64_t count = 0;
	int off, vout_end, base_len, i;

	memset(p, 0, sizeof(*p));
	if (len < 64 || len > GBT_SOLO_COINBASE_MAX) {
		fail_msg(err, errlen, "default_coinbase_tx length out of range");
		return false;
	}
	p->version = le32dec_local(tx);
	/* BIP141 marker/flag is mandatory: the coinbase always carries the
	 * reserved witness value the commitment is computed over. */
	if (tx[4] != 0x00 || tx[5] != 0x01) {
		fail_msg(err, errlen, "default_coinbase_tx is not witness-serialized");
		return false;
	}
	off = read_compact_size_checked(tx, len, 6, 1, &count);
	if (off < 0 || count != 1) {
		fail_msg(err, errlen, "coinbase must have exactly one input");
		return false;
	}
	if (off + 36 > len || !all_zero(tx + off, 32) ||
	    le32dec_local(tx + off + 32) != 0xffffffffu) {
		fail_msg(err, errlen, "coinbase input is not a null prevout");
		return false;
	}
	off += 36;
	off = read_compact_size_checked(tx, len, off, GBT_SOLO_SCRIPTSIG_MAX, &count);
	if (off < 0 || off + (int)count > len) {
		fail_msg(err, errlen, "coinbase scriptSig length invalid");
		return false;
	}
	p->scriptsig.off = off;
	p->scriptsig.len = (int)count;
	off += (int)count;
	if (off + 4 > len) {
		fail_msg(err, errlen, "coinbase input truncated");
		return false;
	}
	p->sequence = le32dec_local(tx + off);
	off += 4;

	off = read_compact_size_checked(tx, len, off, GBT_SOLO_MAX_VOUT, &count);
	if (off < 0 || count < 1) {
		fail_msg(err, errlen, "coinbase output count invalid");
		return false;
	}
	p->vout_count = (int)count;
	for (i = 0; i < p->vout_count; i++) {
		uint64_t slen = 0;
		if (off + 8 > len) {
			fail_msg(err, errlen, "coinbase output truncated");
			return false;
		}
		p->vout_value[i] = read_i64_le(tx + off);
		if (p->vout_value[i] < 0) {
			fail_msg(err, errlen, "coinbase output value is negative");
			return false;
		}
		off += 8;
		off = read_compact_size_checked(tx, len, off, GBT_SOLO_SPK_MAX, &slen);
		if (off < 0 || off + (int)slen > len) {
			fail_msg(err, errlen, "coinbase scriptPubKey length invalid");
			return false;
		}
		p->vout_spk[i].off = off;
		p->vout_spk[i].len = (int)slen;
		off += (int)slen;
	}
	vout_end = off;

	off = read_compact_size_checked(tx, len, off, GBT_SOLO_MAX_WITNESS_ITEMS, &count);
	if (off < 0) {
		fail_msg(err, errlen, "coinbase witness stack invalid");
		return false;
	}
	p->witness_items = (int)count;
	for (i = 0; i < p->witness_items; i++) {
		uint64_t ilen = 0;
		off = read_compact_size_checked(tx, len, off, GBT_SOLO_SPK_MAX, &ilen);
		if (off < 0 || off + (int)ilen > len) {
			fail_msg(err, errlen, "coinbase witness item invalid");
			return false;
		}
		p->witness_item[i].off = off;
		p->witness_item[i].len = (int)ilen;
		off += (int)ilen;
	}
	if (off + 4 != len) {
		fail_msg(err, errlen, "default_coinbase_tx has trailing bytes");
		return false;
	}
	p->locktime = le32dec_local(tx + off);

	/* txid = SHA256d over the node's own bytes minus marker/flag and witness. */
	base_len = 4 + (vout_end - 6) + 4;
	if (base_len < 12 || base_len > (int)sizeof(base)) {
		fail_msg(err, errlen, "coinbase txid buffer overflow");
		return false;
	}
	memcpy(base, tx, 4);
	memcpy(base + 4, tx + 6, (size_t)(vout_end - 6));
	memcpy(base + 4 + (vout_end - 6), tx + len - 4, 4);
	sha256d(p->txid, base, base_len);
	return true;
}

/*
 * Ownership and amount verification against the canonical GBT fields.
 *
 * Every consensus byte still comes from the node; this only proves the node
 * honored the requested payout and did not drop or duplicate a mandatory
 * output before the miner spends work on it.
 */
static bool verify_canonical_coinbase(const uint8_t *cb, int cb_len,
				      const uint8_t *scriptsig, int scriptsig_len,
				      const uint8_t *payout_spk, int payout_len,
				      int64_t cbvalue,
				      const uint8_t *wit_commit,
				      bool has_mint, const uint8_t *mint_spk,
				      int mint_spk_len, int64_t mint_value,
				      struct gbt_cb_parsed *p,
				      char *err, size_t errlen)
{
	int commit_count = 0, mint_count = 0, commit_i = -1, mint_i = -1, i;

	if (!parse_coinbase(cb, cb_len, p, err, errlen))
		return false;
	if (p->version != 2) {
		fail_msg(err, errlen, "coinbase nVersion must be 2");
		return false;
	}
	if (p->sequence != 0xffffffffu) {
		fail_msg(err, errlen, "coinbase nSequence must be final");
		return false;
	}
	if (p->locktime != 0) {
		fail_msg(err, errlen, "coinbase nLockTime must be 0");
		return false;
	}
	if (p->scriptsig.len != scriptsig_len ||
	    memcmp(cb + p->scriptsig.off, scriptsig, (size_t)scriptsig_len) != 0) {
		fail_msg(err, errlen, "coinbase scriptSig != default_coinbase_scriptsig");
		return false;
	}
	if (p->vout_spk[0].len != payout_len ||
	    memcmp(cb + p->vout_spk[0].off, payout_spk, (size_t)payout_len) != 0) {
		fail_msg(err, errlen, "coinbase vout[0] does not pay the requested address");
		return false;
	}
	if (p->vout_value[0] != cbvalue) {
		fail_msg(err, errlen, "coinbase vout[0] value != coinbasevalue");
		return false;
	}
	for (i = 0; i < p->vout_count; i++) {
		const uint8_t *spk = cb + p->vout_spk[i].off;
		const int slen = p->vout_spk[i].len;

		if (slen == GBT_SOLO_WITNESS_COMMITMENT_LEN &&
		    memcmp(spk, wit_commit, GBT_SOLO_WITNESS_COMMITMENT_LEN) == 0) {
			commit_count++;
			commit_i = i;
		} else if (slen > 0 && spk[slen - 1] == 0xbb) {
			mint_count++;
			mint_i = i;
		}
	}
	if (commit_count != 1) {
		fail_msg(err, errlen, "coinbase must carry exactly one default_witness_commitment");
		return false;
	}
	if (p->vout_value[commit_i] != 0) {
		fail_msg(err, errlen, "witness commitment output value must be 0");
		return false;
	}
	if (has_mint) {
		if (mint_count != 1) {
			fail_msg(err, errlen, "coinbase dropped default_res_mint");
			return false;
		}
		if (p->vout_spk[mint_i].len != mint_spk_len ||
		    memcmp(cb + p->vout_spk[mint_i].off, mint_spk, (size_t)mint_spk_len) != 0) {
			fail_msg(err, errlen, "coinbase mint != default_res_mint.scriptPubKey");
			return false;
		}
		if (p->vout_value[mint_i] != mint_value) {
			fail_msg(err, errlen, "coinbase mint value != default_res_mint.value");
			return false;
		}
	} else if (mint_count != 0) {
		fail_msg(err, errlen, "coinbase mints RES without a paid winner");
		return false;
	}
	/* Payout, optional mint, witness commitment. A RES bootstrap output
	 * (devtest -reswalletbootstrap) is not a SOLO template. */
	if (p->vout_count != 2 + (has_mint ? 1 : 0)) {
		fail_msg(err, errlen, "unexpected coinbase output count");
		return false;
	}
	if (p->witness_items != 1 || p->witness_item[0].len != 32 ||
	    !all_zero(cb + p->witness_item[0].off, 32)) {
		fail_msg(err, errlen, "coinbase witness must be one 32-byte zero reserved value");
		return false;
	}
	return true;
}

bool gbt_solo_assemble(const json_t *gbt,
		       const uint8_t *payout_spk, int payout_len,
		       struct gbt_solo_out *out,
		       char *err, size_t errlen)
{
	json_t *tmp, *txa, *rules, *mint;
	const char *hex;
	uint8_t prev_display[32], prev_wire[32];
	uint8_t scriptsig_winner[32], scriptsig_root[32];
	uint8_t paid_display[32], paid_native[32];
	uint8_t wit_commit_gbt[GBT_SOLO_WITNESS_COMMITMENT_LEN];
	uint8_t wit_commit_calc[GBT_SOLO_WITNESS_COMMITMENT_LEN];
	uint8_t reserved[32];
	int tx_count, i, n;
	bool ok;
	int64_t height64, version64, curtime64, cbvalue;
	uint32_t version, curtime, bits;
	bool has_mint = false;
	int64_t mint_value = 0;
	uint8_t mint_spk[GBT_SOLO_MINT_SPK_MAX];
	int mint_spk_len = 0;
	uint8_t (*merkle)[32] = NULL;
	uint8_t (*wtree)[32] = NULL;
	char *txs_end;
	int tx_size = 0;

	memset(out, 0, sizeof(*out));
	memset(reserved, 0, sizeof(reserved));

	if (!gbt_solo_payout_is_oru_v0(payout_spk, payout_len)) {
		fail_msg(err, errlen, "payout script must be ORU witness v0 P2WPKH");
		return false;
	}
	if (!gbt || !json_is_object(gbt)) {
		fail_msg(err, errlen, "GBT result is not an object");
		return false;
	}

	rules = json_object_get(gbt, "rules");
	if (!rules || !json_is_array(rules)) {
		fail_msg(err, errlen, "GBT missing rules");
		return false;
	}
	ok = false;
	for (i = 0; i < (int)json_array_size(rules); i++) {
		const char *r = json_string_value(json_array_get(rules, i));
		if (r && (!strcmp(r, "segwit") || !strcmp(r, "!segwit")))
			ok = true;
	}
	if (!ok) {
		fail_msg(err, errlen, "GBT rules must include segwit");
		return false;
	}

	tmp = json_object_get(gbt, "rigor_coinbase_immutable");
	if (!tmp || !json_is_true(tmp)) {
		fail_msg(err, errlen, "missing rigor_coinbase_immutable=true");
		return false;
	}

	height64 = json_int64_field(gbt, "height", &ok);
	if (!ok || height64 < 1 || height64 > 0x7fffffff) {
		fail_msg(err, errlen, "GBT invalid height");
		return false;
	}
	version64 = json_int64_field(gbt, "version", &ok);
	if (!ok) {
		fail_msg(err, errlen, "GBT invalid version");
		return false;
	}
	curtime64 = json_int64_field(gbt, "curtime", &ok);
	if (!ok) {
		fail_msg(err, errlen, "GBT invalid curtime");
		return false;
	}
	cbvalue = json_int64_field(gbt, "coinbasevalue", &ok);
	if (!ok || cbvalue < 0) {
		fail_msg(err, errlen, "GBT invalid coinbasevalue");
		return false;
	}
	version = (uint32_t)version64;
	curtime = (uint32_t)curtime64;

	{
		int64_t mintime, maxtime;
		mintime = json_int64_field(gbt, "mintime", &ok);
		if (!ok) {
			fail_msg(err, errlen, "missing mintime");
			return false;
		}
		maxtime = json_int64_field(gbt, "maxtime", &ok);
		if (!ok) {
			fail_msg(err, errlen, "missing maxtime");
			return false;
		}
		if (curtime64 < mintime || curtime64 > maxtime) {
			fail_msg(err, errlen, "curtime outside [mintime, maxtime]");
			return false;
		}
	}

	tmp = json_object_get(gbt, "mutable");
	if (tmp) {
		if (!json_is_array(tmp)) {
			fail_msg(err, errlen, "GBT mutable is not an array");
			return false;
		}
		for (i = 0; i < (int)json_array_size(tmp); i++) {
			const char *m = json_string_value(json_array_get(tmp, i));
			if (!m || strcmp(m, "time") != 0) {
				fail_msg(err, errlen, "GBT mutable may only contain time");
				return false;
			}
		}
	}

	hex = json_string_value(json_object_get(gbt, "bits"));
	{
		uint8_t bits_be[4];
		/* Strict 8-digit hex; sscanf("%x") would accept signs and padding. */
		if (!hex || !decode_hex(bits_be, 4, hex)) {
			fail_msg(err, errlen, "GBT invalid bits");
			return false;
		}
		bits = ((uint32_t)bits_be[0] << 24) | ((uint32_t)bits_be[1] << 16) |
		       ((uint32_t)bits_be[2] << 8) | (uint32_t)bits_be[3];
	}

	hex = json_string_value(json_object_get(gbt, "previousblockhash"));
	if (!hex || !decode_hex(prev_display, 32, hex)) {
		fail_msg(err, errlen, "GBT invalid previousblockhash");
		return false;
	}
	memcpy(prev_wire, prev_display, 32);
	memrev_local(prev_wire, 32);

	hex = json_string_value(json_object_get(gbt, "paid_winner_txid"));
	if (!hex || !decode_hex(paid_display, 32, hex)) {
		fail_msg(err, errlen, "missing paid_winner_txid");
		return false;
	}
	memcpy(paid_native, paid_display, 32);
	memrev_local(paid_native, 32);

	hex = json_string_value(json_object_get(gbt, "default_coinbase_scriptsig"));
	if (!hex) {
		fail_msg(err, errlen, "missing default_coinbase_scriptsig");
		return false;
	}
	{
		uint8_t *ss = NULL;
		int ss_len = 0;
		if (!decode_hex_alloc(&ss, &ss_len, hex) || ss_len < 2 || ss_len > GBT_SOLO_SCRIPTSIG_MAX) {
			free(ss);
			fail_msg(err, errlen, "invalid default_coinbase_scriptsig");
			return false;
		}
		if (!parse_scriptsig_three_pushes(ss, ss_len, scriptsig_root, scriptsig_winner, err, errlen)) {
			free(ss);
			return false;
		}
		if (memcmp(scriptsig_winner, paid_native, 32) != 0) {
			free(ss);
			fail_msg(err, errlen, "paid_winner_txid does not match scriptSig");
			return false;
		}
		memcpy(out->scriptsig, ss, (size_t)ss_len);
		out->scriptsig_len = ss_len;
		free(ss);
	}

	mint = json_object_get(gbt, "default_res_mint");
	if (all_zero(paid_native, 32)) {
		if (mint && !json_is_null(mint)) {
			fail_msg(err, errlen, "default_res_mint present but paid_winner is zero");
			return false;
		}
	} else {
		json_t *mv, *ms;
		if (!mint || !json_is_object(mint)) {
			fail_msg(err, errlen, "missing default_res_mint for non-zero paid_winner");
			return false;
		}
		mv = json_object_get(mint, "value");
		ms = json_object_get(mint, "scriptPubKey");
		if (!mv || !ms || !json_is_string(ms)) {
			fail_msg(err, errlen, "invalid default_res_mint");
			return false;
		}
		if (json_is_integer(mv))
			mint_value = (int64_t)json_integer_value(mv);
		else if (json_is_real(mv))
			mint_value = (int64_t)json_real_value(mv);
		else {
			fail_msg(err, errlen, "invalid default_res_mint.value");
			return false;
		}
		if (mint_value <= 0) {
			fail_msg(err, errlen, "default_res_mint.value must be > 0");
			return false;
		}
		hex = json_string_value(ms);
		{
			uint8_t *spk = NULL;
			int slen = 0;
			if (!decode_hex_alloc(&spk, &slen, hex) || slen < 23 || slen > GBT_SOLO_MINT_SPK_MAX) {
				free(spk);
				fail_msg(err, errlen, "invalid default_res_mint.scriptPubKey");
				return false;
			}
			/* Canonical GetResMintScript never uses OP_1..OP_16 for maturity. */
			if (spk[0] >= 0x51 && spk[0] <= 0x60) {
				free(spk);
				fail_msg(err, errlen, "RES mint uses OP_n encoding; require OP_PUSHBYTES");
				return false;
			}
			if (spk[slen - 1] != 0xbb) {
				free(spk);
				fail_msg(err, errlen, "default_res_mint.scriptPubKey is not OP_RES_MINT");
				return false;
			}
			memcpy(mint_spk, spk, (size_t)slen);
			mint_spk_len = slen;
			free(spk);
		}
		has_mint = true;
	}

	hex = json_string_value(json_object_get(gbt, "default_witness_commitment"));
	if (!hex || !decode_hex(wit_commit_gbt, GBT_SOLO_WITNESS_COMMITMENT_LEN, hex)) {
		fail_msg(err, errlen, "missing or invalid default_witness_commitment");
		return false;
	}
	if (wit_commit_gbt[0] != 0x6a || wit_commit_gbt[1] != 0x24 ||
	    wit_commit_gbt[2] != 0xaa || wit_commit_gbt[3] != 0x21 ||
	    wit_commit_gbt[4] != 0xa9 || wit_commit_gbt[5] != 0xed) {
		fail_msg(err, errlen, "default_witness_commitment is not BIP141 header");
		return false;
	}

	txa = json_object_get(gbt, "transactions");
	if (!txa || !json_is_array(txa)) {
		fail_msg(err, errlen, "GBT invalid transactions");
		return false;
	}
	if (json_array_size(txa) > GBT_SOLO_MAX_TXS) {
		fail_msg(err, errlen, "GBT transaction count exceeds block limit");
		return false;
	}
	tx_count = (int)json_array_size(txa);
	for (i = 0; i < tx_count; i++) {
		const json_t *tx = json_array_get(txa, i);
		const char *td = json_string_value(json_object_get(tx, "data"));
		size_t td_len;
		if (!td || (strlen(td) & 1)) {
			fail_msg(err, errlen, "GBT invalid transactions[].data");
			return false;
		}
		td_len = strlen(td) / 2;
		/* Bound the block hex allocation; tx_size must not wrap. */
		if (td_len > GBT_SOLO_MAX_BLOCK_BYTES ||
		    (size_t)tx_size + td_len > GBT_SOLO_MAX_BLOCK_BYTES) {
			fail_msg(err, errlen, "GBT transactions exceed block size limit");
			return false;
		}
		tx_size += (int)td_len;
	}

	wtree = calloc((size_t)tx_count + 2, 32);
	if (!wtree) {
		fail_msg(err, errlen, "OOM");
		return false;
	}
	memset(wtree[0], 0, 32); /* coinbase wtxid is zero in the witness merkle */
	for (i = 0; i < tx_count; i++) {
		const json_t *tx = json_array_get(txa, i);
		const char *wh = json_string_value(json_object_get(tx, "hash"));
		if (!wh || !decode_hex(wtree[1 + i], 32, wh)) {
			free(wtree);
			fail_msg(err, errlen, "GBT invalid transactions[].hash");
			return false;
		}
		memrev_local(wtree[1 + i], 32);
	}
	n = tx_count + 1;
	merkle_root_from_leaves(wtree, n, wit_commit_calc);
	{
		uint8_t cat[64];
		uint8_t digest[32];
		memcpy(cat, wit_commit_calc, 32);
		memcpy(cat + 32, reserved, 32);
		sha256d(digest, cat, 64);
		memcpy(wit_commit_calc, wit_commit_gbt, 6);
		memcpy(wit_commit_calc + 6, digest, 32);
	}
	free(wtree);
	wtree = NULL;
	if (memcmp(wit_commit_calc, wit_commit_gbt, GBT_SOLO_WITNESS_COMMITMENT_LEN) != 0) {
		fail_msg(err, errlen, "recomputed witness commitment != GBT default_witness_commitment");
		return false;
	}

	/* COPY-BYTES: node CreateNewBlock coinbase. Do not rebuild outputs. */
	hex = json_string_value(json_object_get(gbt, "default_coinbase_tx"));
	if (!hex) {
		fail_msg(err, errlen, "missing default_coinbase_tx");
		return false;
	}
	{
		uint8_t *cb = NULL;
		int cb_len = 0;
		struct gbt_cb_parsed cbp;
		uint8_t txid_disp[32], txid_wire[32];

		if (!decode_hex_alloc(&cb, &cb_len, hex)) {
			fail_msg(err, errlen, "invalid default_coinbase_tx hex");
			return false;
		}
		if (!verify_canonical_coinbase(cb, cb_len,
					       out->scriptsig, out->scriptsig_len,
					       payout_spk, payout_len, cbvalue,
					       wit_commit_gbt,
					       has_mint, mint_spk, mint_spk_len, mint_value,
					       &cbp, err, errlen)) {
			free(cb);
			return false;
		}
		hex = json_string_value(json_object_get(gbt, "default_coinbase_txid"));
		if (!hex || !decode_hex(txid_disp, 32, hex)) {
			free(cb);
			fail_msg(err, errlen, "missing or invalid default_coinbase_txid");
			return false;
		}
		memcpy(txid_wire, txid_disp, 32);
		memrev_local(txid_wire, 32);
		if (memcmp(txid_wire, cbp.txid, 32) != 0) {
			free(cb);
			fail_msg(err, errlen, "default_coinbase_txid does not match default_coinbase_tx");
			return false;
		}
		memcpy(out->coinbase_wit, cb, (size_t)cb_len);
		out->coinbase_wit_len = cb_len;
		free(cb);
		merkle = malloc(32 * (size_t)((1 + tx_count + 1) & ~1));
		if (!merkle) {
			fail_msg(err, errlen, "OOM");
			return false;
		}
		memcpy(merkle[0], cbp.txid, 32);
	}
	for (i = 0; i < tx_count; i++) {
		const json_t *tx = json_array_get(txa, i);
		const char *txid = json_string_value(json_object_get(tx, "txid"));
		if (!txid || !decode_hex(merkle[1 + i], 32, txid)) {
			free(merkle);
			fail_msg(err, errlen, "GBT invalid transactions[].txid");
			return false;
		}
		memrev_local(merkle[1 + i], 32);
	}
	n = 1 + tx_count;
	merkle_root_from_leaves(merkle, n, out->merkle_root);
	free(merkle);
	{
		uint8_t mr_disp[32], mr_wire[32];
		hex = json_string_value(json_object_get(gbt, "merkleroot"));
		if (!hex || !decode_hex(mr_disp, 32, hex)) {
			gbt_solo_out_free(out);
			fail_msg(err, errlen, "missing or invalid merkleroot");
			return false;
		}
		memcpy(mr_wire, mr_disp, 32);
		memrev_local(mr_wire, 32);
		if (memcmp(mr_wire, out->merkle_root, 32) != 0) {
			gbt_solo_out_free(out);
			fail_msg(err, errlen, "miner merkle root != GBT merkleroot");
			return false;
		}
	}

	{
		uint8_t txc_vi[9];
		int vn = write_compact_size(txc_vi, (int)sizeof(txc_vi), 0, (uint64_t)(1 + tx_count));
		if (vn < 0) {
			fail_msg(err, errlen, "tx count compact-size overflow");
			return false;
		}
		out->txs_hex = malloc((size_t)(2 * (vn + out->coinbase_wit_len + tx_size) + 1));
		if (!out->txs_hex) {
			fail_msg(err, errlen, "OOM");
			return false;
		}
		bin2hex_local(out->txs_hex, txc_vi, vn);
		bin2hex_local(out->txs_hex + 2 * vn, out->coinbase_wit, out->coinbase_wit_len);
		txs_end = out->txs_hex + strlen(out->txs_hex);
		for (i = 0; i < tx_count; i++) {
			const json_t *tx = json_array_get(txa, i);
			const char *td = json_string_value(json_object_get(tx, "data"));
			strcpy(txs_end, td);
			txs_end += strlen(td);
		}
	}

	out->header_words[0] = version;
	memcpy(out->header_words + 1, prev_wire, 32);
	for (i = 0; i < 8; i++)
		out->header_words[9 + i] = le32dec_local(out->merkle_root + 4 * i);
	out->header_words[17] = curtime;
	out->header_words[18] = bits;
	out->header_words[19] = 0;

	{
		bool fNegative, fOverflow;
		arith_uint256 arith_target;
		arith_uint256_set_compact(&arith_target, bits, &fNegative, &fOverflow);
		if (fNegative || fOverflow) {
			gbt_solo_out_free(out);
			fail_msg(err, errlen, "invalid nBits compact target");
			return false;
		}
		for (i = 0; i < 8; i++)
			out->target[i] = arith_target.data[i];
		{
			char target_hex[65];
			const char *gbt_target;
			int ti;
			arith_uint256_to_hex(&arith_target, target_hex);
			gbt_target = json_string_value(json_object_get(gbt, "target"));
			if (!gbt_target || strlen(gbt_target) != 64) {
				gbt_solo_out_free(out);
				fail_msg(err, errlen, "missing or invalid GBT target");
				return false;
			}
			for (ti = 0; ti < 64; ti++) {
				char a = gbt_target[ti];
				if (a >= 'A' && a <= 'F')
					a = (char)(a - 'A' + 'a');
				if (a != target_hex[ti]) {
					gbt_solo_out_free(out);
					fail_msg(err, errlen, "GBT target does not match bits");
					return false;
				}
			}
		}
	}

	tmp = json_object_get(gbt, "convergence_root");
	if (!tmp || !json_is_string(tmp)) {
		gbt_solo_out_free(out);
		fail_msg(err, errlen, "missing convergence_root");
		return false;
	}
	{
		uint8_t root_display[32], root_native[32];
		if (!decode_hex(root_display, 32, json_string_value(tmp))) {
			gbt_solo_out_free(out);
			fail_msg(err, errlen, "invalid convergence_root");
			return false;
		}
		memcpy(root_native, root_display, 32);
		memrev_local(root_native, 32);
		if (memcmp(root_native, scriptsig_root, 32) != 0) {
			gbt_solo_out_free(out);
			fail_msg(err, errlen, "convergence_root does not match scriptSig C");
			return false;
		}
	}

	tmp = json_object_get(gbt, "workid");
	if (tmp && json_is_string(tmp))
		out->workid = strdup(json_string_value(tmp));

	out->height = (int)height64;
	out->coinbasevalue = cbvalue;
	out->nbits = bits;
	memcpy(out->paid_winner_native, paid_native, 32);
	memcpy(out->witness_commitment, wit_commit_gbt, GBT_SOLO_WITNESS_COMMITMENT_LEN);
	out->has_mint = has_mint ? 1 : 0;
	out->mint_value = mint_value;
	out->mint_spk_len = mint_spk_len;
	if (has_mint)
		memcpy(out->mint_spk, mint_spk, (size_t)mint_spk_len);
	return true;
}
