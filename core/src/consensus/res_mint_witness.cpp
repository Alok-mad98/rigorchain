// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <consensus/res_mint_witness.h>

#include <crypto/common.h>

CScriptWitness BuildPcorbBodyResMintWitness(const uint256& convergence_txid,
    CAmount convergence_amount_sat,
    const uint256& bind_hash,
    uint64_t proof_height,
    uint32_t target_prev,
    const uint256& convergence_root)
{
    CScriptWitness wit;
    wit.stack.reserve(PCORB_BODY_RES_MINT_WITNESS_ITEMS);
    wit.stack.emplace_back(convergence_txid.begin(), convergence_txid.end());
    {
        unsigned char amt_le[8];
        WriteLE64(amt_le, static_cast<uint64_t>(convergence_amount_sat));
        wit.stack.emplace_back(amt_le, amt_le + sizeof(amt_le));
    }
    wit.stack.emplace_back(bind_hash.begin(), bind_hash.end());
    {
        unsigned char hle[8];
        WriteLE64(hle, proof_height);
        wit.stack.emplace_back(hle, hle + sizeof(hle));
    }
    {
        unsigned char tle[4];
        WriteLE32(tle, target_prev);
        wit.stack.emplace_back(tle, tle + sizeof(tle));
    }
    wit.stack.emplace_back(convergence_root.begin(), convergence_root.end());
    return wit;
}
