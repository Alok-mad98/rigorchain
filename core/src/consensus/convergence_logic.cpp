// Convergence minimal-core consensus: selection digest, integer β inequality, deterministic winner.

#include <consensus/convergence_logic.h>

#include <arith_uint256.h>
#include <chain.h>
#include <coins.h>
#include <consensus/rigor_math.h>
#include <consensus/convergence_root.h>
#include <consensus/convergenceres.h>
#include <consensus/draw_digest.h>
#include <consensus/params.h>
#include <consensus/res_owner.h>
#include <consensus/res_proof.h>
#include <consensus/rigor_consensus_lock.h>
#include <consensus/tx_verify.h>
#include <consensus/validation.h>
#include <consensus_rigor/fee.h>
#include <hash.h>
#include <script/predicate_verify.h>
#include <script/standard.h>
#include <undo.h>

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <optional>
#include <vector>

uint64_t GetTransactionConvergenceAmount(const CTransaction& tx)
{
    const CAmount convergence_amount = Consensus::GetConvergenceAmount(tx);
    if (convergence_amount <= 0) return 0;
    return static_cast<uint64_t>(convergence_amount);
}

// RFC-000 §4 (Sigma-PoW-01 revision):
//   σ_i = Hash256( txid || C || B_{h-1} || LE32(h) )
//
// Both txid and C are drawn from block h-1 and B_{h-1} is that block's hash, so σ
// is a function of one already-mined header. See consensus_rigor/sigma_hash.h.
SelectionDigestHash SelectionDigest(const uint256& txid, const uint256& prev_block_hash, int block_height, const uint256& convergence_root)
{
    assert(block_height >= 0);
    return Consensus::CalculateSigmaHash(txid, convergence_root, prev_block_hash, static_cast<uint32_t>(block_height));
}

uint256 ComputeConvergenceHash(const uint256& txid, const uint256& prev_block_hash, int block_height, const uint256& convergence_root)
{
    return SelectionDigest(txid, prev_block_hash, block_height, convergence_root);
}

ConvergenceDrawOutcome ResolveConvergenceDraw(const ConvergenceDrawSet& set,
    const uint256& res_probability_target)
{
    ConvergenceDrawOutcome out{};
    if (!set.IsSet()) {
        return out;
    }

    // The payout height is fixed by the rule, not chosen by the caller: a set committed
    // by block h-1 is drawn by block h.
    const int payout_height = set.source_height + 1;

    out.sigmas.resize(set.candidates.size());
    for (size_t i = 0; i < set.candidates.size(); ++i) {
        out.sigmas[i] = SelectionDigest(set.candidates[i].txid, set.source_block_hash, payout_height, set.convergence_root);
    }

    // RFC-015 trigger floor: sets below the minimum cardinality award nothing, but σ is
    // still reported so introspection can show why.
    if (set.candidates.size() < static_cast<size_t>(Consensus::CONVERGENCE_DRAW_MIN_TXS_FOR_RES)) {
        return out;
    }

    // RFC-014 Conv-Decouple-01: the inequality RHS is the constitutional constant
    // floor(2^246 / MIN_CONVERGENCE_AMOUNT) / CONVERGENCE_DRAW_WINDOW (RFC-017; C=1024 ORU), never a difficulty-derived value.
    out.eligible.assign(set.candidates.size(), false);
    for (size_t i = 0; i < out.sigmas.size(); ++i) {
        out.eligible[i] = Consensus::ConvergenceDrawInequalityHolds(
            out.sigmas[i], res_probability_target, static_cast<CAmount>(set.candidates[i].weight));
    }

    int winner_index = -1;
    for (size_t i = 0; i < out.sigmas.size(); ++i) {
        if (!out.eligible[i]) continue;

        if (winner_index == -1) {
            winner_index = static_cast<int>(i);
            continue;
        }

        // RFC-008 §8: i ≺ j iff H_i * f_j < H_j * f_i, evaluated in 512 bits, no division.
        const arith_uint256 H_i = UintToArith256(out.sigmas[i]);
        const arith_uint256 f_i(set.candidates[i].weight);
        const arith_uint256 H_winner = UintToArith256(out.sigmas[static_cast<size_t>(winner_index)]);
        const arith_uint256 f_winner(set.candidates[static_cast<size_t>(winner_index)].weight);

        const Consensus::uint512 lhs = Consensus::rigor_math_detail::Mul256x256Full(H_i, f_winner);
        const Consensus::uint512 rhs = Consensus::rigor_math_detail::Mul256x256Full(H_winner, f_i);

        if (lhs < rhs) {
            winner_index = static_cast<int>(i);
        } else if (lhs == rhs) {
            // RFC-000 §9: exact tie breaks to the smaller txid.
            if (set.candidates[i].txid < set.candidates[static_cast<size_t>(winner_index)].txid) {
                winner_index = static_cast<int>(i);
            }
        }
    }

    out.winner_index = winner_index;
    return out;
}

namespace {
/** Shared tail of both draw-set builders: weight and A-5 declaration for an accepted txid. */
ConvergenceDrawCandidate MakeDrawCandidate(const CTransaction& tx, int source_height)
{
    ConvergenceDrawCandidate candidate;
    candidate.txid = tx.GetHash();
    candidate.weight = static_cast<uint64_t>(Consensus::GetConvergenceAmount(tx));
    const std::optional<Consensus::ResOwnerDeclaration> owner = Consensus::GetResOwnerDeclaration(tx);
    assert(owner.has_value());
    candidate.res_payout_hash160 = owner->payout_hash160;
    candidate.source_height = source_height;
    return candidate;
}

} // namespace

bool CoinbaseHasResMint(const CTransaction& coinbase)
{
    if (!coinbase.IsCoinBase()) return false;
    size_t n = 0;
    for (const CTxOut& o : coinbase.vout) {
        if (IsResMintScript(o.scriptPubKey)) ++n;
    }
    return n == 1;
}

std::vector<uint256> FindBlockPaidConvergenceTxids(const CBlock& block,
    const std::optional<uint256>& parent_winner_txid)
{
    std::vector<uint256> paid;
    if (block.vtx.empty() || !CoinbaseHasResMint(*block.vtx[0])) return paid;
    if (parent_winner_txid.has_value()) {
        paid.push_back(*parent_winner_txid);
    }
    return paid;
}

namespace {

/**
 * A-9-C: fold the buried part of the window and the source block's own candidates into one F,
 * dropping anything a mint inside the window already paid, then take C over the survivors.
 *
 * The source block's own mint is read here rather than supplied by the caller so that both
 * builders — live view and undo replay — apply the exclusion identically; a divergence between
 * them would be a fork.
 */
void FinalizeWindowedDrawSet(const CBlock& source_block,
    const ConvergenceWindowTail& tail,
    std::vector<ConvergenceDrawCandidate> own_candidates,
    ConvergenceDrawSet& set,
    const std::optional<uint256>& coinbase_paid_txid)
{
    set.candidates.clear();
    set.candidates.reserve(tail.candidates.size() + own_candidates.size());

    const std::vector<uint256> paid = FindBlockPaidConvergenceTxids(source_block, coinbase_paid_txid);
    const auto is_paid = [&paid](const uint256& txid) {
        return std::find(paid.begin(), paid.end(), txid) != paid.end();
    };

    // Oldest first: buried ancestors, then the source block. Ordering does not affect C or the
    // eligible root (both sort lexicographically) but keeps candidate indices reproducible.
    // Buried paid tickets are already absent from `tail.candidates` (dropped at tail assembly).
    for (const ConvergenceDrawCandidate& candidate : tail.candidates) {
        if (is_paid(candidate.txid)) continue;
        set.candidates.push_back(candidate);
    }
    for (ConvergenceDrawCandidate& candidate : own_candidates) {
        if (is_paid(candidate.txid)) continue;
        set.candidates.push_back(std::move(candidate));
    }

    std::vector<uint256> txids;
    txids.reserve(set.candidates.size());
    for (const ConvergenceDrawCandidate& candidate : set.candidates) {
        txids.push_back(candidate.txid);
    }
    set.convergence_root = Consensus::ComputeConvergenceRootFromEligibilityTxids(txids);
}
} // namespace

ConvergenceDrawSet BuildConvergenceDrawSet(const CBlock& block,
    int block_height,
    const CCoinsViewCache& view_at_block_entry,
    const Consensus::Params& consensus_params,
    const ConvergenceWindowTail& tail,
    const std::optional<uint256>& coinbase_paid_txid)
{
    ConvergenceDrawSet set;
    // An incomplete window would yield a different F, hence a different C and winner, so it
    // must surface as "no draw" rather than as a silently shortened window.
    if (!tail.complete) return set;

    set.source_block_hash = block.GetHash();
    set.source_height = block_height;
    set.source_nbits = block.nBits;

    const std::vector<uint256> candidate_txids = Consensus::ListConvergenceRootCommitmentTxids(
        block.vtx, view_at_block_entry, block_height, consensus_params);

    std::vector<ConvergenceDrawCandidate> own_candidates;
    own_candidates.reserve(candidate_txids.size());
    for (const uint256& candidate_txid : candidate_txids) {
        // From index 1: the coinbase is never a candidate, and the block template builder calls
        // this while vtx[0] is still the null placeholder it fills in afterwards.
        for (size_t i = 1; i < block.vtx.size(); ++i) {
            const CTransactionRef& tx = block.vtx[i];
            if (tx->GetHash() == candidate_txid) {
                own_candidates.push_back(MakeDrawCandidate(*tx, block_height));
                break;
            }
        }
    }
    FinalizeWindowedDrawSet(block, tail, std::move(own_candidates), set, coinbase_paid_txid);
    return set;
}

bool BuildBlockConvergenceCandidatesFromUndo(const CBlock& block,
    int block_height,
    const CBlockUndo& block_undo,
    const Consensus::Params& consensus_params,
    std::vector<ConvergenceDrawCandidate>& out)
{
    out.clear();
    if (block_undo.vtxundo.size() + 1 != block.vtx.size()) {
        return false;
    }
    // Same overlay walk as ListConvergenceRootCommitmentTxids. Missing coins are seeded
    // from this tx's undo (block-entry UTXOs); same-block creates come from AddCoins.
    CCoinsView backed;
    CCoinsViewCache overlay(&backed);
    out.reserve(block.vtx.size());
    for (size_t i = 1; i < block.vtx.size(); ++i) {
        const CTransaction& tx = *block.vtx[i];
        const CTxUndo& undo = block_undo.vtxundo[i - 1];
        if (undo.vprevout.size() != tx.vin.size()) {
            return false;
        }
        for (size_t j = 0; j < tx.vin.size(); ++j) {
            if (overlay.HaveCoin(tx.vin[j].prevout)) continue;
            const Coin& src = undo.vprevout[j];
            if (src.IsSpent()) {
                return false;
            }
            overlay.AddCoin(tx.vin[j].prevout, Coin(CTxOut(src.out), src.nHeight, src.IsCoinBase()), false);
        }
        if (Consensus::IsConvergenceDrawMember(tx, overlay, block_height, consensus_params)) {
            out.push_back(MakeDrawCandidate(tx, block_height));
        }
        for (const auto& txin : tx.vin) {
            overlay.SpendCoin(txin.prevout);
        }
        AddCoins(overlay, tx, block_height, /*check=*/true);
    }
    return true;
}

bool BuildConvergenceDrawSetFromUndo(const CBlock& block,
    int block_height,
    const CBlockUndo& block_undo,
    const Consensus::Params& consensus_params,
    const ConvergenceWindowTail& tail,
    ConvergenceDrawSet& out,
    const std::optional<uint256>& coinbase_paid_txid)
{
    out = ConvergenceDrawSet{};
    if (!tail.complete) return false;

    std::vector<ConvergenceDrawCandidate> own_candidates;
    if (!BuildBlockConvergenceCandidatesFromUndo(block, block_height, block_undo, consensus_params, own_candidates)) {
        return false;
    }
    out.source_block_hash = block.GetHash();
    out.source_height = block_height;
    out.source_nbits = block.nBits;
    FinalizeWindowedDrawSet(block, tail, std::move(own_candidates), out, coinbase_paid_txid);
    return true;
}

ConvergenceWindowTail TailFromParentDrawSet(const ConvergenceDrawSet& parent_set, int connect_height)
{
    ConvergenceWindowTail tail;
    if (!parent_set.IsSet()) {
        return tail;
    }
    const int window_first = std::max(0, connect_height - Consensus::CONVERGENCE_DRAW_WINDOW + 1);
    tail.candidates.reserve(parent_set.candidates.size());
    for (const ConvergenceDrawCandidate& candidate : parent_set.candidates) {
        if (candidate.source_height >= window_first) {
            tail.candidates.push_back(candidate);
        }
    }
    tail.complete = true;
    return tail;
}

bool IsConvergenceCandidate(const CTransaction& tx)
{
    return Consensus::IsConvergenceTx(tx) && Consensus::GetConvergenceAmount(tx) >= Consensus::MIN_CONVERGENCE_AMOUNT;
}

CAmount ComputeConvergenceFee(
    const CTransaction& tx,
    const CCoinsViewCache& view_at_block_entry,
    int block_height,
    const Consensus::Params& consensus_params)
{
    if (tx.IsCoinBase() || !IsConvergenceCandidate(tx)) return CAmount{0};
    // SECURITY-001-A / A-5 R1: mirrors ListConvergenceRootCommitmentTxids. This function is
    // the undo/reorg path's candidacy predicate, so the two must agree exactly or a reorg
    // would reconstruct a different F and a different winner.
    if (!Consensus::HasResOwnerDeclaration(tx)) return CAmount{0};
    TxValidationState tx_state;
    CAmount txfee = 0;
    if (!Consensus::CheckTxInputs(tx, tx_state, view_at_block_entry, block_height, consensus_params, txfee)) {
        return CAmount{0};
    }
    return txfee;
}

CAmount ComputeConvergenceFeeFromUndo(
    const CTransaction& tx,
    const CTxUndo& undo,
    int block_height,
    const Consensus::Params& consensus_params)
{
    if (undo.vprevout.size() != tx.vin.size()) return CAmount{0};
    CCoinsView backed;
    CCoinsViewCache cache(&backed);
    for (size_t j = 0; j < tx.vin.size(); ++j) {
        const Coin& src = undo.vprevout[j];
        if (src.IsSpent()) return CAmount{0};
        cache.AddCoin(tx.vin[j].prevout, Coin(CTxOut(src.out), src.nHeight, src.IsCoinBase()), false);
    }
    return ComputeConvergenceFee(tx, cache, block_height, consensus_params);
}

namespace {
std::optional<uint256> WinnerOf(const ConvergenceDrawSet& set, const Consensus::Params& consensus_params)
{
    const ConvergenceDrawOutcome outcome = ResolveConvergenceDraw(set, consensus_params.RES_PROBABILITY_TARGET);
    if (!outcome.HasWinner()) {
        return std::nullopt;
    }
    return set.candidates[static_cast<size_t>(outcome.winner_index)].txid;
}
} // namespace

std::optional<uint256> GetBlockWinner(const CBlock& block, int block_height, const CCoinsViewCache& view_at_block_entry, const Consensus::Params& consensus_params, const ConvergenceWindowTail& tail)
{
    if (block_height < 0) {
        return std::nullopt;
    }
    return WinnerOf(BuildConvergenceDrawSet(block, block_height, view_at_block_entry, consensus_params, tail), consensus_params);
}

std::optional<uint256> GetBlockWinnerFromBlockUndo(const CBlock& block, int block_height, const CBlockUndo& block_undo, const Consensus::Params& consensus_params, const ConvergenceWindowTail& tail)
{
    if (block_height < 0) {
        return std::nullopt;
    }
    ConvergenceDrawSet set;
    if (!BuildConvergenceDrawSetFromUndo(block, block_height, block_undo, consensus_params, tail, set)) {
        return std::nullopt;
    }
    return WinnerOf(set, consensus_params);
}

// RFC-000 §6 Convergence Root Finality: C = Hash256(concat(sort(txid))).
// Byte-wise lexicographical sort; input is ONLY txid; no Merkle tree, no MAST,
// no weight trees, no structured extension.
static uint256 HashConcatenatedSortedTxids(std::vector<uint256> txids)
{
    std::sort(txids.begin(), txids.end());
    HashWriter hw;
    for (const uint256& id : txids) {
        hw << id;
    }
    return hw.GetHash();
}

uint256 ComputeConvergenceDrawMerkleRoot(const std::vector<uint256>& winner_txids)
{
    return HashConcatenatedSortedTxids(winner_txids);
}

uint256 ConvergenceDrawEligibleRoot(const ConvergenceDrawSet& set, const ConvergenceDrawOutcome& outcome)
{
    std::vector<uint256> eligible_txids;
    for (size_t i = 0; i < set.candidates.size(); ++i) {
        if (i < outcome.eligible.size() && outcome.eligible[i]) {
            eligible_txids.push_back(set.candidates[i].txid);
        }
    }
    return HashConcatenatedSortedTxids(std::move(eligible_txids));
}

namespace {
uint256 EligibleRootOf(const ConvergenceDrawSet& set, const Consensus::Params& consensus_params)
{
    return ConvergenceDrawEligibleRoot(set, ResolveConvergenceDraw(set, consensus_params.RES_PROBABILITY_TARGET));
}
} // namespace

uint256 ComputeConvergenceDrawCandidateRoot(const CBlock& block, int block_height, const CCoinsViewCache& view_at_block_entry, const Consensus::Params& consensus_params, const ConvergenceWindowTail& tail)
{
    return EligibleRootOf(BuildConvergenceDrawSet(block, block_height, view_at_block_entry, consensus_params, tail), consensus_params);
}

uint256 ComputeConvergenceDrawCandidateRootFromBlockUndo(const CBlock& block, int block_height, const CBlockUndo& block_undo, const Consensus::Params& consensus_params, const ConvergenceWindowTail& tail)
{
    ConvergenceDrawSet set;
    if (!BuildConvergenceDrawSetFromUndo(block, block_height, block_undo, consensus_params, tail, set)) {
        return uint256();
    }
    return EligibleRootOf(set, consensus_params);
}

bool BuildConvergenceSelectionContextView(const CBlock& block,
    CCoinsViewCache& view_at_block_entry,
    const CBlockIndex* pindex,
    const Consensus::Params& consensus_params,
    const ConvergenceWindowTail& tail,
    Consensus::SelectionContextView& out)
{
    out = Consensus::SelectionContextView{};
    if (pindex == nullptr) {
        return false;
    }

    const ConvergenceDrawSet set = BuildConvergenceDrawSet(block, pindex->nHeight, view_at_block_entry, consensus_params, tail);
    const ConvergenceDrawOutcome sel = ResolveConvergenceDraw(set, consensus_params.RES_PROBABILITY_TARGET);

    std::vector<uint256> candidate_txids;
    candidate_txids.reserve(set.candidates.size());
    std::vector<uint64_t> weights;
    weights.reserve(set.candidates.size());
    for (const ConvergenceDrawCandidate& candidate : set.candidates) {
        candidate_txids.push_back(candidate.txid);
        weights.push_back(candidate.weight);
    }

    out.convergence_root = set.convergence_root;
    out.sigmas = sel.sigmas;
    out.weights = std::move(weights);
    out.has_winner = sel.winner_index >= 0;
    out.winner_index = static_cast<int32_t>(sel.winner_index);
    if (out.has_winner) {
        out.winner_txid = candidate_txids[static_cast<size_t>(sel.winner_index)];
        out.winner_weight = out.weights[static_cast<size_t>(sel.winner_index)];
    }
    return true;
}
