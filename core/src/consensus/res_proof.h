#ifndef BITCOIN_CONSENSUS_RES_PROOF_H
#define BITCOIN_CONSENSUS_RES_PROOF_H

#include <primitives/transaction.h>
#include <script/rigor_tags.h>
#include <script/script.h>
#include <uint256.h>

#include <array>
#include <cstddef>
#include <cstdint>

class CBlock;
class CPubKey;

namespace Consensus {

/**
 * FROZEN CARRIER (coinbase RES mint) — the only proof channel. Do not parse vin witnesses.
 *
 * 1. Locus. The proof and the RES it authorizes both live in the **coinbase** of the block that
 *    pays the draw. No body transaction may create RES, and no body transaction may carry the
 *    tag. There is no funding input, no signature and no network-shared funding script: the
 *    coinbase is authorized by the block's proof of work, and the mint is fully determined by the
 *    parent block (winner, amount, payout program), so there is nothing left for a signature to
 *    authorize.
 *
 * 2. Carrier. Exactly `OP_RETURN` plus one canonical push of `PCORB_OPRETURN_PAYLOAD_BYTES`:
 *    `RigorTags::PCOR` (4) ‖ fields (116), at nValue == 0. The script is
 *    `OP_RETURN OP_PUSHDATA1 0x78 <120 bytes>` (`PCORB_CARRIER_SCRIPT_BYTES`). Fields are
 *    little-endian with no inner length prefixes, in the order of the struct below:
 *      convergence_txid (32) ‖ convergence_amount (8) ‖ bind_hash (32) ‖ height (8) ‖
 *      target_prev (4) ‖ convergence_root (32).
 *    Wrong magic, wrong length, non-canonical push, trailing opcodes or nValue != 0 ⇒ not a
 *    proof. The output index is not consensus: scan every vout.
 *
 * 3. Uniqueness / fail-closed. The coinbase carries 0 or 1 carrier and 0 or 1 `OP_RES_MINT`
 *    output, and the two counts must be equal — a tag without a mint output, or a mint output
 *    without a tag, is invalid. Every non-coinbase transaction MUST carry zero carriers
 *    (`CheckTxInputs`) and MUST NOT create RES from ORU inputs. Because a block has exactly one
 *    coinbase, "at most one mint per block" is structural, not a tie-break between candidates.
 *
 * 4. The tag is not an asset. It is `ASSET_NONE` at nValue 0 and never enters the dual-pool
 *    equation. The RES it authorizes is counted as coinbase RES, on top of `nFeesRes` and the
 *    mockable-chain bootstrap — see `ConnectBlockRigor::VerifyCoinbaseConvergenceAndResLayout`.
 *
 * 5. Binding unchanged from the witness-carried era: `VerifyConvergeProof`,
 *    `CheckConvergeProofBinding`, `CheckResRewardOwner`, W(h), maturity and
 *    `FindBlockPaidConvergenceTxids` all still key off `proof.convergence_txid`. Only the carrier
 *    moved; the 116 field bytes and their meaning did not.
 */

/**
 * RFC-009 §5: Proof serialization (Converge Proof)
 * 5.1 Structure
 * [0] converge_txid   (32)
 * [1] bind_hash       (32)
 * [2] height          (LE32)
 * [3] target_prev     (LE32)
 * [4] converge_root   (32)
 * [5] proof_blob      (var, raw bytes)
 * [6] signature       (64)  -- SUPERSEDED, see SECURITY-001-A / A-7 below
 * Rules
 * - No length prefix inside fixed fields
 * - proof_blob is opaque
 *
 * RFC-004 §10: ConvergeProof Serialization
 * Exact order:
 *   converge_txid      32 bytes
 *   convergence_amount 8 bytes (uint64 LE) - RIGOR extension (not in RFC-004, required by RFC-000)
 *   bind_hash          32 bytes
 *   height             8 bytes (uint64 LE)
 *   target_prev        4 bytes (uint32 LE)
 *   converge_root      32 bytes
 *   proof_blob         variable (not used in current implementation)
 *   signature          64 bytes (Ed25519)  -- SUPERSEDED, see SECURITY-001-A / A-7 below
 *
 * NOTE: RFC-004 §10 does not include convergence_amount in the specification.
 * However, RFC-000 requires the convergence amount for weight/fee calculation.
 * Per Hierarchy Rule: RFC-000-FINAL overrides RFC-004, so convergence_amount is included.
 *
 * RFC-009 §0: Absolute principle
 * All consensus-critical data MUST have exactly ONE valid byte representation.
 * Any alternative encoding is a consensus violation.
 *
 * PCORB proof payload (116 bytes, one representation): convergence_txid (32) ‖
 * convergence_amount (8 LE) ‖ bind_hash (32) ‖ height (8 LE) ‖ target_prev (4 LE) ‖
 * convergence_root (32). The carrier is the tagged coinbase OP_RETURN described in the FROZEN
 * CARRIER block above.
 *
 * CONSENSUS INVARIANT: the proof does not encode its own meaning. A block mints RES because its
 * coinbase carries this tag and the parent's resolved draw names a winner — never because some
 * transaction "looks like" a mint. Inferring issuance from a witness, a script path or an output
 * shape is INVALID and will lead to consensus failure.
 *
 * **Field order is consensus-critical** (payload byte order and in-memory layout). Do not reorder,
 * insert or remove fields without a hard-forking protocol upgrade.
 *
 * **SECURITY-001-A / A-7: the proof carries no signature.** Two witness pushes used to hold a
 * detached Ed25519 signature and the key that verified it. The signed message was σ, which every
 * field above already pins to the block, and the verifying key came from the witness itself, so any
 * freshly generated keypair satisfied it — it authorized nothing while suggesting a "mint key" was a
 * security boundary. Nor is there anything left to authorize: given a block's convergence set, F,
 * the winner, the amount and the payout script are all determined, and `validation.cpp` requires
 * exactly one mint whenever a winner exists.
 *
 * Do not reintroduce a signature here expecting it to gate the mint, and do not reintroduce a
 * funding input for the same purpose. The carrier sits in the coinbase, which proof of work
 * already authenticates.
 *
 */
struct ConvergeProof {
    uint256 convergence_txid;
    uint64_t convergence_amount_sat{0};  // RFC-000 extension (not in RFC-004 §10)
    uint256 bind_hash;
    uint64_t height{0};
    uint32_t target_prev{0};
    uint256 convergence_root{};
};

/** True iff `script` is the canonical tagged PCORB OP_RETURN. Value is checked on the txout. */
bool IsPcorbCarrierScript(const CScript& script);
/** True iff nValue == 0 and the script is `IsPcorbCarrierScript`. */
bool IsPcorbCarrierOutput(const CTxOut& out);
/** How many vouts are `IsPcorbCarrierOutput`. Consensus allows 0 or 1, coinbase only. */
size_t CountPcorbCarrierOutputs(const CTransaction& tx);
/**
 * Decode the coinbase's carrier. Returns false for any non-coinbase transaction, for a coinbase
 * with no carrier, and for a coinbase with more than one — a second tag is not a tie to break.
 */
bool ParseConvergeProof(const CTransaction& coinbase, ConvergeProof& out);

static_assert(RigorTags::PCORB_PROOF_FIELDS_BYTES == 32 + 8 + 32 + 8 + 4 + 32, "PCORB field concat");
static_assert(RigorTags::PCORB_OPRETURN_PAYLOAD_BYTES == 4 + RigorTags::PCORB_PROOF_FIELDS_BYTES, "PCOR ‖ fields");
static_assert(RigorTags::PCORB_CARRIER_SCRIPT_BYTES == 1 + 2 + RigorTags::PCORB_OPRETURN_PAYLOAD_BYTES, "OP_RETURN PUSHDATA1 120");

/**
 * Weight binding: the proof's self-reported `convergence_amount_sat` must equal the amount the
 * drawn block actually committed for `convergence_txid`.
 *
 * SECURITY-001-A8: the caller supplies that amount from the draw set of block h-1, since the
 * winning convergence transaction is in the parent block rather than in the block being
 * validated. **ConnectBlock** separately enforces `convergence_txid ∈ F` — see
 * `rigor::ConnectEvaluateCoinbaseResMint`.
 */
bool CheckConvergeProofBinding(const ConvergeProof& proof, CAmount committed_convergence_amount);

/**
 * PCORB verification from the decoded payload only.
 * When winner_txid is non-null, proof.convergence_txid must equal it (ConnectBlock mint winner coupling).
 * When expected_convergence_root is non-null, proof.convergence_root must equal it (block ConvergenceRoot binding).
 *
 * CONSENSUS NOTE: since SECURITY-001-A8 proof.convergence_root MUST match the PARENT of the block
 * that includes the tx, because that is the block whose draw the mint pays; wallet/template
 * estimates are best-effort only (see ``BuildConvergenceDrawSet`` / convergence_logic.h).
 */
/**
 * RFC-014 Conv-Decouple-01:
 *   The inequality RHS is `res_probability_target` (the constitutional
 *   constant `Consensus::Params::RES_PROBABILITY_TARGET`, value = floor(2^246 / MIN_CONVERGENCE_AMOUNT) / CONVERGENCE_DRAW_WINDOW; RFC-017, C=1024 ORU),
 *   passed in explicitly so that this verification function remains
 *   stateless (RFC-005 §4.12 self-proving architecture).
 *
 *   SECURITY-001-A8: `proof.target_prev` no longer feeds σ. It is retained as a payload field and
 *   is still compared against the parent header by ConnectBlock, but B_{h-1} already commits the
 *   parent's nBits, so the comparison is redundancy rather than the binding itself.
 */
bool VerifyConvergeProof(const ConvergeProof& proof, const uint256& res_probability_target, const uint256* winner_txid, const uint256* expected_convergence_root);

/**
 * RFC-005 §4.12: RES Mint Verification
 * bool VerifyConvergeProof(const CTransaction& tx, const uint256& res_probability_target);
 *
 * This is the canonical interface per RFC-005 Validation State Machine. It decodes the coinbase
 * carrier and verifies the draw inequality; it does not check winner coupling or ownership.
 *
 * RFC-014 Conv-Decouple-01: `res_probability_target` is the constitutional
 * inequality RHS (= floor(2^246 / MIN_CONVERGENCE_AMOUNT) / CONVERGENCE_DRAW_WINDOW; RFC-017, C=1024 ORU; taken from `Consensus::Params::RES_PROBABILITY_TARGET`).
 */
bool VerifyConvergeProof(const CTransaction& coinbase, const uint256& res_probability_target);

uint256 ConvergeProofDigest(const uint256& convergence_txid, const uint256& bind_hash, uint64_t height, const uint256& convergence_root);

// TASK-H2 R-8: `GenerateResKeypair` has been relocated to
// `wallet/res_keypair.h`. The function used strong randomness, which // consensus-discipline-allow
// must never appear in any consensus-adjacent translation unit. If you
// need it, include `<wallet/res_keypair.h>` from a non-consensus path.
bool DeriveResWitnessProgramFromPubKey(const CPubKey& pubkey, std::vector<unsigned char>& program32_out);

} // namespace Consensus

#endif
