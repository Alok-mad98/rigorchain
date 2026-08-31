// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_RIGOR_KERNEL_POW_DIGEST_H
#define BITCOIN_RIGOR_KERNEL_POW_DIGEST_H

#include <uint256.h>

#include <cstddef>

class CBlockHeader;

namespace rigor_kernel {

/** ASCII "RGRPOW01" (SPEC-008 §3.1). */
static constexpr char POW_DOMAIN_TAG[] = {'R', 'G', 'R', 'P', 'O', 'W', '0', '1'};
static constexpr size_t POW_DOMAIN_TAG_LEN = 8;

/** Serialized block header length. */
static constexpr size_t POW_HEADER_LEN = 80;

/** Domain tag + header. Below the 136-byte SHA3-256 rate, hence one permutation. */
static constexpr size_t POW_PREIMAGE_LEN = POW_DOMAIN_TAG_LEN + POW_HEADER_LEN;
static_assert(POW_PREIMAGE_LEN == 88, "SPEC-008 preimage is 88 bytes");

/**
 * Serialize a header into the 88-byte SPEC-008 preimage.
 *
 * Exposed for tests, which pin it against the canonical CDataStream
 * serialization so this fast path cannot drift from it.
 */
void BuildPowPreimage(const CBlockHeader& header, unsigned char out[POW_PREIMAGE_LEN]) noexcept;

/**
 * RIGOR proof-of-work digest: SHA3-256("RGRPOW01" || header_80).
 *
 * Pure: no chain context, no locks, no logging, no heap allocation. One
 * Keccak-f[1600] permutation.
 *
 * The returned uint256 holds the digest bytes in output order, so
 * UintToArith256 reads it little-endian and reproduces the model's
 * uint256_of_bytes. Comparing the result against a target is therefore the
 * same operation on both sides; see formal_verification/coq/PowDigest.v.
 */
uint256 GetRigorPowHash(const CBlockHeader& header) noexcept;

} // namespace rigor_kernel

#endif // BITCOIN_RIGOR_KERNEL_POW_DIGEST_H
