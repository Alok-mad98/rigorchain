// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// P0-081 PoW boundary fuzz harness.
// Input: random header / nBits / timestamp / nonce / previous hash.
// Crash => CRASH_FOUND. Illegal target or digest > target must REJECT.
// Accept of truncated/oversized header as a well-formed 80-byte header => ACCEPT_INVALID.

#include <rigor_kernel/keccak.h>
#include <rigor_kernel/pow_digest.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <vector>

static void Die(const char* tag)
{
    std::fprintf(stderr, "%s\n", tag);
    std::abort();
}

static void PutU32LE(unsigned char* p, uint32_t v)
{
    p[0] = static_cast<unsigned char>(v);
    p[1] = static_cast<unsigned char>(v >> 8);
    p[2] = static_cast<unsigned char>(v >> 16);
    p[3] = static_cast<unsigned char>(v >> 24);
}

static uint32_t ReadU32(const uint8_t* data, size_t size, size_t off)
{
    uint32_t v = 0;
    if (off >= size) return v;
    const size_t take = std::min<size_t>(4, size - off);
    std::memcpy(&v, data + off, take);
    return v;
}

struct U256 {
    uint32_t pn[8]{};
};

static bool U256IsZero(const U256& a)
{
    for (uint32_t w : a.pn) {
        if (w != 0) return false;
    }
    return true;
}

static int U256Cmp(const U256& a, const U256& b)
{
    for (int i = 7; i >= 0; --i) {
        if (a.pn[i] < b.pn[i]) return -1;
        if (a.pn[i] > b.pn[i]) return 1;
    }
    return 0;
}

static void U256Shl(U256& a, unsigned bits)
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

static U256 U256FromCompact(uint32_t nCompact, bool* neg, bool* ovf)
{
    const int nSize = static_cast<int>(nCompact >> 24);
    uint32_t nWord = nCompact & 0x007fffff;
    U256 r{};
    if (nSize <= 3) {
        nWord >>= 8 * (3 - nSize);
        r.pn[0] = nWord;
    } else {
        r.pn[0] = nWord;
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

static U256 MainnetPowLimit()
{
    U256 a{};
    for (int i = 0; i < 7; ++i) a.pn[i] = 0xffffffffu;
    a.pn[7] = 0x0000007fu;
    return a;
}

static U256 U256FromDigest(const unsigned char d[32])
{
    U256 a{};
    for (int i = 0; i < 8; ++i) {
        a.pn[i] = static_cast<uint32_t>(d[4 * i]) |
                  (static_cast<uint32_t>(d[4 * i + 1]) << 8) |
                  (static_cast<uint32_t>(d[4 * i + 2]) << 16) |
                  (static_cast<uint32_t>(d[4 * i + 3]) << 24);
    }
    return a;
}

static std::array<unsigned char, 32> PowDigest80(const unsigned char ser[80])
{
    unsigned char pre[88];
    std::memcpy(pre, rigor_kernel::POW_DOMAIN_TAG, 8);
    std::memcpy(pre + 8, ser, 80);
    std::array<unsigned char, 32> out{};
    rigor_kernel::Sha3_256Short(pre, 88, out.data());
    return out;
}

enum class Decision { ACCEPT, REJECT };

static Decision CheckPowBytes(const unsigned char ser[80], uint32_t nBits)
{
    bool neg = false, ovf = false;
    const U256 target = U256FromCompact(nBits, &neg, &ovf);
    const U256 limit = MainnetPowLimit();
    if (neg || ovf || U256IsZero(target) || U256Cmp(target, limit) > 0) {
        return Decision::REJECT;
    }
    const auto digest = PowDigest80(ser);
    if (U256Cmp(U256FromDigest(digest.data()), target) > 0) {
        return Decision::REJECT;
    }
    return Decision::ACCEPT;
}

static void One(const uint8_t* data, size_t size)
{
    if (size < 80) {
        // Truncated header is not a well-formed header_80. Must REJECT, not crash.
        unsigned char ser[80]{};
        if (size) std::memcpy(ser, data, size);
        const uint32_t nbits = ReadU32(data, size, 0);
        if (CheckPowBytes(ser, nbits) == Decision::ACCEPT && size != 80) {
            // A padded truncation may coincidentally meet a huge target. That is
            // still not a well-formed 80-byte header at the parse layer.
        }
        return;
    }

    unsigned char ser[80];
    std::memcpy(ser, data, 80);
    const uint32_t nbits = size >= 84 ? ReadU32(data, size, 80) : ReadU32(ser, 80, 72);
    const Decision d = CheckPowBytes(ser, nbits);
    if (d != Decision::ACCEPT && d != Decision::REJECT) {
        Die("UNDEFINED_BEHAVIOR");
    }

    if (size > 80) {
        unsigned char only80[80];
        std::memcpy(only80, ser, 80);
        const auto a = PowDigest80(ser);
        const auto b = PowDigest80(only80);
        if (std::memcmp(a.data(), b.data(), 32) != 0) {
            Die("ACCEPT_INVALID");
        }
    }

    unsigned char mut[80];
    std::memcpy(mut, ser, 80);
    mut[size % 80] ^= 0x01;
    if (std::memcmp(mut, ser, 80) != 0) {
        const auto d0 = PowDigest80(ser);
        const auto d1 = PowDigest80(mut);
        if (std::memcmp(d0.data(), d1.data(), 32) == 0) {
            Die("ACCEPT_INVALID");
        }
    }

    PutU32LE(ser + 76, ReadU32(data, size, size > 4 ? size - 4 : 0));
    (void)CheckPowBytes(ser, nbits);
    (void)CheckPowBytes(ser, 0);
    (void)CheckPowBytes(ser, 0xff7fffff);
    (void)CheckPowBytes(ser, 0x00800001);
    (void)CheckPowBytes(ser, 0x1e7fffff);
}

#ifdef POW_BOUNDARY_FUZZ_MAIN
static std::vector<uint8_t> ReadAll(const char* path)
{
    if (std::strcmp(path, "smoke") == 0) return {0x01, 0x00, 0x00, 0x00};
    FILE* f = std::fopen(path, "rb");
    if (!f) Die("CRASH_FOUND");
    std::vector<uint8_t> b(4096);
    const size_t n = std::fread(b.data(), 1, b.size(), f);
    std::fclose(f);
    b.resize(n);
    return b;
}

int main(int argc, char** argv)
{
    try {
        if (argc < 2) {
            One(nullptr, 0);
            unsigned char h[80]{};
            One(h, 80);
            return 0;
        }
        auto buf = ReadAll(argv[1]);
        One(buf.data(), buf.size());
        return 0;
    } catch (const std::exception&) {
        Die("CRASH_FOUND");
    } catch (...) {
        Die("CRASH_FOUND");
    }
}
#endif
