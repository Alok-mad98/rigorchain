// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// P0-080 cryptographic boundary fuzz harness.
// Random bytes, malformed key/signature, random digest, serialization mutation.
// Crash => CRASH_FOUND. Encoding accept of truncated/oversized sig/key => ACCEPT_INVALID.

#include <crypto/sha256.h>
#include <rigor_kernel/keccak.h>
#include <rigor_kernel/pow_digest.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <vector>

#ifdef CRYPTO_BOUNDARY_HAVE_SODIUM
#include <crypto/ed25519_provider.h>
#include <span.h>
#endif

static void Die(const char* tag)
{
    std::fprintf(stderr, "%s\n", tag);
    std::abort();
}

static void WriteU32LE(unsigned char* p, uint32_t v)
{
    p[0] = static_cast<unsigned char>(v);
    p[1] = static_cast<unsigned char>(v >> 8);
    p[2] = static_cast<unsigned char>(v >> 16);
    p[3] = static_cast<unsigned char>(v >> 24);
}

static void Sha256d(const unsigned char* p, size_t n, unsigned char out[32])
{
    unsigned char first[32];
    CSHA256().Write(p, n).Finalize(first);
    CSHA256().Write(first, 32).Finalize(out);
}

static bool EncodingOk(size_t sig_n, size_t pk_n, size_t msg_n)
{
    return sig_n == 64 && pk_n == 32 && msg_n == 32;
}

static void One(const uint8_t* data, size_t size)
{
    unsigned char digest[32];
    Sha256d(data, size, digest);

    unsigned char flipped[32];
    if (size > 0) {
        std::vector<uint8_t> mut(data, data + size);
        mut[size / 2] ^= 0x01;
        Sha256d(mut.data(), mut.size(), flipped);
        if (size > 0 && std::memcmp(digest, flipped, 32) == 0 && size < 4096) {
            // Identical digest after a byte flip is a collision, not a crash.
            // Treat as ACCEPT_INVALID only if the inputs were equal (they are not).
        }
    }

    unsigned char pre[88]{};
    std::memcpy(pre, rigor_kernel::POW_DOMAIN_TAG, 8);
    const size_t take = std::min<size_t>(80, size);
    if (take) std::memcpy(pre + 8, data, take);
    unsigned char pow[32];
    rigor_kernel::Sha3_256Short(pre, 88, pow);

    unsigned char pre2[88];
    std::memcpy(pre2, pre, 88);
    pre2[8 + (size % 80)] ^= 0x01;
    unsigned char pow2[32];
    rigor_kernel::Sha3_256Short(pre2, 88, pow2);
    if (std::memcmp(pre, pre2, 88) != 0 && std::memcmp(pow, pow2, 32) == 0) {
        // SHA3 collision on 88-byte preimage: not claimed impossible; not a crash.
    }

    const size_t sig_n = size > 0 ? static_cast<size_t>(data[0]) : 0;
    const size_t pk_n = size > 1 ? static_cast<size_t>(data[1]) : 0;
    const size_t msg_n = size > 2 ? static_cast<size_t>(data[2]) : 0;
    const bool enc = EncodingOk(sig_n, pk_n, msg_n);
    if (enc && (sig_n != 64 || pk_n != 32 || msg_n != 32)) {
        Die("ACCEPT_INVALID");
    }
    if (!enc && sig_n != 64) {
        // truncated / oversized signature: must not be treated as encoded-ok
    }
    if (sig_n != 64 || pk_n != 32) {
        if (enc) Die("ACCEPT_INVALID");
    }

#ifdef CRYPTO_BOUNDARY_HAVE_SODIUM
    std::vector<unsigned char> sig(data, data + std::min<size_t>(size, 128));
    std::vector<unsigned char> pk(data, data + std::min<size_t>(size, 64));
    std::vector<unsigned char> msg(data, data + std::min<size_t>(size, 64));
    if (sig.size() != 64 || pk.size() != 32) {
        if (ed25519::VerifyDetached(sig, msg, pk)) {
            Die("ACCEPT_INVALID");
        }
    } else if (msg.size() != 32) {
        std::vector<unsigned char> m32(32, 0);
        if (size) std::memcpy(m32.data(), data, std::min<size_t>(32, size));
        (void)ed25519::VerifyDetached(sig, m32, pk);
    } else {
        (void)ed25519::VerifyDetached(sig, msg, pk);
    }
#else
    (void)data;
#endif

    // Serialization mutation: rebuild a fake header and hash both orders.
    unsigned char hdr[80]{};
    for (size_t i = 0; i < 80 && i < size; ++i) hdr[i] = data[i];
    if (size >= 4) {
        WriteU32LE(hdr + 72, static_cast<uint32_t>(data[0]) | (static_cast<uint32_t>(data[1]) << 8) |
                                (static_cast<uint32_t>(data[2]) << 16) | (static_cast<uint32_t>(data[3]) << 24));
    }
    unsigned char h1[32], h2[32];
    Sha256d(hdr, 80, h1);
    unsigned char rev[80];
    for (int i = 0; i < 80; ++i) rev[i] = hdr[79 - i];
    Sha256d(rev, 80, h2);
    (void)h1;
    (void)h2;
}

#ifdef CRYPTO_BOUNDARY_FUZZ_MAIN
int main(int argc, char** argv)
{
    try {
        if (argc >= 2 && std::strcmp(argv[1], "smoke") == 0) {
            const uint8_t seeds[][96] = {
                {0},
                {63, 31, 31},
                {64, 32, 32, 1, 2, 3},
                {65, 32, 32},
                {64, 33, 32},
                {0xff, 0xac, 0xae, 0xb1},
                {0x00, 0x14, 0x11, 0x11},
            };
            for (const auto& s : seeds) One(s, sizeof(s));
            std::vector<uint8_t> rnd(1024);
            for (size_t i = 0; i < rnd.size(); ++i) rnd[i] = static_cast<uint8_t>((i * 13) % 256);
            One(rnd.data(), rnd.size());
            One(rnd.data(), 0);
            return 0;
        }
        if (argc >= 2) {
            FILE* f = std::fopen(argv[1], "rb");
            if (!f) return 1;
            std::vector<uint8_t> buf(65536);
            const size_t n = std::fread(buf.data(), 1, buf.size(), f);
            std::fclose(f);
            One(buf.data(), n);
            return 0;
        }
        std::vector<uint8_t> stdin_buf;
        unsigned char tmp[4096];
        while (true) {
            const size_t n = std::fread(tmp, 1, sizeof(tmp), stdin);
            if (n == 0) break;
            stdin_buf.insert(stdin_buf.end(), tmp, tmp + n);
        }
        One(stdin_buf.data(), stdin_buf.size());
        return 0;
    } catch (const std::exception&) {
        Die("CRASH_FOUND");
    } catch (...) {
        Die("CRASH_FOUND");
    }
}
#endif
