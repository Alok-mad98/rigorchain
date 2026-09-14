/*
 * RIGOR SOLO PoW: SHA3-256("RGRPOW01" || header_80).
 *
 * Byte-identical to core/src/rigor_kernel/{keccak,pow_digest}.cpp
 * (SPEC-008). Miner-only copy — do not change the node.
 */

#include "rgrpow.h"

#include <string.h>

#define SHA3_256_RATE 136

static const uint64_t ROUND_CONSTANTS[24] = {
    0x0000000000000001ULL, 0x0000000000008082ULL, 0x800000000000808AULL, 0x8000000080008000ULL,
    0x000000000000808BULL, 0x0000000080000001ULL, 0x8000000080008081ULL, 0x8000000000008009ULL,
    0x000000000000008AULL, 0x0000000000000088ULL, 0x0000000080008009ULL, 0x000000008000000AULL,
    0x000000008000808BULL, 0x800000000000008BULL, 0x8000000000008089ULL, 0x8000000000008003ULL,
    0x8000000000008002ULL, 0x8000000000000080ULL, 0x000000000000800AULL, 0x800000008000000AULL,
    0x8000000080008081ULL, 0x8000000000008080ULL, 0x0000000080000001ULL, 0x8000000080008008ULL
};

static const int ROTATION[24] = {
    1, 3, 6, 10, 15, 21, 28, 36, 45, 55, 2, 14,
    27, 41, 56, 8, 25, 43, 62, 18, 39, 61, 20, 44
};

static const int PI_LANE[24] = {
    10, 7, 11, 17, 18, 3, 5, 16, 8, 21, 24, 4,
    15, 23, 19, 13, 12, 2, 20, 14, 22, 9, 6, 1
};

static uint64_t rotl64(uint64_t x, int n)
{
    return (x << n) | (x >> (64 - n));
}

static uint64_t load_le64(const uint8_t *p)
{
    return (uint64_t)p[0]
         | ((uint64_t)p[1] << 8)
         | ((uint64_t)p[2] << 16)
         | ((uint64_t)p[3] << 24)
         | ((uint64_t)p[4] << 32)
         | ((uint64_t)p[5] << 40)
         | ((uint64_t)p[6] << 48)
         | ((uint64_t)p[7] << 56);
}

static void store_le64(uint8_t *p, uint64_t x)
{
    p[0] = (uint8_t)x;
    p[1] = (uint8_t)(x >> 8);
    p[2] = (uint8_t)(x >> 16);
    p[3] = (uint8_t)(x >> 24);
    p[4] = (uint8_t)(x >> 32);
    p[5] = (uint8_t)(x >> 40);
    p[6] = (uint8_t)(x >> 48);
    p[7] = (uint8_t)(x >> 56);
}

static void keccak_f1600(uint64_t state[25])
{
    uint64_t bc[5];
    int round, i, j;

    for (round = 0; round < 24; ++round) {
        for (i = 0; i < 5; ++i) {
            bc[i] = state[i] ^ state[i + 5] ^ state[i + 10] ^ state[i + 15] ^ state[i + 20];
        }
        for (i = 0; i < 5; ++i) {
            const uint64_t t = bc[(i + 4) % 5] ^ rotl64(bc[(i + 1) % 5], 1);
            for (j = 0; j < 25; j += 5)
                state[j + i] ^= t;
        }

        {
            uint64_t last = state[1];
            for (i = 0; i < 24; ++i) {
                const int lane = PI_LANE[i];
                const uint64_t tmp = state[lane];
                state[lane] = rotl64(last, ROTATION[i]);
                last = tmp;
            }
        }

        for (j = 0; j < 25; j += 5) {
            for (i = 0; i < 5; ++i)
                bc[i] = state[j + i];
            for (i = 0; i < 5; ++i)
                state[j + i] ^= (~bc[(i + 1) % 5]) & bc[(i + 2) % 5];
        }

        state[0] ^= ROUND_CONSTANTS[round];
    }
}

void rgrpow_sha3_256_short(const uint8_t *msg, unsigned int len, uint8_t out32[32])
{
    uint8_t block[SHA3_256_RATE];
    uint64_t state[25];
    unsigned int i;

    /* SPEC-008 §3.3: 88-byte preimage; merged 0x86 padding must not arise. */
    if (len > SHA3_256_RATE - 2) {
        memset(out32, 0xff, 32);
        return;
    }

    memset(block, 0, sizeof(block));
    memcpy(block, msg, len);
    block[len] = 0x06;
    block[SHA3_256_RATE - 1] |= 0x80;

    memset(state, 0, sizeof(state));
    for (i = 0; i < SHA3_256_RATE / 8; ++i)
        state[i] ^= load_le64(block + 8 * i);

    keccak_f1600(state);

    for (i = 0; i < 4; ++i)
        store_le64(out32 + 8 * i, state[i]);
}

void rgrpow_hash80(const uint8_t header80[RGRPOW_HEADER_LEN], uint8_t out32[32])
{
    uint8_t preimage[RGRPOW_PREIMAGE_LEN];

    memcpy(preimage, RGRPOW_DOMAIN_TAG, RGRPOW_DOMAIN_TAG_LEN);
    memcpy(preimage + RGRPOW_DOMAIN_TAG_LEN, header80, RGRPOW_HEADER_LEN);
    rgrpow_sha3_256_short(preimage, RGRPOW_PREIMAGE_LEN, out32);
}
