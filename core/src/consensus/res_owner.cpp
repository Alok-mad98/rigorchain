// Copyright (c) The Rigor Chain developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <consensus/res_owner.h>

#include <primitives/transaction.h>
#include <script/interpreter.h>
#include <script/script.h>
#include <script/standard.h>

#include <algorithm>

namespace Consensus {

std::optional<ResOwnerDeclaration> GetResOwnerDeclaration(const CTransaction& tx)
{
    std::optional<ResOwnerDeclaration> found;
    for (const auto& out : tx.vout) {
        if (out.nValue <= 0) continue;
        uint160 hash;
        if (!ParseConvergeScript(out.scriptPubKey, hash)) continue;
        if (found.has_value()) return std::nullopt;
        found = ResOwnerDeclaration{hash};
    }
    return found;
}

bool HasResOwnerDeclaration(const CTransaction& tx)
{
    return GetResOwnerDeclaration(tx).has_value();
}

bool CheckResRewardOwner(const uint160& declared_hash160, const CScript& payout_script)
{
    int64_t maturity_height = 0;
    std::vector<unsigned char> pubkey_hash;
    if (!ParseResMintScript(payout_script, maturity_height, pubkey_hash)) return false;
    return pubkey_hash.size() == declared_hash160.size() &&
           std::equal(declared_hash160.begin(), declared_hash160.end(), pubkey_hash.begin());
}

} // namespace Consensus
