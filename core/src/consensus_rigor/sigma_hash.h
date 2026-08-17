// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// RFC-001 (Sigma Hash) — canonical 100-byte input construction.
// Normative source: RFC-000 §4.2 (Sigma nomenclature) and §5 (Hash Domain
// Separation). This header is consensus-critical and must NEVER be altered
// without a hard fork.

#ifndef BITCOIN_CONSENSUS_RIGOR_SIGMA_HASH_H
#define BITCOIN_CONSENSUS_RIGOR_SIGMA_HASH_H

#include <uint256.h>

#include <cstddef>
#include <cstdint>

namespace rigor {

/**
 * RFC-000 §4 (Sigma-PoW-01 revision): CANONICAL HASHING
 * Hash256(x) = SHA256(SHA256(x))
 * σ_i input (fully locked): [32] txid_i + [32] ConvergenceRoot (C) + [32] B_{h-1} + [4] height (LE)
 * TOTAL = 100 bytes EXACT
 * len(input) MUST == 100, otherwise → REJECT (bad-hash-length)
 *
 * The slot at offset 64 is the **parent block hash** B_{h-1}, taken verbatim
 * from the `hashPrevBlock` field of the header being validated.
 */
inline constexpr std::size_t SIGMA_INPUT_SIZE = 100;

/**
 * RFC-000 §4 (Sigma-PoW-01 revision): CANONICAL HASHING
 * Sigma (σ_i) — Deterministic randomness projection.
 *
 *     σ_i = Hash256( txid_i || ConvergenceRoot || B_{h-1} || LE32(height) )
 *
 * Input layout, in order, with NO padding and NO separators:
 *
 *     offset  size  field
 *     ------  ----  ------------------------------------------------------
 *     0       32    txid_i            (32-byte transaction id, drawn from F)
 *     32      32    ConvergenceRoot   (C over F, RFC-000 §6)
 *     64      32    B_{h-1}           (parent block hash == header hashPrevBlock)
 *     96      4     height            (uint32, little-endian, RFC-000 §3)
 *
 * Total: 100 bytes, verified by `static_assert` in sigma_hash.cpp.
 * Coq counterpart: `formal_verification/coq/CanonicalDrawEncoder.v`
 * (offsets, little-endian height, SHA-256d). The two are connected by golden
 * vectors, not by a refinement proof.
 *
 * Hash function: Hash256 = SHA-256(SHA-256(·)) — Bitcoin canonical double SHA.
 * Byte agreement with Coq RigorSha256 is gated differentially
 * (`formal_verification/tests/test_hash_kernel_tripartite.py`), not by refinement.
 *
 * SECURITY-001-A8 (Sigma-PoW-01) — why the slot at offset 64 is a raw parent
 * block hash and why both `txid_i` and `C` are drawn from block h-1:
 *
 *   σ decides who mints RES. Before A-8 every σ input was either fixed and
 *   known at assembly time (the old `bind_commit` over B_{h-10} and
 *   target_prev) or freely chosen by the block producer (txid_i and C, both
 *   taken from the block being assembled). Nothing cost anything to vary, so a
 *   producer could re-roll the draw offline: regenerate its own convergence
 *   transaction, recompute σ, keep only a variant that satisfies the
 *   inequality, and include just that one. At the constitutional target the
 *   per-try probability is 2^-16, so a guaranteed win cost about 13 ms of CPU
 *   and one minimum convergence — the whole RES issuance stream was free to
 *   anyone producing blocks.
 *
 *   Adding more *fixed* entropy cannot fix this; a value the producer already
 *   knows is not an obstacle. The variable inputs are what must become
 *   expensive. So the draw resolved at height h now runs over the convergence
 *   set committed by block h-1, seeded by that same block's hash:
 *
 *     - C and every candidate txid are committed by block h-1's merkle root,
 *       hence by its header. Choosing a different set is a different header,
 *       i.e. a fresh proof-of-work attempt.
 *     - B_{h-1} is the output of that proof-of-work and cannot be predicted
 *       while the set is still being chosen.
 *
 *   Therefore every distinct σ assignment requires a distinct valid block
 *   h-1 — one full PoW solution. The residual is that whoever mines h-1 may
 *   discard a block whose outcome it dislikes, at the cost of a full block
 *   reward, which is the same residual any proof-of-work lottery carries.
 *
 *   `target_prev` is deliberately absent: B_{h-1} is a hash of the parent
 *   header, which contains nBits, so the parent's compact target is committed
 *   more strongly than the old explicit fold achieved. The cross-difficulty
 *   replay domain that RFC-008-CR-01 opened is closed by construction.
 *
 * CONSENSUS RULES (RFC-000 §4, post-PoW-01):
 *   - Input MUST be exactly 100 bytes.
 *   - Field order MUST NOT change.
 *   - No additional fields may be appended.
 *   - The height field MUST be serialized little-endian and MUST be the height
 *     of the block that pays the mint (h), not the drawn block (h-1).
 *   - `prev_block_hash` MUST be the parent block hash verbatim. Folding it
 *     through any further hash, or substituting an ancestor at any other
 *     depth, is a consensus violation.
 *   - All other fields are 32-byte uint256 values in their native
 *     on-the-wire byte order.
 */
uint256 ComputeSigmaHash(const uint256& txid,
                         const uint256& convergence_root,
                         const uint256& prev_block_hash,
                         uint32_t       height);

} // namespace rigor

#endif // BITCOIN_CONSENSUS_RIGOR_SIGMA_HASH_H
