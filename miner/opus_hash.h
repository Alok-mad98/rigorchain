#ifndef OPUS_HASH_H
#define OPUS_HASH_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void opus_hash(const uint8_t *input80, uint8_t *output32);

/** Same PoW as the node on a precomputed 32-byte header id (LE uint256 memory order). */
void opus_hash_from_base_hash(const uint8_t *base_hash32, uint8_t *output32);

#ifdef __cplusplus
}
#endif

#endif /* OPUS_HASH_H */
