// Convergence RES consensus: draw target, integer β convergence weights (minimal core).

#include <consensus/convergenceres.h>

#include <consensus/amount.h>
#include <consensus/rigor_math.h>
#include <consensus/params.h>
#include <arith_uint256.h>
#include <chain.h>
#include <hash.h>
#include <primitives/block.h>
#include <primitives/transaction.h>
#include <script/script.h>
#include <script/standard.h>
#include <uint256.h>

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <array>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>

namespace Consensus {

arith_uint256 ResMintTargetArith(uint8_t bits)
{
    arith_uint256 max;
    max.SetHex("ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
    return max >> static_cast<unsigned int>(bits);
}

uint256 InitialResDrawTarget(const Params& consensusParams)
{
    return ArithToUint256(ResMintTargetArith(consensusParams.res_mint_target_bits));
}

const CScript& ConvergenceOutputScript()
{
    static const std::vector<unsigned char> kDefaultconvergenceProgram = [] {
        std::vector<unsigned char> p(32, 0x00);
        // MUST match RigorTags::CVG2 prefix — `MatchCvg2WitnessScript` / RFC-000 §13 CVG2 container.
        p[0] = 0x43; // C
        p[1] = 0x56; // V
        p[2] = 0x47; // G
        p[3] = 0x32; // 2
        return p;
    }();
    static const CScript script = CScript() << OP_1 << kDefaultconvergenceProgram;
    return script;
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

// RFC-000 §7: f_i MUST NOT be transformed. These helpers now return f directly
// (identity mapping) rather than the removed β = 7/5 weight. The surrounding
// selection call site compares H < T · f at 512-bit precision.
arith_uint256 PowConvergenceAmount(CAmount amount)
{
    if (amount < MIN_CONVERGENCE_AMOUNT || !MoneyRange(amount)) {
        return arith_uint256(0);
    }
    return arith_uint256(static_cast<uint64_t>(amount));
}

uint64_t ConvergenceDrawEffectiveWeightU64(CAmount convergence_amount)
{
    if (convergence_amount < MIN_CONVERGENCE_AMOUNT || !MoneyRange(convergence_amount)) {
        return 0;
    }
    return static_cast<uint64_t>(convergence_amount);
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

uint256 ResDrawTargetForBlock(const CBlockIndex* pindex_prev, const Params& consensusParams)
{
    (void)pindex_prev;
    return InitialResDrawTarget(consensusParams);
}

uint256 MaxResDrawTarget()
{
    return ArithToUint256(ResMintTargetArith(0));
}

CAmount ComputeResReward(const CBlock& block, const CBlockIndex* pindex_prev, int nHeight, const Params& consensusParams)
{
    // RES is minted only via self-proof transactions in the block body (see consensus/rigor_res.cpp).
    (void)block;
    (void)pindex_prev;
    (void)nHeight;
    (void)consensusParams;
    return 0;
}

uint256 GetOruOnlyCoinbaseTxId(const CTransaction& coinbase_tx)
{
    (void)coinbase_tx;
    throw std::runtime_error(
        "Deprecated: GetOruOnlyCoinbaseTxId — coinbase-derived Res mint is forbidden; consensus uses rigor_res + res_proof only");
}

uint256 ResMintWitnessProgram(const uint256& oru_only_coinbase_txid, const uint256& parent_block_hash)
{
    (void)oru_only_coinbase_txid;
    (void)parent_block_hash;
    throw std::runtime_error(
        "Deprecated: ResMintWitnessProgram — coinbase-derived Res mint is forbidden; consensus uses rigor_res + res_proof only");
}

CScript ResMintScriptPubKey(const uint256& oru_only_coinbase_txid, const uint256& parent_block_hash)
{
    (void)oru_only_coinbase_txid;
    (void)parent_block_hash;
    throw std::runtime_error(
        "Deprecated: ResMintScriptPubKey — coinbase-derived Res mint is forbidden; consensus uses rigor_res + res_proof only");
}

bool IsValidResMintOutput(const CTxOut& out, const uint256& oru_only_cb_txid, const uint256& parent_block_hash)
{
    (void)out;
    (void)oru_only_cb_txid;
    (void)parent_block_hash;
    throw std::runtime_error(
        "Deprecated: IsValidResMintOutput — coinbase-derived Res mint is forbidden; consensus uses rigor_res + res_proof only");
}

} // namespace Consensus
