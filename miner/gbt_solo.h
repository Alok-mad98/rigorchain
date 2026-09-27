#ifndef GBT_SOLO_H
#define GBT_SOLO_H

/*
 * RIGOR SOLO GBT copy-bytes assembler .
 *
 * Node is the only consensus authority. This module copies
 * default_coinbase_tx and the GBT transaction set. RFC-020: coinbase
 * scriptSig is BIP34 height plus OP_0 padding; coinbase pays ORU only
 * (genesis 0; every h ≥ 1 is 128 ORU plus claimable fees; no halving)
 * and does not mint RES. Lottery fields (paid_winner, convergence_root,
 * default_res_mint) are refused.
 */

#include <jansson.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define GBT_SOLO_SCRIPTSIG_MAX 100
#define GBT_SOLO_MINT_SPK_MAX 80
#define GBT_SOLO_WITNESS_COMMITMENT_LEN 38
#define GBT_SOLO_COINBASE_MAX 8192
#define GBT_SOLO_PAYOUT_V0_LEN 22
/* Consensus block ceiling; bounds every length-derived allocation below. */
#define GBT_SOLO_MAX_BLOCK_BYTES 4000000
#define GBT_SOLO_MAX_TXS 100000

struct gbt_solo_out {
	uint32_t header_words[20];
	uint32_t target[8];
	int height;
	int64_t coinbasevalue;
	uint32_t nbits;
	uint8_t scriptsig[GBT_SOLO_SCRIPTSIG_MAX];
	int scriptsig_len;
	uint8_t paid_winner_native[32];
	uint8_t coinbase_wit[GBT_SOLO_COINBASE_MAX];
	int coinbase_wit_len;
	uint8_t merkle_root[32];
	uint8_t witness_commitment[GBT_SOLO_WITNESS_COMMITMENT_LEN];
	int has_mint;
	uint8_t mint_spk[GBT_SOLO_MINT_SPK_MAX];
	int mint_spk_len;
	int64_t mint_value;
	char *txs_hex;
	char *workid;
};

void gbt_solo_out_free(struct gbt_solo_out *out);

/* True iff scriptPubKey is witness v0 P2WPKH (OP_0 PUSHBYTES_20). */
bool gbt_solo_payout_is_oru_v0(const uint8_t *spk, int len);

/*
 * Assemble SOLO work from a GBT result object (not the JSON-RPC envelope).
 * payout_spk must be ORU witness v0/20. On failure, err is populated and
 * out->txs_hex is NULL. Fail closed: no lottery-era GBT fields, no coinbase
 * RES mint, no subsidy reconstruction, no getwork / RIGR path.
 */
bool gbt_solo_assemble(const json_t *gbt,
		       const uint8_t *payout_spk, int payout_len,
		       struct gbt_solo_out *out,
		       char *err, size_t errlen);

#endif /* GBT_SOLO_H */
