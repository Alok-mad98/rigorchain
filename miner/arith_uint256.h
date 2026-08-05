/* Bitcoin-compatible SetCompact implementation for cpuminer
 * Based on Bitcoin Core's arith_uint256::SetCompact
 */

#ifndef ARITH_UINT256_H
#define ARITH_UINT256_H

#include <stdint.h>
#include <stdbool.h>

/* 256-bit unsigned integer (8 x uint32_t) */
typedef struct {
    uint32_t data[8];
} arith_uint256;

/* SetCompact: Convert nBits (compact representation) to 256-bit target */
void arith_uint256_set_compact(arith_uint256 *target, uint32_t nCompact, 
                                bool *pfNegative, bool *pfOverflow);

/* Compare two arith_uint256: return -1 if a < b, 0 if a == b, 1 if a > b */
int arith_uint256_compare(const arith_uint256 *a, const arith_uint256 *b);

/* Convert arith_uint256 to hex string (64 chars + null) */
void arith_uint256_to_hex(const arith_uint256 *a, char *out);

#endif /* ARITH_UINT256_H */
