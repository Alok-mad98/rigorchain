// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <consensus/convergenceres.h>

#include <arith_uint256.h>
#include <chain.h>
#include <consensus/amount.h>
#include <primitives/transaction.h>
#include <script/rigor_tags.h>
#include <script/interpreter.h>
#include <script/script.h>
#include <uint256.h>

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <limits>

namespace {

/**
 * RFC-000 §13: V1 CONTAINER LAW
 * ScriptPubKey = OP_1 <32 bytes>
 * semantic = f(input_asset_domain)
 * 
 * Forbidden: Taproot, MAST, control block, script path
 */
bool MatchCvg2WitnessScript(const CScript& scriptPubKey)
{
    int witnessversion;
    std::vector<unsigned char> program;
    if (!scriptPubKey.IsWitnessProgram(witnessversion, program)) return false;
    // RFC-000 §13: witness version 1, program size 32 bytes (OP_1 <32 bytes>)
    if (witnessversion != 1 || program.size() != WITNESS_V1_RES_SIZE) return false;
    return std::equal(RigorTags::CVG2.begin(), RigorTags::CVG2.end(), program.begin());
}

} // namespace

namespace Consensus {

int CountConvergenceOutputs(const CTransaction& tx)
{
    int count = 0;
    for (const auto& out : tx.vout) {
        if (out.nValue > 0 && MatchCvg2WitnessScript(out.scriptPubKey)) {
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
        if (!MatchCvg2WitnessScript(out.scriptPubKey)) continue;
        if (!MoneyRange(out.nValue)) return 0;
        if (!MoneyRange(total) || out.nValue > MAX_MONEY - total) return 0;
        total += out.nValue;
    }
    return total;
}

} // namespace Consensus
