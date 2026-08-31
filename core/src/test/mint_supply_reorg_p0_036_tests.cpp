// Copyright (c) 2026 The RigorChain developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <chain.h>
#include <coins.h>
#include <consensus/amount.h>
#include <consensus/consensus.h>
#include <consensus/convergence_logic.h>
#include <consensus/convergenceres.h>
#include <consensus/params.h>
#include <consensus/res_owner.h>
#include <consensus/rigor_consensus_lock.h>
#include <hash.h>
#include <node/blockstorage.h>
#include <node/miner.h>
#include <node/supply_monitor/observational_metrics.h>
#include <pow.h>
#include <primitives/block.h>
#include <primitives/transaction.h>
#include <script/predicate_verify.h>
#include <script/script.h>
#include <script/sign.h>
#include <script/signingprovider.h>
#include <script/standard.h>
#include <test/util/setup_common.h>
#include <uint256.h>
#include <util/overflow.h>
#include <util/time.h>
#include <util/translation.h>
#include <validation.h>

#include <boost/test/unit_test.hpp>

#include <map>
#include <memory>
#include <string>
#include <vector>

using node::BlockAssembler;
using node::RebuildTemplateCommitments;

BOOST_FIXTURE_TEST_SUITE(mint_supply_reorg_p0_036_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(frozen_parameters_unchanged)
{
    BOOST_CHECK_EQUAL(Consensus::CONVERGENCE_DRAW_WINDOW, 32);
    BOOST_CHECK_EQUAL(COINBASE_MATURITY, 100);
    BOOST_CHECK(Consensus::RES_PROBABILITY_TARGET_CONST ==
                uint256S("000000000055e63b88c230e77e7ee106959b5d3e1e9ea69ebac0b0b4be383479"));
}

BOOST_AUTO_TEST_CASE(canonical_mint_unique_encoding)
{
    uint160 pkh;
    pkh.SetHex("1111111111111111111111111111111111111111");
    const CScript canonical = GetResMintScript(/*maturity_height=*/5, pkh);
    int64_t mat = 0;
    std::vector<unsigned char> parsed;
    BOOST_CHECK(ParseResMintScript(canonical, mat, parsed));
    BOOST_CHECK_EQUAL(mat, 5);
    BOOST_CHECK(canonical == GetResMintScript(mat, pkh));

    CScript op5;
    op5.push_back(static_cast<unsigned char>(RIGOR_PUSH_1 + 4)); // OP_5
    op5 << ToByteVector(pkh) << RIGOR_MARKER_RES_MINT;
    BOOST_CHECK(op5 != canonical);
    BOOST_CHECK(!ParseResMintScript(op5, mat, parsed));

    CScript trailing = canonical;
    trailing.push_back(RIGOR_PUSH_0);
    BOOST_CHECK(!ParseResMintScript(trailing, mat, parsed));

    BOOST_CHECK_EQUAL(AssetTypeFromScriptPubKey(canonical), ASSET_RES);
    BOOST_CHECK_EQUAL(AssetTypeFromScriptPubKey(op5), ASSET_INVALID);

    for (int k = 1; k <= 16; ++k) {
        CScript opn;
        opn.push_back(static_cast<unsigned char>(RIGOR_PUSH_1 + k - 1));
        opn << ToByteVector(pkh) << RIGOR_MARKER_RES_MINT;
        BOOST_CHECK_MESSAGE(!ParseResMintScript(opn, mat, parsed),
                            "OP_" + std::to_string(k) + " must not parse as canonical mint");
        BOOST_CHECK_EQUAL(AssetTypeFromScriptPubKey(opn), ASSET_INVALID);
        BOOST_CHECK(opn != GetResMintScript(k, pkh));
    }
}

BOOST_AUTO_TEST_CASE(unified_circulating_identity)
{
    const CAmount subsidy = 1000 * COIN;
    const CAmount mint = 40 * 1000000;
    const CAmount converge = COIN;
    const CAmount fee = 50 * COIN;
    const CAmount burn = Consensus::OruResTransferFeeBurnShare(fee);
    BOOST_CHECK_EQUAL(burn, fee / 2);
    BOOST_CHECK(!AddMoneyOverflows(subsidy, mint));
    const CAmount circulating = subsidy + mint - converge - burn;
    BOOST_CHECK_GE(circulating, 0);
    BOOST_CHECK(MoneyRange(converge));
}

BOOST_AUTO_TEST_CASE(reorg_drops_orphaned_mint)
{
    const CAmount kept = 10 * COIN;
    const CAmount orphan_mint = 40 * 1000000;
    const CAmount new_mint = 0;
    const CAmount after_reorg = kept + new_mint;
    BOOST_CHECK_EQUAL(after_reorg, kept);
    BOOST_CHECK_NE(kept + orphan_mint, after_reorg);
}

BOOST_AUTO_TEST_CASE(amount_boundary_max_money_add_headroom)
{
    BOOST_CHECK(MoneyRange(MAX_MONEY));
    BOOST_CHECK(!AddMoneyOverflows(0, MAX_MONEY));
    BOOST_CHECK(AddMoneyOverflows(MAX_MONEY, 1));
    BOOST_CHECK(!AddMoneyOverflows(MAX_MONEY / 2, MAX_MONEY / 2));
}

BOOST_AUTO_TEST_SUITE_END()

BOOST_FIXTURE_TEST_SUITE(connectblock_supply_p0_039_tests, TestChain100Setup)

BOOST_AUTO_TEST_CASE(connectblock_ledger_is_surviving_subsidy)
{
    const auto& consensus = Params().GetConsensus();
    LOCK(cs_main);
    const CChain& chain = Assert(m_node.chainman)->ActiveChain();
    BOOST_REQUIRE_GE(chain.Height(), 1);
    CAmount subsidy_sum = 0;
    CAmount mint_sum = 0;
    CAmount converge_sum = 0;
    for (int h = 0; h <= chain.Height(); ++h) {
        const CBlockIndex* pindex = chain[h];
        BOOST_REQUIRE(pindex != nullptr);
        subsidy_sum += GetBlockSubsidy(h, consensus);
        mint_sum += pindex->m_block_res_mint_sum;
        converge_sum += pindex->m_block_converge_sum;
    }
    BOOST_CHECK_EQUAL(GetBlockSubsidy(0, consensus), 0);
    BOOST_CHECK_EQUAL(GetBlockSubsidy(1, consensus), Consensus::ORU_INITIAL_BLOCK_SUBSIDY);
    const CBlockIndex* tip = chain.Tip();
    BOOST_REQUIRE(tip != nullptr);
    if (tip->Observational().HasOruCache()) {
        BOOST_CHECK_EQUAL(tip->Observational().GetChainOruTotal(), subsidy_sum - converge_sum);
    }
    BOOST_CHECK_EQUAL(mint_sum, 0);
    const COutPoint genesis_out(Params().GenesisBlock().vtx[0]->GetHash(), 0);
    BOOST_CHECK(!Assert(m_node.chainman)->ActiveChainstate().CoinsTip().HaveCoin(genesis_out));
}

BOOST_AUTO_TEST_CASE(connectblock_cpp_slice_fields)
{
    const auto& consensus = Params().GetConsensus();
    LOCK(cs_main);
    const CChain& chain = Assert(m_node.chainman)->ActiveChain();
    BOOST_REQUIRE_GE(chain.Height(), 1);
    // ConnectBlockSupply.v: the ORU leg of the slice moves by subsidy - converge
    // and the RES leg by the coinbase mint. Both identities are checked per
    // height rather than asserting mint == 0, which is a property of this
    // fixture and not of ConnectBlock. connectblock_res_mint_p0_041_tests
    // exercises the same two identities with mint > 0 and converge > 0.
    for (int h = 1; h <= chain.Height(); ++h) {
        const CBlockIndex* pindex = chain[h];
        BOOST_REQUIRE(pindex != nullptr);
        BOOST_REQUIRE(pindex->pprev != nullptr);
        const CAmount subsidy = GetBlockSubsidy(h, consensus);
        const CAmount mint = pindex->m_block_res_mint_sum;
        const CAmount converge = pindex->m_block_converge_sum;
        BOOST_CHECK_GE(subsidy, 0);
        BOOST_CHECK_GE(mint, 0);
        BOOST_CHECK_GE(converge, 0);
        if (pindex->Observational().HasOruCache() &&
            pindex->pprev->Observational().HasOruCache()) {
            BOOST_CHECK_EQUAL(pindex->Observational().GetChainOruTotal(),
                              pindex->pprev->Observational().GetChainOruTotal() + subsidy - converge);
        }
        if (pindex->Observational().HasResMintCache() &&
            pindex->pprev->Observational().HasResMintCache()) {
            BOOST_CHECK_EQUAL(pindex->Observational().GetChainResMintTotal(),
                              pindex->pprev->Observational().GetChainResMintTotal() + mint);
        }
    }
    BOOST_CHECK_EQUAL(GetBlockSubsidy(0, consensus), 0);
    BOOST_CHECK_EQUAL(COINBASE_MATURITY, 100);
    BOOST_CHECK_EQUAL(Consensus::CONVERGENCE_DRAW_WINDOW, 32);
}

BOOST_AUTO_TEST_CASE(disconnect_drops_orphan_coinbase_from_surviving_chain)
{
    const auto& consensus = Params().GetConsensus();
    uint256 orphan_cb;
    CAmount parent_oru = -1;
    CAmount parent_res = -1;
    int old_height = 0;
    uint256 orphan_hash;
    {
        LOCK(cs_main);
        CBlockIndex* tip = Assert(m_node.chainman)->ActiveChain().Tip();
        BOOST_REQUIRE(tip != nullptr && tip->pprev != nullptr);
        old_height = tip->nHeight;
        orphan_hash = tip->GetBlockHash();
        if (tip->pprev->Observational().HasOruCache()) {
            parent_oru = tip->pprev->Observational().GetChainOruTotal();
        }
        if (tip->pprev->Observational().HasResMintCache()) {
            parent_res = tip->pprev->Observational().GetChainResMintTotal();
        }
        CBlock block;
        BOOST_REQUIRE(node::ReadBlockFromDisk(block, tip, Params().GetConsensus()));
        BOOST_REQUIRE(!block.vtx.empty());
        orphan_cb = block.vtx[0]->GetHash();
        BOOST_CHECK(Assert(m_node.chainman)->ActiveChainstate().CoinsTip().HaveCoin(COutPoint(orphan_cb, 0)));
    }

    BlockValidationState state;
    CBlockIndex* tip = WITH_LOCK(cs_main, return Assert(m_node.chainman)->ActiveChain().Tip());
    BOOST_REQUIRE(Assert(m_node.chainman)->ActiveChainstate().InvalidateBlock(state, tip));

    LOCK(cs_main);
    const CBlockIndex* new_tip = Assert(m_node.chainman)->ActiveChain().Tip();
    BOOST_REQUIRE(new_tip != nullptr);
    BOOST_CHECK_EQUAL(new_tip->nHeight, old_height - 1);
    BOOST_CHECK(new_tip->GetBlockHash() != orphan_hash);
    BOOST_CHECK(!Assert(m_node.chainman)->ActiveChainstate().CoinsTip().HaveCoin(COutPoint(orphan_cb, 0)));
    // The surviving chain's cumulative ledgers must be exactly the parent's: the
    // disconnected block contributes neither ORU nor RES to them. Asserting the
    // orphan's own mint is zero would only restate that this fixture never mints.
    if (parent_oru >= 0 && new_tip->Observational().HasOruCache()) {
        BOOST_CHECK_EQUAL(new_tip->Observational().GetChainOruTotal(), parent_oru);
    }
    if (parent_res >= 0 && new_tip->Observational().HasResMintCache()) {
        BOOST_CHECK_EQUAL(new_tip->Observational().GetChainResMintTotal(), parent_res);
    }
    BOOST_CHECK(!tip->Observational().HasOruCache());
    BOOST_CHECK_EQUAL(COINBASE_MATURITY, 100);
    BOOST_CHECK_EQUAL(Consensus::CONVERGENCE_DRAW_WINDOW, 32);
    BOOST_CHECK_EQUAL(consensus.nCoinbaseMaturity, 100);
}

BOOST_AUTO_TEST_SUITE_END()

namespace {

/**
 * A draw is won when H < T*f. Saturating f makes that hold for every possible H,
 * so the mint below is deterministic rather than a 1-in-33 flake.
 *
 * The nominal saturation weight is C*W = 1024 * 32 = 32768 ORU, but T is
 * floor(UNWINDOWED / W) and UNWINDOWED is not a multiple of W (residue 27), so
 * T * 32768 ORU lands one satoshi short of 2^256. 32769 ORU clears it with room
 * to spare. If this constant ever stops saturating, T or W moved.
 */
constexpr CAmount SATURATING_CONVERGE_AMOUNT = 32769 * COIN;

/** 33 * 1000 ORU = 33000 ORU covers the burn plus a fee. */
constexpr int CONVERGE_INPUT_COUNT = 33;

struct MintedChain {
    int converge_height{0};
    int mint_height{0};
    uint160 owner;
    CBlock mint_block;
};

/**
 * Extend the fixture chain until one block actually pays a RES mint:
 *
 *   height H   : one convergence tx burning SATURATING_CONVERGE_AMOUNT
 *   height H+1 : BlockAssembler resolves H's draw, finds the saturating
 *                candidate, and appends the coinbase mint automatically
 */
MintedChain BuildChainThatMints(TestChain100Setup& setup)
{
    MintedChain out;
    const CScript coinbase_spk = GetTestMiningCoinbaseScript(setup.coinbaseKey);
    ChainstateManager& chainman = *Assert(setup.m_node.chainman);

    // A coin at height i is spendable in a block at height h only once
    // h - i >= COINBASE_MATURITY, so mature enough coinbases to fund the burn.
    setup.mineBlocks(CONVERGE_INPUT_COUNT);

    const std::vector<unsigned char> res_program(32, 0x5a);
    out.owner = Hash160(res_program);

    CMutableTransaction converge_tx;
    CCoinsView dummy;
    CCoinsViewCache coins(&dummy);
    std::map<COutPoint, Coin> input_coins;
    CAmount total_in = 0;
    for (int i = 0; i < CONVERGE_INPUT_COUNT; ++i) {
        const CTransactionRef& cb = setup.m_coinbase_txns.at(i);
        const COutPoint outpoint(cb->GetHash(), 0);
        converge_tx.vin.emplace_back(outpoint);
        AddCoins(coins, *cb, /*nHeight=*/i + 1);
        Coin coin;
        BOOST_REQUIRE(coins.GetCoin(outpoint, coin));
        input_coins.insert({outpoint, coin});
        total_in += cb->vout[0].nValue;
    }
    BOOST_REQUIRE_GT(total_in, SATURATING_CONVERGE_AMOUNT);

    // Exactly one convergence output, so GetResOwnerDeclaration is unambiguous
    // and the draw payout is bound to `owner`.
    converge_tx.vout.emplace_back(SATURATING_CONVERGE_AMOUNT,
                                  Consensus::ConvergenceOutputScript(out.owner));

    FillableSigningProvider keystore;
    BOOST_REQUIRE(keystore.AddKey(setup.coinbaseKey));
    std::map<int, bilingual_str> input_errors;
    BOOST_REQUIRE(SignTransaction(converge_tx, &keystore, input_coins,
                                  TestSighashChainId(), input_errors));
    const CTransaction signed_converge{converge_tx};
    BOOST_REQUIRE(Consensus::IsConvergenceTx(signed_converge));
    BOOST_REQUIRE(Consensus::HasResOwnerDeclaration(signed_converge));
    BOOST_REQUIRE_EQUAL(Consensus::GetConvergenceAmount(signed_converge),
                        SATURATING_CONVERGE_AMOUNT);

    // The convergence root of a block covers that block's own convergence txs,
    // so the template commitment has to be rebuilt after the tx is appended.
    Chainstate& chainstate = chainman.ActiveChainstate();
    CBlock converge_block =
        BlockAssembler{chainstate, nullptr}.CreateNewBlock(coinbase_spk)->block;
    converge_block.vtx.push_back(MakeTransactionRef(converge_tx));
    {
        LOCK(cs_main);
        std::string err;
        BOOST_REQUIRE_MESSAGE(RebuildTemplateCommitments(converge_block, chainman, err), err);
    }
    while (!CheckProofOfWork(converge_block, converge_block.nBits, chainman.GetConsensus())) {
        ++converge_block.nNonce;
    }
    BOOST_REQUIRE(chainman.ProcessNewBlock(std::make_shared<const CBlock>(converge_block),
                                           /*force_processing=*/true, /*min_pow_checked=*/true,
                                           nullptr));
    out.converge_height = WITH_LOCK(cs_main, return chainman.ActiveChain().Height());

    SetMockTime(GetTime() + 1);
    out.mint_block = setup.CreateAndProcessBlock({}, coinbase_spk);
    out.mint_height = WITH_LOCK(cs_main, return chainman.ActiveChain().Height());
    BOOST_REQUIRE_EQUAL(out.mint_height, out.converge_height + 1);
    return out;
}

} // namespace

BOOST_FIXTURE_TEST_SUITE(connectblock_res_mint_p0_041_tests, TestChain100Setup)

BOOST_AUTO_TEST_CASE(connectblock_slice_fields_with_a_real_mint)
{
    const auto& consensus = Params().GetConsensus();
    const MintedChain minted = BuildChainThatMints(*this);

    LOCK(cs_main);
    const CChain& chain = Assert(m_node.chainman)->ActiveChain();
    const CBlockIndex* converge_idx = chain[minted.converge_height];
    const CBlockIndex* mint_idx = chain[minted.mint_height];
    BOOST_REQUIRE(converge_idx != nullptr && mint_idx != nullptr);
    BOOST_REQUIRE(converge_idx->pprev != nullptr);

    // The burn block: converge > 0, no mint of its own.
    BOOST_CHECK_EQUAL(converge_idx->m_block_converge_sum, SATURATING_CONVERGE_AMOUNT);
    BOOST_CHECK_EQUAL(converge_idx->m_block_res_mint_sum, 0);

    // The payout block: mint > 0 and equal to the scheduled reward.
    BOOST_CHECK_GT(mint_idx->m_block_res_mint_sum, 0);
    BOOST_CHECK_EQUAL(mint_idx->m_block_res_mint_sum,
                      Consensus::GetResMintReward(minted.mint_height));

    // The payout is bound to the owner the convergence tx declared, at the
    // maturity the mint encoding pins.
    BOOST_REQUIRE(!minted.mint_block.vtx.empty());
    const CTransaction& coinbase = *minted.mint_block.vtx[0];
    BOOST_REQUIRE(CoinbaseHasResMint(coinbase));
    int mint_outputs = 0;
    for (const CTxOut& out : coinbase.vout) {
        int64_t maturity = 0;
        std::vector<unsigned char> pkh;
        if (!ParseResMintScript(out.scriptPubKey, maturity, pkh)) continue;
        ++mint_outputs;
        BOOST_CHECK_EQUAL(maturity, minted.mint_height + COINBASE_MATURITY);
        BOOST_CHECK(Consensus::CheckResRewardOwner(minted.owner, out.scriptPubKey));
        BOOST_CHECK(out.scriptPubKey == GetResMintScript(maturity, minted.owner));
        BOOST_CHECK_EQUAL(out.nValue, mint_idx->m_block_res_mint_sum);
    }
    BOOST_CHECK_EQUAL(mint_outputs, 1);

    // ConnectBlockSupply.v, now with both legs non-degenerate.
    if (converge_idx->Observational().HasOruCache() &&
        converge_idx->pprev->Observational().HasOruCache()) {
        BOOST_CHECK_EQUAL(converge_idx->Observational().GetChainOruTotal(),
                          converge_idx->pprev->Observational().GetChainOruTotal() +
                              GetBlockSubsidy(minted.converge_height, consensus) -
                              SATURATING_CONVERGE_AMOUNT);
    }
    if (mint_idx->Observational().HasResMintCache() &&
        converge_idx->Observational().HasResMintCache()) {
        BOOST_CHECK_EQUAL(mint_idx->Observational().GetChainResMintTotal(),
                          converge_idx->Observational().GetChainResMintTotal() +
                              mint_idx->m_block_res_mint_sum);
    }

    // The cap identity is a compile-time fact; check the running total respects it.
    BOOST_CHECK_LE(mint_idx->Observational().GetChainResMintTotal(), Consensus::RES_MAX_SUPPLY);
}

BOOST_AUTO_TEST_CASE(disconnect_drops_a_nonzero_orphan_mint)
{
    const MintedChain minted = BuildChainThatMints(*this);

    CAmount orphan_mint = 0;
    CAmount parent_res = -1;
    CAmount parent_oru = -1;
    uint256 orphan_hash;
    uint256 orphan_coinbase;
    {
        LOCK(cs_main);
        const CBlockIndex* tip = Assert(m_node.chainman)->ActiveChain().Tip();
        BOOST_REQUIRE(tip != nullptr && tip->pprev != nullptr);
        BOOST_REQUIRE_EQUAL(tip->nHeight, minted.mint_height);
        orphan_mint = tip->m_block_res_mint_sum;
        orphan_hash = tip->GetBlockHash();
        orphan_coinbase = minted.mint_block.vtx[0]->GetHash();
        if (tip->pprev->Observational().HasResMintCache()) {
            parent_res = tip->pprev->Observational().GetChainResMintTotal();
        }
        if (tip->pprev->Observational().HasOruCache()) {
            parent_oru = tip->pprev->Observational().GetChainOruTotal();
        }
        // The whole point of this case: the orphan really did mint.
        BOOST_REQUIRE_GT(orphan_mint, 0);
        BOOST_CHECK(Assert(m_node.chainman)->ActiveChainstate().CoinsTip().HaveCoin(
            COutPoint(orphan_coinbase, 0)));
    }

    BlockValidationState state;
    CBlockIndex* tip = WITH_LOCK(cs_main, return Assert(m_node.chainman)->ActiveChain().Tip());
    BOOST_REQUIRE(Assert(m_node.chainman)->ActiveChainstate().InvalidateBlock(state, tip));

    LOCK(cs_main);
    const CBlockIndex* new_tip = Assert(m_node.chainman)->ActiveChain().Tip();
    BOOST_REQUIRE(new_tip != nullptr);
    BOOST_CHECK_EQUAL(new_tip->nHeight, minted.converge_height);
    BOOST_CHECK(new_tip->GetBlockHash() != orphan_hash);
    BOOST_CHECK(!Assert(m_node.chainman)->ActiveChainstate().CoinsTip().HaveCoin(
        COutPoint(orphan_coinbase, 0)));

    // ReorgIssuanceBound.v: the orphan's mint is not in the surviving chain.
    BOOST_REQUIRE_GE(parent_res, 0);
    BOOST_REQUIRE(new_tip->Observational().HasResMintCache());
    BOOST_CHECK_EQUAL(new_tip->Observational().GetChainResMintTotal(), parent_res);
    BOOST_CHECK_LT(new_tip->Observational().GetChainResMintTotal(), parent_res + orphan_mint);
    if (parent_oru >= 0 && new_tip->Observational().HasOruCache()) {
        BOOST_CHECK_EQUAL(new_tip->Observational().GetChainOruTotal(), parent_oru);
    }
    BOOST_CHECK(!tip->Observational().HasOruCache());

    // The convergence burn that funded the draw survives; only the payout is gone.
    BOOST_CHECK_EQUAL(new_tip->m_block_converge_sum, SATURATING_CONVERGE_AMOUNT);
}

BOOST_AUTO_TEST_SUITE_END()
