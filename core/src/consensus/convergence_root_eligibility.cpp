// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

// Pure ConvergenceRoot from a txid list. Isolated from coins/script so the
// test-only portable CLI can compile this translation unit without bitcoind.

#include <consensus/convergence_root_eligibility.h>

#include <hash.h>
#include <span.h>

#include <algorithm>
#include <vector>

namespace Consensus {

uint256 ComputeConvergenceRootFromEligibilityTxids(std::vector<uint256> convergence_txids) noexcept
{
    /*
     * RFC-008 §1: Mathematical domain definition (Domain Separation)
     * 1.1 The three domains (MUST stay isolated): Asset Domain (ORU, RES, Fees / Mint Cost (f_i))
     * Mandatory rule: Asset values MUST NEVER be used as hash input except explicitly defined
     *
     * RFC-008 §2: Basic data types (Canonical Types)
     * 2.1 Definition: bytes32 := exactly 32 bytes (no interpretation)
     *
     * RFC-009 §3: Convergence Root（C）
     * 3.1 Input: txids: array of bytes32
     * 3.2 Sort rule (the only one): Sort lexicographically by raw byte comparison
     * 3.3 Construction: buf = txid_1 || txid_2 || ... || txid_n, C = Hash256(buf)
     * 3.4 Empty set: if n == 0: C = 32 zero bytes
     * ❌ FORBIDDEN: Merkle tree, pair hashing, length prefix
     *
     * RFC-008 §10: Edge Cases
     * 10.2 Empty set: If F is empty: C = 32 zero bytes, No winner
     *
     * RFC-008 §3: Concatenation Rule
     * 3.1 The only legal concatenation: A || B := byte-wise concatenation
     * 3.2 SIGMA input (MUST match exactly): Input Buffer (100 bytes EXACTLY)
     *
     * RIGOR IMPLEMENTATION BIBLE §6: ROOT (Convergence Root)
     * C = Hash256(concat(sorted(txids)))
     *
     * RFC-000 §6: CONVERGENCE ROOT (convergence root lock)
     * C = Hash256(concat(sort(txid_i)))
     *
     * Normative ConvergenceRoot (flat commitment; **not** a Merkle tree):
     *
     *   ConvergenceRoot = Hash256( concat( sorted_txid_bytes ) )
     *
     *   - Each element is a **txid** as `uint256`: **32 bytes**, fixed width, in **native memory
     *     order** as stored in `uint256` (little-endian on all supported platforms — same bytes
     *     as `tx.GetHash()` / `uint256` object representation). **MUST NOT** apply endian swaps
     *     or `SerializeHash(tx)` / any serialization of the whole transaction.
     *   - **Sort key:** `std::sort` on `std::vector<uint256>` (unsigned byte-wise lexicographical order
     *     per RFC-000 §6 - memcmp byte order of those 32-byte representations). **MUST NOT** sort by
     *     block index, feerate, or convergence amount.
     *   - **Concat order:** ascending sorted order only (after sort).
     *   - Empty F (n == 0): return **all-zero** `uint256()` **before** any hashing (RFC-008 §10.2).
     *     A singleton hashes like any larger F: C = Hash256(txid). The old n < 2 zero
     *     boundary was a leftover of |F|≥2 coordination; RFC-015 already draws at |F|≥1.
     *
     * **Explicitly forbidden** (consensus fork if introduced):
     *   - Merkle tree over **F**
     *   - Per-leaf `Hash256(txid)` before concatenation
     *   - Hashing anything other than raw 32-byte txid bytes in sorted order
     *   - Block-internal vtx order as input to the commitment
     *
     * RIGOR IMPLEMENTATION BIBLE §6: SORT RULE (critical)
     * Sort using unsigned byte comparison
     * ❌ FORBIDDEN: string compare, locale compare, signed char compare
     *
     * RFC-009 §9: Prohibited practices
     * Using std::sort default (platform dependent) ❌
     * CRITICAL: std::sort on uint256 uses memcmp byte order - this is REQUIRED per RFC-000 §6.
     * Historical Failure Example: Ethereum's Geth vs Parity used different sorting
     * algorithms for transaction ordering, leading to different block templates.
     */
    // RFC-008 §10.2 / RFC-009 §3.4: empty F commits to 32 zero bytes. A singleton is hashed.
    if (convergence_txids.empty()) {
        return uint256();
    }
    // RFC-009 §3.2: Sort rule (the only one)- Sort lexicographically by raw byte comparison
    // RIGOR IMPLEMENTATION BIBLE §6: unsigned byte comparison sort
    std::sort(convergence_txids.begin(), convergence_txids.end());
    // RFC-008 §3: Concatenation Rule- A || B := byte-wise concatenation
    // RFC-009 §3.3: Construction - buf = txid_1 || txid_2 || ... || txid_n, C = Hash256(buf)
    // RIGOR IMPLEMENTATION BIBLE §2: BYTE CONCAT (no delimiters, length prefixes, padding)
    CHash256 hasher;
    for (const uint256& id : convergence_txids) {
        hasher.Write(MakeUCharSpan(id));
    }
    // RFC-008 §4: Hash function definition (Hash256)- Hash256(x) = SHA256(SHA256(x))
    // RIGOR IMPLEMENTATION BIBLE §3: Hash256 only
    uint256 root;
    hasher.Finalize(root);
    return root;
}

} // namespace Consensus
