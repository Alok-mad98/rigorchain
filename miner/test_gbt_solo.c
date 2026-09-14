#include "gbt_solo.h"
#include "rgrpow.h"
#include "arith_uint256.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

bool opt_debug_pow = false;

static int failures;

static void expect(int cond, const char *name)
{
	if (cond) {
		printf("PASS %s\n", name);
	} else {
		printf("FAIL %s\n", name);
		failures++;
	}
}

/* RFC-020: height ≥ 1 coinbase subsidy is 128 ORU = 12_800_000_000 atoms.
 * Height-1 GBT fixtures use this so they cannot be read as the retired
 * 1000 ORU / three-halving schedule. */
#define ORU_FLAT_BLOCK_SUBSIDY_ATOMS 12800000000LL

static const uint8_t kPayoutV0[22] = {
	0x00, 0x14,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

/* Height 1 = OP_1 plus OP_0 padding (BuildCoinbaseScriptSig). */
static const char *kScriptSigH1 = "5100";

static const char *kEmptyWitnessCommitment =
	"6a24aa21a9ede2f61c3f71d1defd3fa999dfa36953755c690689799962b48bebd836974e8cf9";

static const char *kRegtestTarget =
	"7fffff0000000000000000000000000000000000000000000000000000000000";
static const char *kZeroTxid =
	"0000000000000000000000000000000000000000000000000000000000000000";
static const char *kOneTxid =
	"0000000000000000000000000000000000000000000000000000000000000001";

static json_t *load_obj(const char *s)
{
	json_error_t e;
	json_t *v = json_loads(s, &e);
	if (!v) {
		fprintf(stderr, "json: %s\n", e.text);
		failures++;
	}
	return v;
}

void applog(int prio, const char *fmt, ...)
{
	(void)prio;
	(void)fmt;
}

void bin2hex(char *s, const unsigned char *p, size_t len)
{
	static const char hex[] = "0123456789abcdef";
	size_t i;
	for (i = 0; i < len; i++) {
		s[2 * i] = hex[(p[i] >> 4) & 0xf];
		s[2 * i + 1] = hex[p[i] & 0xf];
	}
	s[2 * len] = 0;
}

static int hex_decode_local(uint8_t *out, int cap, const char *hex)
{
	static const char *digits = "0123456789abcdefABCDEF";
	int n, i;

	if (!hex)
		return -1;
	n = (int)strlen(hex);
	if (n < 2 || (n & 1) || n / 2 > cap)
		return -1;
	for (i = 0; i < n / 2; i++) {
		const char *hi = strchr(digits, hex[2 * i]);
		const char *lo = strchr(digits, hex[2 * i + 1]);
		int h, l;
		if (!hi || !lo)
			return -1;
		h = (int)(hi - digits);
		l = (int)(lo - digits);
		if (h >= 16)
			h -= 6;
		if (l >= 16)
			l -= 6;
		out[i] = (uint8_t)((h << 4) | l);
	}
	return n / 2;
}

void sha256d(unsigned char *hash, const unsigned char *data, int len);

struct cb_fixture {
	char tx_hex[4096];
	/* RPC-hex txid; for a coinbase-only block this is also the merkle root. */
	char txid_display[65];
};

static void bin2hex_disp(char *out, const uint8_t *p, int len, bool reverse)
{
	static const char hexd[] = "0123456789abcdef";
	int i;

	for (i = 0; i < len; i++) {
		const uint8_t b = reverse ? p[len - 1 - i] : p[i];
		out[2 * i] = hexd[b >> 4];
		out[2 * i + 1] = hexd[b & 0xf];
	}
	out[2 * len] = 0;
}

/*
 * Knobs for building malformed coinbases. Defaults reproduce exactly what
 * CreateNewBlock emits: nVersion 2, final sequence, nLockTime 0, witness
 * serialization, and outputs ordered payout then witness commitment.
 */
struct cb_opts {
	int64_t value;
	const char *mint_spk_hex;
	int64_t mint_value;
	uint32_t version;
	uint32_t sequence;
	uint32_t locktime;
	int64_t commit_value;
	bool legacy_serialization;
	bool payout_not_first;
	bool extra_output;
	bool short_witness_reserved;
	int trailing_junk;
};

static struct cb_opts cb_defaults(void)
{
	struct cb_opts o;

	memset(&o, 0, sizeof(o));
	o.value = ORU_FLAT_BLOCK_SUBSIDY_ATOMS;
	o.version = 2;
	o.sequence = 0xffffffffu;
	return o;
}

struct cb_out {
	int64_t value;
	uint8_t spk[80];
	int spk_len;
};

static void put_u32_le(uint8_t *p, uint32_t v)
{
	p[0] = (uint8_t)v;
	p[1] = (uint8_t)(v >> 8);
	p[2] = (uint8_t)(v >> 16);
	p[3] = (uint8_t)(v >> 24);
}

/* Test-only witness coinbase plus the txid the node would report for it. */
static bool make_coinbase_tx(struct cb_fixture *fx, const char *scriptsig_hex,
			     const struct cb_opts *o)
{
	uint8_t raw[1024], base[1024], txid[32];
	uint8_t ss[128];
	struct cb_out outs[4];
	int off = 0, ss_len, i, j, n = 0, vout_end, base_len, marker = 2;

	memset(fx, 0, sizeof(*fx));
	ss_len = hex_decode_local(ss, (int)sizeof(ss), scriptsig_hex);
	if (ss_len < 0)
		return false;

	outs[n].value = o->value;
	memcpy(outs[n].spk, kPayoutV0, 22);
	outs[n].spk_len = 22;
	n++;
	if (o->mint_spk_hex) {
		outs[n].value = o->mint_value;
		outs[n].spk_len = hex_decode_local(outs[n].spk, 80, o->mint_spk_hex);
		if (outs[n].spk_len < 0)
			return false;
		n++;
	}
	outs[n].value = o->commit_value;
	if (hex_decode_local(outs[n].spk, 80, kEmptyWitnessCommitment) != 38)
		return false;
	outs[n].spk_len = 38;
	n++;
	if (o->extra_output) {
		outs[n].value = 0;
		outs[n].spk[0] = 0x6a;
		outs[n].spk[1] = 0x00;
		outs[n].spk_len = 2;
		n++;
	}
	if (o->payout_not_first) {
		const struct cb_out t = outs[0];
		outs[0] = outs[1];
		outs[1] = t;
	}

	put_u32_le(raw + off, o->version);
	off += 4;
	if (o->legacy_serialization) {
		marker = 0;
	} else {
		raw[off++] = 0x00;
		raw[off++] = 0x01;
	}
	raw[off++] = 1;
	memset(raw + off, 0, 32);
	off += 32;
	memset(raw + off, 0xff, 4);
	off += 4;
	raw[off++] = (uint8_t)ss_len;
	memcpy(raw + off, ss, (size_t)ss_len);
	off += ss_len;
	put_u32_le(raw + off, o->sequence);
	off += 4;
	raw[off++] = (uint8_t)n;
	for (i = 0; i < n; i++) {
		for (j = 0; j < 8; j++)
			raw[off++] = (uint8_t)((uint64_t)outs[i].value >> (8 * j));
		raw[off++] = (uint8_t)outs[i].spk_len;
		memcpy(raw + off, outs[i].spk, (size_t)outs[i].spk_len);
		off += outs[i].spk_len;
	}
	vout_end = off;
	if (!o->legacy_serialization) {
		const int wlen = o->short_witness_reserved ? 1 : 32;
		raw[off++] = 1;
		raw[off++] = (uint8_t)wlen;
		memset(raw + off, 0, (size_t)wlen);
		off += wlen;
	}
	put_u32_le(raw + off, o->locktime);
	off += 4;
	for (i = 0; i < o->trailing_junk; i++)
		raw[off++] = 0xcc;

	/* txid is over the no-witness serialization, junk included or not. */
	base_len = 4 + (vout_end - 4 - marker) + 4;
	memcpy(base, raw, 4);
	memcpy(base + 4, raw + 4 + marker, (size_t)(vout_end - 4 - marker));
	put_u32_le(base + 4 + (vout_end - 4 - marker), o->locktime);
	sha256d(txid, base, base_len);

	if (2 * off + 1 > (int)sizeof(fx->tx_hex))
		return false;
	bin2hex_disp(fx->tx_hex, raw, off, false);
	bin2hex_disp(fx->txid_display, txid, 32, true);
	return true;
}

static bool make_default_coinbase_tx(struct cb_fixture *fx,
				     const char *scriptsig_hex,
				     int64_t value,
				     const char *mint_spk_hex,
				     int64_t mint_value)
{
	struct cb_opts o = cb_defaults();

	o.value = value;
	o.mint_spk_hex = mint_spk_hex;
	o.mint_value = mint_value;
	return make_coinbase_tx(fx, scriptsig_hex, &o);
}

static char *gbt_json(const char *scriptsig)
{
	static char buf[8192];
	struct cb_fixture fx;

	if (!make_default_coinbase_tx(&fx, scriptsig, ORU_FLAT_BLOCK_SUBSIDY_ATOMS, NULL, 0)) {
		expect(0, "coinbase fixture");
		return "{}";
	}
	snprintf(buf, sizeof(buf),
		 "{"
		 "\"version\":536870912,"
		 "\"rules\":[\"segwit\"],"
		 "\"previousblockhash\":\"11%062d\","
		 "\"transactions\":[],"
		 "\"coinbasevalue\":%lld,"
		 "\"curtime\":1700000000,"
		 "\"mintime\":1,"
		 "\"maxtime\":2000000000,"
		 "\"mutable\":[\"time\"],"
		 "\"bits\":\"207fffff\","
		 "\"target\":\"%s\","
		 "\"height\":1,"
		 "\"rigor_coinbase_immutable\":true,"
		 "\"default_coinbase_scriptsig\":\"%s\","
		 "\"default_coinbase_tx\":\"%s\","
		 "\"default_coinbase_txid\":\"%s\","
		 "\"merkleroot\":\"%s\","
		 "\"default_witness_commitment\":\"%s\""
		 "}",
		 0, (long long)ORU_FLAT_BLOCK_SUBSIDY_ATOMS, kRegtestTarget,
		 scriptsig, fx.tx_hex, fx.txid_display,
		 fx.txid_display, kEmptyWitnessCommitment);
	return buf;
}

static void test_valid_copy_bytes(void)
{
	char err[256];
	struct gbt_solo_out out;
	const char *cb_hex;
	json_t *gbt = load_obj(gbt_json(kScriptSigH1));
	if (!gbt)
		return;
	cb_hex = json_string_value(json_object_get(gbt, "default_coinbase_tx"));
	expect(gbt_solo_assemble(gbt, kPayoutV0, 22, &out, err, sizeof(err)), "valid GBT assembles");
	expect(out.height == 1, "height 1");
	expect(out.coinbasevalue == ORU_FLAT_BLOCK_SUBSIDY_ATOMS, "coinbasevalue is 128 ORU");
	expect(out.scriptsig_len == 2, "scriptSig BIP34 height + OP_0 padding");
	expect(out.scriptsig[0] == 0x51, "height OP_1");
	expect(out.scriptsig[1] == 0x00, "padding OP_0");
	expect(out.has_mint == 0, "RFC-020 coinbase does not mint RES");
	expect(memcmp(out.witness_commitment, (const uint8_t[]){
		0x6a, 0x24, 0xaa, 0x21, 0xa9, 0xed
	}, 6) == 0, "witness commitment header copied");
	expect(out.header_words[0] == 536870912u, "version copied");
	expect(out.header_words[17] == 1700000000u, "nTime copied");
	expect(out.header_words[18] == 0x207fffff, "nBits copied");
	expect(out.header_words[19] == 0, "nonce starts at 0");
	{
		int wi;
		for (wi = 0; wi < 8; wi++) {
			uint32_t w = (uint32_t)out.merkle_root[4 * wi] |
				((uint32_t)out.merkle_root[4 * wi + 1] << 8) |
				((uint32_t)out.merkle_root[4 * wi + 2] << 16) |
				((uint32_t)out.merkle_root[4 * wi + 3] << 24);
			if (out.header_words[9 + wi] != w) {
				expect(0, "header merkle words match coinbase txid");
				break;
			}
		}
		if (wi == 8)
			expect(1, "header merkle words match coinbase txid");
	}
	expect(out.txs_hex && out.txs_hex[0] == '0' && out.txs_hex[1] == '1',
	       "submitblock tx count compact-size 1");
	{
		char ss_hex[201];
		int i;
		static const char hex[] = "0123456789abcdef";
		for (i = 0; i < out.scriptsig_len; i++) {
			ss_hex[2 * i] = hex[out.scriptsig[i] >> 4];
			ss_hex[2 * i + 1] = hex[out.scriptsig[i] & 0xf];
		}
		ss_hex[2 * out.scriptsig_len] = 0;
		expect(strcmp(ss_hex, kScriptSigH1) == 0, "scriptSig byte-identical to GBT");
	}
	{
		char cb_got[4096];
		int i;
		static const char hex[] = "0123456789abcdef";
		if (out.coinbase_wit_len * 2 < (int)sizeof(cb_got)) {
			for (i = 0; i < out.coinbase_wit_len; i++) {
				cb_got[2 * i] = hex[out.coinbase_wit[i] >> 4];
				cb_got[2 * i + 1] = hex[out.coinbase_wit[i] & 0xf];
			}
			cb_got[2 * out.coinbase_wit_len] = 0;
			expect(cb_hex && strcmp(cb_got, cb_hex) == 0,
			       "coinbase bytes identical to default_coinbase_tx");
		} else {
			expect(0, "coinbase bytes identical to default_coinbase_tx");
		}
	}
	/* No RIGR / CVGE tag in coinbase. */
	{
		int i, found = 0;
		for (i = 0; i + 4 <= out.coinbase_wit_len; i++) {
			if (out.coinbase_wit[i] == 'R' && out.coinbase_wit[i + 1] == 'I' &&
			    out.coinbase_wit[i + 2] == 'G' && out.coinbase_wit[i + 3] == 'R')
				found = 1;
		}
		expect(!found, "no RIGR OP_RETURN");
	}
	gbt_solo_out_free(&out);
	json_decref(gbt);
}

static void test_reject_lottery_gbt_fields(void)
{
	char err[256];
	struct gbt_solo_out out;
	json_t *gbt;

	gbt = load_obj(gbt_json(kScriptSigH1));
	if (!gbt)
		return;
	json_object_set_new(gbt, "paid_winner_txid", json_string(kZeroTxid));
	expect(!gbt_solo_assemble(gbt, kPayoutV0, 22, &out, err, sizeof(err)),
	       "paid_winner_txid refused");
	expect(strstr(err, "paid_winner_txid") != NULL, "error names paid_winner_txid");
	gbt_solo_out_free(&out);
	json_decref(gbt);

	gbt = load_obj(gbt_json(kScriptSigH1));
	if (!gbt)
		return;
	json_object_set_new(gbt, "convergence_root", json_string(kZeroTxid));
	expect(!gbt_solo_assemble(gbt, kPayoutV0, 22, &out, err, sizeof(err)),
	       "convergence_root refused");
	expect(strstr(err, "convergence_root") != NULL, "error names convergence_root");
	gbt_solo_out_free(&out);
	json_decref(gbt);

	gbt = load_obj(gbt_json(kScriptSigH1));
	if (!gbt)
		return;
	json_t *mint = json_object();
	json_object_set_new(mint, "value", json_integer(40000000));
	json_object_set_new(mint, "scriptPubKey",
			    json_string("0105141111111111111111111111111111111111111111bb"));
	json_object_set_new(gbt, "default_res_mint", mint);
	expect(!gbt_solo_assemble(gbt, kPayoutV0, 22, &out, err, sizeof(err)),
	       "default_res_mint refused");
	expect(strstr(err, "default_res_mint") != NULL, "error names default_res_mint");
	gbt_solo_out_free(&out);
	json_decref(gbt);
}

static void test_reject_coinbase_res_mint(void)
{
	char err[256];
	struct gbt_solo_out out;
	struct cb_fixture fx;
	char buf[8192];

	if (!make_default_coinbase_tx(&fx, kScriptSigH1, ORU_FLAT_BLOCK_SUBSIDY_ATOMS,
				      "0105141111111111111111111111111111111111111111bb",
				      40000000LL)) {
		expect(0, "mint coinbase fixture");
		return;
	}
	snprintf(buf, sizeof(buf),
		 "{"
		 "\"version\":536870912,\"rules\":[\"segwit\"],"
		 "\"previousblockhash\":\"11%062d\",\"transactions\":[],"
		 "\"coinbasevalue\":%lld,\"curtime\":1700000000,"
		 "\"mintime\":1,\"maxtime\":2000000000,\"mutable\":[\"time\"],"
		 "\"bits\":\"207fffff\",\"target\":\"%s\",\"height\":1,"
		 "\"rigor_coinbase_immutable\":true,"
		 "\"default_coinbase_scriptsig\":\"%s\","
		 "\"default_coinbase_tx\":\"%s\","
		 "\"default_coinbase_txid\":\"%s\","
		 "\"merkleroot\":\"%s\","
		 "\"default_witness_commitment\":\"%s\"}",
		 0, (long long)ORU_FLAT_BLOCK_SUBSIDY_ATOMS, kRegtestTarget,
		 kScriptSigH1, fx.tx_hex, fx.txid_display,
		 fx.txid_display, kEmptyWitnessCommitment);
	json_t *gbt = load_obj(buf);
	if (!gbt)
		return;
	expect(!gbt_solo_assemble(gbt, kPayoutV0, 22, &out, err, sizeof(err)),
	       "coinbase 0xbb mint refused");
	expect(strstr(err, "must not mint RES") != NULL, "error names coinbase mint ban");
	gbt_solo_out_free(&out);
	json_decref(gbt);
}

static void test_reject_p2pkh_payout(void)
{
	char err[256];
	struct gbt_solo_out out;
	uint8_t p2pkh[25];
	json_t *gbt = load_obj(gbt_json(kScriptSigH1));
	memset(p2pkh, 0x76, sizeof(p2pkh));
	if (!gbt)
		return;
	expect(!gbt_solo_assemble(gbt, p2pkh, 25, &out, err, sizeof(err)), "P2PKH payout refused");
	gbt_solo_out_free(&out);
	json_decref(gbt);
}

static void test_reject_height0(void)
{
	char err[256];
	struct gbt_solo_out out;
	char buf[4096];
	json_t *gbt;
	snprintf(buf, sizeof(buf),
		 "{\"version\":536870912,\"rules\":[\"segwit\"],"
		 "\"previousblockhash\":\"%064d\",\"transactions\":[],"
		 "\"coinbasevalue\":0,\"curtime\":1,\"mintime\":1,\"maxtime\":2000000000,"
		 "\"bits\":\"207fffff\","
		 "\"target\":\"%s\",\"height\":0,"
		 "\"rigor_coinbase_immutable\":true,"
		 "\"default_coinbase_scriptsig\":\"%s\","
		 "\"default_witness_commitment\":\"%s\"}",
		 0, kRegtestTarget, kScriptSigH1, kEmptyWitnessCommitment);
	gbt = load_obj(buf);
	if (!gbt)
		return;
	expect(!gbt_solo_assemble(gbt, kPayoutV0, 22, &out, err, sizeof(err)),
	       "height 0 GBT refused (genesis subsidy 0; no 1000 ORU fallback)");
	gbt_solo_out_free(&out);
	json_decref(gbt);
}

static int rgrpow_meets_nbits(const uint8_t digest[32], uint32_t nbits)
{
	arith_uint256 hash, target;
	bool neg = false, over = false;
	int i;

	memset(&hash, 0, sizeof(hash));
	for (i = 0; i < 8; i++)
		hash.data[i] = (uint32_t)digest[4 * i]
			     | ((uint32_t)digest[4 * i + 1] << 8)
			     | ((uint32_t)digest[4 * i + 2] << 16)
			     | ((uint32_t)digest[4 * i + 3] << 24);
	arith_uint256_set_compact(&target, nbits, &neg, &over);
	if (over || neg || nbits == 0)
		return 0;
	return arith_uint256_compare(&hash, &target) <= 0;
}

static void test_sha3_fips202_abc(void)
{
	static const uint8_t abc[3] = { 'a', 'b', 'c' };
	static const uint8_t want[32] = {
		0x3a, 0x98, 0x5d, 0xa7, 0x4f, 0xe2, 0x25, 0xb2,
		0x04, 0x5c, 0x17, 0x2d, 0x6b, 0xd3, 0x90, 0xbd,
		0x85, 0x5f, 0x08, 0x6e, 0x3e, 0x9d, 0x52, 0x5b,
		0x46, 0xbf, 0xe2, 0x45, 0x11, 0x43, 0x15, 0x32
	};
	uint8_t got[32];
	rgrpow_sha3_256_short(abc, 3, got);
	expect(memcmp(got, want, 32) == 0, "SHA3_POW_VECTOR FIPS 202 SHA3-256('abc')");
}

static void test_rgrpow_zero_header_matches_node(void)
{
	static const uint8_t want[32] = {
		0xdd, 0x05, 0x7f, 0xc1, 0xdf, 0x8f, 0x5b, 0x94, 0x66, 0xe8, 0x34,
		0xcc, 0x9d, 0x7b, 0x81, 0x9e, 0x2d, 0x51, 0x1c, 0x75, 0xb4, 0x36,
		0x75, 0xb5, 0x77, 0xf6, 0xd9, 0xf5, 0xd6, 0xde, 0x04, 0xfc
	};
	uint8_t header[80];
	uint8_t got[32];
	memset(header, 0, sizeof(header));
	rgrpow_hash80(header, got);
	expect(memcmp(got, want, 32) == 0,
	       "SHA3_POW_VECTOR zero header matches GetRigorPowHash");
	expect(got[0] == 0xdd, "digest byte0 is LSB (UintToArith256)");
}

static void test_rgrpow_typed_header_matches_node(void)
{
	uint8_t header[80];
	uint8_t got[32];
	static const uint8_t want[32] = {
		0x8b, 0x35, 0x71, 0x82, 0x9a, 0xd2, 0x02, 0x10,
		0x5c, 0xc7, 0xfc, 0x4b, 0x1c, 0x8b, 0xee, 0xf1,
		0xf7, 0x19, 0x0e, 0x6b, 0xfd, 0xbf, 0x2f, 0x0a,
		0xc5, 0x43, 0x55, 0xe7, 0xb4, 0xf0, 0x41, 0xf0
	};
	static const uint8_t prev[32] = {
		0x18, 0x17, 0x16, 0x15, 0x14, 0x13, 0x12, 0x11,
		0x10, 0x0f, 0x0e, 0x0d, 0x0c, 0x0b, 0x0a, 0x89,
		0xf7, 0xe6, 0xd5, 0xc4, 0xb3, 0xa2, 0x01, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
	};
	static const uint8_t merkle[32] = {
		0xdd, 0x15, 0x35, 0x8b, 0xf3, 0xbc, 0xa0, 0xe8,
		0x92, 0x65, 0x95, 0x9d, 0x05, 0xbf, 0x13, 0x59,
		0x84, 0xd5, 0xd8, 0x75, 0xe9, 0x57, 0x2c, 0x8e,
		0xfc, 0x27, 0xf9, 0x7a, 0x20, 0xbd, 0x3f, 0x9d
	};
	uint32_t ver = 0x20000000, ntime = 1786125600, nbits = 0x1d008000, nonce = 0x89abcdef;
	memset(header, 0, sizeof(header));
	header[0] = (uint8_t)ver;
	header[1] = (uint8_t)(ver >> 8);
	header[2] = (uint8_t)(ver >> 16);
	header[3] = (uint8_t)(ver >> 24);
	memcpy(header + 4, prev, 32);
	memcpy(header + 36, merkle, 32);
	header[68] = (uint8_t)ntime;
	header[69] = (uint8_t)(ntime >> 8);
	header[70] = (uint8_t)(ntime >> 16);
	header[71] = (uint8_t)(ntime >> 24);
	header[72] = (uint8_t)nbits;
	header[73] = (uint8_t)(nbits >> 8);
	header[74] = (uint8_t)(nbits >> 16);
	header[75] = (uint8_t)(nbits >> 24);
	header[76] = (uint8_t)nonce;
	header[77] = (uint8_t)(nonce >> 8);
	header[78] = (uint8_t)(nonce >> 16);
	header[79] = (uint8_t)(nonce >> 24);
	rgrpow_hash80(header, got);
	expect(memcmp(got, want, 32) == 0,
	       "SHA3_POW_VECTOR typed header matches GetRigorPowHash");
}

static void test_opuhash_digest_rejected_by_sha3_pow(void)
{
	static const uint8_t opuhash_zero[32] = {
		0x75, 0x3c, 0x70, 0x2a, 0xd1, 0x24, 0xc2, 0xf3,
		0x86, 0xf9, 0xf3, 0x99, 0x4c, 0xd2, 0x38, 0x95,
		0xd9, 0x77, 0x4c, 0xc1, 0xa8, 0x1a, 0xf9, 0x2e,
		0xd1, 0x4a, 0x4f, 0x30, 0x37, 0x19, 0xbe, 0x19
	};
	uint8_t header[80];
	uint8_t sha3[32];
	memset(header, 0, sizeof(header));
	rgrpow_hash80(header, sha3);
	expect(memcmp(sha3, opuhash_zero, 32) != 0,
	       "OpuHash(zero header) != SHA3-256(RGRPOW01||zero header)");
	expect(!rgrpow_meets_nbits(sha3, 0x03000001u),
	       "OpuHash-era header fails SHA3 CheckProofOfWork vs tight nBits");
}

static void test_miner_header_sha3_checkproofofwork_pass(void)
{
	uint8_t header[80];
	uint8_t digest[32];
	uint32_t n;
	int found = 0;
	memset(header, 0, sizeof(header));
	header[0] = 1;
	for (n = 0; n < 400000u; n++) {
		header[76] = (uint8_t)n;
		header[77] = (uint8_t)(n >> 8);
		header[78] = (uint8_t)(n >> 16);
		header[79] = (uint8_t)(n >> 24);
		rgrpow_hash80(header, digest);
		if (rgrpow_meets_nbits(digest, 0x207fffffu)) {
			found = 1;
			break;
		}
	}
	expect(found, "SOLO_BLOCK_TEMPLATE miner-generated header meets SHA3 CheckProofOfWork");
}

static void test_reject_ntime_below_mintime(void)
{
	char err[256];
	struct gbt_solo_out out;
	char buf[4096];
	json_t *gbt;
	snprintf(buf, sizeof(buf),
		 "{\"version\":536870912,\"rules\":[\"segwit\"],"
		 "\"previousblockhash\":\"11%062d\",\"transactions\":[],"
		 "\"coinbasevalue\":1,\"curtime\":10,\"mintime\":100,\"maxtime\":2000000000,"
		 "\"bits\":\"207fffff\",\"target\":\"%s\",\"height\":1,"
		 "\"rigor_coinbase_immutable\":true,"
		 "\"default_coinbase_scriptsig\":\"%s\","
		 "\"default_witness_commitment\":\"%s\"}",
		 0, kRegtestTarget, kScriptSigH1, kEmptyWitnessCommitment);
	gbt = load_obj(buf);
	if (!gbt)
		return;
	expect(!gbt_solo_assemble(gbt, kPayoutV0, 22, &out, err, sizeof(err)),
	       "curtime < mintime refused");
	gbt_solo_out_free(&out);
	json_decref(gbt);
}

static void test_reject_mutable_coinbase(void)
{
	char err[256];
	struct gbt_solo_out out;
	char buf[4096];
	json_t *gbt;
	snprintf(buf, sizeof(buf),
		 "{\"version\":536870912,\"rules\":[\"segwit\"],"
		 "\"previousblockhash\":\"11%062d\",\"transactions\":[],"
		 "\"coinbasevalue\":1,\"curtime\":1700000000,\"mintime\":1,\"maxtime\":2000000000,"
		 "\"mutable\":[\"time\",\"coinbase\"],"
		 "\"bits\":\"207fffff\",\"target\":\"%s\",\"height\":1,"
		 "\"rigor_coinbase_immutable\":true,"
		 "\"default_coinbase_scriptsig\":\"%s\","
		 "\"default_witness_commitment\":\"%s\"}",
		 0, kRegtestTarget, kScriptSigH1, kEmptyWitnessCommitment);
	gbt = load_obj(buf);
	if (!gbt)
		return;
	expect(!gbt_solo_assemble(gbt, kPayoutV0, 22, &out, err, sizeof(err)),
	       "mutable coinbase refused");
	gbt_solo_out_free(&out);
	json_decref(gbt);
}

static void test_header_80_rgrpow_stable(void)
{
	char err[256];
	struct gbt_solo_out out;
	json_t *gbt = load_obj(gbt_json(kScriptSigH1));
	uint8_t wire[80];
	uint8_t h1[32], h2[32];
	int i;
	if (!gbt)
		return;
	if (!gbt_solo_assemble(gbt, kPayoutV0, 22, &out, err, sizeof(err))) {
		expect(0, "header 80 assemble");
		json_decref(gbt);
		return;
	}
	for (i = 0; i < 20; i++) {
		uint32_t w = out.header_words[i];
		wire[4 * i] = (uint8_t)w;
		wire[4 * i + 1] = (uint8_t)(w >> 8);
		wire[4 * i + 2] = (uint8_t)(w >> 16);
		wire[4 * i + 3] = (uint8_t)(w >> 24);
	}
	rgrpow_hash80(wire, h1);
	rgrpow_hash80(wire, h2);
	expect(memcmp(h1, h2, 32) == 0, "POW-01 same 80-byte header => same RGRPOW01");
	wire[76] = (uint8_t)(wire[76] + 1);
	rgrpow_hash80(wire, h2);
	expect(memcmp(h1, h2, 32) != 0, "nonce change changes RGRPOW01 digest");
	gbt_solo_out_free(&out);
	json_decref(gbt);
}

static void test_reject_missing_coinbase_tx(void)
{
	char err[256];
	struct gbt_solo_out out;
	char buf[4096];
	json_t *gbt;
	snprintf(buf, sizeof(buf),
		 "{\"version\":536870912,\"rules\":[\"segwit\"],"
		 "\"previousblockhash\":\"11%062d\",\"transactions\":[],"
		 "\"coinbasevalue\":12800000000,\"curtime\":1700000000,\"mintime\":1,\"maxtime\":2000000000,"
		 "\"mutable\":[\"time\"],\"bits\":\"207fffff\",\"target\":\"%s\",\"height\":1,"
		 "\"rigor_coinbase_immutable\":true,"
		 "\"default_coinbase_scriptsig\":\"%s\","
		 "\"default_witness_commitment\":\"%s\"}",
		 0, kRegtestTarget, kScriptSigH1, kEmptyWitnessCommitment);
	gbt = load_obj(buf);
	if (!gbt)
		return;
	expect(!gbt_solo_assemble(gbt, kPayoutV0, 22, &out, err, sizeof(err)),
	       "missing default_coinbase_tx refused");
	expect(strstr(err, "default_coinbase_tx") != NULL, "error names default_coinbase_tx");
	gbt_solo_out_free(&out);
	json_decref(gbt);
}

/* Valid height-1 GBT whose coinbase is rebuilt from o, keeping txid/merkleroot consistent. */
static json_t *gbt_with_coinbase(const struct cb_opts *o)
{
	struct cb_fixture fx;
	json_t *gbt = load_obj(gbt_json(kScriptSigH1));

	if (!gbt)
		return NULL;
	if (!make_coinbase_tx(&fx, kScriptSigH1, o)) {
		json_decref(gbt);
		return NULL;
	}
	json_object_set_new(gbt, "default_coinbase_tx", json_string(fx.tx_hex));
	json_object_set_new(gbt, "default_coinbase_txid", json_string(fx.txid_display));
	json_object_set_new(gbt, "merkleroot", json_string(fx.txid_display));
	return gbt;
}

static void expect_reject(json_t *gbt, const char *needle, const char *name)
{
	char err[256] = "";
	struct gbt_solo_out out;

	if (!gbt) {
		expect(0, name);
		return;
	}
	if (gbt_solo_assemble(gbt, kPayoutV0, 22, &out, err, sizeof(err))) {
		expect(0, name);
	} else if (needle && !strstr(err, needle)) {
		printf("     wanted \"%s\", got \"%s\"\n", needle, err);
		expect(0, name);
	} else {
		expect(1, name);
	}
	gbt_solo_out_free(&out);
	json_decref(gbt);
}

static void expect_reject_coinbase(const struct cb_opts *o, const char *needle,
				   const char *name)
{
	expect_reject(gbt_with_coinbase(o), needle, name);
}

static void test_reject_coinbase_structure(void)
{
	struct cb_opts o;

	o = cb_defaults();
	o.version = 3;
	expect_reject_coinbase(&o, "nVersion must be 2", "coinbase nVersion != 2 refused");

	o = cb_defaults();
	o.sequence = 0;
	expect_reject_coinbase(&o, "nSequence must be final", "non-final coinbase sequence refused");

	o = cb_defaults();
	o.locktime = 1;
	expect_reject_coinbase(&o, "nLockTime must be 0", "non-zero coinbase nLockTime refused");

	o = cb_defaults();
	o.legacy_serialization = true;
	expect_reject_coinbase(&o, "not witness-serialized", "legacy-serialized coinbase refused");

	o = cb_defaults();
	o.payout_not_first = true;
	expect_reject_coinbase(&o, "vout[0] does not pay", "payout not at vout[0] refused");

	o = cb_defaults();
	o.value = 999999999LL;
	expect_reject_coinbase(&o, "value != coinbasevalue", "vout[0] value != coinbasevalue refused");

	o = cb_defaults();
	o.commit_value = 1;
	expect_reject_coinbase(&o, "commitment output value must be 0", "paid witness commitment refused");

	o = cb_defaults();
	o.extra_output = true;
	expect_reject_coinbase(&o, "unexpected coinbase output count", "extra coinbase output refused");

	o = cb_defaults();
	o.short_witness_reserved = true;
	expect_reject_coinbase(&o, "32-byte zero reserved value", "short witness reserved value refused");

	o = cb_defaults();
	o.trailing_junk = 3;
	expect_reject_coinbase(&o, "trailing bytes", "trailing bytes after coinbase refused");
}

static void test_reject_coinbase_txid_and_merkleroot(void)
{
	struct cb_opts o = cb_defaults();
	json_t *gbt;

	gbt = gbt_with_coinbase(&o);
	if (gbt)
		json_object_del(gbt, "default_coinbase_txid");
	expect_reject(gbt, "default_coinbase_txid", "missing default_coinbase_txid refused");

	gbt = gbt_with_coinbase(&o);
	if (gbt)
		json_object_set_new(gbt, "default_coinbase_txid", json_string(kOneTxid));
	expect_reject(gbt, "does not match default_coinbase_tx", "wrong default_coinbase_txid refused");

	gbt = gbt_with_coinbase(&o);
	if (gbt)
		json_object_del(gbt, "merkleroot");
	expect_reject(gbt, "merkleroot", "missing merkleroot refused");

	gbt = gbt_with_coinbase(&o);
	if (gbt)
		json_object_set_new(gbt, "merkleroot", json_string(kOneTxid));
	expect_reject(gbt, "merkle root != GBT merkleroot", "GBT merkleroot mismatch refused");
}

/*
 * RFC-020 height-1 GBT: BIP34 height + OP_0, 128 ORU payout, witness
 * commitment, no lottery fields, no coinbase RES mint. Coinbase bytes come
 * from the same encoder the other fixtures use so the copy-bytes check
 * stays exact.
 */
static void test_golden_vector_regtest_height1(void)
{
	char err[256] = "";
	char buf[8192];
	char cb_got[4096];
	struct gbt_solo_out out;
	struct cb_fixture fx;
	json_t *gbt;

	if (!make_default_coinbase_tx(&fx, kScriptSigH1, ORU_FLAT_BLOCK_SUBSIDY_ATOMS, NULL, 0)) {
		expect(0, "golden fixture");
		return;
	}
	snprintf(buf, sizeof(buf),
		 "{"
		 "\"version\":536870912,"
		 "\"rules\":[\"csv\",\"!segwit\"],"
		 "\"mutable\":[\"time\"],"
		 "\"previousblockhash\":\"3070f47e2c1b153a3288382f3258396bb48b016eab76eae9f235d006dcc2af63\","
		 "\"transactions\":[],"
		 "\"coinbasevalue\":%lld,"
		 "\"target\":\"%s\","
		 "\"mintime\":1778224716,"
		 "\"maxtime\":1788058420,"
		 "\"curtime\":1788051220,"
		 "\"bits\":\"207fffff\","
		 "\"height\":1,"
		 "\"default_witness_commitment\":\"%s\","
		 "\"default_coinbase_scriptsig\":\"%s\","
		 "\"default_coinbase_tx\":\"%s\","
		 "\"default_coinbase_txid\":\"%s\","
		 "\"merkleroot\":\"%s\","
		 "\"rigor_coinbase_immutable\":true"
		 "}",
		 (long long)ORU_FLAT_BLOCK_SUBSIDY_ATOMS, kRegtestTarget,
		 kEmptyWitnessCommitment, kScriptSigH1,
		 fx.tx_hex, fx.txid_display, fx.txid_display);
	gbt = load_obj(buf);
	if (!gbt)
		return;
	if (!gbt_solo_assemble(gbt, kPayoutV0, 22, &out, err, sizeof(err))) {
		printf("     %s\n", err);
		expect(0, "golden regtest GBT assembles");
		json_decref(gbt);
		return;
	}
	expect(1, "golden regtest GBT assembles");
	expect(out.height == 1, "golden height 1");
	expect(out.coinbasevalue == ORU_FLAT_BLOCK_SUBSIDY_ATOMS, "golden coinbasevalue is 128 ORU");
	expect(out.has_mint == 0, "golden has no RES mint");
	expect(out.scriptsig_len == 2 && out.scriptsig[0] == 0x51 && out.scriptsig[1] == 0x00,
	       "golden scriptSig is height + OP_0");
	expect(out.header_words[18] == 0x207fffff, "golden nBits");
	expect(out.header_words[17] == 1788051220u, "golden nTime");
	bin2hex_disp(cb_got, out.coinbase_wit, out.coinbase_wit_len, false);
	expect(strcmp(cb_got, fx.tx_hex) == 0,
	       "golden coinbase bytes copied verbatim");
	{
		char mr[65];
		bin2hex_disp(mr, out.merkle_root, 32, true);
		expect(strcmp(mr, fx.txid_display) == 0,
		       "golden merkle root derived from node bytes");
	}
	gbt_solo_out_free(&out);
	json_decref(gbt);
}

int main(void)
{
	test_valid_copy_bytes();
	test_reject_lottery_gbt_fields();
	test_reject_coinbase_res_mint();
	test_reject_p2pkh_payout();
	test_reject_height0();
	test_reject_missing_coinbase_tx();
	test_reject_coinbase_structure();
	test_reject_coinbase_txid_and_merkleroot();
	test_golden_vector_regtest_height1();
	test_reject_ntime_below_mintime();
	test_reject_mutable_coinbase();
	test_header_80_rgrpow_stable();
	test_sha3_fips202_abc();
	test_rgrpow_zero_header_matches_node();
	test_rgrpow_typed_header_matches_node();
	test_opuhash_digest_rejected_by_sha3_pow();
	test_miner_header_sha3_checkproofofwork_pass();
	printf("%s (%d failures)\n", failures ? "FAILED" : "ALL PASS", failures);
	return failures ? 1 : 0;
}
