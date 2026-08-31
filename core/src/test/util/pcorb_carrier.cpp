// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <test/util/pcorb_carrier.h>

#include <consensus/amount.h>
#include <consensus/convergenceres.h>
#include <consensus/draw_digest.h>
#include <consensus/res_proof.h>
#include <consensus/rigor_math.h>
#include <crypto/common.h>
#include <script/rigor_tags.h>
#include <script/script.h>
#include <span.h>

#include <algorithm>
#include <cstring>
#include <vector>

namespace {
std::vector<unsigned char> BuildPcorbPayload(const uint256& convergence_txid,
    CAmount convergence_amount_sat,
    const uint256& bind_hash,
    uint64_t proof_height,
    uint32_t target_prev,
    const uint256& convergence_root)
{
    std::vector<unsigned char> payload(RigorTags::PCORB_OPRETURN_PAYLOAD_BYTES);
    unsigned char* p = payload.data();
    p = std::copy(RigorTags::PCOR.begin(), RigorTags::PCOR.end(), p);
    p = std::copy(convergence_txid.begin(), convergence_txid.end(), p);
    WriteLE64(p, static_cast<uint64_t>(convergence_amount_sat));
    p += 8;
    p = std::copy(bind_hash.begin(), bind_hash.end(), p);
    WriteLE64(p, proof_height);
    p += 8;
    WriteLE32(p, target_prev);
    p += 4;
    std::copy(convergence_root.begin(), convergence_root.end(), p);
    return payload;
}
} // namespace

CScript BuildPcorbCarrierScript(const uint256& convergence_txid,
    CAmount convergence_amount_sat,
    const uint256& bind_hash,
    uint64_t proof_height,
    uint32_t target_prev,
    const uint256& convergence_root)
{
    return CScript() << RIGOR_MARKER_RETURN << BuildPcorbPayload(convergence_txid, convergence_amount_sat,
        bind_hash, proof_height, target_prev, convergence_root);
}

CTxOut BuildPcorbCarrierOutput(const uint256& convergence_txid,
    CAmount convergence_amount_sat,
    const uint256& bind_hash,
    uint64_t proof_height,
    uint32_t target_prev,
    const uint256& convergence_root)
{
    return CTxOut{0, BuildPcorbCarrierScript(convergence_txid, convergence_amount_sat, bind_hash,
        proof_height, target_prev, convergence_root)};
}

namespace Consensus {

static void DecodePcorbFields(const unsigned char* fields, ConvergeProof& out)
{
    out = ConvergeProof{};
    out.convergence_txid = uint256(Span<const unsigned char>(fields, 32));
    out.convergence_amount_sat = ReadLE64(fields + 32);
    out.bind_hash = uint256(Span<const unsigned char>(fields + 40, 32));
    out.height = ReadLE64(fields + 72);
    out.target_prev = ReadLE32(fields + 80);
    out.convergence_root = uint256(Span<const unsigned char>(fields + 84, 32));
}

bool ParseConvergeProof(const CTransaction& coinbase, ConvergeProof& out)
{
    out = ConvergeProof{};
    if (!coinbase.IsCoinBase()) return false;
    const CTxOut* found = nullptr;
    for (const CTxOut& o : coinbase.vout) {
        if (!IsPcorbCarrierOutput(o)) continue;
        if (found != nullptr) return false;
        found = &o;
    }
    if (found == nullptr) return false;
    DecodePcorbFields(found->scriptPubKey.data() + 3 + RigorTags::PCOR.size(), out);
    return true;
}

uint256 ConvergeProofDigest(const uint256& convergence_txid, const uint256& bind_hash, uint64_t height, const uint256& convergence_root)
{
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
    return ConvergenceDrawInequalityHolds(h, res_probability_target, amt);
}

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
    return committed_convergence_amount == static_cast<CAmount>(proof.convergence_amount_sat);
}

} // namespace Consensus
