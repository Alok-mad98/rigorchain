#ifndef RGRPOW_H
#define RGRPOW_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** ASCII "RGRPOW01" — must match core/src/rigor_kernel/pow_digest.h. */
#define RGRPOW_DOMAIN_TAG "RGRPOW01"
#define RGRPOW_DOMAIN_TAG_LEN 8
#define RGRPOW_HEADER_LEN 80
#define RGRPOW_PREIMAGE_LEN 88

/**
 * Production PoW digest: SHA3-256("RGRPOW01" || header_80).
 *
 * header_80 is the canonical 80-byte header (LE version/time/bits/nonce,
 * 32-byte prev/merkle in internal byte order). Digest bytes are SHA3-256
 * output order (byte 0 is the least-significant limb vs target).
 */
void rgrpow_hash80(const uint8_t header80[RGRPOW_HEADER_LEN], uint8_t out32[32]);

/** SHA3-256 (FIPS 202, domain 0x06) of a message ≤ 134 bytes. */
void rgrpow_sha3_256_short(const uint8_t *msg, unsigned int len, uint8_t out32[32]);

#ifdef __cplusplus
}
#endif

#endif /* RGRPOW_H */
