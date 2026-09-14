// Convergence RES consensus: OP_CONVERGE burn helpers (RFC-019).

#include <consensus/convergenceres.h>

#include <primitives/block.h>
#include <primitives/transaction.h>
#include <script/predicate_verify.h>
#include <script/script.h>
#include <script/standard.h>

namespace Consensus {

const CScript& ConvergenceOutputScript()
{
    static const CScript script = [] {
        std::vector<unsigned char> program(32, 0xa1);
        return GetConvergeScriptForPubKey(program);
    }();
    return script;
}

CScript ConvergenceOutputScript(const uint160& owner_hash160)
{
    return GetConvergeScript(owner_hash160);
}

bool IsConvergenceOutputScript(const CScript& script)
{
    return IsConvergenceScript(script);
}

int CountConvergenceTransactions(const CBlock& block)
{
    int n = 0;
    for (size_t i = 1; i < block.vtx.size(); ++i) {
        if (IsConvergenceTx(*block.vtx[i])) ++n;
    }
    return n;
}

CAmount GetOutputValueByAsset(const CTransaction& tx, AssetType asset)
{
    CAmount total = 0;
    for (const auto& tx_out : tx.vout) {
        if (AssetTypeFromScriptPubKey(tx_out.scriptPubKey) != asset) continue;
        total += tx_out.nValue;
    }
    return total;
}

} // namespace Consensus
