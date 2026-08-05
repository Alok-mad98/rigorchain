#ifndef BITCOIN_CONSENSUS_RES_PROOF_H
#define BITCOIN_CONSENSUS_RES_PROOF_H

#include <primitives/transaction.h>
#include <script/script.h>
#include <uint256.h>

#include <array>
#include <cstddef>
#include <cstdint>

class CBlock;
class CPubKey;

namespace Consensus {

/** Upper bound on raw RES proof bytes in witness (6 proof pushes). */
static constexpr size_t MAX_CONVERGE_PROOF_WITNESS_BYTES = 320;

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
 * PCORB proof payload (the whole witness, six `scriptWitness` pushes): convergence_txid (32) ‖
 * convergence_amount (8 LE) ‖ bind_hash (32) ‖ height (8 LE) ‖ target_prev (4 LE) ‖
 * convergence_root (32).
 *
 * CONSENSUS INVARIANT: ConvergeProof structure is reused across asset domains. However, its semantic
 * meaning is NOT encoded in the proof itself, but strictly determined by the input asset domain
 * (`GetTransactionInputAsset`). Any implementation that attempts to infer semantics from witness,
 * script path, or outputs is INVALID and will lead to consensus failure.
 *
 * This structure serves **Mint mode** only (ORU-domain body mint). SECURITY-001-A / A-7 removed the
 * RES-domain script-path claim that used to share it. Consensus meaning is still **only**
 * `f(GetTransactionInputAsset)` — see `rigor_res.cpp` / `convergence-consensus-v1.md` §4.1.
 *
 * **Field order is consensus-critical** (witness push order and in-memory layout). Do not reorder,
 * insert, or remove fields without a hard-forking protocol upgrade.
 *
 * **SECURITY-001-A / A-7: the proof carries no signature.** `wit[6]` and `wit[7]` used to hold a
 * detached Ed25519 signature and the key that verified it. The signed message was σ, which every
 * field above already pins to the block, and the verifying key came from the witness itself, so any
 * freshly generated keypair satisfied it — it authorized nothing while suggesting a "mint key" was a
 * security boundary. Nor is there anything left to authorize: given a block's convergence set, F,
 * the winner, the amount, and the payout script are all determined, and `validation.cpp` requires
 * exactly one mint whenever a winner exists.
 *
 * Do not reintroduce a signature here expecting it to gate the mint. The mint transaction is
 * authenticated by its own ORU funding input, whose signature commits to the whole transaction.
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

bool ParseConvergeProof(const CTransaction& tx, ConvergeProof& out);

/**
 * Weight binding: the proof's self-reported `convergence_amount_sat` must equal the amount the
 * drawn block actually committed for `convergence_txid`.
 *
 * SECURITY-001-A8: the caller supplies that amount from the draw set of block h-1, since the
 * winning convergence transaction is in the parent block rather than in the block being
 * validated. **ConnectBlock** separately enforces `convergence_txid ∈ F` — see
 * `rigor::ConnectEvaluateResMintTx`.
 */
bool CheckConvergeProofBinding(const ConvergeProof& proof, CAmount committed_convergence_amount);

/**
 * PCORB verification from witness payload only.
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
 *   constant `Consensus::Params::RES_PROBABILITY_TARGET`, value = floor(2^240 / MIN_CONVERGENCE_AMOUNT)),
 *   passed in explicitly so that this verification function remains
 *   stateless (RFC-005 §4.12 self-proving architecture).
 *
 *   SECURITY-001-A8: `proof.target_prev` no longer feeds σ. It is retained as a witness field and
 *   is still compared against the parent header by ConnectBlock, but B_{h-1} already commits the
 *   parent's nBits, so the comparison is redundancy rather than the binding itself.
 */
bool VerifyConvergeProof(const ConvergeProof& proof, const uint256& res_probability_target, const uint256* winner_txid, const uint256* expected_convergence_root);

/**
 * RFC-005 §4.12: RES Mint Verification
 * bool VerifyConvergeProof(const CTransaction& tx, const uint256& res_probability_target);
 *
 * This is the canonical interface per RFC-005 Validation State Machine.
 * Parses the transaction and verifies the converge proof.
 *
 * RFC-014 Conv-Decouple-01: `res_probability_target` is the constitutional
 * inequality RHS (= floor(2^240 / MIN_CONVERGENCE_AMOUNT), taken from `Consensus::Params::RES_PROBABILITY_TARGET`).
 */
bool VerifyConvergeProof(const CTransaction& tx, const uint256& res_probability_target);

uint256 ConvergeProofDigest(const uint256& convergence_txid, const uint256& bind_hash, uint64_t height, const uint256& convergence_root);

// TASK-H2 R-8: `GenerateResKeypair` has been relocated to
// `wallet/res_keypair.h`. The function used strong randomness, which // consensus-discipline-allow
// must never appear in any consensus-adjacent translation unit. If you
// need it, include `<wallet/res_keypair.h>` from a non-consensus path.
bool DeriveResWitnessProgramFromPubKey(const CPubKey& pubkey, std::vector<unsigned char>& program32_out);

} // namespace Consensus

#endif
