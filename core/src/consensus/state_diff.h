// Copyright (c) 2026 The RigorChain developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_CONSENSUS_STATE_DIFF_H
#define BITCOIN_CONSENSUS_STATE_DIFF_H

#include <coins.h>
#include <uint256.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

class CBlockIndex;

namespace Consensus {

/**
 * STEP-0102: Deterministic Chainstate Diff Tool
 *
 * Strictly diagnostic: this module does NOT modify consensus. It produces a
 * deterministic byte-for-byte summary of a node's chainstate plus the most
 * recent selection context, and provides an ordered first-divergence
 * comparator over two such summaries.
 *
 * Authoritative replay artifact remains `state_hash` from
 * `Consensus::ComputeDeterministicStateHash` (STEP-0101). This module
 * delegates state_hash computation to that function verbatim and never
 * reimplements it; any divergence between two nodes' `state_hash` values is
 * authoritative evidence of replay disagreement.
 *
 * In addition to `state_hash`, the summary carries:
 *   - 256-bucket UTXO MuHash array (bucketed by `outpoint.hash.begin()[0]`,
 *     i.e., the most-significant byte of the txid as serialized). This
 *     allows the diff to localize a UTXO-set divergence to 1/256 of the
 *     set when `state_hash` differs but the coarse counters/balances agree.
 *   - Diagnostic counters (utxo_count, mint/lock/converge counts, RES /
 *     other-asset balances).
 *   - Optional SelectionContextView projection (covers RFC-000 §4/§6/§9
 *     SelectionProof / Sigma / ConvergenceRoot fields).
 *
 * Wire format:
 *   - Magic "CSM1" (0x314D5343 little-endian, ASCII).
 *   - Fixed-width little-endian fields; constant record size; no
 *     variable-length payloads.
 *   - `Deserialize` refuses any record whose magic is not "CSM1" or whose
 *     size is not exactly `kSerializedRecordSize`.
 */

/**
 * Optional caller-provided view of the rigor selection context at the tip.
 *
 * This module deliberately does not read blocks from disk: doing so would
 * couple a consensus-namespace header to the `node::BlockManager` /
 * `validation` machinery, expanding the diagnostic tool's blast radius. The
 * caller (RPC handler, test harness) is responsible for reading the tip
 * block, reconstructing the block-entry coins view, and invoking
 * `BuildConvergenceSelectionContextView` (same draw semantics as `ConnectBlock`).
 *
 * Two nodes that have validated the same chain MUST produce identical
 * `SelectionContextView` values for the same tip; any divergence here is
 * authoritative evidence of selection-pipeline disagreement.
 *
 * Field semantics:
 *   - `has_winner`              Whether a winner was selected for the tip.
 *   - `winner_index`            Index into `sigmas` / `weights` in
 *                               `ListConvergenceRootCommitmentTxids` (**F**) order, -1 if `!has_winner`.
 *   - `convergence_root`        RFC-000 §6 root (zero if no selection).
 *   - `winner_txid`             Empty (zero) if `!has_winner`.
 *   - `winner_weight`           0 if `!has_winner`.
 *   - `sigmas` / `weights`      Per-candidate in **F** order. Sizes must match; checked by Capture.
 */
struct SelectionContextView {
    bool has_winner{false};
    int32_t winner_index{-1};
    uint256 convergence_root{};
    uint256 winner_txid{};
    uint64_t winner_weight{0};
    std::vector<uint256> sigmas;
    std::vector<uint64_t> weights;
};

struct ChainstateSummary {
    //! Canonical magic: ASCII "CSM1" little-endian = 0x31 0x4D 0x53 0x43.
    static constexpr uint32_t kWireMagic = 0x314D5343;

    //! UTXO bucket count. Fixed at 256 so the bucket index can be addressed
    //! by a single byte of the txid and the wire size is constant.
    static constexpr size_t kBucketCount = 256;

    // ----- Identity (must match exactly across nodes) ---------------------
    int32_t height{-1};
    uint256 block_hash{};
    //! Authoritative replay digest from STEP-0101.
    uint256 state_hash{};

    // ----- Aggregate diagnostic counters / balances ----------------------
    uint64_t utxo_count{0};
    uint64_t res_balance{0};
    uint64_t other_balance{0};
    uint32_t mint_count{0};      // RES-asset UTXO count (every RES UTXO ⇒ a mint)
    uint32_t lock_count{0};      // V1-shape UTXO count (RFC-003 §5 lock-program form)
    uint32_t converge_count{0};  // Must be 0; non-zero is chainstate-integrity bug

    // ----- UTXO 256-bucket MuHash (drilldown when state_hash differs) ----
    std::array<uint256, kBucketCount> utxo_bucket_hashes{};

    // ----- Selection-context fields (optional) ---------------------------
    bool selection_present{false};
    bool selection_has_winner{false};
    int32_t selection_winner_index{-1};
    uint32_t selection_candidate_count{0};
    uint64_t selection_winner_weight{0};
    uint256 selection_convergence_root{};
    uint256 selection_winner_txid{};
    //! SHA256d over canonical concatenation of `sigmas` in **F** order.
    uint256 selection_sigmas_hash{};
    //! SHA256d over canonical concatenation of LE64(`weights[i]`) in **F** order.
    uint256 selection_weights_hash{};

    //! Canonical serialization to byte string. Always exactly
    //! `kSerializedRecordSize` bytes; prefixed with `kWireMagic`.
    std::string Serialize() const;

    //! Canonical deserialization. Returns true iff input is well-formed
    //! (correct magic, correct length, no trailing bytes).
    bool Deserialize(const std::string& data);
};

/**
 * Single canonical first-divergence description.
 *
 * Comparison order is fixed and documented in `DiffChainstateSummaries`.
 * When the divergence is in `utxo_bucket_hashes`, `bucket_index` is set to
 * the first mismatching bucket (0..255) so the caller can drill further
 * into the UTXO subtree by re-bucketing inside that range.
 *
 * `value_a` / `value_b` are hex / decimal strings sized for human reading;
 * they are NOT a stable wire format and must not be parsed by automation.
 * For machine-readable diffs, use the raw `ChainstateSummary` fields.
 */
struct DiffResult {
    bool identical{true};
    std::string field;
    std::string value_a;
    std::string value_b;
    //! -1 unless `field == "utxo_bucket_hashes"`.
    int32_t bucket_index{-1};
    /**
     * Start offset in `ChainstateSummary::Serialize()` for the logical field
     * reported in `field`, or the first differing byte offset for
     * `DiffSerializedChainstateSummaries`. -1 if unknown.
     */
    int64_t serialized_byte_offset{-1};
    //! Human-readable CSM1 region (e.g. `state_hash`, `utxo_bucket[3]`).
    std::string wire_segment;

    //! Render a one-line human-readable description.
    std::string ToString() const;
};

/**
 * Capture a ChainstateSummary from chainstate.
 *
 * Two cursor passes, both O(1) memory:
 *   Pass 1: delegates to `Consensus::ComputeDeterministicStateHash` for the
 *           authoritative `state_hash`. By delegating we guarantee that the
 *           summary's `state_hash` is bit-identical to the one exposed via
 *           RPC `getdeterministicstatehash`, eliminating drift risk.
 *   Pass 2: walks the cursor again to build the 256-bucket MuHash array,
 *           the counters, and the balances. Inserts each canonical leaf
 *           (`COutPoint::Serialize || Coin::Serialize`) into the bucket
 *           addressed by `outpoint.hash.begin()[0]`.
 *
 * Pre/postconditions:
 *   - Caller MUST hold cs_main (the underlying cursor is read under the
 *     chainstate lock).
 *   - `view.GetBestBlock()` MUST equal `pindex->GetBlockHash()` when
 *     `pindex != nullptr`; otherwise the captured summary would be
 *     mislabeled. This is asserted via the call into
 *     `ComputeDeterministicStateHash`.
 *   - `selection` is optional. When nullptr, `selection_present == false`
 *     and selection_* fields remain at default values.
 *   - When `selection != nullptr`, `selection->sigmas.size()` MUST equal
 *     `selection->weights.size()`, and `selection->winner_index` MUST be
 *     -1 or a valid index. Violations are asserted.
 */
ChainstateSummary CaptureChainstateSummary(const CCoinsViewCache& view,
                                           const CBlockIndex* pindex,
                                           const SelectionContextView* selection);

/**
 * Deterministic first-divergence diff. Canonical field-comparison order:
 *
 *   1.  height
 *   2.  block_hash
 *   3.  state_hash
 *   4.  utxo_count
 *   5.  res_balance
 *   6.  other_balance
 *   7.  mint_count
 *   8.  lock_count
 *   9.  converge_count
 *   10. utxo_bucket_hashes[0..255]   ← drilldown when state_hash differs
 *                                      but coarse counters agree
 *   11. selection_present
 *   12. selection_convergence_root
 *   13. selection_has_winner
 *   14. selection_winner_index
 *   15. selection_winner_txid
 *   16. selection_winner_weight
 *   17. selection_candidate_count
 *   18. selection_sigmas_hash
 *   19. selection_weights_hash
 *
 * Returns a `DiffResult` with `identical == true` iff every field matches.
 * Otherwise `field`, `value_a`, `value_b`, and (if applicable) `bucket_index`
 * describe the first mismatch in the order above.
 */
DiffResult DiffChainstateSummaries(const ChainstateSummary& a,
                                   const ChainstateSummary& b);

/**
 * Byte-wise compare two `ChainstateSummary::Serialize()` payloads (CSM1).
 *
 * Deterministic: scans increasing byte offsets (lexicographic order on the
 * wire). The first differing byte wins; if one buffer is a prefix of the
 * other, reports `wire_length` at offset `min(size_a, size_b)`.
 *
 * On mismatch, `serialized_byte_offset` is set to the offset, `wire_segment`
 * names the CSM1 region containing that offset, and `value_a` / `value_b` are
 * single-byte hex encodings (or decimal lengths for `wire_length`).
 */
DiffResult DiffSerializedChainstateSummaries(std::string_view wire_a,
                                             std::string_view wire_b);

} // namespace Consensus

#endif // BITCOIN_CONSENSUS_STATE_DIFF_H
