/* Bitcoin-compatible SetCompact implementation for cpuminer
 * Based on Bitcoin Core's arith_uint256::SetCompact
 */

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

/* 256-bit unsigned integer (8 x uint32_t) */
typedef struct {
    uint32_t data[8];
} arith_uint256;

/* Left shift by n bits (Bitcoin-compatible: data[0] is least significant) */
static void arith_uint256_lshift(arith_uint256 *a, unsigned int shift)
{
    arith_uint256 b;
    int i;
    unsigned int k = shift / 32;
    unsigned int d = shift % 32;

    memcpy(&b, a, sizeof(b));
    memset(a, 0, sizeof(*a));

    for (i = 0; i < 8; i++) {
        if (i + k + 1 < 8 && d != 0)
            a->data[i + k + 1] |= (b.data[i] >> (32 - d));
        if (i + k < 8)
            a->data[i + k] |= (b.data[i] << d);
    }
}

/* Set from uint32_t */
static void arith_uint256_set_u32(arith_uint256 *a, uint32_t val)
{
    memset(a, 0, sizeof(*a));
    a->data[0] = val;
}

/* SetCompact: Convert nBits (compact representation) to 256-bit target */
void arith_uint256_set_compact(arith_uint256 *target, uint32_t nCompact, 
                                bool *pfNegative, bool *pfOverflow)
{
    int nSize = nCompact >> 24;
    uint32_t nWord = nCompact & 0x007fffff;

    if (nSize <= 3) {
        nWord >>= 8 * (3 - nSize);
        arith_uint256_set_u32(target, nWord);
    } else {
        arith_uint256_set_u32(target, nWord);
        arith_uint256_lshift(target, 8 * (nSize - 3));
    }

    if (pfNegative)
        *pfNegative = nWord != 0 && (nCompact & 0x00800000) != 0;
    if (pfOverflow)
        *pfOverflow = nWord != 0 && ((nSize > 34) ||
                                     (nWord > 0xff && nSize > 33) ||
                                     (nWord > 0xffff && nSize > 32));
}

/* Compare two arith_uint256: return -1 if a < b, 0 if a == b, 1 if a > b */
int arith_uint256_compare(const arith_uint256 *a, const arith_uint256 *b)
{
    int i;
    for (i = 7; i >= 0; i--) {
        if (a->data[i] < b->data[i])
            return -1;
        if (a->data[i] > b->data[i])
            return 1;
    }
    return 0;
}

/* Convert arith_uint256 to hex string (64 chars + null) - big-endian display */
void arith_uint256_to_hex(const arith_uint256 *a, char *out)
{
    int i, j;
    const char *hex = "0123456789abcdef";
    /* data[0] is least significant, data[7] is most significant
     * For hex display, we want most significant byte first
     * Each uint32_t needs to be output in big-endian byte order
     */
    for (i = 7; i >= 0; i--) {
        for (j = 3; j >= 0; j--) {
            uint8_t byte = (a->data[i] >> (j * 8)) & 0xff;
            out[(7 - i) * 8 + (3 - j) * 2 + 0] = hex[byte >> 4];
            out[(7 - i) * 8 + (3 - j) * 2 + 1] = hex[byte & 0xf];
        }
    }
    out[64] = '\0';
}
