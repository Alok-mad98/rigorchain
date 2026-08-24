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
#include <script/rigor_tags.h>
#include <script/script.h>
#include <serialize.h>
#include <span.h>

#include <algorithm>
#include <cassert>
#include <cstring>

// PCORB has one locus: the **coinbase** of the block that pays the draw. The proof binds the
// convergence draw and ConvergenceRoot membership of the parent block. SECURITY-001-A / A-7
// removed the RES-input "claim" mode, whose script path authorized spends with a signature over σ
// instead of over the spending transaction; the carrier migration then removed the ORU-funded body
// mint as well. Nothing here reads a vin witness, and no body transaction can mint.

namespace Consensus {

// RFC-004 §2 / RFC-009 §1.1: little-endian, fixed width, no inner length prefixes.
static void DecodePcorbFields(const unsigned char* fields, ConvergeProof& out)
{
    out = ConvergeProof{};
    out.convergence_txid = uint256(Span<const unsigned char>(fields, 32));
    out.convergence_amount_sat = ReadLE64(fields + 32);  // RFC-000 extension (not in RFC-004 §10)
    out.bind_hash = uint256(Span<const unsigned char>(fields + 40, 32));
    out.height = ReadLE64(fields + 72);
    out.target_prev = ReadLE32(fields + 80);
    out.convergence_root = uint256(Span<const unsigned char>(fields + 84, 32));
}

bool IsPcorbCarrierScript(const CScript& script)
{
    // RFC-009 §0: one byte representation. The length test pins the whole script, so there is no
    // room for a trailing opcode, and 120 bytes has no minimal encoding other than PUSHDATA1.
    if (script.size() != RigorTags::PCORB_CARRIER_SCRIPT_BYTES) return false;
    if (script[0] != OP_RETURN) return false;
    if (script[1] != OP_PUSHDATA1) return false;
    if (script[2] != RigorTags::PCORB_OPRETURN_PAYLOAD_BYTES) return false;
    return std::equal(RigorTags::PCOR.begin(), RigorTags::PCOR.end(), script.begin() + 3);
}

bool IsPcorbCarrierOutput(const CTxOut& out)
{
    return out.nValue == 0 && IsPcorbCarrierScript(out.scriptPubKey);
}

size_t CountPcorbCarrierOutputs(const CTransaction& tx)
{
    size_t n = 0;
    for (const CTxOut& out : tx.vout) {
        if (IsPcorbCarrierOutput(out)) ++n;
    }
    return n;
}

bool ParseConvergeProof(const CTransaction& coinbase, ConvergeProof& out)
{
    out = ConvergeProof{};
    // The carrier is a coinbase-only channel. A body transaction carrying the tag is rejected by
    // CheckTxInputs; refusing to decode it here means no caller can be tricked into treating one
    // as issuance even if that rule were ever relaxed.
    if (!coinbase.IsCoinBase()) return false;
    const CTxOut* found = nullptr;
    for (const CTxOut& o : coinbase.vout) {
        if (!IsPcorbCarrierOutput(o)) continue;
        if (found != nullptr) return false; // two tags are not a proof, and not a choice
        found = &o;
    }
    if (found == nullptr) return false;
    DecodePcorbFields(found->scriptPubKey.data() + 3 + RigorTags::PCOR.size(), out);
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
 *
 * This is the canonical interface per RFC-005 Validation State Machine. It decodes the coinbase
 * carrier and evaluates the draw inequality on it.
 */
bool VerifyConvergeProof(const CTransaction& coinbase, const uint256& res_probability_target)
{
    ConvergeProof proof;
    if (!ParseConvergeProof(coinbase, proof)) {
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
    // out-of-range payload value simply fails the equality.
    return committed_convergence_amount == static_cast<CAmount>(proof.convergence_amount_sat);
}

bool DeriveResWitnessProgramFromPubKey(const CPubKey& pubkey, std::vector<unsigned char>& program32_out)
{
    program32_out.clear();
    const std::vector<unsigned char> pk32 = pubkey.GetEd25519Payload();
    if (pk32.size() != ed25519::PUBKEY_SIZE) return false;
    program32_out.resize(32);
    if (!ed25519::ResWitnessProgram(pk32, program32_out)) {
        program32_out.clear();
        return false;
    }
    return true;
}

// TASK-H2 R-8: `GenerateResKeypair` has been relocated to
// `wallet/res_keypair.cpp`. Keeping a randomness-using helper next to
// consensus code is a replay-equivalence hazard. See header comment.

} // namespace Consensus
