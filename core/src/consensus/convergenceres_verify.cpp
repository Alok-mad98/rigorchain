// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <consensus/convergenceres.h>

#include <arith_uint256.h>
#include <chain.h>
#include <consensus/amount.h>
#include <primitives/transaction.h>
#include <script/predicate_verify.h>
#include <script/script.h>
#include <script/standard.h>
#include <uint256.h>

namespace Consensus {

int CountConvergenceOutputs(const CTransaction& tx)
{
    int count = 0;
    for (const auto& out : tx.vout) {
        if (out.nValue > 0 && IsConvergenceScript(out.scriptPubKey)) {
            ++count;
        }
    }
    return count;
}

bool IsConvergenceTx(const CTransaction& tx)
{
    return CountConvergenceOutputs(tx) == 1;
}

CAmount GetConvergenceAmount(const CTransaction& tx)
{
    CAmount total{0};
    for (const auto& out : tx.vout) {
        if (!IsConvergenceScript(out.scriptPubKey)) continue;
        if (AddMoneyOverflows(total, out.nValue)) return 0;
        total += out.nValue;
    }
    return total;
}

} // namespace Consensus
