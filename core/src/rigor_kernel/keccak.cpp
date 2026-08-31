// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <rigor_kernel/keccak.h>

#include <crypto/common.h>

#include <cassert>
#include <cstring>

namespace rigor_kernel {

namespace {

constexpr uint64_t ROUND_CONSTANTS[24] = {
    0x0000000000000001ULL, 0x0000000000008082ULL, 0x800000000000808AULL, 0x8000000080008000ULL,
    0x000000000000808BULL, 0x0000000080000001ULL, 0x8000000080008081ULL, 0x8000000000008009ULL,
    0x000000000000008AULL, 0x0000000000000088ULL, 0x0000000080008009ULL, 0x000000008000000AULL,
    0x000000008000808BULL, 0x800000000000008BULL, 0x8000000000008089ULL, 0x8000000000008003ULL,
    0x8000000000008002ULL, 0x8000000000000080ULL, 0x000000000000800AULL, 0x800000008000000AULL,
    0x8000000080008081ULL, 0x8000000000008080ULL, 0x0000000080000001ULL, 0x8000000080008008ULL};

/** rho rotation offsets, in the order the rho/pi lane walk visits them. */
constexpr int ROTATION[24] = {1, 3, 6, 10, 15, 21, 28, 36, 45, 55, 2, 14,
                              27, 41, 56, 8, 25, 43, 62, 18, 39, 61, 20, 44};

/** pi lane permutation, same walk order as ROTATION. */
constexpr int PI_LANE[24] = {10, 7, 11, 17, 18, 3, 5, 16, 8, 21, 24, 4,
                             15, 23, 19, 13, 12, 2, 20, 14, 22, 9, 6, 1};

inline uint64_t Rotl64(uint64_t x, int n) noexcept
{
    return (x << n) | (x >> (64 - n));
}

} // namespace

void KeccakF1600(uint64_t state[25]) noexcept
{
    uint64_t bc[5];

    for (int round = 0; round < 24; ++round) {
        // theta
        for (int i = 0; i < 5; ++i) {
            bc[i] = state[i] ^ state[i + 5] ^ state[i + 10] ^ state[i + 15] ^ state[i + 20];
        }
        for (int i = 0; i < 5; ++i) {
            const uint64_t t = bc[(i + 4) % 5] ^ Rotl64(bc[(i + 1) % 5], 1);
            for (int j = 0; j < 25; j += 5) state[j + i] ^= t;
        }

        // rho + pi
        uint64_t last = state[1];
        for (int i = 0; i < 24; ++i) {
            const int j = PI_LANE[i];
            const uint64_t tmp = state[j];
            state[j] = Rotl64(last, ROTATION[i]);
            last = tmp;
        }

        // chi
        for (int j = 0; j < 25; j += 5) {
            for (int i = 0; i < 5; ++i) bc[i] = state[j + i];
            for (int i = 0; i < 5; ++i) {
                state[j + i] ^= (~bc[(i + 1) % 5]) & bc[(i + 2) % 5];
            }
        }

        // iota
        state[0] ^= ROUND_CONSTANTS[round];
    }
}

void Sha3_256Short(const unsigned char* msg, size_t len, unsigned char out[32]) noexcept
{
    // At len == RATE-1 the domain suffix and the final 0x80 would share a byte
    // (the merged 0x86 case). SPEC-008 §3.3 forbids implementing that case, so
    // it is excluded here rather than handled.
    assert(len <= SHA3_256_MAX_SHORT_MSG);

    unsigned char block[SHA3_256_RATE];
    std::memset(block, 0, sizeof(block));
    std::memcpy(block, msg, len);
    block[len] = 0x06;
    block[SHA3_256_RATE - 1] |= 0x80;

    uint64_t state[25];
    std::memset(state, 0, sizeof(state));
    for (size_t i = 0; i < SHA3_256_RATE / 8; ++i) {
        state[i] ^= ReadLE64(block + 8 * i);
    }

    KeccakF1600(state);

    for (size_t i = 0; i < 4; ++i) {
        WriteLE64(out + 8 * i, state[i]);
    }
}

} // namespace rigor_kernel
