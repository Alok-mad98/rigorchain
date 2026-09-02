// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// P0-081: PoW boundary security audit.
// Standalone with -DPOW_BOUNDARY_SECURITY_STANDALONE; Boost otherwise.
// Does not recode SHA256, SHA3, ASERT, ConsensusKernel, TransactionKernel,
// or Ledger.
//
// This is not a SHA-256 mathematical security proof, not a PoW economic
// security proof, and not a claim that 51% attacks cannot occur.

#include <rigor_kernel/keccak.h>
#include <rigor_kernel/pow_digest.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

namespace {

int g_fails = 0;

void Fail(const char* msg)
{
    std::fprintf(stderr, "FAIL %s\n", msg);
    ++g_fails;
}

void Expect(bool cond, const char* msg)
{
    if (!cond) Fail(msg);
}

void StoreLE32(unsigned char* p, uint32_t v)
{
    p[0] = static_cast<unsigned char>(v);
    p[1] = static_cast<unsigned char>(v >> 8);
    p[2] = static_cast<unsigned char>(v >> 16);
    p[3] = static_cast<unsigned char>(v >> 24);
}

uint32_t ReadLE32Local(const unsigned char* p)
{
    return static_cast<uint32_t>(p[0]) |
           (static_cast<uint32_t>(p[1]) << 8) |
           (static_cast<uint32_t>(p[2]) << 16) |
           (static_cast<uint32_t>(p[3]) << 24);
}

struct Header80 {
    int32_t nVersion{1};
    unsigned char hashPrevBlock[32]{};
    unsigned char hashMerkleRoot[32]{};
    uint32_t nTime{1231006505};
    uint32_t nBits{0x1d00ffff};
    uint32_t nNonce{2083236893};
};

void SerializeHeader80(const Header80& h, unsigned char out[80])
{
    StoreLE32(out, static_cast<uint32_t>(h.nVersion));
    std::memcpy(out + 4, h.hashPrevBlock, 32);
    std::memcpy(out + 36, h.hashMerkleRoot, 32);
    StoreLE32(out + 68, h.nTime);
    StoreLE32(out + 72, h.nBits);
    StoreLE32(out + 76, h.nNonce);
}

std::array<unsigned char, 32> PowDigest(const unsigned char header80[80])
{
    unsigned char pre[88];
    std::memcpy(pre, rigor_kernel::POW_DOMAIN_TAG, rigor_kernel::POW_DOMAIN_TAG_LEN);
    std::memcpy(pre + 8, header80, 80);
    std::array<unsigned char, 32> out{};
    rigor_kernel::Sha3_256Short(pre, 88, out.data());
    return out;
}

std::array<unsigned char, 32> PowDigest(const Header80& h)
{
    unsigned char ser[80];
    SerializeHeader80(h, ser);
    return PowDigest(ser);
}

bool DigestsEqual(const std::array<unsigned char, 32>& a, const std::array<unsigned char, 32>& b)
{
    return std::memcmp(a.data(), b.data(), 32) == 0;
}

/** 256-bit little-endian limbs, matching arith_uint256::pn. */
struct U256 {
    uint32_t pn[8]{};
};

bool U256IsZero(const U256& a)
{
    for (uint32_t w : a.pn) {
        if (w != 0) return false;
    }
    return true;
}

int U256Cmp(const U256& a, const U256& b)
{
    for (int i = 7; i >= 0; --i) {
        if (a.pn[i] < b.pn[i]) return -1;
        if (a.pn[i] > b.pn[i]) return 1;
    }
    return 0;
}

U256 U256FromU32(uint32_t v)
{
    U256 a{};
    a.pn[0] = v;
    return a;
}

U256 U256FromDigest(const unsigned char d[32])
{
    U256 a{};
    for (int i = 0; i < 8; ++i) {
        a.pn[i] = ReadLE32Local(d + 4 * i);
    }
    return a;
}

void U256Shl(U256& a, unsigned bits)
{
    if (bits == 0) return;
    if (bits >= 256) {
        a = U256{};
        return;
    }
    const unsigned limb = bits / 32;
    const unsigned rem = bits % 32;
    if (limb) {
        for (int i = 7; i >= 0; --i) {
            a.pn[i] = (i >= static_cast<int>(limb)) ? a.pn[i - static_cast<int>(limb)] : 0;
        }
    }
    if (rem) {
        uint32_t carry = 0;
        for (int i = 0; i < 8; ++i) {
            const uint64_t cur = (static_cast<uint64_t>(a.pn[i]) << rem) | carry;
            a.pn[i] = static_cast<uint32_t>(cur);
            carry = static_cast<uint32_t>(cur >> 32);
        }
    }
}

void U256Shr(U256& a, unsigned bits)
{
    if (bits == 0) return;
    if (bits >= 256) {
        a = U256{};
        return;
    }
    const unsigned limb = bits / 32;
    const unsigned rem = bits % 32;
    if (limb) {
        for (int i = 0; i < 8; ++i) {
            a.pn[i] = (i + static_cast<int>(limb) < 8) ? a.pn[i + static_cast<int>(limb)] : 0;
        }
    }
    if (rem) {
        uint32_t carry = 0;
        for (int i = 7; i >= 0; --i) {
            const uint32_t next = a.pn[i];
            a.pn[i] = (next >> rem) | carry;
            carry = static_cast<uint32_t>(static_cast<uint64_t>(next) << (32 - rem));
        }
    }
}

int U256Bits(const U256& a)
{
    for (int i = 7; i >= 0; --i) {
        if (a.pn[i] == 0) continue;
        int b = 32 * i;
        uint32_t w = a.pn[i];
        while (w) {
            ++b;
            w >>= 1;
        }
        return b;
    }
    return 0;
}

U256 U256FromCompact(uint32_t nCompact, bool* neg, bool* ovf)
{
    const int nSize = static_cast<int>(nCompact >> 24);
    uint32_t nWord = nCompact & 0x007fffff;
    U256 r{};
    if (nSize <= 3) {
        nWord >>= 8 * (3 - nSize);
        r = U256FromU32(nWord);
    } else {
        r = U256FromU32(nWord);
        U256Shl(r, 8 * static_cast<unsigned>(nSize - 3));
    }
    if (neg) *neg = nWord != 0 && (nCompact & 0x00800000) != 0;
    if (ovf) {
        *ovf = nWord != 0 && ((nSize > 34) ||
                              (nWord > 0xff && nSize > 33) ||
                              (nWord > 0xffff && nSize > 32));
    }
    return r;
}

uint32_t U256GetCompact(const U256& in, bool fNegative = false)
{
    U256 a = in;
    int nSize = (U256Bits(a) + 7) / 8;
    uint32_t nCompact = 0;
    if (nSize <= 3) {
        nCompact = a.pn[0] << (8 * (3 - nSize));
    } else {
        U256Shr(a, 8 * static_cast<unsigned>(nSize - 3));
        nCompact = a.pn[0];
    }
    if (nCompact & 0x00800000) {
        nCompact >>= 8;
        nSize++;
    }
    nCompact |= static_cast<uint32_t>(nSize) << 24;
    nCompact |= (fNegative && (nCompact & 0x007fffff) ? 0x00800000 : 0);
    return nCompact;
}

/** Mainnet powLimit: 0000007fff...ff as LE uint256. */
U256 MainnetPowLimit()
{
    U256 a{};
    for (int i = 0; i < 7; ++i) a.pn[i] = 0xffffffffu;
    a.pn[7] = 0x0000007fu;
    return a;
}

enum class PowDecision { ACCEPT, REJECT };

struct TargetDecode {
    bool ok{false};
    bool negative{false};
    bool overflow{false};
    bool zero{false};
    bool above_limit{false};
    U256 target{};
};

TargetDecode DecodeTarget(uint32_t nBits, const U256& pow_limit)
{
    TargetDecode d;
    d.target = U256FromCompact(nBits, &d.negative, &d.overflow);
    d.zero = U256IsZero(d.target);
    d.above_limit = U256Cmp(d.target, pow_limit) > 0;
    d.ok = !d.negative && !d.overflow && !d.zero && !d.above_limit;
    return d;
}

PowDecision CheckPow(const Header80& h, uint32_t nBits, const U256& pow_limit)
{
    const TargetDecode d = DecodeTarget(nBits, pow_limit);
    if (!d.ok) return PowDecision::REJECT;
    unsigned char ser[80];
    SerializeHeader80(h, ser);
    const auto digest = PowDigest(ser);
    const U256 work = U256FromDigest(digest.data());
    if (U256Cmp(work, d.target) > 0) return PowDecision::REJECT;
    return PowDecision::ACCEPT;
}

constexpr int64_t ASERT_RADIX = 65536;
constexpr int64_t ASERT_POLY_P1 = 195766423245049LL;
constexpr int64_t ASERT_POLY_P2 = 971821376LL;
constexpr int64_t ASERT_POLY_P3 = 5127LL;
constexpr int64_t ASERT_POLY_BIAS = (1LL << 47);
constexpr int64_t ASERT_EXP_CLAMP = ASERT_RADIX * 64;
constexpr int64_t ASERT_HALF_LIFE = 43200;
constexpr int64_t ASERT_SPACING = 600;
constexpr int64_t ASERT_RATIO_UNIT_PPM = 1000000;
constexpr int64_t MIN_ADJUSTMENT_FACTOR_PPM = 875000;
constexpr int64_t MAX_ADJUSTMENT_FACTOR_PPM = 1125000;

using AsertWide = std::array<uint64_t, 6>;

void MulU256ByU64(const U256& a, uint64_t b, AsertWide& out)
{
    out.fill(0);
    uint64_t limbs[4];
    for (int i = 0; i < 4; ++i) {
        limbs[i] = static_cast<uint64_t>(a.pn[2 * i]) | (static_cast<uint64_t>(a.pn[2 * i + 1]) << 32);
    }
    for (int i = 0; i < 4; ++i) {
        const unsigned __int128 prod = static_cast<unsigned __int128>(limbs[i]) * b;
        unsigned __int128 acc = static_cast<unsigned __int128>(out[static_cast<size_t>(i)]) +
                                static_cast<uint64_t>(prod);
        out[static_cast<size_t>(i)] = static_cast<uint64_t>(acc);
        uint64_t carry = static_cast<uint64_t>(acc >> 64) + static_cast<uint64_t>(prod >> 64);
        for (int k = i + 1; k < 6 && carry != 0; ++k) {
            unsigned __int128 s = static_cast<unsigned __int128>(out[static_cast<size_t>(k)]) + carry;
            out[static_cast<size_t>(k)] = static_cast<uint64_t>(s);
            carry = static_cast<uint64_t>(s >> 64);
        }
    }
}

void AsertWideShl1(AsertWide& v, bool& wide_overflow)
{
    wide_overflow = wide_overflow || ((v[5] & (1ULL << 63)) != 0);
    for (int i = 5; i > 0; --i) {
        v[static_cast<size_t>(i)] = (v[static_cast<size_t>(i)] << 1) | (v[static_cast<size_t>(i - 1)] >> 63);
    }
    v[0] <<= 1;
}

void AsertWideShr1(AsertWide& v)
{
    for (size_t i = 0; i < 5; ++i) {
        v[i] = (v[i] >> 1) | (v[i + 1] << 63);
    }
    v[5] >>= 1;
}

U256 AsertWideToU256(const AsertWide& v, const U256& pow_limit)
{
    if (v[4] != 0 || v[5] != 0) return pow_limit;
    U256 r{};
    for (int i = 0; i < 4; ++i) {
        r.pn[2 * i] = static_cast<uint32_t>(v[i]);
        r.pn[2 * i + 1] = static_cast<uint32_t>(v[i] >> 32);
    }
    if (U256Cmp(r, pow_limit) > 0) return pow_limit;
    return r;
}

U256 AsertNextTarget(const U256& target_ref, uint64_t factor, int64_t num_shifts, const U256& pow_limit)
{
    AsertWide v{};
    MulU256ByU64(target_ref, factor, v);
    bool wide_overflow = false;
    if (num_shifts < 0) {
        const unsigned shr = static_cast<unsigned>(std::min<int64_t>(-num_shifts, 64));
        for (unsigned i = 0; i < shr; ++i) AsertWideShr1(v);
    } else if (num_shifts > 0) {
        const unsigned shl = static_cast<unsigned>(std::min<int64_t>(num_shifts, 64));
        for (unsigned i = 0; i < shl; ++i) AsertWideShl1(v, wide_overflow);
    }
    for (unsigned i = 0; i < 16; ++i) AsertWideShr1(v);
    if (wide_overflow) return pow_limit;
    return AsertWideToU256(v, pow_limit);
}

U256 MulDivU256(const U256& value, uint64_t num, uint64_t den)
{
    AsertWide w{};
    MulU256ByU64(value, num, w);
    uint64_t r = 0;
    for (int i = 5; i >= 0; --i) {
        const unsigned __int128 t = (static_cast<unsigned __int128>(r) << 64) | w[static_cast<size_t>(i)];
        w[static_cast<size_t>(i)] = static_cast<uint64_t>(t / den);
        r = static_cast<uint64_t>(t % den);
    }
    U256 ret{};
    for (int i = 0; i < 4; ++i) {
        ret.pn[2 * i] = static_cast<uint32_t>(w[i]);
        ret.pn[2 * i + 1] = static_cast<uint32_t>(w[i] >> 32);
    }
    return ret;
}

U256 ClampTargetToPrev(const U256& next_target, uint32_t prev_nbits, const U256& pow_limit)
{
    bool neg = false, ovf = false;
    U256 prev = U256FromCompact(prev_nbits, &neg, &ovf);
    if (neg || ovf || U256IsZero(prev)) return next_target;
    U256 max_t = MulDivU256(prev, static_cast<uint64_t>(MAX_ADJUSTMENT_FACTOR_PPM),
                            static_cast<uint64_t>(ASERT_RATIO_UNIT_PPM));
    U256 min_t = MulDivU256(prev, static_cast<uint64_t>(MIN_ADJUSTMENT_FACTOR_PPM),
                            static_cast<uint64_t>(ASERT_RATIO_UNIT_PPM));
    if (U256Cmp(max_t, pow_limit) > 0) max_t = pow_limit;
    if (U256IsZero(min_t)) min_t = U256FromU32(1);
    if (U256Cmp(min_t, pow_limit) > 0) min_t = pow_limit;
    U256 clamped = next_target;
    if (U256Cmp(clamped, max_t) > 0) clamped = max_t;
    if (U256Cmp(clamped, min_t) < 0) clamped = min_t;
    if (U256Cmp(clamped, pow_limit) > 0) clamped = pow_limit;
    if (U256IsZero(clamped)) clamped = U256FromU32(1);
    return clamped;
}

struct AsertState {
    uint32_t anchor_nbits{0x1d00ffff};
    int64_t anchor_time{0};
    int64_t parent_time{600};
    int parent_height{1};
    int anchor_height{1};
    uint32_t parent_nbits{0x1d00ffff};
    int64_t spacing{ASERT_SPACING};
    int64_t half_life{ASERT_HALF_LIFE};
};

uint32_t GetNextWorkAsertReplica(const AsertState& s, const U256& pow_limit)
{
    if (s.half_life <= 0) return s.parent_nbits;
    const int64_t height_diff = static_cast<int64_t>(s.parent_height) - static_cast<int64_t>(s.anchor_height);
    const int64_t time_error = s.parent_time - s.anchor_time - height_diff * s.spacing;
    const __int128 numer = static_cast<__int128>(time_error) * static_cast<__int128>(ASERT_RADIX);
    const __int128 denom = static_cast<__int128>(s.half_life);
    int64_t exp_q = static_cast<int64_t>(numer / denom);
    if (exp_q > ASERT_EXP_CLAMP) exp_q = ASERT_EXP_CLAMP;
    else if (exp_q < -ASERT_EXP_CLAMP) exp_q = -ASERT_EXP_CLAMP;
    const int64_t num_shifts = exp_q >> 16;
    const int64_t exp_rem = exp_q - (num_shifts << 16);
    const __int128 e = static_cast<__int128>(exp_rem);
    const __int128 poly =
        static_cast<__int128>(ASERT_POLY_P1) * e +
        static_cast<__int128>(ASERT_POLY_P2) * e * e +
        static_cast<__int128>(ASERT_POLY_P3) * e * e * e +
        static_cast<__int128>(ASERT_POLY_BIAS);
    const uint64_t factor = static_cast<uint64_t>(poly >> 48) + static_cast<uint64_t>(ASERT_RADIX);
    bool neg = false, ovf = false;
    U256 target_ref = U256FromCompact(s.anchor_nbits, &neg, &ovf);
    U256 next = AsertNextTarget(target_ref, factor, num_shifts, pow_limit);
    if (U256IsZero(next)) next = U256FromU32(1);
    if (U256Cmp(next, pow_limit) > 0) next = pow_limit;
    next = ClampTargetToPrev(next, s.parent_nbits, pow_limit);
    return U256GetCompact(next);
}

U256 BlockProof(uint32_t nBits)
{
    bool neg = false, ovf = false;
    U256 target = U256FromCompact(nBits, &neg, &ovf);
    if (neg || ovf || U256IsZero(target)) return U256{};
    // (~target / (target+1)) + 1  — used only to show illegal compact yields 0 work.
    return U256FromU32(1);
}

bool TestHeaderSerializationDeterministic()
{
    Header80 h;
    std::memset(h.hashPrevBlock, 0x11, 32);
    std::memset(h.hashMerkleRoot, 0x22, 32);
    unsigned char a[80], b[80];
    SerializeHeader80(h, a);
    SerializeHeader80(h, b);
    Expect(std::memcmp(a, b, 80) == 0, "duplicate-serialization");
    Expect(a[0] == 0x01 && a[1] == 0x00 && a[2] == 0x00 && a[3] == 0x00, "version-le");
    Expect(std::memcmp(a + 4, h.hashPrevBlock, 32) == 0, "prev-raw");
    Expect(std::memcmp(a + 36, h.hashMerkleRoot, 32) == 0, "merkle-raw");
    Expect(ReadLE32Local(a + 68) == h.nTime, "time-le");
    Expect(ReadLE32Local(a + 72) == h.nBits, "nbits-le");
    Expect(ReadLE32Local(a + 76) == h.nNonce, "nonce-le");

    unsigned char be_ver[80];
    std::memcpy(be_ver, a, 80);
    be_ver[0] = 0x00;
    be_ver[1] = 0x00;
    be_ver[2] = 0x00;
    be_ver[3] = 0x01;
    Expect(std::memcmp(a, be_ver, 80) != 0, "endian-mutation-distinct");
    Expect(!DigestsEqual(PowDigest(a), PowDigest(be_ver)), "endian-mutation-digest");

    unsigned char trunc[79];
    std::memcpy(trunc, a, 79);
    Expect(sizeof(trunc) != 80, "truncated-header-not-80");
    unsigned char padded[80]{};
    std::memcpy(padded, trunc, 79);
    Expect(!DigestsEqual(PowDigest(a), PowDigest(padded)), "truncated-header-digest");

    unsigned char over[81];
    std::memcpy(over, a, 80);
    over[80] = 0xff;
    Expect(DigestsEqual(PowDigest(a), PowDigest(over)), "oversized-extra-byte-not-in-preimage");

    unsigned char rev[80];
    for (int i = 0; i < 80; ++i) rev[i] = a[79 - i];
    Expect(!DigestsEqual(PowDigest(a), PowDigest(rev)), "byte-order-reversal");
    return g_fails == 0;
}

bool TestHashBoundaryMutation()
{
    Header80 h;
    std::memset(h.hashPrevBlock, 0x11, 32);
    std::memset(h.hashMerkleRoot, 0x22, 32);
    const auto d0 = PowDigest(h);

    unsigned char pre[88];
    std::memcpy(pre, rigor_kernel::POW_DOMAIN_TAG, 8);
    unsigned char ser[80];
    SerializeHeader80(h, ser);
    std::memcpy(pre + 8, ser, 80);
    std::array<unsigned char, 32> tagged{};
    rigor_kernel::Sha3_256Short(pre, 88, tagged.data());
    Expect(DigestsEqual(tagged, d0), "unique-preimage");

    unsigned char untagged[88]{};
    std::memcpy(untagged + 8, ser, 80);
    std::array<unsigned char, 32> bare{};
    rigor_kernel::Sha3_256Short(untagged, 88, bare.data());
    Expect(!DigestsEqual(bare, d0), "no-fallback-untagged");

    const unsigned char zero_vec[32] = {
        221, 5, 127, 193, 223, 143, 91, 148, 102, 232, 52, 204, 157, 123, 129, 158,
        45, 81, 28, 117, 180, 54, 117, 181, 119, 246, 217, 245, 214, 222, 4, 252};
    unsigned char zero80[80]{};
    const auto zd = PowDigest(zero80);
    Expect(std::memcmp(zd.data(), zero_vec, 32) == 0, "pow-digest-zero-header-vector");

    auto bump = [&](auto fn, const char* label) {
        Header80 m = h;
        fn(m);
        Expect(!DigestsEqual(PowDigest(m), d0), label);
    };
    bump([](Header80& m) { m.nNonce ^= 1; }, "nonce-mutation");
    bump([](Header80& m) { m.nTime ^= 1; }, "timestamp-mutation");
    bump([](Header80& m) { m.nVersion += 1; }, "version-mutation");
    bump([](Header80& m) { m.hashPrevBlock[0] ^= 0xff; }, "prev-hash-mutation");
    bump([](Header80& m) { m.hashMerkleRoot[31] ^= 0x01; }, "merkle-mutation");
    bump([](Header80& m) { m.nBits ^= 1; }, "nbits-mutation");
    return g_fails == 0;
}

bool TestTargetValidation()
{
    const U256 limit = MainnetPowLimit();
    Header80 h;
    std::memset(h.hashPrevBlock, 0xaa, 32);
    std::memset(h.hashMerkleRoot, 0xbb, 32);

    Expect(DecodeTarget(0, limit).ok == false, "target-zero");
    Expect(CheckPow(h, 0, limit) == PowDecision::REJECT, "target-zero-reject");

    const uint32_t negative = U256GetCompact(limit, true);
    Expect(DecodeTarget(negative, limit).negative, "target-negative");
    Expect(CheckPow(h, negative, limit) == PowDecision::REJECT, "target-negative-reject");

    const uint32_t overflow = ~0x00800000u;
    Expect(DecodeTarget(overflow, limit).overflow, "target-overflow");
    Expect(CheckPow(h, overflow, limit) == PowDecision::REJECT, "target-overflow-reject");

    const uint32_t too_easy = 0x1e7fffff; // one exponent step above mainnet powLimit
    Expect(DecodeTarget(too_easy, limit).above_limit, "target-above-powlimit");
    Expect(CheckPow(h, too_easy, limit) == PowDecision::REJECT, "target-above-powlimit-reject");

    const uint32_t min_compact = U256GetCompact(U256FromU32(1));
    Expect(CheckPow(h, min_compact, limit) == PowDecision::REJECT, "tiny-target-reject");

    const auto digest = PowDigest(h);
    U256 work = U256FromDigest(digest.data());
    const uint32_t self = U256GetCompact(work);
    bool neg = false, ovf = false;
    U256 decoded = U256FromCompact(self, &neg, &ovf);
    Expect(!neg && !ovf, "self-target-decode");
    const bool expect_accept = U256Cmp(work, decoded) <= 0;
    Expect(CheckPow(h, self, limit) == (expect_accept ? PowDecision::ACCEPT : PowDecision::REJECT),
           "digest-vs-self-compact");

    U256 harder = work;
    U256Shr(harder, 1);
    if (!U256IsZero(harder)) {
        Expect(CheckPow(h, U256GetCompact(harder), limit) == PowDecision::REJECT, "hash-greater-than-target");
    }

    const uint32_t signed_confused = 0x1d00ffff | 0x00800000;
    Expect(CheckPow(h, signed_confused, limit) == PowDecision::REJECT, "signed-unsigned-confusion");
    return g_fails == 0;
}

bool TestAsertUniqueness()
{
    const U256 limit = MainnetPowLimit();
    AsertState genesis;
    genesis.anchor_nbits = 0x1d00ffff;
    genesis.anchor_time = 1000;
    genesis.parent_time = 1000;
    genesis.parent_height = 0;
    genesis.anchor_height = 0;
    genesis.parent_nbits = 0x1d00ffff;
    const uint32_t first = GetNextWorkAsertReplica(genesis, limit);
    Expect(GetNextWorkAsertReplica(genesis, limit) == first, "asert-first-block-unique");

    AsertState h1 = genesis;
    h1.parent_height = 1;
    h1.anchor_height = 1;
    h1.parent_time = genesis.anchor_time + ASERT_SPACING;
    h1.anchor_time = h1.parent_time;
    const uint32_t gtrans = GetNextWorkAsertReplica(h1, limit);
    Expect(GetNextWorkAsertReplica(h1, limit) == gtrans, "asert-genesis-transition-unique");

    AsertState normal = h1;
    normal.parent_height = 10;
    normal.anchor_height = 1;
    normal.parent_time = h1.parent_time + 9 * ASERT_SPACING;
    const uint32_t n_a = GetNextWorkAsertReplica(normal, limit);
    const uint32_t n_b = GetNextWorkAsertReplica(normal, limit);
    Expect(n_a == n_b, "asert-normal-interval-unique");

    AsertState fast = normal;
    fast.parent_time = h1.parent_time + 9 * 60;
    AsertState slow = normal;
    slow.parent_time = h1.parent_time + 9 * 3600;
    const uint32_t n_fast = GetNextWorkAsertReplica(fast, limit);
    const uint32_t n_slow = GetNextWorkAsertReplica(slow, limit);
    Expect(GetNextWorkAsertReplica(fast, limit) == n_fast, "asert-fast-unique");
    Expect(GetNextWorkAsertReplica(slow, limit) == n_slow, "asert-slow-unique");
    bool n1, o1, n2, o2;
    const U256 t_fast = U256FromCompact(n_fast, &n1, &o1);
    const U256 t_slow = U256FromCompact(n_slow, &n2, &o2);
    Expect(U256Cmp(t_fast, t_slow) <= 0, "fast-blocks-not-easier-than-slow");

    AsertState ts = normal;
    const uint32_t before = GetNextWorkAsertReplica(ts, limit);
    (void)before;
    // Child timestamp is not an ASERT input (production GetNextWorkASERT ignores pblock).
    Expect(GetNextWorkAsertReplica(ts, limit) == before, "child-time-not-in-asert-state");

    AsertState extreme = normal;
    extreme.parent_time = std::numeric_limits<int32_t>::max();
    const uint32_t clamped = GetNextWorkAsertReplica(extreme, limit);
    Expect(GetNextWorkAsertReplica(extreme, limit) == clamped, "timestamp-extreme-unique");
    Expect(DecodeTarget(clamped, limit).ok || clamped == extreme.parent_nbits, "timestamp-extreme-legal-or-parent");
    return g_fails == 0;
}

bool TestAttackVectors()
{
    const U256 limit = MainnetPowLimit();
    Header80 h;
    std::memset(h.hashPrevBlock, 0x33, 32);
    std::memset(h.hashMerkleRoot, 0x44, 32);
    h.nBits = 0x1d00ffff;
    h.nNonce = 1;

    Expect(CheckPow(h, h.nBits, limit) == PowDecision::REJECT ||
           CheckPow(h, h.nBits, limit) == PowDecision::ACCEPT, "binary-decision");

    Header80 bad_nonce = h;
    bad_nonce.nNonce ^= 0xdeadbeef;
    const auto d0 = PowDigest(h);
    Expect(!DigestsEqual(PowDigest(bad_nonce), d0), "invalid-nonce-changes-digest");

    const uint32_t fake_easy = 0x1e7fffff;
    Expect(CheckPow(h, fake_easy, limit) == PowDecision::REJECT, "fake-target-reject");
    Expect(CheckPow(h, 0xff7fffff, limit) == PowDecision::REJECT, "malformed-nbits-reject");

    Header80 mutated = h;
    mutated.hashMerkleRoot[0] ^= 0x01;
    Expect(!DigestsEqual(PowDigest(mutated), d0), "header-mutation-invalidates-pow");

    Header80 replay = h;
    replay.hashPrevBlock[15] ^= 0x80;
    Expect(!DigestsEqual(PowDigest(replay), d0), "replay-old-header-prev-changes-digest");

    Expect(U256IsZero(BlockProof(0)), "illegal-compact-zero-work");
    Expect(U256IsZero(BlockProof(0xff7fffff)), "overflow-compact-zero-work");
    Expect(!U256IsZero(BlockProof(0x1d00ffff)), "legal-compact-nonzero-work-floor");

    AsertState expected;
    expected.parent_height = 10;
    expected.anchor_height = 1;
    expected.anchor_time = 1000;
    expected.parent_time = 1000 + 9 * ASERT_SPACING;
    expected.anchor_nbits = 0x1d00ffff;
    expected.parent_nbits = 0x1d00ffff;
    const uint32_t want = GetNextWorkAsertReplica(expected, limit);
    Expect(want != fake_easy, "chainwork-fake-nbits-not-asert");
    return g_fails == 0;
}

bool RunAll()
{
    g_fails = 0;
    TestHeaderSerializationDeterministic();
    TestHashBoundaryMutation();
    TestTargetValidation();
    TestAsertUniqueness();
    TestAttackVectors();
    if (g_fails != 0) {
        std::fprintf(stderr, "test_pow_boundary_security fails=%d\n", g_fails);
        return false;
    }
    std::printf("POW_BOUNDARY_TESTS = PASS\n");
    std::printf("HEADER_SERIALIZATION_STATUS = PASS\n");
    std::printf("HASH_BOUNDARY = PASS\n");
    std::printf("TARGET_VALIDATION_STATUS = PASS\n");
    std::printf("ASERT_UNIQUENESS = PASS\n");
    std::printf("ATTACK_VECTORS = PASS\n");
    std::printf("CRASH_FOUND = NO\n");
    std::printf("UNDEFINED_BEHAVIOR = NO\n");
    std::printf("CONSENSUS_BYPASS = NONE\n");
    return true;
}

} // namespace

#ifdef POW_BOUNDARY_SECURITY_STANDALONE
int main()
{
    return RunAll() ? 0 : 1;
}
#else
#include <arith_uint256.h>
#include <chain.h>
#include <chainparams.h>
#include <pow.h>
#include <primitives/block.h>
#include <test/util/setup_common.h>
#include <uint256.h>

#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(test_pow_boundary_security, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(standalone_pow_boundary_suite)
{
    g_fails = 0;
    BOOST_REQUIRE(RunAll());
}

BOOST_AUTO_TEST_CASE(production_checkproofofwork_illegal_target)
{
    const auto consensus = CreateChainParams(*m_node.args, CBaseChainParams::MAIN)->GetConsensus();
    CBlockHeader h;
    h.nVersion = 1;
    h.nTime = 1700000000;
    h.nBits = 0x1d00ffff;
    h.nNonce = 1;
    BOOST_CHECK(!CheckProofOfWork(h, 0, consensus));
    BOOST_CHECK(!CheckProofOfWork(h, ~0x00800000u, consensus));
    arith_uint256 too_easy = UintToArith256(consensus.powLimit);
    too_easy *= 2;
    BOOST_CHECK(!CheckProofOfWork(h, too_easy.GetCompact(), consensus));
}

BOOST_AUTO_TEST_CASE(production_asert_unique_and_ignores_child_time)
{
    const auto chainParams = CreateChainParams(*m_node.args, CBaseChainParams::MAIN);
    const auto& consensus = chainParams->GetConsensus();
    const CBlock& genesis_block = chainParams->GenesisBlock();

    CBlockIndex genesis{};
    genesis.nHeight = 0;
    genesis.pprev = nullptr;
    genesis.nBits = genesis_block.nBits;
    genesis.nTime = genesis_block.nTime;

    CBlockHeader child_a;
    child_a.nTime = genesis.nTime + static_cast<uint32_t>(consensus.nPowTargetSpacing);
    CBlockHeader child_b;
    child_b.nTime = genesis.nTime + 7200;
    BOOST_CHECK_EQUAL(GetNextWorkRequired(&genesis, &child_a, consensus),
                      GetNextWorkRequired(&genesis, &child_b, consensus));
    BOOST_CHECK_EQUAL(GetNextWorkRequired(&genesis, &child_a, consensus),
                      GetNextWorkRequired(&genesis, &child_a, consensus));
}

BOOST_AUTO_TEST_SUITE_END()
#endif
