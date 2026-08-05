#include <consensus/res_proof.h>

#include <consensus/amount.h>
#include <consensus/rigor_math.h>
#include <consensus/convergenceres.h>
#include <consensus/draw_digest.h>
#include <crypto/common.h>
#include <crypto/ed25519_provider.h>
#include <hash.h>
#include <primitives/block.h>
#include <pubkey.h>
// TASK-H2 R-8: `<random.h>` deliberately removed. The only consumer of
// randomness in this TU was `GenerateResKeypair`, which has been moved to
// `wallet/res_keypair.cpp`. Re-adding `<random.h>` here would re-open a
// replay hazard and should be rejected at review.
#include <script/script.h>
#include <serialize.h>
#include <span.h>

#include <cassert>
#include <cstring>

// PCORB has one mode: **Mint (ORU inputs)** — the proof binds the convergence draw and ConvergenceRoot
// membership, and may score a body RES mint. SECURITY-001-A / A-7 removed the RES-input "claim" mode,
// whose script path authorized spends with a signature over σ instead of over the spending transaction.
// The domain is still taken **only** from `Consensus::GetTransactionInputAsset` — see `rigor_res.cpp` —
// never from witness branch labels, output counts, or the spending path.

namespace Consensus {

// RFC-009 §5: Proof serialization (Converge Proof)
// 5.1 Structure: [0] converge_txid (32), [1] bind_hash (32), [2] height (LE32), [3] target_prev (LE32),
//             [4] converge_root (32), [5] proof_blob (var), [6] signature (64)
// Rules: No length prefix inside fixed fields, proof_blob is opaque, signature MUST be exactly 64 bytes
//
// RFC-004 §10: ConvergeProof Serialization
// RFC-004 §2: Integer Rules - little-endian, fixed-width
bool ParseConvergeProof(const CTransaction& tx, ConvergeProof& out)
{
    out = ConvergeProof{};
    if (tx.IsCoinBase()) return false;
    if (tx.vin.empty()) return false;
    const auto& wit = tx.vin[0].scriptWitness.stack;
    if (wit.size() < 6) return false;
    // RFC-009 §0: Absolute principle - All consensus-critical data MUST have exactly ONE valid byte representation
    // RFC-004 §10: Exact field sizes
    if (wit[0].size() != 32 || wit[1].size() != 8 || wit[2].size() != 32 || wit[3].size() != 8 || wit[4].size() != 4 ||
        wit[5].size() != 32) {
        return false;
    }
    size_t nbytes = 32 + 8 + 32 + 8 + 4 + 32;
    // RFC-009 §1.1: Integer encoding (the sole rule)- uint32 → 4 bytes, little-endian, uint64 → 8 bytes, little-endian
    // RFC-004 §2: little-endian, fixed-width
    out.convergence_txid = uint256(wit[0]);
    out.convergence_amount_sat = ReadLE64(wit[1].data());  // RFC-000 extension (not in RFC-004 §10)
    out.bind_hash = uint256(wit[2]);
    out.height = ReadLE64(wit[3].data());
    out.target_prev = ReadLE32(wit[4].data());
    out.convergence_root = uint256(wit[5]);
    // SECURITY-001-A / A-7: no signature is parsed. `wit[6]`/`wit[7]` formerly held a detached
    // Ed25519 signature over σ plus the key verifying it; see `res_proof.h` for why that
    // authorized nothing. Pushes beyond the six proof fields are not part of the proof — for a
    // P2WSH-funded mint the trailing push is the redeem script.
    if (nbytes > MAX_CONVERGE_PROOF_WITNESS_BYTES) return false;
    return true;
}

// σ, the draw digest. Implementation follows RFC-000 §4 (Sigma-CR-01 revision); per Hierarchy Rule
// RFC-000-FINAL overrides RFC-004 §13.
//
// SECURITY-001-A / A-7: nothing signs this digest any more. It remains the draw's core — the
// eligibility inequality is evaluated on it — but a signature over it proved only key possession
// over public data. See `res_proof.h`.
//
// SECURITY-001-A8: `bind_hash` is the hash of block h-1, and `convergence_txid` / `convergence_root`
// describe the set that block committed, so the digest is pinned to one already-mined header.
uint256 ConvergeProofDigest(const uint256& convergence_txid, const uint256& bind_hash, uint64_t height, const uint256& convergence_root)
{
    // RFC-000 §4 (post-PoW-01): σ_i = Hash256( txid || C || B_{h-1} || LE32(h) )
    return CalculateSigmaHash(convergence_txid, convergence_root, bind_hash, static_cast<uint32_t>(height));
}

bool VerifyConvergeProof(const ConvergeProof& proof, const uint256& res_probability_target, const uint256* winner_txid, const uint256* expected_convergence_root)
{
    if (winner_txid != nullptr && proof.convergence_txid != *winner_txid) {
        return false;
    }
    if (expected_convergence_root != nullptr && proof.convergence_root != *expected_convergence_root) {
        return false;
    }
    const CAmount amt = static_cast<CAmount>(proof.convergence_amount_sat);
    if (!MoneyRange(amt) || amt < MIN_CONVERGENCE_AMOUNT) {
        return false;
    }
    const uint256 h = ConvergeProofDigest(proof.convergence_txid, proof.bind_hash, proof.height, proof.convergence_root);
    // RFC-014 Conv-Decouple-01: inequality RHS is the constitutional constant.
    return ConvergenceDrawInequalityHolds(h, res_probability_target, amt);
}

/**
 * RFC-005 §4.12: RES Mint Verification
 * bool VerifyConvergeProof(const CTransaction& tx);
 * 
 * This is the canonical interface per RFC-005 Validation State Machine.
 * Parses the transaction and verifies the converge proof.
 */
bool VerifyConvergeProof(const CTransaction& tx, const uint256& res_probability_target)
{
    ConvergeProof proof;
    if (!ParseConvergeProof(tx, proof)) {
        return false;
    }
    return VerifyConvergeProof(proof, res_probability_target, nullptr, nullptr);
}

bool CheckConvergeProofBinding(const ConvergeProof& proof, CAmount committed_convergence_amount)
{
    if (!MoneyRange(committed_convergence_amount) || committed_convergence_amount < MIN_CONVERGENCE_AMOUNT) return false;
    // SECURITY-001-A — the draw weight f must be the value actually committed on chain.
    // The committed amount was previously located and then discarded, so
    // `convergence_amount_sat` was free within [MIN_CONVERGENCE_AMOUNT, MAX_MONEY]: it feeds
    // both the σ < T·f inequality here and the argmin H/f comparison in ConnectBlock, so an
    // inflated self-reported weight bought roughly 24 bits of advantage over honest candidates.
    //
    // SECURITY-001-A8: the committed amount now comes from the draw set of block h-1 rather
    // than from a scan of the block being validated, because that is where the winning
    // convergence transaction lives.
    //
    // The uint64 -> CAmount narrowing is safe: the committed value is MoneyRange-valid, so any
    // out-of-range witness value simply fails the equality.
    return committed_convergence_amount == static_cast<CAmount>(proof.convergence_amount_sat);
}

bool DeriveResWitnessProgramFromPubKey(const CPubKey& pubkey, std::vector<unsigned char>& program32_out)
{
    program32_out.clear();
    if (!pubkey.IsCompressed() || pubkey.size() != CPubKey::COMPRESSED_SIZE) return false;
    if (pubkey[0] != 0x00 && pubkey[0] != 0xFF) return false;
    program32_out.assign(pubkey.begin() + 1, pubkey.end());
    return program32_out.size() == ed25519::PUBKEY_SIZE;
}

// TASK-H2 R-8: `GenerateResKeypair` has been relocated to
// `wallet/res_keypair.cpp`. Keeping a randomness-using helper next to
// consensus code is a replay-equivalence hazard. See header comment.

} // namespace Consensus
