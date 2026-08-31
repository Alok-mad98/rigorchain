// Copyright (c) 2026 The RigorChain developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_CONSENSUS_STATE_SNAPSHOT_H
#define BITCOIN_CONSENSUS_STATE_SNAPSHOT_H

#include <coins.h>
#include <uint256.h>

class CBlockIndex;

namespace Consensus {

/**
 * Deterministic Chainstate Snapshot Hash (STEP-0101)
 *
 * Returns a single 32-byte digest that uniquely identifies the validated
 * chainstate of this node at `pindex`, for the explicit purpose of
 * **replay verification across nodes** (i.e., two honest nodes that have
 * applied the same blocks MUST produce the same hash).
 *
 * Design (RIGOR Constitution: RULE-1 determinism, RULE-2 replay equivalence,
 * RULE-3 no hidden order dependence, RULE-4 minimal consensus surface):
 *
 *   state_hash = SHA256d(
 *       "RIGOR/STATE_HASH/v3"        // 19 bytes ASCII domain tag
 *    || LE32(pindex->nHeight)        //  4 bytes
 *    || pindex->GetBlockHash()       // 32 bytes (matches view.GetBestBlock())
 *    || utxo_muhash                  // 32 bytes (MuHash3072 over the UTXO set)
 *    || pindex_consensus_projection  // fixed 105 bytes (see below)
 *    || utxo_aggregate_projection    // fixed  36 bytes (see below)
 *   )
 *
 * The UTXO set contribution is a MuHash3072 over the canonical
 * `COutPoint || Coin::Serialize` byte string of every unspent coin. MuHash
 * is intrinsically order-independent (algebraically equivalent to hashing a
 * multiset sorted by the same canonical leaf bytes), so the digest cannot
 * depend on `unordered_map` iteration order, hash bucket order, pointer
 * addresses, allocation order, or thread scheduling.
 *
 * Items from the STEP-0101 MUST-INCLUDE list are covered as follows:
 *   - current height           : LE32 in the digest preamble
 *   - best block hash          : 32 bytes in the digest preamble
 *   - UTXO set                 : MuHash3072 leaf set (authoritative)
 *   - RES balances             : LE64 sum of `nValue` over UTXOs matching
 *                                `IsResScript(scriptPubKey)` (same predicate
 *                                as `ReplaySnapshot` / `ChainstateSummary`)
 *   - Selection state          : `CBlockIndex` fields written in fixed order:
 *                                ObservationalMetrics cumulative (LE64),
 *                                `m_draw_candidate_root` (32),
 *                                `has_res` (1),
 *                                `last_res_mint_block_hash` (32),
 *                                then per-block / chain mint + ORU supply
 *                                caches (four LE64 `CAmount` values)
 *   - Mint state               : the same tail fields as selection above
 *                                (`m_block_res_mint_sum`,
 *                                ObservationalMetrics RES mint total,
 *                                `m_block_converge_sum`, ObservationalMetrics S_ORU)
 *                                plus explicit RES supply / classifier counts
 *                                in the 36-byte aggregate trailer
 *
 * The 36-byte aggregate trailer (after the 105-byte index projection) is:
 *   LE64(utxo_count) || LE64(res_balance) || LE64(other_balance)
 *   || LE32(mint_count) || LE32(lock_count) || LE32(converge_count)
 * where counters are accumulated in a single cursor pass together with the
 * MuHash inserts, using the same script-shape predicates as
 * `CaptureChainstateSummary` / `CaptureReplaySnapshot` (no raw map iteration).
 *
 * Stability guarantees:
 *   - Canonical serialization: uses `COutPoint::Serialize` and
 *     `Coin::Serialize` (VARINT height/coinbase + TxOutCompression),
 *     pinned at SER_DISK / version 0 for leaves; `HashWriter` + `serialize.h`
 *     for all numeric / `uint256` trailer fields (fixed width, LE).
 *   - Stable cross-platform: SHA256d and MuHash3072 are byte-exact across
 *     every supported compiler/OS combination already exercised by the
 *     Bitcoin Core test surface.
 *   - Byte-identical output: 32 bytes.
 *
 * Versioning:
 *   The ASCII domain tag embeds the format version ("v3"). Any future
 *   change to leaf layout, trailer layout, or preamble MUST bump the tag.
 *
 * Forbidden in this hash path (REQUIRED-2 INDIRECT/DIRECT consensus impact):
 *   - Raw unordered_map / unordered_set traversal without an order-independent
 *     multiset commitment (here: MuHash3072 over canonical leaves).
 *   - Materializing the full UTXO set into RAM (operational DoS).
 *   - Coin serialization via any encoder other than `Coin::Serialize` for UTXO
 *     leaves (the numeric trailer MUST use `HashWriter` / `serialize.h`).
 *
 * Pre/postconditions:
 *   - Caller MUST hold cs_main (enforced at the Chainstate wrapper).
 *   - `view.GetBestBlock()` MUST equal `pindex->GetBlockHash()` when
 *     `pindex != nullptr`; this is asserted (a skew would silently emit
 *     a mislabeled snapshot, the exact replay hazard this exists to prevent).
 *   - `pindex == nullptr` is permitted and yields a defined hash with the
 *     v2 preamble plus an all-zero 105-byte index projection and all-zero
 *     aggregate trailer (pre-genesis test harnesses).
 *
 * @param view   The active coins view (UTXO set).
 * @param pindex The block index at which the snapshot is being taken, or
 *               nullptr for the pre-genesis state.
 * @return Deterministic 32-byte state hash.
 */
uint256 ComputeDeterministicStateHash(const CCoinsViewCache& view, const CBlockIndex* pindex);

} // namespace Consensus

#endif // BITCOIN_CONSENSUS_STATE_SNAPSHOT_H
