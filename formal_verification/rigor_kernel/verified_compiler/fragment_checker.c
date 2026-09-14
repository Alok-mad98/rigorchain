/* Hand-written C99 subset rehearsal of the comparable kernel fragment.
 *
 * Stock Coq has no native C backend (no Extraction Language C).
 * This file is NOT CertiCoq output, NOT a CompCert refinement of the
 * Coq validators, and NOT binary semantic equivalence.
 * Arithmetic is signed long long, not unbounded Coq Z.
 * See release/VerifiedCompilerPath.md.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_MONEY 1000000000000000000LL
#define RES_REWARD_GENESIS 40000000LL
#define RES_REWARD_EPOCH 210000LL
#define RES_REWARD_LAST_EPOCH 250LL
#define ASERT_RADIX 65536LL

static long long isqrt_ll(long long n)
{
    long long x, y;
    if (n <= 0)
        return 0;
    if (n < 2)
        return n;
    x = n;
    y = (x + 1) / 2;
    while (y < x) {
        x = y;
        y = (x + n / x) / 2;
    }
    return x;
}

static long long res_W(long long height)
{
    long long k, e, scale;
    if (height < 0)
        return 0;
    k = height / RES_REWARD_EPOCH;
    if (k < 0 || k > RES_REWARD_LAST_EPOCH)
        return 0;
    e = k + 1;
    /* (e^3 << 32) fits these rehearsal heights (0 and epoch 0). */
    scale = isqrt_ll((e * e * e) << 32);
    if (scale == 0)
        return 0;
    return (RES_REWARD_GENESIS << 16) / scale;
}

static int money_range(long long v)
{
    return (0 <= v) && (v <= MAX_MONEY);
}

static int check_utxo_value(long long value)
{
    return money_range(value);
}

static int check_oru(long long oi, long long oo, long long ri, long long ro, int dom_oru)
{
    if (oi < 0 || oo < 0 || ri < 0 || ro < 0)
        return 0;
    if (oo > oi)
        return 0;
    if (dom_oru)
        return (ri == 0) && (ro == 0);
    return (oi > 0) && (ro <= ri);
}

static int check_res(long long mint, long long height, int coinbase, int mint_ok, int draw_ok)
{
    if (!coinbase || !mint_ok || !draw_ok)
        return 0;
    if (mint <= 0)
        return 0;
    if (mint > res_W(height))
        return 0;
    return 1;
}

static long long compact_encode(long long t)
{
    long long raw_size, m, s, tmp, bit;
    if (t <= 0)
        return 0;
    raw_size = 0;
    tmp = t;
    while (tmp > 0) {
        tmp = tmp / 256;
        raw_size = raw_size + 1;
    }
    /* (Z.log2 t + 8) / 8 == byte length for t > 0 */
    if (raw_size <= 3)
        m = t << (8 * (3 - raw_size));
    else
        m = t >> (8 * (raw_size - 3));
    s = raw_size;
    bit = (m >> 23) & 1;
    if (bit) {
        m = m >> 8;
        s = s + 1;
    }
    return s * 16777216LL + m;
}

static long long compact_decode(long long nbits)
{
    long long size, word;
    size = nbits / 16777216LL;
    word = nbits % 8388608LL;
    if (size <= 3)
        return word >> (8 * (3 - size));
    return word << (8 * (size - 3));
}

static int check_pow(long long digest, long long target, long long compact_t)
{
    if (target <= 0)
        return 0;
    if (digest > target)
        return 0;
    if (compact_t < 0)
        return 0;
    if (compact_decode(compact_encode(compact_t)) > compact_t)
        return 0;
    return 1;
}

/* C99 signed division is toward zero — same as Coq Z.quot when the
 * product te * ASERT_RADIX stays inside signed long long. Overflow of
 * that product is undefined / wrap; Coq Z does not overflow.
 * Live document: scripts/verified_compiler_path.sh asert_llong_overflow. */
static int check_asert(long long te, long long tau, long long expected_q)
{
    long long got;
    if (te >= 0)
        return 0;
    if (tau <= 0)
        return 0;
    got = (te * ASERT_RADIX) / tau;
    return got == expected_q;
}

static void print_verdict(int ok)
{
    if (ok)
        puts("VALID");
    else
        puts("INVALID");
}

int main(int argc, char **argv)
{
    if (argc < 3) {
        fprintf(stderr, "usage: fragment-checker <domain> <args...>\n");
        return 2;
    }
    if (strcmp(argv[1], "utxo") == 0) {
        if (argc != 3)
            return 2;
        print_verdict(check_utxo_value(strtoll(argv[2], 0, 10)));
        return 0;
    }
    if (strcmp(argv[1], "oru") == 0) {
        if (argc != 7)
            return 2;
        print_verdict(check_oru(strtoll(argv[2], 0, 10),
                                strtoll(argv[3], 0, 10),
                                strtoll(argv[4], 0, 10),
                                strtoll(argv[5], 0, 10),
                                atoi(argv[6])));
        return 0;
    }
    if (strcmp(argv[1], "res") == 0) {
        if (argc != 7)
            return 2;
        print_verdict(check_res(strtoll(argv[2], 0, 10),
                                strtoll(argv[3], 0, 10),
                                atoi(argv[4]),
                                atoi(argv[5]),
                                atoi(argv[6])));
        return 0;
    }
    if (strcmp(argv[1], "pow") == 0) {
        if (argc != 5)
            return 2;
        print_verdict(check_pow(strtoll(argv[2], 0, 10),
                                strtoll(argv[3], 0, 10),
                                strtoll(argv[4], 0, 10)));
        return 0;
    }
    if (strcmp(argv[1], "asert") == 0) {
        if (argc != 5)
            return 2;
        print_verdict(check_asert(strtoll(argv[2], 0, 10),
                                  strtoll(argv[3], 0, 10),
                                  strtoll(argv[4], 0, 10)));
        return 0;
    }
    fprintf(stderr, "unknown domain\n");
    return 2;
}
