// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_RIGOR_KERNEL_KECCAK_H
#define BITCOIN_RIGOR_KERNEL_KECCAK_H

#include <cstddef>
#include <cstdint>

namespace rigor_kernel {

/** Rate of SHA3-256 in bytes (1600 - 2*256 bits). */
static constexpr size_t SHA3_256_RATE = 136;

/** Longest message this single-block construction accepts. */
static constexpr size_t SHA3_256_MAX_SHORT_MSG = SHA3_256_RATE - 2;

/**
 * Keccak-f[1600] (FIPS PUB 202), 24 rounds, in place.
 *
 * State is 25 little-endian 64-bit lanes indexed i = x + 5y.
 * Exposed separately from SHA3-256 so it can be tested and reasoned about on
 * its own; this mirrors the Coq split between RigorKeccak.v and PowDigest.v.
 */
void KeccakF1600(uint64_t state[25]) noexcept;

/**
 * SHA3-256 of a message of at most SHA3_256_MAX_SHORT_MSG bytes.
 *
 * Absorbs exactly one rate block and squeezes without a second permutation,
 * so this is one KeccakF1600 call and no allocation. Longer messages are
 * rejected by assertion rather than silently handled: RIGOR's only caller
 * hashes 88 fixed bytes (SPEC-008 §3), and a streaming sponge would add
 * consensus surface that nothing needs.
 *
 * Domain suffix is 0x06 (SHA3, FIPS 202), not 0x01 (the pre-standard Keccak
 * used by Ethereum). The two produce entirely different digests.
 */
void Sha3_256Short(const unsigned char* msg, size_t len, unsigned char out[32]) noexcept;

} // namespace rigor_kernel

#endif // BITCOIN_RIGOR_KERNEL_KECCAK_H
