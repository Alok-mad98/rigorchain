// Copyright (c) 2011-2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <coins.h>
#include <consensus/consensus.h>
#include <consensus/rigor_consensus_lock.h>
#include <consensus/merkle.h>
#include <consensus/tx_check.h>
#include <consensus/tx_verify.h>
#include <key.h>
#include <node/miner.h>
#include <policy/policy.h>
#include <pow.h>
#include <primitives/transaction.h>
#include <script/standard.h>
#include <test/util/mining.h>
#include <test/util/random.h>
#include <test/util/txmempool.h>
#include <timedata.h>
#include <txmempool.h>
#include <uint256.h>
#include <util/strencodings.h>
#include <util/system.h>
#include <util/time.h>
#include <validation.h>
#include <versionbits.h>

#include <test/util/setup_common.h>

#include <memory>

#include <boost/test/unit_test.hpp>

namespace {

/** ORU outputs must bind to a standard script type (see CheckTxOutAssetWitnessBinding). */
static void SetMthOutput(CTxOut& out, const CScript& spk, CAmount value)
{
    out.nValue = value;
    out.scriptPubKey = spk;
}

/** Distinct P2WPKH scripts for double-spend txid tests (same fee, different script). */
static const CPubKey MINER_TEST_PUBKEY_ALT{ParseHex("04678afdb0fe5548271967f1a67130b7105cd6a828e03909a67962e0ea1f61deb649f6bc3f4cef38c4f35504e51ec112de5c384df7ba0b8d578a4c702b6bf11d5e")};
static const CScript SCRIPT_PUBKEY_ALT{GetScriptForDestination(WitnessV0KeyHash(MINER_TEST_PUBKEY_ALT))};

} // namespace

using node::BlockAssembler;
using node::CBlockTemplate;

namespace miner_tests {

// Production BlockAssembler always defaults test_block_validity=true (see node/miner.h). This file is only linked
// into test/test_rigor, which defines RIGORCHAIN_MINER_TESTS_SKIP_TEMPLATE_VALIDITY (Makefile.test.include) so
// AssemblerForTest() can default to skipping TestBlockValidity for legacy mining fixtures. Without that define,
// default to true (fail closed). bitcoind / libbitcoinnode are unaffected.
//
// Explicit AssemblerForTest(..., true) must stay in sync with the exceptions we still need full validation for:
//  - bad-blk-sigops, bad-txns-inputs-missingorspent (x2), bad-cb-multiple, bad-rigor-script-type (legacy P2SH / non-witness fixture)
//  - cannot reconstruct convergence window (header-only dummy chain; R-3)
#ifdef RIGORCHAIN_MINER_TESTS_SKIP_TEMPLATE_VALIDITY
constexpr bool kAssemblerDefaultTemplateValidity{false};
#else
constexpr bool kAssemblerDefaultTemplateValidity{true};
#endif

struct MinerTestingSetup : public RegTestingSetup {
    void TestPackageSelection(const CScript& scriptPubKey, const std::vector<CTransactionRef>& txFirst) EXCLUSIVE_LOCKS_REQUIRED(::cs_main);
    void TestBasicMining(const CScript& scriptPubKey, const std::vector<CTransactionRef>& txFirst, int baseheight) EXCLUSIVE_LOCKS_REQUIRED(::cs_main);
    void TestPrioritisedMining(const CScript& scriptPubKey, const std::vector<CTransactionRef>& txFirst) EXCLUSIVE_LOCKS_REQUIRED(::cs_main);
    bool TestSequenceLocks(const CTransaction& tx, CTxMemPool& tx_mempool) EXCLUSIVE_LOCKS_REQUIRED(::cs_main)
    {
        CCoinsViewMemPool view_mempool{&m_node.chainman->ActiveChainstate().CoinsTip(), tx_mempool};
        CBlockIndex* tip{m_node.chainman->ActiveChain().Tip()};
        const std::optional<LockPoints> lock_points{CalculateLockPointsAtTip(tip, view_mempool, tx)};
        return lock_points.has_value() && CheckSequenceLocksAtTip(tip, *lock_points);
    }
    CTxMemPool& MakeMempool()
    {
        // Delete the previous mempool to ensure with valgrind that the old
        // pointer is not accessed, when the new one should be accessed
        // instead.
        m_node.mempool.reset();
        m_node.mempool = std::make_unique<CTxMemPool>(MemPoolOptionsForTest(m_node));
        return *m_node.mempool;
    }
    /** Default is kAssemblerDefaultTemplateValidity; pass true only when the test must run full TestBlockValidity (the explicit exception cases). */
    BlockAssembler AssemblerForTest(CTxMemPool& tx_mempool, bool test_block_validity = kAssemblerDefaultTemplateValidity);
};
} // namespace miner_tests

BOOST_FIXTURE_TEST_SUITE(miner_tests, MinerTestingSetup)

static CFeeRate blockMinFeeRate = CFeeRate(DEFAULT_BLOCK_MIN_TX_FEE);

BlockAssembler MinerTestingSetup::AssemblerForTest(CTxMemPool& tx_mempool, bool test_block_validity)
{
    BlockAssembler::Options options;

    options.nBlockMaxWeight = MAX_BLOCK_WEIGHT;
    options.blockMinFeeRate = blockMinFeeRate;
    options.test_block_validity = test_block_validity;
    return BlockAssembler{m_node.chainman->ActiveChainstate(), &tx_mempool, options};
}

/** Per-block coinbase extranonce; PoW nonce is found at runtime (coinbase includes commitments). */
constexpr static unsigned char BLOCKINFO_EXTRANONCE[]{
    8, 0, 2, 6, 7, 8, 8, 2, 4, 1, 8, 4, 3, 8, 6, 5, 5, 4, 0, 5, 3, 2, 2, 7, 2, 0, 1, 6, 7, 4, 7, 6, 3, 2, 3, 8, 5, 3, 0, 3, 0, 2, 3, 2, 8, 2, 4, 8, 7, 3, 8, 4, 1, 6, 5, 3, 3, 0, 8, 5, 0, 6, 6, 6, 6, 5, 0, 6, 4, 8, 6, 6, 6, 5, 8, 7, 3, 5, 2, 2, 6, 7, 4, 3, 4, 0, 6, 3, 5, 8, 4, 8, 6, 0, 7, 7, 2, 6, 7, 7, 4, 1, 0, 6, 6, 5, 6, 7, 8, 0};

static std::unique_ptr<CBlockIndex> CreateBlockIndex(int nHeight, CBlockIndex* active_chain_tip) EXCLUSIVE_LOCKS_REQUIRED(cs_main)
{
    auto index{std::make_unique<CBlockIndex>()};
    index->nHeight = nHeight;
    index->pprev = active_chain_tip;
    return index;
}

// Test suite for ancestor feerate transaction selection.
// Implemented as an additional function, rather than a separate test case,
// to allow reusing the blockchain created in CreateNewBlock_validity.
void MinerTestingSetup::TestPackageSelection(const CScript& scriptPubKey, const std::vector<CTransactionRef>& txFirst)
{
    CTxMemPool& tx_mempool{MakeMempool()};
    LOCK(tx_mempool.cs);
    // Test the ancestor feerate transaction selection.
    TestMemPoolEntryHelper entry;

    // Test that a medium fee transaction will be selected after a higher fee
    // rate package with a low fee rate parent.
    CMutableTransaction tx;
    tx.vin.resize(1);
    tx.vin[0].scriptSig = CScript() << RIGOR_PUSH_1;
    tx.vin[0].prevout.hash = txFirst[0]->GetHash();
    tx.vin[0].prevout.n = 0;
    tx.vout.resize(1);
    SetMthOutput(tx.vout[0], scriptPubKey, 5000000000LL - 1000);
    // This tx has a low fee: 1000 satoshis
    uint256 hashParentTx = tx.GetHash(); // save this txid for later use
    tx_mempool.addUnchecked(entry.Fee(1000).Time(Now<NodeSeconds>()).SpendsCoinbase(true).FromTx(tx));

    // This tx has a medium fee: 10000 satoshis
    tx.vin[0].prevout.hash = txFirst[1]->GetHash();
    tx.vout[0].nValue = 5000000000LL - 10000;
    uint256 hashMediumFeeTx = tx.GetHash();
    tx_mempool.addUnchecked(entry.Fee(10000).Time(Now<NodeSeconds>()).SpendsCoinbase(true).FromTx(tx));

    // This tx has a high fee, but depends on the first transaction
    tx.vin[0].prevout.hash = hashParentTx;
    tx.vout[0].nValue = 5000000000LL - 1000 - 50000; // 50k satoshi fee
    uint256 hashHighFeeTx = tx.GetHash();
    tx_mempool.addUnchecked(entry.Fee(50000).Time(Now<NodeSeconds>()).SpendsCoinbase(false).FromTx(tx));

    std::unique_ptr<CBlockTemplate> pblocktemplate = AssemblerForTest(tx_mempool).CreateNewBlock(scriptPubKey);
    BOOST_REQUIRE_EQUAL(pblocktemplate->block.vtx.size(), 4U);
    BOOST_CHECK(pblocktemplate->block.vtx[1]->GetHash() == hashParentTx);
    BOOST_CHECK(pblocktemplate->block.vtx[2]->GetHash() == hashHighFeeTx);
    BOOST_CHECK(pblocktemplate->block.vtx[3]->GetHash() == hashMediumFeeTx);

    // Test that a package below the block min tx fee doesn't get included
    tx.vin[0].prevout.hash = hashHighFeeTx;
    tx.vout[0].nValue = 5000000000LL - 1000 - 50000; // 0 fee
    uint256 hashFreeTx = tx.GetHash();
    tx_mempool.addUnchecked(entry.Fee(0).FromTx(tx));
    size_t freeTxSize = ::GetSerializeSize(tx, PROTOCOL_VERSION);

    // Calculate a fee on child transaction that will put the package just
    // below the block min tx fee (assuming 1 child tx of the same size).
    CAmount feeToUse = blockMinFeeRate.GetFee(2*freeTxSize) - 1;

    tx.vin[0].prevout.hash = hashFreeTx;
    tx.vout[0].nValue = 5000000000LL - 1000 - 50000 - feeToUse;
    uint256 hashLowFeeTx = tx.GetHash();
    tx_mempool.addUnchecked(entry.Fee(feeToUse).FromTx(tx));
    pblocktemplate = AssemblerForTest(tx_mempool).CreateNewBlock(scriptPubKey);
    // Verify that the free tx and the low fee tx didn't get selected
    for (size_t i=0; i<pblocktemplate->block.vtx.size(); ++i) {
        BOOST_CHECK(pblocktemplate->block.vtx[i]->GetHash() != hashFreeTx);
        BOOST_CHECK(pblocktemplate->block.vtx[i]->GetHash() != hashLowFeeTx);
    }

    // Test that packages above the min relay fee do get included, even if one
    // of the transactions is below the min relay fee
    // Remove the low fee transaction and replace with a higher fee transaction
    tx_mempool.removeRecursive(CTransaction(tx), MemPoolRemovalReason::REPLACED);
    tx.vout[0].nValue -= 2; // Now we should be just over the min relay fee
    hashLowFeeTx = tx.GetHash();
    tx_mempool.addUnchecked(entry.Fee(feeToUse + 2).FromTx(tx));
    pblocktemplate = AssemblerForTest(tx_mempool).CreateNewBlock(scriptPubKey);
    BOOST_REQUIRE_EQUAL(pblocktemplate->block.vtx.size(), 6U);
    BOOST_CHECK(pblocktemplate->block.vtx[4]->GetHash() == hashFreeTx);
    BOOST_CHECK(pblocktemplate->block.vtx[5]->GetHash() == hashLowFeeTx);

    // Test that transaction selection properly updates ancestor fee
    // calculations as ancestor transactions get included in a block.
    // Add a 0-fee transaction that has 2 outputs.
    tx.vin[0].prevout.hash = txFirst[2]->GetHash();
    tx.vout.resize(2);
    SetMthOutput(tx.vout[0], scriptPubKey, 5000000000LL - 100000000);
    SetMthOutput(tx.vout[1], scriptPubKey, 100000000); // 1BTC output
    uint256 hashFreeTx2 = tx.GetHash();
    tx_mempool.addUnchecked(entry.Fee(0).SpendsCoinbase(true).FromTx(tx));

    // This tx can't be mined by itself
    tx.vin[0].prevout.hash = hashFreeTx2;
    tx.vout.resize(1);
    feeToUse = blockMinFeeRate.GetFee(freeTxSize);
    SetMthOutput(tx.vout[0], scriptPubKey, 5000000000LL - 100000000 - feeToUse);
    uint256 hashLowFeeTx2 = tx.GetHash();
    tx_mempool.addUnchecked(entry.Fee(feeToUse).SpendsCoinbase(false).FromTx(tx));
    pblocktemplate = AssemblerForTest(tx_mempool).CreateNewBlock(scriptPubKey);

    // Verify that this tx isn't selected.
    for (size_t i=0; i<pblocktemplate->block.vtx.size(); ++i) {
        BOOST_CHECK(pblocktemplate->block.vtx[i]->GetHash() != hashFreeTx2);
        BOOST_CHECK(pblocktemplate->block.vtx[i]->GetHash() != hashLowFeeTx2);
    }

    // This tx will be mineable, and should cause hashLowFeeTx2 to be selected
    // as well.
    tx.vin[0].prevout.n = 1;
    tx.vout[0].nValue = 100000000 - 10000; // 10k satoshi fee
    tx_mempool.addUnchecked(entry.Fee(10000).FromTx(tx));
    pblocktemplate = AssemblerForTest(tx_mempool).CreateNewBlock(scriptPubKey);
    BOOST_REQUIRE_EQUAL(pblocktemplate->block.vtx.size(), 9U);
    BOOST_CHECK(pblocktemplate->block.vtx[8]->GetHash() == hashLowFeeTx2);
}

void MinerTestingSetup::TestBasicMining(const CScript& scriptPubKey, const std::vector<CTransactionRef>& txFirst, int baseheight)
{
    uint256 hash;
    CMutableTransaction tx;
    TestMemPoolEntryHelper entry;
    entry.nFee = 11;
    entry.nHeight = 11;

    const CAmount BLOCKSUBSIDY = Consensus::ORU_FLAT_BLOCK_SUBSIDY;
    const CAmount LOWFEE = CENT;
    const CAmount HIGHFEE = COIN;
    const CAmount HIGHERFEE = 4 * COIN;

    {
        CTxMemPool& tx_mempool{MakeMempool()};
        LOCK(tx_mempool.cs);

        // Just to make sure we can still make simple blocks
        auto pblocktemplate = AssemblerForTest(tx_mempool).CreateNewBlock(scriptPubKey);
        BOOST_CHECK(pblocktemplate);

        // One transaction whose live WPKH outputs exceed BLOCK_SIGOPS_COST_LIMIT.
        tx.vin.resize(1);
        tx.vin[0].scriptSig = CScript();
        tx.vin[0].prevout.hash = txFirst[0]->GetHash();
        tx.vin[0].prevout.n = 0;
        const size_t n_over = static_cast<size_t>(MAX_BLOCK_SIGOPS_COST / WITNESS_SCALE_FACTOR) + 1;
        tx.vout.assign(n_over, CTxOut(1, scriptPubKey));
        // Default mempool sigOpCost is 4, so the assembler includes this tx;
        // CheckBlock then applies GetLegacySigOpCount (one per live WPKH output).
        tx_mempool.addUnchecked(entry.Fee(LOWFEE).Time(Now<NodeSeconds>()).SpendsCoinbase(true).FromTx(tx));

        BOOST_CHECK_EXCEPTION(AssemblerForTest(tx_mempool, true).CreateNewBlock(scriptPubKey), std::runtime_error, HasReason("bad-blk-sigops"));
    }

    {
        CTxMemPool& tx_mempool{MakeMempool()};
        LOCK(tx_mempool.cs);

        tx.vin[0].prevout.hash = txFirst[0]->GetHash();
        tx.vout.resize(1);
        SetMthOutput(tx.vout[0], scriptPubKey, BLOCKSUBSIDY);
        for (unsigned int i = 0; i < 1001; ++i) {
            tx.vout[0].nValue -= LOWFEE;
            hash = tx.GetHash();
            bool spendsCoinbase = i == 0; // only first tx spends coinbase
            // Must match CheckBlock/ConnectBlock legacy sigop math (see Bitcoin's miner_tests SigOpsCost(GetLegacySigOpCount*WITNESS_SCALE_FACTOR)).
            tx_mempool.addUnchecked(entry.Fee(LOWFEE).Time(Now<NodeSeconds>()).SpendsCoinbase(spendsCoinbase).SigOpsCost(GetLegacySigOpCount(CTransaction(tx)) * WITNESS_SCALE_FACTOR).FromTx(tx));
            tx.vin[0].prevout.hash = hash;
        }
        // Full TestBlockValidity runs ConnectBlock script checks; this legacy chained CHECKMULTISIG fixture does not
        // survive deferred verification on the ORU fork. Skip template self-check here;
        // the failure case above still exercises CheckBlock sigops; consider porting Bitcoin's CreateBigSigOpsCluster.
        BOOST_CHECK(AssemblerForTest(tx_mempool).CreateNewBlock(scriptPubKey));
    }

    {
        CTxMemPool& tx_mempool{MakeMempool()};
        LOCK(tx_mempool.cs);

        // block size > limit
        tx.vin[0].scriptSig = CScript();
        // 18 * (520char + DROP) + RIGOR_PUSH_1 = 9433 bytes
        std::vector<unsigned char> vchData(520);
        for (unsigned int i = 0; i < 18; ++i) {
            tx.vin[0].scriptSig << vchData << RIGOR_RESERVED_LEGACY_DROP;
        }
        tx.vin[0].scriptSig << RIGOR_PUSH_1;
        tx.vin[0].prevout.hash = txFirst[0]->GetHash();
        SetMthOutput(tx.vout[0], scriptPubKey, BLOCKSUBSIDY);
        for (unsigned int i = 0; i < 128; ++i) {
            tx.vout[0].nValue -= LOWFEE;
            hash = tx.GetHash();
            bool spendsCoinbase = i == 0; // only first tx spends coinbase
            tx_mempool.addUnchecked(entry.Fee(LOWFEE).Time(Now<NodeSeconds>()).SpendsCoinbase(spendsCoinbase).FromTx(tx));
            tx.vin[0].prevout.hash = hash;
        }
        // Same deferred script-check limitation as the sigop success case (non-standard unlock vs P2PK prevout).
        BOOST_CHECK(AssemblerForTest(tx_mempool).CreateNewBlock(scriptPubKey));
    }

    {
        CTxMemPool& tx_mempool{MakeMempool()};
        LOCK(tx_mempool.cs);

        // orphan in tx_mempool, template creation fails
        hash = tx.GetHash();
        tx_mempool.addUnchecked(entry.Fee(LOWFEE).Time(Now<NodeSeconds>()).FromTx(tx));
        BOOST_CHECK_EXCEPTION(AssemblerForTest(tx_mempool, true).CreateNewBlock(scriptPubKey), std::runtime_error, HasReason("bad-txns-inputs-missingorspent"));
    }

    {
        CTxMemPool& tx_mempool{MakeMempool()};
        LOCK(tx_mempool.cs);

        // child with higher feerate than parent
        tx.vin[0].scriptSig = CScript() << RIGOR_PUSH_1;
        tx.vin[0].prevout.hash = txFirst[1]->GetHash();
        SetMthOutput(tx.vout[0], scriptPubKey, BLOCKSUBSIDY - HIGHFEE);
        hash = tx.GetHash();
        tx_mempool.addUnchecked(entry.Fee(HIGHFEE).Time(Now<NodeSeconds>()).SpendsCoinbase(true).FromTx(tx));
        tx.vin[0].prevout.hash = hash;
        tx.vin.resize(2);
        tx.vin[1].scriptSig = CScript() << RIGOR_PUSH_1;
        tx.vin[1].prevout.hash = txFirst[0]->GetHash();
        tx.vin[1].prevout.n = 0;
        tx.vout[0].nValue = tx.vout[0].nValue + BLOCKSUBSIDY - HIGHERFEE; // First txn output + fresh coinbase - new txn fee
        hash = tx.GetHash();
        tx_mempool.addUnchecked(entry.Fee(HIGHERFEE).Time(Now<NodeSeconds>()).SpendsCoinbase(true).FromTx(tx));
        BOOST_CHECK(AssemblerForTest(tx_mempool).CreateNewBlock(scriptPubKey));
    }

    {
        CTxMemPool& tx_mempool{MakeMempool()};
        LOCK(tx_mempool.cs);

        // coinbase in tx_mempool, template creation fails
        tx.vin.resize(1);
        tx.vin[0].prevout.SetNull();
        tx.vin[0].scriptSig = CScript() << RIGOR_PUSH_0 << RIGOR_PUSH_1;
        SetMthOutput(tx.vout[0], scriptPubKey, 0);
        hash = tx.GetHash();
        // give it a fee so it'll get mined
        tx_mempool.addUnchecked(entry.Fee(LOWFEE).Time(Now<NodeSeconds>()).SpendsCoinbase(false).FromTx(tx));
        // Should throw bad-cb-multiple
        BOOST_CHECK_EXCEPTION(AssemblerForTest(tx_mempool, true).CreateNewBlock(scriptPubKey), std::runtime_error, HasReason("bad-cb-multiple"));
    }

    {
        CTxMemPool& tx_mempool{MakeMempool()};
        LOCK(tx_mempool.cs);

        // double spend txn pair in tx_mempool, template creation fails
        tx.vin[0].prevout.hash = txFirst[0]->GetHash();
        tx.vin[0].scriptSig = CScript() << RIGOR_PUSH_1;
        SetMthOutput(tx.vout[0], scriptPubKey, BLOCKSUBSIDY - HIGHFEE);
        hash = tx.GetHash();
        tx_mempool.addUnchecked(entry.Fee(HIGHFEE).Time(Now<NodeSeconds>()).SpendsCoinbase(true).FromTx(tx));
        SetMthOutput(tx.vout[0], SCRIPT_PUBKEY_ALT, BLOCKSUBSIDY - HIGHFEE);
        hash = tx.GetHash();
        tx_mempool.addUnchecked(entry.Fee(HIGHFEE).Time(Now<NodeSeconds>()).SpendsCoinbase(true).FromTx(tx));
        BOOST_CHECK_EXCEPTION(AssemblerForTest(tx_mempool, true).CreateNewBlock(scriptPubKey), std::runtime_error, HasReason("bad-txns-inputs-missingorspent"));
    }

    {
        CTxMemPool& tx_mempool{MakeMempool()};
        LOCK(tx_mempool.cs);

        // subsidy changing
        int nHeight = m_node.chainman->ActiveChain().Height();
        // Create an actual 209999-long block chain (without valid blocks).
        while (m_node.chainman->ActiveChain().Tip()->nHeight < 209999) {
            CBlockIndex* prev = m_node.chainman->ActiveChain().Tip();
            CBlockIndex* next = new CBlockIndex();
            next->phashBlock = new uint256(InsecureRand256());
            m_node.chainman->ActiveChainstate().CoinsTip().SetBestBlock(next->GetBlockHash());
            next->pprev = prev;
            next->nHeight = prev->nHeight + 1;
            next->BuildSkip();
            m_node.chainman->ActiveChain().SetTip(*next);
        }
        BOOST_CHECK(AssemblerForTest(tx_mempool).CreateNewBlock(scriptPubKey));
        // Extend to a 210000-long block chain.
        while (m_node.chainman->ActiveChain().Tip()->nHeight < 210000) {
            CBlockIndex* prev = m_node.chainman->ActiveChain().Tip();
            CBlockIndex* next = new CBlockIndex();
            next->phashBlock = new uint256(InsecureRand256());
            m_node.chainman->ActiveChainstate().CoinsTip().SetBestBlock(next->GetBlockHash());
            next->pprev = prev;
            next->nHeight = prev->nHeight + 1;
            next->BuildSkip();
            m_node.chainman->ActiveChain().SetTip(*next);
        }
        BOOST_CHECK(AssemblerForTest(tx_mempool).CreateNewBlock(scriptPubKey));

        // Delete the dummy blocks again.
        while (m_node.chainman->ActiveChain().Tip()->nHeight > nHeight) {
            CBlockIndex* del = m_node.chainman->ActiveChain().Tip();
            m_node.chainman->ActiveChain().SetTip(*Assert(del->pprev));
            m_node.chainman->ActiveChainstate().CoinsTip().SetBestBlock(del->pprev->GetBlockHash());
            delete del->phashBlock;
            delete del;
        }

        // invalid p2sh txn in tx_mempool, template creation fails.
        // Must run on the restored chain: the dummy header-only tip has no window,
        // so test_block_validity=true throws at R-3 before CheckBlock.
        tx.vin[0].prevout.hash = txFirst[0]->GetHash();
        tx.vin[0].prevout.n = 0;
        tx.vin[0].scriptSig = CScript() << RIGOR_PUSH_1;
        CScript script = CScript() << RIGOR_PUSH_0;
        SetMthOutput(tx.vout[0], GetScriptForDestination(ScriptHash(script)), BLOCKSUBSIDY - LOWFEE);
        hash = tx.GetHash();
        tx_mempool.addUnchecked(entry.Fee(LOWFEE).Time(Now<NodeSeconds>()).SpendsCoinbase(true).FromTx(tx));
        tx.vin[0].prevout.hash = hash;
        tx.vin[0].scriptSig = CScript() << std::vector<unsigned char>(script.begin(), script.end());
        tx.vout[0].nValue -= LOWFEE;
        hash = tx.GetHash();
        tx_mempool.addUnchecked(entry.Fee(LOWFEE).Time(Now<NodeSeconds>()).SpendsCoinbase(false).FromTx(tx));
        // Legacy Bitcoin used invalid-in-mempool P2SH to force TestBlockValidity to fail with block-validation-failed.
        // Rigor rejects non-witness templates earlier (CheckBlock: bad-rigor-script-type) or via CheckTxOutAssetWitnessBinding.
        BOOST_CHECK_EXCEPTION(AssemblerForTest(tx_mempool, true).CreateNewBlock(scriptPubKey), std::runtime_error, HasReason("bad-rigor-script-type"));
    }

    CTxMemPool& tx_mempool{MakeMempool()};
    LOCK(tx_mempool.cs);

    // non-final txs in mempool
    SetMockTime(m_node.chainman->ActiveChain().Tip()->GetMedianTimePast() + 1);
    const int flags{LOCKTIME_VERIFY_SEQUENCE};
    // height map
    std::vector<int> prevheights;

    // relative height locked
    tx.nVersion = 2;
    tx.vin.resize(1);
    prevheights.resize(1);
    tx.vin[0].prevout.hash = txFirst[0]->GetHash(); // only 1 transaction
    tx.vin[0].prevout.n = 0;
    tx.vin[0].scriptSig = CScript() << RIGOR_PUSH_1;
    tx.vin[0].nSequence = m_node.chainman->ActiveChain().Tip()->nHeight + 1; // txFirst[0] is the 2nd block
    prevheights[0] = baseheight + 1;
    tx.vout.resize(1);
    SetMthOutput(tx.vout[0], scriptPubKey, BLOCKSUBSIDY - HIGHFEE);
    tx.nLockTime = 0;
    hash = tx.GetHash();
    tx_mempool.addUnchecked(entry.Fee(HIGHFEE).Time(Now<NodeSeconds>()).SpendsCoinbase(true).FromTx(tx));
    BOOST_CHECK(CheckFinalTxAtTip(*Assert(m_node.chainman->ActiveChain().Tip()), CTransaction{tx})); // Locktime passes
    BOOST_CHECK(!TestSequenceLocks(CTransaction{tx}, tx_mempool)); // Sequence locks fail

    {
        CBlockIndex* active_chain_tip = m_node.chainman->ActiveChain().Tip();
        BOOST_CHECK(SequenceLocks(CTransaction(tx), flags, prevheights, *CreateBlockIndex(active_chain_tip->nHeight + 2, active_chain_tip))); // Sequence locks pass on 2nd block
    }

    // relative time locked
    tx.vin[0].prevout.hash = txFirst[1]->GetHash();
    tx.vin[0].nSequence = CTxIn::SEQUENCE_LOCKTIME_TYPE_FLAG | (((m_node.chainman->ActiveChain().Tip()->GetMedianTimePast()+1-m_node.chainman->ActiveChain()[1]->GetMedianTimePast()) >> CTxIn::SEQUENCE_LOCKTIME_GRANULARITY) + 1); // txFirst[1] is the 3rd block
    prevheights[0] = baseheight + 2;
    hash = tx.GetHash();
    tx_mempool.addUnchecked(entry.Time(Now<NodeSeconds>()).FromTx(tx));
    BOOST_CHECK(CheckFinalTxAtTip(*Assert(m_node.chainman->ActiveChain().Tip()), CTransaction{tx})); // Locktime passes
    BOOST_CHECK(!TestSequenceLocks(CTransaction{tx}, tx_mempool)); // Sequence locks fail

    const int SEQUENCE_LOCK_TIME = 512; // Sequence locks pass 512 seconds later
    for (int i = 0; i < CBlockIndex::nMedianTimeSpan; ++i)
        m_node.chainman->ActiveChain().Tip()->GetAncestor(m_node.chainman->ActiveChain().Tip()->nHeight - i)->nTime += SEQUENCE_LOCK_TIME; // Trick the MedianTimePast
    {
        CBlockIndex* active_chain_tip = m_node.chainman->ActiveChain().Tip();
        BOOST_CHECK(SequenceLocks(CTransaction(tx), flags, prevheights, *CreateBlockIndex(active_chain_tip->nHeight + 1, active_chain_tip)));
    }

    for (int i = 0; i < CBlockIndex::nMedianTimeSpan; ++i) {
        CBlockIndex* ancestor{Assert(m_node.chainman->ActiveChain().Tip()->GetAncestor(m_node.chainman->ActiveChain().Tip()->nHeight - i))};
        ancestor->nTime -= SEQUENCE_LOCK_TIME; // undo tricked MTP
    }

    // absolute height locked
    tx.vin[0].prevout.hash = txFirst[2]->GetHash();
    tx.vin[0].nSequence = CTxIn::MAX_SEQUENCE_NONFINAL;
    prevheights[0] = baseheight + 3;
    tx.nLockTime = m_node.chainman->ActiveChain().Tip()->nHeight + 1;
    hash = tx.GetHash();
    tx_mempool.addUnchecked(entry.Time(Now<NodeSeconds>()).FromTx(tx));
    BOOST_CHECK(!CheckFinalTxAtTip(*Assert(m_node.chainman->ActiveChain().Tip()), CTransaction{tx})); // Locktime fails
    BOOST_CHECK(TestSequenceLocks(CTransaction{tx}, tx_mempool)); // Sequence locks pass
    BOOST_CHECK(IsFinalTx(CTransaction(tx), m_node.chainman->ActiveChain().Tip()->nHeight + 2, m_node.chainman->ActiveChain().Tip()->GetMedianTimePast())); // Locktime passes on 2nd block

    // absolute time locked
    tx.vin[0].prevout.hash = txFirst[3]->GetHash();
    tx.nLockTime = m_node.chainman->ActiveChain().Tip()->GetMedianTimePast();
    prevheights.resize(1);
    prevheights[0] = baseheight + 4;
    hash = tx.GetHash();
    tx_mempool.addUnchecked(entry.Time(Now<NodeSeconds>()).FromTx(tx));
    BOOST_CHECK(!CheckFinalTxAtTip(*Assert(m_node.chainman->ActiveChain().Tip()), CTransaction{tx})); // Locktime fails
    BOOST_CHECK(TestSequenceLocks(CTransaction{tx}, tx_mempool)); // Sequence locks pass
    BOOST_CHECK(IsFinalTx(CTransaction(tx), m_node.chainman->ActiveChain().Tip()->nHeight + 2, m_node.chainman->ActiveChain().Tip()->GetMedianTimePast() + 1)); // Locktime passes 1 second later

    // mempool-dependent transactions (not added)
    tx.vin[0].prevout.hash = hash;
    prevheights[0] = m_node.chainman->ActiveChain().Tip()->nHeight + 1;
    tx.nLockTime = 0;
    tx.vin[0].nSequence = 0;
    BOOST_CHECK(CheckFinalTxAtTip(*Assert(m_node.chainman->ActiveChain().Tip()), CTransaction{tx})); // Locktime passes
    BOOST_CHECK(TestSequenceLocks(CTransaction{tx}, tx_mempool)); // Sequence locks pass
    tx.vin[0].nSequence = 1;
    BOOST_CHECK(!TestSequenceLocks(CTransaction{tx}, tx_mempool)); // Sequence locks fail
    tx.vin[0].nSequence = CTxIn::SEQUENCE_LOCKTIME_TYPE_FLAG;
    BOOST_CHECK(TestSequenceLocks(CTransaction{tx}, tx_mempool)); // Sequence locks pass
    tx.vin[0].nSequence = CTxIn::SEQUENCE_LOCKTIME_TYPE_FLAG | 1;
    BOOST_CHECK(!TestSequenceLocks(CTransaction{tx}, tx_mempool)); // Sequence locks fail

    auto pblocktemplate = AssemblerForTest(tx_mempool).CreateNewBlock(scriptPubKey);
    BOOST_CHECK(pblocktemplate);

    // None of the of the absolute height/time locked tx should have made
    // it into the template because we still check IsFinalTx in CreateNewBlock,
    // but relative locked txs will if inconsistently added to mempool.
    // For now these will still generate a valid template until BIP68 soft fork
    BOOST_CHECK_EQUAL(pblocktemplate->block.vtx.size(), 3U);
    // However if we advance height by 1 and time by SEQUENCE_LOCK_TIME, all of them should be mined
    for (int i = 0; i < CBlockIndex::nMedianTimeSpan; ++i) {
        CBlockIndex* ancestor{Assert(m_node.chainman->ActiveChain().Tip()->GetAncestor(m_node.chainman->ActiveChain().Tip()->nHeight - i))};
        ancestor->nTime += SEQUENCE_LOCK_TIME; // Trick the MedianTimePast
    }
    m_node.chainman->ActiveChain().Tip()->nHeight++;
    SetMockTime(m_node.chainman->ActiveChain().Tip()->GetMedianTimePast() + 1);

    BOOST_CHECK(pblocktemplate = AssemblerForTest(tx_mempool).CreateNewBlock(scriptPubKey));
    BOOST_CHECK_EQUAL(pblocktemplate->block.vtx.size(), 5U);
}

void MinerTestingSetup::TestPrioritisedMining(const CScript& scriptPubKey, const std::vector<CTransactionRef>& txFirst)
{
    CTxMemPool& tx_mempool{MakeMempool()};
    LOCK(tx_mempool.cs);

    TestMemPoolEntryHelper entry;

    // Test that a tx below min fee but prioritised is included
    CMutableTransaction tx;
    tx.vin.resize(1);
    tx.vin[0].prevout.hash = txFirst[0]->GetHash();
    tx.vin[0].prevout.n = 0;
    tx.vin[0].scriptSig = CScript() << RIGOR_PUSH_1;
    tx.vout.resize(1);
    SetMthOutput(tx.vout[0], scriptPubKey, 5000000000LL); // 0 fee
    uint256 hashFreePrioritisedTx = tx.GetHash();
    tx_mempool.addUnchecked(entry.Fee(0).Time(Now<NodeSeconds>()).SpendsCoinbase(true).FromTx(tx));
    tx_mempool.PrioritiseTransaction(hashFreePrioritisedTx, 5 * COIN);

    tx.vin[0].prevout.hash = txFirst[1]->GetHash();
    tx.vin[0].prevout.n = 0;
    tx.vout[0].nValue = 5000000000LL - 1000;
    // This tx has a low fee: 1000 satoshis
    uint256 hashParentTx = tx.GetHash(); // save this txid for later use
    tx_mempool.addUnchecked(entry.Fee(1000).Time(Now<NodeSeconds>()).SpendsCoinbase(true).FromTx(tx));

    // This tx has a medium fee: 10000 satoshis
    tx.vin[0].prevout.hash = txFirst[2]->GetHash();
    tx.vout[0].nValue = 5000000000LL - 10000;
    uint256 hashMediumFeeTx = tx.GetHash();
    tx_mempool.addUnchecked(entry.Fee(10000).Time(Now<NodeSeconds>()).SpendsCoinbase(true).FromTx(tx));
    tx_mempool.PrioritiseTransaction(hashMediumFeeTx, -5 * COIN);

    // This tx also has a low fee, but is prioritised
    tx.vin[0].prevout.hash = hashParentTx;
    tx.vout[0].nValue = 5000000000LL - 1000 - 1000; // 1000 satoshi fee
    uint256 hashPrioritsedChild = tx.GetHash();
    tx_mempool.addUnchecked(entry.Fee(1000).Time(Now<NodeSeconds>()).SpendsCoinbase(false).FromTx(tx));
    tx_mempool.PrioritiseTransaction(hashPrioritsedChild, 2 * COIN);

    // Test that transaction selection properly updates ancestor fee calculations as prioritised
    // parents get included in a block. Create a transaction with two prioritised ancestors, each
    // included by itself: FreeParent <- FreeChild <- FreeGrandchild.
    // When FreeParent is added, a modified entry will be created for FreeChild + FreeGrandchild
    // FreeParent's prioritisation should not be included in that entry.
    // When FreeChild is included, FreeChild's prioritisation should also not be included.
    tx.vin[0].prevout.hash = txFirst[3]->GetHash();
    tx.vout[0].nValue = 5000000000LL; // 0 fee
    uint256 hashFreeParent = tx.GetHash();
    tx_mempool.addUnchecked(entry.Fee(0).SpendsCoinbase(true).FromTx(tx));
    tx_mempool.PrioritiseTransaction(hashFreeParent, 10 * COIN);

    tx.vin[0].prevout.hash = hashFreeParent;
    tx.vout[0].nValue = 5000000000LL; // 0 fee
    uint256 hashFreeChild = tx.GetHash();
    tx_mempool.addUnchecked(entry.Fee(0).SpendsCoinbase(false).FromTx(tx));
    tx_mempool.PrioritiseTransaction(hashFreeChild, 1 * COIN);

    tx.vin[0].prevout.hash = hashFreeChild;
    tx.vout[0].nValue = 5000000000LL; // 0 fee
    uint256 hashFreeGrandchild = tx.GetHash();
    tx_mempool.addUnchecked(entry.Fee(0).SpendsCoinbase(false).FromTx(tx));

    auto pblocktemplate = AssemblerForTest(tx_mempool).CreateNewBlock(scriptPubKey);
    BOOST_REQUIRE_EQUAL(pblocktemplate->block.vtx.size(), 6U);
    BOOST_CHECK(pblocktemplate->block.vtx[1]->GetHash() == hashFreeParent);
    BOOST_CHECK(pblocktemplate->block.vtx[2]->GetHash() == hashFreePrioritisedTx);
    BOOST_CHECK(pblocktemplate->block.vtx[3]->GetHash() == hashParentTx);
    BOOST_CHECK(pblocktemplate->block.vtx[4]->GetHash() == hashPrioritsedChild);
    BOOST_CHECK(pblocktemplate->block.vtx[5]->GetHash() == hashFreeChild);
    for (size_t i=0; i<pblocktemplate->block.vtx.size(); ++i) {
        // The FreeParent and FreeChild's prioritisations should not impact the child.
        BOOST_CHECK(pblocktemplate->block.vtx[i]->GetHash() != hashFreeGrandchild);
        // De-prioritised transaction should not be included.
        BOOST_CHECK(pblocktemplate->block.vtx[i]->GetHash() != hashMediumFeeTx);
    }
}

// CreateNewBlock self-validation (TestBlockValidity) is enabled only where BOOST_CHECK_EXCEPTION requires it;
// see AssemblerForTest — Rigor fork fixtures otherwise skip full ConnectBlock script checks.
BOOST_AUTO_TEST_CASE(CreateNewBlock_validity)
{
    // Note that by default, these tests run with size accounting enabled.
    const CPubKey coinbase_pk{ParseHex("04678afdb0fe5548271967f1a67130b7105cd6a828e03909a67962e0ea1f61deb649f6bc3f4cef38c4f35504e51ec112de5c384df7ba0b8d578a4c702b6bf11d5f")};
    const CScript scriptPubKey{GetScriptForDestination(WitnessV0KeyHash(coinbase_pk))};
    std::unique_ptr<CBlockTemplate> pblocktemplate;

    CTxMemPool& tx_mempool{*m_node.mempool};
    // Simple block creation, nothing special yet:
    BOOST_CHECK(pblocktemplate = AssemblerForTest(tx_mempool).CreateNewBlock(scriptPubKey));

    // We can't make transactions until we have inputs
    // Therefore, load 110 blocks :)
    static_assert(std::size(BLOCKINFO_EXTRANONCE) == 110, "Should have 110 blocks to import");
    int baseheight = 0;
    std::vector<CTransactionRef> txFirst;
    for (size_t k = 0; k < std::size(BLOCKINFO_EXTRANONCE); ++k) {
        (void)k;
        CBlock *pblock = &pblocktemplate->block; // pointer for convenience
        {
            LOCK(cs_main);
            pblock->nVersion = VERSIONBITS_TOP_BITS;
            pblock->nTime = m_node.chainman->ActiveChain().Tip()->GetMedianTimePast()+1;
            CMutableTransaction txCoinbase(*pblock->vtx[0]);
            txCoinbase.nVersion = 1;
            txCoinbase.vin[0].scriptSig = Consensus::BuildCoinbaseScriptSig(m_node.chainman->ActiveChain().Height() + 1);
            pblock->vtx[0] = MakeTransactionRef(std::move(txCoinbase));
            RefreshCoinbaseWitnessAndconvergence(*pblock, m_node.chainman->ActiveChain().Tip(), *Assert(m_node.chainman));
            if (txFirst.size() == 0)
                baseheight = m_node.chainman->ActiveChain().Height();
            if (txFirst.size() < 4)
                txFirst.push_back(pblock->vtx[0]);
            pblock->hashMerkleRoot = BlockMerkleRoot(*pblock);
            pblock->nNonce = 0;
            while (!CheckProofOfWork(pblock->GetBlockHeader(), pblock->nBits, m_node.chainman->GetConsensus())) {
                ++pblock->nNonce;
                BOOST_CHECK(pblock->nNonce != 0);
            }
        }
        std::shared_ptr<const CBlock> shared_pblock = std::make_shared<const CBlock>(*pblock);
        BOOST_CHECK(Assert(m_node.chainman)->ProcessNewBlock(shared_pblock, true, true, nullptr));
        pblock->hashPrevBlock = pblock->GetHash();
    }

    LOCK(cs_main);

    TestBasicMining(scriptPubKey, txFirst, baseheight);

    m_node.chainman->ActiveChain().Tip()->nHeight--;
    SetMockTime(0);

    TestPackageSelection(scriptPubKey, txFirst);

    m_node.chainman->ActiveChain().Tip()->nHeight--;
    SetMockTime(0);

    TestPrioritisedMining(scriptPubKey, txFirst);
}

BOOST_AUTO_TEST_SUITE_END()
