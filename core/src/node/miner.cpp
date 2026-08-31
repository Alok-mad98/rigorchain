// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <node/miner.h>

#include <node/devtest_res_bootstrap.h>
#include <node/auto_res_mint.h>
#include <chain.h>
#include <chainparams.h>
#include <coins.h>
#include <consensus/amount.h>
#include <consensus/convergence_commitment.h>
#include <consensus/convergence_logic.h>
#include <consensus/convergenceres.h>
#include <consensus/consensus.h>
#include <consensus/convergence_root.h>
#include <consensus/merkle.h>
#include <consensus/tx_verify.h>
#include <consensus/validation.h>
#include <script/script.h>
#include <deploymentstatus.h>
#include <key_io.h>
#include <logging.h>
#include <policy/feerate.h>
#include <policy/policy.h>
#include <pow.h>
#include <primitives/transaction.h>
#include <script/standard.h>
#include <timedata.h>
#include <undo.h>
#include <util/check.h>
#include <util/moneystr.h>
#include <util/system.h>
#include <validation.h>

#include <algorithm>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>

namespace node {

/** Resolve the mockable-chain bootstrap sink. Never reads gArgs; the flag is
 *  parsed into Options at construction. Non-mockable chains always return empty.
 */
static CScript ResolveMiningResFeeScript(const CScript& explicit_script, const CScript& from_options, const CChainParams& chainparams)
{
    if (!chainparams.IsMockableChain()) {
        return {};
    }
    if (!explicit_script.empty()) {
        return explicit_script;
    }
    return from_options;
}
int64_t UpdateTime(CBlockHeader* pblock, const Consensus::Params& consensusParams, const CBlockIndex* pindexPrev)
{
    int64_t nOldTime = pblock->nTime;
    int64_t nNewTime{std::max<int64_t>(pindexPrev->GetMedianTimePast() + 1, TicksSinceEpoch<std::chrono::seconds>(GetAdjustedTime()))};

    if (nOldTime < nNewTime) {
        pblock->nTime = nNewTime;
    }

    // Testnet min-difficulty: a sufficiently future nTime switches the next
    // compact target to powLimit. ASERT uses only the parent timestamp, so
    // mainnet nBits does not change when GBT rolls nTime.
    if (consensusParams.fPowAllowMinDifficultyBlocks) {
        pblock->nBits = GetNextWorkRequired(pindexPrev, pblock, consensusParams);
    }

    return nNewTime - nOldTime;
}

void RegenerateCommitments(CBlock& block, ChainstateManager& chainman)
{
    const int commitpos = GetWitnessCommitmentIndex(block);
    if (commitpos != NO_WITNESS_COMMITMENT) {
        CMutableTransaction tx{*block.vtx.at(0)};
        tx.vout.erase(tx.vout.begin() + commitpos);
        block.vtx.at(0) = MakeTransactionRef(tx);
    }

    const CBlockIndex* prev_block = WITH_LOCK(::cs_main, return chainman.m_blockman.LookupBlockIndex(block.hashPrevBlock));
    chainman.GenerateCoinbaseCommitment(block, prev_block);

    block.hashMerkleRoot = BlockMerkleRoot(block);
}

bool RebuildTemplateCommitments(CBlock& block, ChainstateManager& chainman, std::string& err)
{
    AssertLockHeld(::cs_main);
    if (block.vtx.empty() || !block.vtx[0]->IsCoinBase()) {
        err = "RebuildTemplateCommitments: missing coinbase";
        return false;
    }
    const CBlockIndex* prev = chainman.m_blockman.LookupBlockIndex(block.hashPrevBlock);
    if (prev == nullptr) {
        err = "RebuildTemplateCommitments: prev block not found";
        return false;
    }
    // C comes from the active chainstate's UTXO view, which is only the view at this
    // block's entry while `prev` is still the tip. cs_main cannot express that, so the
    // caller must hold it across template creation and this call.
    if (prev != chainman.ActiveChain().Tip()) {
        err = "RebuildTemplateCommitments: block does not build on the active tip";
        return false;
    }
    const Consensus::Params& consensus = chainman.GetParams().GetConsensus();
    const int height = prev->nHeight + 1;

    std::optional<uint256> paid_winner;
    if (!GetTemplatePaidWinner(block, prev, consensus, paid_winner)) {
        err = "RebuildTemplateCommitments: cannot read parent draw set";
        return false;
    }

    CCoinsViewCache view(&chainman.ActiveChainstate().CoinsTip());
    const ConvergenceDrawSet committed = GetTemplateConvergenceDrawSet(block, prev, view, consensus);
    if (!committed.IsSet()) {
        err = strprintf("RebuildTemplateCommitments: cannot reconstruct convergence window at height=%d hash=%s",
                        prev->nHeight, prev->GetBlockHash().ToString());
        return false;
    }

    CMutableTransaction mtx(*block.vtx[0]);
    mtx.vin[0].scriptSig = Consensus::BuildCoinbaseCommitmentScriptSig(
        height, committed.convergence_root, paid_winner.value_or(uint256{}));
    block.vtx[0] = MakeTransactionRef(std::move(mtx));

    RegenerateCommitments(block, chainman);
    return true;
}

static BlockAssembler::Options ClampOptions(BlockAssembler::Options options)
{
    // Limit weight to between 4K and DEFAULT_BLOCK_MAX_WEIGHT for sanity:
    options.nBlockMaxWeight = std::clamp<size_t>(options.nBlockMaxWeight, 4000, DEFAULT_BLOCK_MAX_WEIGHT);
    return options;
}

BlockAssembler::BlockAssembler(Chainstate& chainstate, const CTxMemPool* mempool, const Options& options)
    : chainparams{chainstate.m_chainman.GetParams()},
      m_mempool{mempool},
      m_chainstate{chainstate},
      m_options{ClampOptions(options)}
{
}

void ApplyArgsManOptions(const ArgsManager& args, const CChainParams& chainparams, BlockAssembler::Options& options)
{
    // Block resource limits
    options.nBlockMaxWeight = args.GetIntArg("-blockmaxweight", options.nBlockMaxWeight);
    if (const auto blockmintxfee{args.GetArg("-blockmintxfee")}) {
        if (const auto parsed{ParseMoney(*blockmintxfee)}) options.blockMinFeeRate = CFeeRate{*parsed};
    }
    if (args.IsArgSet("-blockversion")) {
        options.block_version_override = static_cast<int32_t>(args.GetIntArg("-blockversion", 0));
    }
    if (chainparams.IsMockableChain() && args.IsArgSet("-miningresfeeaddr")) {
        std::string dest_err;
        const CTxDestination dest = DecodeDestination(args.GetArg("-miningresfeeaddr", ""), chainparams, dest_err, nullptr);
        if (IsValidDestination(dest) && std::holds_alternative<WitnessV1Res>(dest)) {
            options.res_bootstrap_script = GetScriptForDestination(dest);
        }
    }
}
static BlockAssembler::Options ConfiguredOptions(const CChainParams& chainparams)
{
    BlockAssembler::Options options;
    ApplyArgsManOptions(gArgs, chainparams, options);
    return options;
}

BlockAssembler::BlockAssembler(Chainstate& chainstate, const CTxMemPool* mempool)
    : BlockAssembler(chainstate, mempool, ConfiguredOptions(chainstate.m_chainman.GetParams())) {}

void BlockAssembler::resetBlock()
{
    inBlock.clear();
    m_assemble_coins.reset();

    // Reserve space for coinbase tx
    nBlockWeight = 4000;
    nBlockSigOpsCost = 400;

    // These counters do not include coinbase tx
    nBlockTx = 0;
    nFeesOru = 0;
    nFeesOruResTransferBurn = 0;
}

std::unique_ptr<CBlockTemplate> BlockAssembler::CreateNewBlock(const CScript& scriptPubKeyIn, const CScript& scriptPubKeyResFeeIn)
{
    const auto time_start{SteadyClock::now()};

    resetBlock();

    pblocktemplate.reset(new CBlockTemplate());

    if (!pblocktemplate.get()) {
        return nullptr;
    }
    CBlock* const pblock = &pblocktemplate->block; // pointer for convenience

    // Add dummy coinbase tx as first transaction
    pblock->vtx.emplace_back();
    pblocktemplate->vTxFees.push_back(-1); // updated at end
    pblocktemplate->vTxSigOpsCost.push_back(-1); // updated at end

    LOCK(::cs_main);
    CBlockIndex* pindexPrev = m_chainstate.m_chain.Tip();
    assert(pindexPrev != nullptr);
    m_assemble_coins = std::make_unique<CCoinsViewCache>(&m_chainstate.CoinsTip(), /*deterministic=*/false);
    nHeight = pindexPrev->nHeight + 1;

    pblock->nVersion = m_chainstate.m_chainman.m_versionbitscache.ComputeBlockVersion(pindexPrev, chainparams.GetConsensus());
    // -regtest only: allow overriding block.nVersion with -blockversion=N
    if (chainparams.MineBlocksOnDemand() && m_options.block_version_override) {
        pblock->nVersion = *m_options.block_version_override;
    }

    pblock->nTime = TicksSinceEpoch<std::chrono::seconds>(GetAdjustedTime());
    m_lock_time_cutoff = pindexPrev->GetMedianTimePast();

    int nPackagesSelected = 0;
    int nDescendantsUpdated = 0;
    if (m_mempool) {
        LOCK(m_mempool->cs);
        addPackageTxs(*m_mempool, nPackagesSelected, nDescendantsUpdated);
    }
    // POOL-F (P8): template assembly sums only ORU `CTxMemPoolEntry::GetFee()`. RES transfer
    // amounts never become fees, so there is no RES fee accumulator to reconcile here.
    Assert(nFeesOruResTransferBurn >= 0);
    Assert(nFeesOruResTransferBurn <= nFeesOru);

    const auto time_1{SteadyClock::now()};

    m_last_block_num_txs = nBlockTx;
    m_last_block_weight = nBlockWeight;

    // Create coinbase transaction.
    CMutableTransaction coinbaseTx;
    coinbaseTx.vin.resize(1);
    coinbaseTx.vin[0].prevout.SetNull();
    coinbaseTx.vout.resize(1);
    coinbaseTx.vout[0].scriptPubKey = scriptPubKeyIn;
    const Consensus::Params& consensus = chainparams.GetConsensus();
    // RFC-018: do not claim the RES-transfer fee burn share.
    coinbaseTx.vout[0].nValue = (nFeesOru - nFeesOruResTransferBurn) + GetBlockSubsidy(nHeight, consensus);
    // Placeholder scriptSig: ConvergenceRoot C is written AFTER optional coinbase mint
    // append. FinalizeWindowedDrawSet excludes winners already paid by a mint in this block
    // (BlockPaidWinner), so C computed before the carrier is appended disagrees with ConnectBlock
    // and fails TestBlockValidity with bad-cb-convergence-root on payout blocks.
    coinbaseTx.vin[0].scriptSig = CScript() << nHeight << std::vector<unsigned char>(32, 0);
    const CScript res_fee_script = ResolveMiningResFeeScript(scriptPubKeyResFeeIn, m_options.res_bootstrap_script, chainparams);
    const CAmount res_wallet_bootstrap = DevTestResWalletBootstrap(chainparams);
    if (res_wallet_bootstrap > 0) {
        if (res_fee_script.empty()) {
            throw std::runtime_error(
                strprintf("%s: -reswalletbootstrap requires -miningresfeeaddr=<witness-v1-RES> to arm mint assembly (not a fee address; network fees are ORU-only)", __func__));
        }
        CTxOut res_boot_out;
        res_boot_out.nValue = res_wallet_bootstrap;
        res_boot_out.scriptPubKey = res_fee_script;
        coinbaseTx.vout.push_back(res_boot_out);
    }
    pblock->vtx[0] = MakeTransactionRef(std::move(coinbaseTx));

    // Append the RES mint to the coinbase before the witness commitment, so
    // BlockWitnessMerkleRoot matches ConnectBlock. There is no body mint and
    // no funding input: the prize is fully determined by the parent, and the coinbase is
    // authenticated by proof of work.
    //
    // While W(h) > 0, every template MUST carry the mint whenever the parent draw has a winner
    // — otherwise TestBlockValidity fails with bad-rigor-res-winner-mismatch. After
    // RES_REWARD_LAST_EPOCH, W(h) = 0: a winner does not authorize a mint (H-1). Omit without
    // throwing so mining continues. Assembly is not gated on -miningresfeeaddr.
    std::optional<uint256> template_paid_txid;
    {
        ConvergenceDrawSet parent_draw_set;
        const bool have_parent_draw = GetConvergenceDrawSetForBlock(*pindexPrev, consensus, parent_draw_set);
        if (!have_parent_draw && pindexPrev->nHeight > 0) {
            LogPrintf("CreateNewBlock(): cannot read parent convergence draw set at height=%d hash=%s "
                      "(template will omit any required RES mint and TestBlockValidity may fail)\n",
                      pindexPrev->nHeight, pindexPrev->GetBlockHash().ToString());
        }
        const ConvergenceDrawOutcome parent_outcome =
            have_parent_draw ? ResolveConvergenceDraw(parent_draw_set, consensus.RES_PROBABILITY_TARGET)
                             : ConvergenceDrawOutcome{};
        if (parent_outcome.HasWinner()) {
            const CAmount res_reward = Consensus::GetResMintReward(nHeight);
            if (res_reward <= 0) {
                LogPrintf("CreateNewBlock(): RES schedule exhausted at height=%d; omitting mint despite draw winner\n",
                          nHeight);
            } else {
                LogPrintf("CreateNewBlock(): parent draw has winner %s — appending coinbase RES mint for height %d\n",
                          parent_draw_set.candidates[static_cast<size_t>(parent_outcome.winner_index)].txid.ToString(),
                          nHeight);
                CMutableTransaction mtx_cb(*pblock->vtx[0]);
                if (!TryAppendCoinbaseResMint(mtx_cb, parent_draw_set, parent_outcome, nHeight)) {
                    throw std::runtime_error(strprintf(
                        "%s: automatic Convergence RES mint assembly failed (winner present)",
                        __func__));
                }
                pblock->vtx[0] = MakeTransactionRef(std::move(mtx_cb));
                template_paid_txid = parent_draw_set.candidates[static_cast<size_t>(parent_outcome.winner_index)].txid;
            }
        }
    }

    // Coinbase scriptSig: height || C || paid-winner txid (zeros if this block does not mint).
    {
        CCoinsViewCache view_convergence_root_at_entry(&m_chainstate.CoinsTip());
        const ConvergenceDrawSet committed = GetTemplateConvergenceDrawSet(
            *pblock, pindexPrev, view_convergence_root_at_entry, consensus);
        if (!committed.IsSet()) {
            if (m_options.test_block_validity) {
                throw std::runtime_error(strprintf(
                    "%s: cannot reconstruct convergence window at height=%d hash=%s",
                    __func__, pindexPrev->nHeight, pindexPrev->GetBlockHash().ToString()));
            }
            // Header-only test chains have no window records on disk. Leave the
            // placeholder scriptSig; do not write an all-zero C as if it were a root.
        } else {
            CMutableTransaction mtx_cb(*pblock->vtx[0]);
            mtx_cb.vin[0].scriptSig = Consensus::BuildCoinbaseCommitmentScriptSig(
                nHeight, committed.convergence_root, template_paid_txid.value_or(uint256{}));
            pblock->vtx[0] = MakeTransactionRef(std::move(mtx_cb));
        }
    }

    pblocktemplate->vchCoinbaseCommitment = m_chainstate.m_chainman.GenerateCoinbaseCommitment(*pblock, pindexPrev);
    pblocktemplate->vTxFees[0] = -nFeesOru;

    LogPrintf("CreateNewBlock(): block weight: %u txs: %u oru_fees: %ld oru_fee_burn: %ld sigops %d\n",
              GetBlockWeight(*pblock), nBlockTx, nFeesOru, nFeesOruResTransferBurn, nBlockSigOpsCost);

    // Fill in header
    pblock->hashPrevBlock  = pindexPrev->GetBlockHash();
    UpdateTime(pblock, chainparams.GetConsensus(), pindexPrev);
    pblock->nBits          = GetNextWorkRequired(pindexPrev, pblock, chainparams.GetConsensus());
    pblock->nNonce         = 0;
    pblocktemplate->vTxSigOpsCost[0] = WITNESS_SCALE_FACTOR * GetLegacySigOpCount(*pblock->vtx[0]);

    BlockValidationState state;
    if (m_options.test_block_validity && !TestBlockValidity(state, chainparams, m_chainstate, *pblock, pindexPrev,
                                                  GetAdjustedTime, /*fCheckPOW=*/false, /*fCheckMerkleRoot=*/false)) {
        throw std::runtime_error(strprintf("%s: TestBlockValidity failed: %s", __func__, state.ToString()));
    }
    const auto time_2{SteadyClock::now()};

    LogPrint(BCLog::BENCH, "CreateNewBlock() packages: %.2fms (%d packages, %d updated descendants), validity: %.2fms (total %.2fms)\n",
             Ticks<MillisecondsDouble>(time_1 - time_start), nPackagesSelected, nDescendantsUpdated,
             Ticks<MillisecondsDouble>(time_2 - time_1),
             Ticks<MillisecondsDouble>(time_2 - time_start));

    return std::move(pblocktemplate);
}

void BlockAssembler::onlyUnconfirmed(CTxMemPool::setEntries& testSet)
{
    for (CTxMemPool::setEntries::iterator iit = testSet.begin(); iit != testSet.end(); ) {
        // Only test txs not already in the block
        if (inBlock.count(*iit)) {
            testSet.erase(iit++);
        } else {
            iit++;
        }
    }
}

bool BlockAssembler::TestPackage(uint64_t packageSize, int64_t packageSigOpsCost) const
{
    // TODO: switch to weight-based accounting for packages instead of vsize-based accounting.
    if (nBlockWeight + WITNESS_SCALE_FACTOR * packageSize >= m_options.nBlockMaxWeight) {
        return false;
    }
    if (nBlockSigOpsCost + packageSigOpsCost >= MAX_BLOCK_SIGOPS_COST) {
        return false;
    }
    return true;
}

// Perform transaction-level checks before adding to block:
// - transaction finality (locktime)
bool BlockAssembler::TestPackageTransactions(const CTxMemPool::setEntries& package) const
{
    for (CTxMemPool::txiter it : package) {
        if (!IsFinalTx(it->GetTx(), nHeight, m_lock_time_cutoff)) {
            return false;
        }
    }
    return true;
}

void BlockAssembler::AddToBlock(CTxMemPool::txiter iter)
{
    pblocktemplate->block.vtx.emplace_back(iter->GetSharedTx());
    pblocktemplate->vTxFees.push_back(iter->GetFee());
    pblocktemplate->vTxSigOpsCost.push_back(iter->GetSigOpCost());
    nBlockWeight += iter->GetTxWeight();
    ++nBlockTx;
    nBlockSigOpsCost += iter->GetSigOpCost();
    const CTransaction& mtx = iter->GetTx();
    if (mtx.vout.empty()) {
        throw std::runtime_error(strprintf("%s: transaction has no outputs", __func__));
    }
    if (!m_assemble_coins) {
        throw std::runtime_error(strprintf("%s: assemble coin view not initialized", __func__));
    }
    if (mtx.IsCoinBase()) {
        throw std::runtime_error(strprintf("%s: TestBlockValidity failed: bad-cb-multiple", __func__));
    }
    if (!m_assemble_coins->HaveInputs(mtx)) {
        throw std::runtime_error(strprintf("%s: TestBlockValidity failed: bad-txns-inputs-missingorspent", __func__));
    }
    CTxUndo undo;
    // POOL-F: mempool entry fee is always ORU (dual-pool RES spends pay ORU fee).
    const CAmount fee = iter->GetFee();
    nFeesOru += fee;
    // RFC-018: classify before UpdateCoins while prevouts remain in the assemble view.
    if (IsORBClaimTx(mtx, *m_assemble_coins)) {
        nFeesOruResTransferBurn += Consensus::OruResTransferFeeBurnShare(fee);
    }
    UpdateCoins(mtx, *m_assemble_coins, undo, nHeight);
    inBlock.insert(iter);

    bool fPrintPriority = gArgs.GetBoolArg("-printpriority", DEFAULT_PRINTPRIORITY);
    if (fPrintPriority) {
        LogPrintf("fee rate %s txid %s\n",
                  CFeeRate(iter->GetModifiedFee(), iter->GetTxSize()).ToString(),
                  iter->GetTx().GetHash().ToString());
    }
}

/** Add descendants of given transactions to mapModifiedTx with ancestor
 * state updated assuming given transactions are inBlock. Returns number
 * of updated descendants. */
static int UpdatePackagesForAdded(const CTxMemPool& mempool,
                                  const CTxMemPool::setEntries& alreadyAdded,
                                  indexed_modified_transaction_set& mapModifiedTx) EXCLUSIVE_LOCKS_REQUIRED(mempool.cs)
{
    AssertLockHeld(mempool.cs);

    int nDescendantsUpdated = 0;
    for (CTxMemPool::txiter it : alreadyAdded) {
        CTxMemPool::setEntries descendants;
        mempool.CalculateDescendants(it, descendants);
        // Insert all descendants (not yet in block) into the modified set
        for (CTxMemPool::txiter desc : descendants) {
            if (alreadyAdded.count(desc)) {
                continue;
            }
            ++nDescendantsUpdated;
            modtxiter mit = mapModifiedTx.find(desc);
            if (mit == mapModifiedTx.end()) {
                CTxMemPoolModifiedEntry modEntry(desc);
                mit = mapModifiedTx.insert(modEntry).first;
            }
            mapModifiedTx.modify(mit, update_for_parent_inclusion(it));
        }
    }
    return nDescendantsUpdated;
}

void BlockAssembler::SortForBlock(const CTxMemPool::setEntries& package, std::vector<CTxMemPool::txiter>& sortedEntries)
{
    // Sort package by ancestor count
    // If a transaction A depends on transaction B, then A's ancestor count
    // must be greater than B's.  So this is sufficient to validly order the
    // transactions for block inclusion.
    sortedEntries.clear();
    sortedEntries.insert(sortedEntries.begin(), package.begin(), package.end());
    std::sort(sortedEntries.begin(), sortedEntries.end(), CompareTxIterByAncestorCount());
}

// This transaction selection algorithm orders the mempool based
// on feerate of a transaction including all unconfirmed ancestors.
// Since we don't remove transactions from the mempool as we select them
// for block inclusion, we need an alternate method of updating the feerate
// of a transaction with its not-yet-selected ancestors as we go.
// This is accomplished by walking the in-mempool descendants of selected
// transactions and storing a temporary modified state in mapModifiedTxs.
// Each time through the loop, we compare the best transaction in
// mapModifiedTxs with the next transaction in the mempool to decide what
// transaction package to work on next.
void BlockAssembler::addPackageTxs(const CTxMemPool& mempool, int& nPackagesSelected, int& nDescendantsUpdated)
{
    AssertLockHeld(mempool.cs);

    // mapModifiedTx will store sorted packages after they are modified
    // because some of their txs are already in the block
    indexed_modified_transaction_set mapModifiedTx;
    // Keep track of entries that failed inclusion, to avoid duplicate work
    CTxMemPool::setEntries failedTx;

    CTxMemPool::indexed_transaction_set::index<ancestor_score>::type::iterator mi = mempool.mapTx.get<ancestor_score>().begin();
    CTxMemPool::txiter iter;

    // Limit the number of attempts to add transactions to the block when it is
    // close to full; this is just a simple heuristic to finish quickly if the
    // mempool has a lot of entries.
    const int64_t MAX_CONSECUTIVE_FAILURES = 1000;
    int64_t nConsecutiveFailed = 0;

    while (mi != mempool.mapTx.get<ancestor_score>().end() || !mapModifiedTx.empty()) {
        // First try to find a new transaction in mapTx to evaluate.
        //
        // Skip entries in mapTx that are already in a block or are present
        // in mapModifiedTx (which implies that the mapTx ancestor state is
        // stale due to ancestor inclusion in the block)
        // Also skip transactions that we've already failed to add. This can happen if
        // we consider a transaction in mapModifiedTx and it fails: we can then
        // potentially consider it again while walking mapTx.  It's currently
        // guaranteed to fail again, but as a belt-and-suspenders check we put it in
        // failedTx and avoid re-evaluation, since the re-evaluation would be using
        // cached size/sigops/fee values that are not actually correct.
        /** Return true if given transaction from mapTx has already been evaluated,
         * or if the transaction's cached data in mapTx is incorrect. */
        if (mi != mempool.mapTx.get<ancestor_score>().end()) {
            auto it = mempool.mapTx.project<0>(mi);
            assert(it != mempool.mapTx.end());
            if (mapModifiedTx.count(it) || inBlock.count(it) || failedTx.count(it)) {
                ++mi;
                continue;
            }
        }

        // Now that mi is not stale, determine which transaction to evaluate:
        // the next entry from mapTx, or the best from mapModifiedTx?
        bool fUsingModified = false;

        modtxscoreiter modit = mapModifiedTx.get<ancestor_score>().begin();
        if (mi == mempool.mapTx.get<ancestor_score>().end()) {
            // We're out of entries in mapTx; use the entry from mapModifiedTx
            iter = modit->iter;
            fUsingModified = true;
        } else {
            // Try to compare the mapTx entry to the mapModifiedTx entry
            iter = mempool.mapTx.project<0>(mi);
            if (modit != mapModifiedTx.get<ancestor_score>().end() &&
                    CompareTxMemPoolEntryByAncestorFee()(*modit, CTxMemPoolModifiedEntry(iter))) {
                // The best entry in mapModifiedTx has higher score
                // than the one from mapTx.
                // Switch which transaction (package) to consider
                iter = modit->iter;
                fUsingModified = true;
            } else {
                // Either no entry in mapModifiedTx, or it's worse than mapTx.
                // Increment mi for the next loop iteration.
                ++mi;
            }
        }

        // We skip mapTx entries that are inBlock, and mapModifiedTx shouldn't
        // contain anything that is inBlock.
        assert(!inBlock.count(iter));

        uint64_t packageSize = iter->GetSizeWithAncestors();
        CAmount packageFees = iter->GetModFeesWithAncestors();
        int64_t packageSigOpsCost = iter->GetSigOpCostWithAncestors();
        if (fUsingModified) {
            packageSize = modit->nSizeWithAncestors;
            packageFees = modit->nModFeesWithAncestors;
            packageSigOpsCost = modit->nSigOpCostWithAncestors;
        }

        if (packageFees < m_options.blockMinFeeRate.GetFee(packageSize)) {
            // Everything else we might consider has a lower fee rate
            return;
        }

        if (!TestPackage(packageSize, packageSigOpsCost)) {
            if (fUsingModified) {
                // Since we always look at the best entry in mapModifiedTx,
                // we must erase failed entries so that we can consider the
                // next best entry on the next loop iteration
                mapModifiedTx.get<ancestor_score>().erase(modit);
                failedTx.insert(iter);
            }

            ++nConsecutiveFailed;

            if (nConsecutiveFailed > MAX_CONSECUTIVE_FAILURES && nBlockWeight >
                    m_options.nBlockMaxWeight - 4000) {
                // Give up if we're close to full and haven't succeeded in a while
                break;
            }
            continue;
        }

        auto ancestors{mempool.AssumeCalculateMemPoolAncestors(__func__, *iter, CTxMemPool::Limits::NoLimits(), /*fSearchForParents=*/false)};

        onlyUnconfirmed(ancestors);
        ancestors.insert(iter);

        // Test if all tx's are Final
        if (!TestPackageTransactions(ancestors)) {
            if (fUsingModified) {
                mapModifiedTx.get<ancestor_score>().erase(modit);
                failedTx.insert(iter);
            }
            continue;
        }

        // This transaction will make it in; reset the failed counter.
        nConsecutiveFailed = 0;

        // Package can be added. Sort the entries in a valid order.
        std::vector<CTxMemPool::txiter> sortedEntries;
        SortForBlock(ancestors, sortedEntries);

        for (size_t i = 0; i < sortedEntries.size(); ++i) {
            AddToBlock(sortedEntries[i]);
            // Erase from the modified set, if present
            mapModifiedTx.erase(sortedEntries[i]);
        }

        ++nPackagesSelected;

        // Update transactions that depend on each of these
        nDescendantsUpdated += UpdatePackagesForAdded(mempool, ancestors, mapModifiedTx);
    }
}
} // namespace node
