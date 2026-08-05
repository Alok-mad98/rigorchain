// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <consensus_rigor/sigma_hash.h>

#include <hash.h>
#include <span.h>
#include <uint256.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace rigor {

namespace {

// Offsets are normative (RFC-000 §5). Changing any of these is a hard fork.
inline constexpr std::size_t kOffTxid   = 0;
inline constexpr std::size_t kOffCRoot  = 32;
inline constexpr std::size_t kOffBind   = 64;
inline constexpr std::size_t kOffHeight = 96;
inline constexpr std::size_t kHeightLen = 4;

static_assert(kOffTxid == 0,            "Sigma layout: txid must start at offset 0");
static_assert(kOffCRoot == 32,          "Sigma layout: ConvergenceRoot at offset 32");
static_assert(kOffBind == 64,           "Sigma layout: B_{h-1} at offset 64");
static_assert(kOffHeight == 96,         "Sigma layout: height at offset 96");
static_assert(kOffHeight + kHeightLen == SIGMA_INPUT_SIZE,
              "Sigma total input size must be exactly 100 bytes (RFC-000 §5)");

} // namespace

uint256 ComputeSigmaHash(const uint256& txid,
                         const uint256& convergence_root,
                         const uint256& prev_block_hash,
                         uint32_t       height)
{
    // RFC-008 §1: Mathematical domain definition (Domain Separation)
    // 1.1 The three domains (MUST stay isolated): Sigma Domain (Hash outputs, Randomness, Selection)
    // Mandatory rule: Sigma values MUST NEVER be interpreted as asset values
    //
    // RFC-009 §4 (Sigma-PoW-01 revision): Sigma input serialization (most critical)
    // The only format (100 bytes)
    // offset | size | field
    // -----------------------------
    // 0      | 32   | txid      (from F, the convergence set of block h-1)
    // 32     | 32   | C         (ConvergenceRoot over that same set)
    // 64     | 32   | B_{h-1}   (parent block hash, verbatim)
    // 96     | 4    | LE32(height)
    // Mandatory constraint: Total size MUST be EXACTLY 100 bytes
    //
    // RIGOR IMPLEMENTATION BIBLE §4 (post-PoW-01): HASH INPUT (100-byte lock)
    // HARD CHECK: len(input) != 100 → REJECT (bad-hash-length)
    //
    // SECURITY-001-A8: all three 32-byte inputs are committed by block h-1 —
    // txid and C by its merkle root, B_{h-1} by the header itself. Producing a
    // different σ assignment therefore costs one proof-of-work solution rather
    // than one hash. See sigma_hash.h for the full rationale and for why
    // target_prev is no longer a separate input.

    // RFC-008 §2: Basic data types (Canonical Types)
    // 2.1 Definition: bytes32 := exactly 32 bytes (no interpretation)
    // Forbidden: No floating point, No signed integer, No implicit casting, No big-endian reinterpretation
    //
    // RIGOR IMPLEMENTATION BIBLE §2: BYTE CONCAT
    // concat(a, b, c) = a || b || c
    // ❌ FORBIDDEN: delimiter, length prefix, padding
    std::array<unsigned char, SIGMA_INPUT_SIZE> buf{};
    std::memcpy(buf.data() + kOffTxid,  txid.begin(),             32);
    std::memcpy(buf.data() + kOffCRoot, convergence_root.begin(), 32);
    std::memcpy(buf.data() + kOffBind,  prev_block_hash.begin(),  32);

    // RIGOR IMPLEMENTATION BIBLE §1: DATA TYPES
    // uint32 → 4 bytes → little-endian (RFC-000 §4 Deterministic Encoding Law).
    buf[kOffHeight + 0] = static_cast<unsigned char>((height >> 0)  & 0xFF);
    buf[kOffHeight + 1] = static_cast<unsigned char>((height >> 8)  & 0xFF);
    buf[kOffHeight + 2] = static_cast<unsigned char>((height >> 16) & 0xFF);
    buf[kOffHeight + 3] = static_cast<unsigned char>((height >> 24) & 0xFF);

    // Step 3: final Hash256 over the 100-byte canonical buffer.
    //
    // RFC-008 §4: Hash function definition (Hash256)
    // 4.1 Definition: Hash256(x) = SHA256(SHA256(x))
    // ❗FORBIDDEN: Single SHA256 ❌, Keccak ❌, Blake ❌, Truncated hash ❌
    //
    // RFC-009 §0: Absolute principle
    // All consensus-critical data MUST have exactly ONE valid byte representation.
    uint256 out;
    CHash256().Write(buf).Finalize(out);
    return out;
}

} // namespace rigor
