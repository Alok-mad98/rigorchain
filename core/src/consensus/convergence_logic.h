// =============================================================================
// CONVERGENCE CONSENSUS — MINIMAL CORE (integer β, nBits target, no supply coupling)
// =============================================================================

#ifndef BITCOIN_CONSENSUS_CONVERGENCE_LOGIC_H
#define BITCOIN_CONSENSUS_CONVERGENCE_LOGIC_H

#include <consensus/amount.h>
#include <consensus/state_diff.h>
#include <primitives/block.h>
#include <primitives/transaction.h>
#include <serialize.h>
#include <uint256.h>

#include <cstring>
#include <cstdint>
#include <optional>
#include <vector>

class CCoinsViewCache;
class CBlockIndex;
class CBlockUndo;
class CTxUndo;

namespace Consensus {
struct Params;
}

/** Selection digest σ = Hash256(txid ‖ convergence_root ‖ B_{h-1} ‖ LE32(height)). */
using SelectionDigestHash = uint256;

/**
 * Per-tx contribution to the NON-CONSENSUS per-block C(t) ledger (`m_block_converge_sum`, and
 * its prefix sum `CBlockIndex::GetChainConvergeSum`). No ConnectBlock predicate reads either:
 * the retired coinbase CVGE cumulative commitment and its `bad-convergence-cumulative` reject
 * are gone. It must still count only ORU that is actually destroyed, because the supply
 * accounting and `state_hash` subtract it — see `Consensus::GetConvergenceAmount` on why every
 * convergence output currently qualifies, and what would stop being true.
 */
uint64_t GetTransactionConvergenceAmount(const CTransaction& tx);

/**
 * @brief σ for one candidate. `prev_block_hash` is B_{h-1}; `block_height` is the payout height h.
 */
SelectionDigestHash SelectionDigest(const uint256& txid, const uint256& prev_block_hash, int block_height, const uint256& convergence_root);

// =============================================================================
// SECURITY-001-A8 (Sigma-PoW-01) — the draw runs one block behind.
//
// The set F drawn from, and the root C over it, come from block h-1; the seed is
// that block's hash; the payout happens at height h. Every σ input is therefore
// committed by a header that already cost a proof-of-work solution, which is what
// makes re-rolling the draw expensive. See consensus_rigor/sigma_hash.h for the
// full rationale.
//
// A block thus plays two independent roles:
//   - it commits its OWN convergence set in its coinbase (the set future block
//     h+1 will draw from), and
//   - it pays the winner of the set committed by its parent.
// =============================================================================

/** One member of a block's convergence set. */
struct ConvergenceDrawCandidate {
    uint256 txid;
    /** Draw weight f_i — this candidate's RIGOR_MARKER_CONVERGE output nValue in sats. */
    uint64_t weight{0};
    /**
     * The 20-byte HASH160 of the witness v1 RES program this convergence declared.
     * A mint naming this candidate must pay RIGOR_MARKER_RES_MINT with this pubkey hash.
     */
    uint160 res_payout_hash160;
    /**
     * A-9-C: height of the block that introduced this candidate. Not a σ input and not
     * committed anywhere; carried for window bookkeeping and RPC introspection only.
     */
    int source_height{-1};

    SERIALIZE_METHODS(ConvergenceDrawCandidate, obj)
    {
        READWRITE(obj.txid);
        READWRITE(obj.weight);
        READWRITE(obj.res_payout_hash160);
        READWRITE(VARINT_MODE(obj.source_height, VarIntMode::NONNEGATIVE_SIGNED));
    }
};

/**
 * A-9-C: the buried part of a draw window — everything the window covers EXCEPT the source block
 * itself. For a source block at height s the window is [s - CONVERGENCE_DRAW_WINDOW + 1, s], so
 * the tail spans [s - W + 1, s - 1], clamped at genesis. Equivalently, and this is how callers
 * build it: the W-1 blocks ending at the source block's parent, inclusive.
 *
 * Assembled by the chain layer (validation.cpp), which owns block and undo reads; keeping it a
 * plain value here leaves consensus/ free of any disk or chainstate dependency and lets tests
 * drive arbitrary windows directly.
 */
struct ConvergenceWindowTail {
    /** Candidates from the ancestors in window order (oldest first). Order is irrelevant to
     *  the roots, which sort lexicographically, but is kept stable for reproducibility. */
    std::vector<ConvergenceDrawCandidate> candidates;
    /**
     * False when the chain layer could not read the whole window. A partial window would
     * produce a different F, hence a different C and a different winner, so callers MUST treat
     * an incomplete tail as "draw unavailable" rather than falling back to a shorter window.
     * The sole exception is the genesis-side clamp, which is complete by definition.
     *
     * Double-spend of a covenant ticket is ordinary UTXO consumption. Paid tickets leave F
     * because `FinalizeWindowedDrawSet` drops txids named by a mint in the source block, and
     * the chain layer drops buried paid candidates before they enter `candidates`.
     */
    bool complete{false};

    /** The tail genesis legitimately has: nothing buried behind it yet. */
    static ConvergenceWindowTail Empty()
    {
        ConvergenceWindowTail tail;
        tail.complete = true;
        return tail;
    }
};

/**
 * The convergence set committed by one block — everything the draw resolved at the
 * next height needs. Built once when that block is connected and reused thereafter,
 * so no consumer ever re-derives F from a different source.
 */
struct ConvergenceDrawSet {
    /** B_{h-1}: hash of the block that committed this set; the σ seed. */
    uint256 source_block_hash;
    /** Height of that block (h-1). -1 marks an unset value. */
    int source_height{-1};
    /** That block's compact target, i.e. the `target_prev` a mint at h must report. */
    uint32_t source_nbits{0};
    /** C over `candidates`, identical to that block's coinbase commitment. */
    uint256 convergence_root;
    /** F, in `ListConvergenceRootCommitmentTxids` enumeration order. */
    std::vector<ConvergenceDrawCandidate> candidates;

    bool IsSet() const { return source_height >= 0; }

    SERIALIZE_METHODS(ConvergenceDrawSet, obj)
    {
        READWRITE(obj.source_block_hash);
        READWRITE(VARINT_MODE(obj.source_height, VarIntMode::NONNEGATIVE_SIGNED));
        READWRITE(obj.source_nbits);
        READWRITE(obj.convergence_root);
        READWRITE(obj.candidates);
    }
};

/** Result of resolving a draw set at a payout height. */
struct ConvergenceDrawOutcome {
    /** σ per candidate, parallel to `ConvergenceDrawSet::candidates`. */
    std::vector<uint256> sigmas;
    /** Whether each candidate satisfied σ_i < T · f_i, parallel to `candidates`. */
    std::vector<bool> eligible;
    /** Index into `candidates`, or -1 when nobody is eligible. */
    int winner_index{-1};

    bool HasWinner() const { return winner_index >= 0; }
};

/**
 * Consensus Kernel draw resolver (pure).
 *
 *   F     = set.candidates
 *   σ     = computed inside from (txid, C, B_{h-1}, h) — not a caller argument
 *   T     = res_probability_target
 *
 * Does not read chain, UTXO, DB, or globals. Window assembly stays in
 * GetConvergenceDrawSetForBlock (validation.cpp).
 *
 * The outcome is fully determined by `set`. The payout height is always
 * `source_height + 1` and the seed is always `source_block_hash`, so neither is a
 * parameter — there is no way for a caller to resolve a set against the wrong
 * context.
 */
ConvergenceDrawOutcome ResolveConvergenceDraw(const ConvergenceDrawSet& set,
    const uint256& res_probability_target);

/** True iff `coinbase` has exactly one `RIGOR_MARKER_RES_MINT` output. */
bool CoinbaseHasResMint(const CTransaction& coinbase);

/**
 * Convergence txids paid by this block's coinbase mint.
 *
 * Issuance is `RIGOR_MARKER_RES_MINT` only; the paid ticket is the parent-draw winner and is not
 * restated on the coinbase. Callers supply `parent_winner_txid` when a mint is present.
 * A leftover PCORB tag or body witness is not a payment.
 */
std::vector<uint256> FindBlockPaidConvergenceTxids(const CBlock& block,
    const std::optional<uint256>& parent_winner_txid = std::nullopt);

/**
 * Build the set a block commits, from the coins view as of that block's entry, unioned with
 * `tail` (the rest of the window). Used while the block is being connected, and by the block
 * template builder.
 *
 * `coinbase_paid_txid` is the parent-draw winner when this block's coinbase mints; it is
 * dropped from F so a ticket is payable only once.
 *
 * A-9-C: `tail` MUST cover the full window or be marked incomplete; passing
 * `ConvergenceWindowTail::Empty()` for a block that actually has buried ancestors silently
 * reproduces the pre-A9C single-block behaviour, which is the exact defect A-9-C removes.
 */
ConvergenceDrawSet BuildConvergenceDrawSet(const CBlock& block,
    int block_height,
    const CCoinsViewCache& view_at_block_entry,
    const Consensus::Params& consensus_params,
    const ConvergenceWindowTail& tail,
    const std::optional<uint256>& coinbase_paid_txid = {});

/**
 * Same set, reconstructed from an already-connected block plus its undo data —
 * the only way to recover F once the block's inputs have been spent. Uses the
 * same `IsConvergenceDrawMember` overlay walk as `BuildConvergenceDrawSet`.
 * ConnectBlock rejects the block if the two disagree (TASK-010 C-2).
 */
bool BuildConvergenceDrawSetFromUndo(const CBlock& block,
    int block_height,
    const CBlockUndo& block_undo,
    const Consensus::Params& consensus_params,
    const ConvergenceWindowTail& tail,
    ConvergenceDrawSet& out,
    const std::optional<uint256>& coinbase_paid_txid = {});

/**
 * A-9-C: the candidates a single block contributes to a window, with no window context and no
 * root. This is the per-block primitive the chain layer accumulates to assemble a tail; it is
 * deliberately NOT a draw set, because a single block's candidates are never a valid F.
 */
bool BuildBlockConvergenceCandidatesFromUndo(const CBlock& block,
    int block_height,
    const CBlockUndo& block_undo,
    const Consensus::Params& consensus_params,
    std::vector<ConvergenceDrawCandidate>& out);

/**
 * Buried half of the window at `connect_height`, taken from the parent's already-assembled F.
 *
 * F at height H-1 covers [H-W, H-1]; F at H covers [H-W+1, H]. Candidates whose
 * `source_height` is H-W leave the window; the rest are exactly the tail ConnectBlock
 * would have rebuilt from those ancestors' blk/rev. Used so a pruned node can connect
 * a child without rereading a parent window it has already dropped.
 *
 * Incomplete when `parent_set` is unset — callers must not treat that as an empty tail.
 */
ConvergenceWindowTail TailFromParentDrawSet(const ConvergenceDrawSet& parent_set, int connect_height);

/**
 * RFC-010 §1: Execution flow (unique order)
 * Step 6: Select winner - winner = argmin_i (H_i * f_j < H_j * f_i)
 *
 * RFC-010 §0: Principle - Every consensus decision MUST be reproducible step-by-step.
 *
 * RFC-014 Conv-Decouple-01: inequality RHS is `Consensus::Params::RES_PROBABILITY_TARGET`
 * (constitutional constant floor(2^246 / MIN_CONVERGENCE_AMOUNT) / CONVERGENCE_DRAW_WINDOW; RFC-017, C=1024 ORU). Parent `nBits` / `target_prev` is not a σ input
 * (SECURITY-001-A8 / Sigma-PoW-01). Historical `bind_commit` is superseded.
 *
 * RFC-008 §8: Winner selection (no division)
 * 8.1 Order definition: i ≺ j iff H_i * f_j < H_j * f_i
 * 8.2 Implementation: Left = (uint512)H_i * f_j, Right = (uint512)H_j * f_i
 * FORBIDDEN: H_i / f_i, float comparison
 *
 * RFC-008 §5: Sigma (σ) definition (sole randomness source)
 * 5.1 Definition: H_i = Hash256(txid_i || C || B_{h-1} || LE32(h))
 * 5.2 Semantics: H_i ∈ [0, 2^256 - 1]
 * FORBIDDEN: Interpreting H_i as float, Normalizing H_i, Dividing H_i
 *
 * RFC-008 §1: Mathematical domain separation
 * 1.1 Three domains (must be isolated): Sigma Domain (Hash outputs, Randomness, Selection)
 * Mandatory rule: Sigma values MUST NEVER be interpreted as asset values
 *
 * RIGOR IMPLEMENTATION BIBLE §11: TIE-BREAK
 * IF equal: → smaller txid wins
 *
 * RFC-000 §9: TIE-BREAK RULE
 * IF: H_i * f_j == H_j * f_i
 * THEN: txid_i < txid_j → winner
 *
 * Direct memcmp on txids (unsigned byte-wise lexicographical order).
 * Negative iff txid_a is strictly less than txid_b.
 */
inline int ConvergenceWinnerTieBreakCompare(const uint256& txid_a, const uint256& txid_b, const uint256& convergence_root)
{
    // RFC-000 §9: tie-break uses direct txid comparison, NOT XOR with convergence_root
    // convergence_root parameter kept for API compatibility but not used per RFC-000
    return std::memcmp(txid_a.begin(), txid_b.begin(), 32);
}

uint256 ComputeConvergenceHash(const uint256& txid, const uint256& prev_block_hash, int block_height, const uint256& convergence_root);

/** Structural convergence: exactly one RIGOR_MARKER_CONVERGE output with nValue >= 1 ORU (fee not checked). */
bool IsConvergenceCandidate(const CTransaction& tx);

/** Unified convergence fee computation. Returns 0 when the tx is not an F member
 *  (wrong shape, no owner, or inputs fail). A member may itself have fee == 0. */
CAmount ComputeConvergenceFee(
    const CTransaction& tx,
    const CCoinsViewCache& view_at_block_entry,
    int block_height,
    const Consensus::Params& consensus_params);

/** Same convergence fee computation using tx undo prevouts (RPC/reorg/read paths). */
CAmount ComputeConvergenceFeeFromUndo(
    const CTransaction& tx,
    const CTxUndo& undo,
    int block_height,
    const Consensus::Params& consensus_params);

/**
 * RFC-010 §1: Execution Flow (Unique Order)
 * Step 1: Classify transactions in block h-1 — convergence-shaped, ≥ MIN_CONVERGENCE_AMOUNT,
 *         carrying an A-5 owner declaration, with valid inputs. Fee is relay policy.
 * Step 2: Construct set F from those transactions.
 * Step 3: Compute C = ConvergenceRoot(F); empty F is the zero root, otherwise Hash256(sorted txids).
 *         Block h-1's coinbase commits this window C.
 * Step 4: For each tx_i in F: σ_i = Hash256(txid_i || C || B_{h-1} || LE32(h)), f_i = convergence amount.
 * Step 5: Filter - Eligible if σ_i < T · f_i.
 * Step 6: Select winner = argmin_i (σ_i * f_j < σ_j * f_i), txid tie-break.
 * Step 7: The block at height h must carry exactly one RES mint naming that winner, paying the
 *         owner the winning convergence declared.
 * Step 8: State update - Mint RES, Update supply, Consume ORU.
 *
 * RFC-010 §0: Principle - Every consensus decision MUST be reproducible step-by-step.
 * RFC-010 §3: Verifiability - Given trace: Any node MUST recompute and match: C, σ_i, eligibility, winner.
 * RFC-010 §4: Fork Detection - If ANY field differs: → Consensus mismatch → Block MUST be rejected.
 *
 * Steps 1-6 are exactly `ResolveConvergenceDraw(BuildConvergenceDrawSet(block_{h-1}))`.
 *
 * Winner: requires |F| >= CONVERGENCE_DRAW_MIN_TXS_FOR_RES (RFC-015); among inequality-eligible,
 * argmin σ. Returns none when |F| is below the floor or nothing is eligible.
 */

/**
 * Winner awarded by this block's convergence set — i.e. the winner that the block at
 * `block_height + 1` must pay. Determined entirely by the block itself, since the σ seed
 * is the block's own hash.
 */
std::optional<uint256> GetBlockWinner(const CBlock& block, int block_height, const CCoinsViewCache& view_at_block_entry, const Consensus::Params& consensus_params, const ConvergenceWindowTail& tail);

/** Same, for an already-connected block, reconstructing F from its undo data. */
std::optional<uint256> GetBlockWinnerFromBlockUndo(const CBlock& block, int block_height, const CBlockUndo& block_undo, const Consensus::Params& consensus_params, const ConvergenceWindowTail& tail);

/**
 * Flat commitment over the inequality-eligible subset of a resolved draw. The block that
 * PAYS the draw carries this in its coinbase, so the value is fully fixed by the parent
 * and the producer has nothing to choose.
 */
uint256 ConvergenceDrawEligibleRoot(const ConvergenceDrawSet& set, const ConvergenceDrawOutcome& outcome);

/** Same commitment for the set a block commits, resolving the draw on the way. */
uint256 ComputeConvergenceDrawCandidateRoot(const CBlock& block, int block_height, const CCoinsViewCache& view_at_block_entry, const Consensus::Params& consensus_params, const ConvergenceWindowTail& tail);

uint256 ComputeConvergenceDrawCandidateRootFromBlockUndo(const CBlock& block, int block_height, const CBlockUndo& block_undo, const Consensus::Params& consensus_params, const ConvergenceWindowTail& tail);

uint256 ComputeConvergenceDrawMerkleRoot(const std::vector<uint256>& winner_txids);

/**
 * Fill `out` with the draw this block awards: **F** = `ListConvergenceRootCommitmentTxids`,
 * weights = `GetConvergenceAmount`, σ seeded by the block's own hash at payout height
 * `block_height + 1`, winner = uint512 cross-multiply + txid tie-break.
 */
bool BuildConvergenceSelectionContextView(const CBlock& block,
    CCoinsViewCache& view_at_block_entry,
    const CBlockIndex* pindex,
    const Consensus::Params& consensus_params,
    const ConvergenceWindowTail& tail,
    Consensus::SelectionContextView& out);

#endif
