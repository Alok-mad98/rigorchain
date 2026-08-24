// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <consensus/res_mint_carrier.h>

#include <crypto/common.h>
#include <script/rigor_tags.h>
#include <script/script.h>

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
    // A 120-byte push serializes as OP_PUSHDATA1, which is the only encoding the parser accepts.
    return CScript() << OP_RETURN << BuildPcorbPayload(convergence_txid, convergence_amount_sat,
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
