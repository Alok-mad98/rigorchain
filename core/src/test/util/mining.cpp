// Copyright (c) 2019-2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <test/util/mining.h>

#include <algorithm>
#include <chainparams.h>
#include <coins.h>
#include <consensus/convergence_root.h>
#include <consensus/merkle.h>
#include <consensus/validation.h>
#include <key_io.h>
#include <node/context.h>
#include <pow.h>
#include <script/rigor_tags.h>
#include <script/standard.h>
#include <test/util/script.h>
#include <util/check.h>
#include <validation.h>
#include <versionbits.h>

using node::ApplyArgsManOptions;
using node::BlockAssembler;
using node::NodeContext;

CTxIn generatetoaddress(const NodeContext& node, const std::string& address)
{
    const auto dest = DecodeDestination(address);
    assert(IsValidDestination(dest));
    const auto coinbase_script = GetScriptForDestination(dest);

    return MineBlock(node, coinbase_script);
}

std::vector<std::shared_ptr<CBlock>> CreateBlockChain(size_t total_height, const CChainParams& params)
{
    std::vector<std::shared_ptr<CBlock>> ret{total_height};
    auto time{params.GenesisBlock().nTime};
    for (size_t height{0}; height < total_height; ++height) {
        CBlock& block{*(ret.at(height) = std::make_shared<CBlock>())};

        CMutableTransaction coinbase_tx;
        coinbase_tx.vin.resize(1);
        coinbase_tx.vin[0].prevout.SetNull();
        coinbase_tx.vout.resize(1);
        coinbase_tx.vout[0].scriptPubKey = P2WSH_OP_TRUE;
        coinbase_tx.vout[0].nValue = GetBlockSubsidy(height + 1, params.GetConsensus());
        block.vtx = {MakeTransactionRef(coinbase_tx)};
        // A-9-C: elsewhere a coinbase must commit the WINDOW root via GetTemplateConvergenceRoot,
        // because C spans CONVERGENCE_DRAW_WINDOW blocks. The block-local call is correct here and
        // only here: these blocks are coinbase-only, so F is empty at every height and the window
        // root and the block-local root are both null. There is also no CBlockIndex to walk — this
        // chain is synthesised before anything is connected. Do not copy this into a producer that
        // can carry convergence transactions.
        CCoinsView convergence_root_dummy_base;
        CCoinsViewCache convergence_root_dummy(&convergence_root_dummy_base);
        const uint256 convergence_root = Consensus::ComputeConvergenceRootFromConvergenceSet(
            block.vtx, convergence_root_dummy, static_cast<int>(height + 1), params.GetConsensus());
        assert(convergence_root.IsNull());
        coinbase_tx = CMutableTransaction(*block.vtx[0]);
        coinbase_tx.vin[0].scriptSig = Consensus::BuildCoinbaseCommitmentScriptSig(
            static_cast<int>(height + 1), convergence_root, uint256{});
        block.vtx[0] = MakeTransactionRef(std::move(coinbase_tx));

        block.nVersion = VERSIONBITS_LAST_OLD_BLOCK_VERSION;
        block.hashPrevBlock = (height >= 1 ? *ret.at(height - 1) : params.GenesisBlock()).GetHash();
        block.hashMerkleRoot = BlockMerkleRoot(block);
        block.nTime = ++time;
        block.nBits = params.GenesisBlock().nBits;
        block.nNonce = 0;

        while (!CheckProofOfWork(block, block.nBits, params.GetConsensus())) {
            ++block.nNonce;
            assert(block.nNonce);
        }
    }
    return ret;
}

CTxIn MineBlock(const NodeContext& node, const CScript& coinbase_scriptPubKey)
{
    auto block = PrepareBlock(node, coinbase_scriptPubKey);

    while (!CheckProofOfWork(*block, block->nBits, Params().GetConsensus())) {
        ++block->nNonce;
        assert(block->nNonce);
    }

    bool processed{Assert(node.chainman)->ProcessNewBlock(block, true, true, nullptr)};
    assert(processed);

    return CTxIn{block->vtx[0]->GetHash(), 0};
}

std::shared_ptr<CBlock> PrepareBlock(const NodeContext& node, const CScript& coinbase_scriptPubKey,
                                     const BlockAssembler::Options& assembler_options)
{
    auto block = std::make_shared<CBlock>(
        BlockAssembler{Assert(node.chainman)->ActiveChainstate(), Assert(node.mempool.get()), assembler_options}
            .CreateNewBlock(coinbase_scriptPubKey)
            ->block);

    LOCK(cs_main);
    block->nTime = Assert(node.chainman)->ActiveChain().Tip()->GetMedianTimePast() + 1;
    block->hashMerkleRoot = BlockMerkleRoot(*block);

    return block;
}
std::shared_ptr<CBlock> PrepareBlock(const NodeContext& node, const CScript& coinbase_scriptPubKey)
{
    BlockAssembler::Options assembler_options;
    ApplyArgsManOptions(*node.args, Assert(node.chainman)->GetParams(), assembler_options);
    return PrepareBlock(node, coinbase_scriptPubKey, assembler_options);
}

static bool VoutLooksLikeConvergenceCommitment(const CTxOut& out)
{
    const CScript& s = out.scriptPubKey;
    CScript::const_iterator pc = s.begin();
    opcodetype opcode;
    std::vector<unsigned char> vch;
    if (!s.GetOp(pc, opcode) || opcode != RIGOR_MARKER_RETURN) return false;
    if (!s.GetOp(pc, opcode, vch)) return false;
    return vch.size() >= 4 && vch[0] == 0x43 && vch[1] == 0x56 && vch[2] == 0x47 && vch[3] == 0x45;
}

static bool VoutLooksLikeRigorDrawTargetCommitment(const CTxOut& out)
{
    const CScript& s = out.scriptPubKey;
    CScript::const_iterator pc = s.begin();
    opcodetype opcode;
    std::vector<unsigned char> vch;
    if (!s.GetOp(pc, opcode) || opcode != RIGOR_MARKER_RETURN) return false;
    if (!s.GetOp(pc, opcode, vch)) return false;
    if (vch.size() != RigorTags::RIGOR_COMMITMENT_PAYLOAD_SIZE) return false;
    return vch.size() >= RigorTags::RIGOR.size() &&
           std::equal(RigorTags::RIGOR.begin(), RigorTags::RIGOR.end(), vch.begin());
}

void RefreshCoinbaseWitnessAndconvergence(CBlock& block, const CBlockIndex* pindexPrev, ChainstateManager& chainman)
{
    assert(pindexPrev != nullptr);

    CMutableTransaction mtx(*block.vtx[0]);
    for (int i = int(mtx.vout.size()) - 1; i >= 0; --i) {
        if (VoutLooksLikeConvergenceCommitment(mtx.vout[i]) || VoutLooksLikeRigorDrawTargetCommitment(mtx.vout[i])) {
            mtx.vout.erase(mtx.vout.begin() + i);
        }
    }
    block.vtx[0] = MakeTransactionRef(std::move(mtx));

    const int wpos = GetWitnessCommitmentIndex(block);
    if (wpos != NO_WITNESS_COMMITMENT) {
        mtx = CMutableTransaction(*block.vtx[0]);
        mtx.vout.erase(mtx.vout.begin() + wpos);
        block.vtx[0] = MakeTransactionRef(std::move(mtx));
    }

    chainman.GenerateCoinbaseCommitment(block, pindexPrev);
}
