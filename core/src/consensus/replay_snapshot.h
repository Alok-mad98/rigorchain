// Copyright (c) 2026 The RigorChain developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_CONSENSUS_REPLAY_SNAPSHOT_H
#define BITCOIN_CONSENSUS_REPLAY_SNAPSHOT_H

#include <coins.h>
#include <consensus/amount.h>
#include <primitives/transaction.h>
#include <uint256.h>

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

class CBlockIndex;

namespace Consensus {

/**
 * ReplaySnapshot - diagnostic record for divergence triage.
 *
 * STRICTLY DIAGNOSTIC. The authoritative replay artifact is `state_hash`
 * (produced by `Consensus::ComputeDeterministicStateHash`). All other
 * fields are *derived projections* of chainstate intended for human-readable
 * triage when two nodes report different `state_hash` values.
 *
 * Fields mirror the STEP-0101 v2 preimage (RES totals, classifier counts,
 * index projection) so operators can diff a serialized snapshot without
 * recomputing the digest locally.
 *
 * Wire format version:
 *   - The serialized form is prefixed with a 4-byte magic ("RPL2", LE).
 *   - Deserialization REFUSES any record whose magic is not "RPL2".
 *   - The earlier "RPL1"-style format produced by the legacy implementation
 *     is intentionally incompatible to prevent silent acceptance of records
 *     whose semantic meaning has changed in this rewrite.
 */
struct ReplaySnapshot {
    //! Canonical magic. ASCII "RPL2" little-endian = 0x32 0x4C 0x50 0x52.
    static constexpr uint32_t kWireMagic = 0x324C5052;

    //! Block height of the snapshotted tip (-1 if no tip).
    int32_t height;

    //! Hash of the snapshotted tip block.
    uint256 block_hash;

    //! Authoritative deterministic state hash for this snapshot. See
    //! Consensus::ComputeDeterministicStateHash().
    uint256 state_hash;

    //! Equal to block_hash; kept for backwards-compatible API ergonomics.
    uint256 best_block_hash;

    //! Total number of unspent outputs in the UTXO set at the snapshot.
    uint64_t utxo_count;

    //! Diagnostic-only counters. These are derived projections, NOT inputs to
    //! state_hash. Renaming and reinterpreting the legacy names:
    //!   mint_count     = number of UTXOs whose scriptPubKey matches
    //!                    `IsResScript()` (i.e., RES asset outputs). Every
    //!                    RES UTXO traces back to a mint event by construction.
    //!   lock_count     = number of UTXOs whose scriptPubKey matches the
    //!                    witness-V1 lock-program shape (`OP_1 <32-byte push>`
    //!                    of exactly 34 bytes). RFC-003 §5. Note that this
    //!                    matches ALL witness-V1 outputs by design, so the
    //!                    count is informational only.
    //!   converge_count = number of UTXOs matching `IsConvergenceScript()`.
    //!                    By chain invariant (AddCoins drops convergence
    //!                    outputs) this MUST be 0; a non-zero value is logged
    //!                    as a chainstate-integrity warning.
    uint32_t mint_count;
    uint32_t lock_count;
    uint32_t converge_count;

    //! Sum of `nValue` over all RES UTXOs. Also committed inside the v2
    //! `state_hash` preimage (LE64 trailer); kept here for human triage.
    uint64_t res_balance;

    //! Sum of `nValue` over all non-RES UTXOs (predominantly ORU). Same
    //! diagnostic status as `res_balance`.
    uint64_t other_balance;

    ReplaySnapshot()
        : height(-1),
          utxo_count(0),
          mint_count(0),
          lock_count(0),
          converge_count(0),
          res_balance(0),
          other_balance(0) {}

    //! Canonical serialization to a byte string. Prefixed with `kWireMagic`.
    //! Layout is fixed-width, little-endian, no padding, no unaligned reads.
    std::string Serialize() const;

    //! Canonical deserialization. Returns true iff the input is exactly a
    //! well-formed `RPL2` record (correct magic, exact byte length). On
    //! failure, *this is left in an undefined intermediate state; callers
    //! must discard the snapshot.
    bool Deserialize(const std::string& data);
};

/**
 * Capture a ReplaySnapshot from chainstate.
 *
 * Streaming, O(1) memory. Walks the coin cursor exactly once and accumulates
 * the diagnostic counters incrementally; never materializes the UTXO set.
 *
 * @param view        Active coins view (UTXO set).
 * @param pindex      Tip block index, or nullptr for pre-genesis.
 * @param state_hash  Pre-computed deterministic state hash for this tip.
 *                    Recorded verbatim into the snapshot; not recomputed.
 */
ReplaySnapshot CaptureReplaySnapshot(const CCoinsViewCache& view,
                                     const CBlockIndex* pindex,
                                     const uint256& state_hash);

/**
 * Compare two ReplaySnapshots and return a human-readable description of
 * the first detected mismatch, or "Snapshots are identical" if none.
 *
 * Comparison order is from most consensus-bearing to least:
 *   1. height                  - chain alignment
 *   2. block_hash              - tip identity
 *   3. state_hash              - authoritative replay artifact
 *   4. best_block_hash         - view/tip skew
 *   5. utxo_count              - UTXO cardinality
 *   6. res_balance / other_*   - asset-domain divergence
 *   7. mint/lock/converge      - shape-class divergence
 */
std::string CompareReplaySnapshots(const ReplaySnapshot& snapshot_a,
                                   const ReplaySnapshot& snapshot_b);

} // namespace Consensus

#endif // BITCOIN_CONSENSUS_REPLAY_SNAPSHOT_H
