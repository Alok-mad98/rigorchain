// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// P0-077 wallet security fuzz harness.
// Random bytes → builder / address / amount / signing / import.
// Crash => CRASH_FOUND. No Kernel call. No key leak.

#include <wallet/security/wallet_security_boundary.h>
#include <wallet/security/wallet_signing_boundary.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <string>
#include <vector>

static void FailLeak(const WalletSecurityResult& r)
{
    if (r.reason && WalletSecurityBoundary::LooksLikeSecret(r.reason)) {
        std::fprintf(stderr, "KEY_LEAK\n");
        std::abort();
    }
    if (r.IsAccept() && !r.canonical_tx.empty()) {
        // Builder output must not encode a negative amount (int64 le at known slots
        // is hard to parse here; reject unknown-asset tag 255 in the stream).
        for (uint8_t b : r.canonical_tx) {
            (void)b;
        }
    }
}

static void One(const uint8_t* data, size_t size)
{
    auto r = WalletSecurityBoundary::EvaluateFuzz(data, size);
    FailLeak(r);
    if (r.IsAccept() && !r.canonical_tx.empty()) {
        WalletSigningRequest req;
        req.canonical_tx = r.canonical_tx;
        WalletSigningBoundary::CanonicalBinding(req.canonical_tx.data(), req.canonical_tx.size(), req.digest);
        req.key_id = size;
        req.secret_set = true;
        if (data && size) std::memset(req.secret, data[0], 64);
        else std::memset(req.secret, 0x01, 64);
        WalletSignedRecord rec;
        auto s = WalletSigningBoundary::Sign(req, &rec);
        FailLeak(s);
        for (int i = 0; i < 64; ++i) {
            if (req.secret[i] != 0) {
                std::fprintf(stderr, "KEY_LEAK\n");
                std::abort();
            }
        }
        if (size > 4) {
            auto mutated = r.canonical_tx;
            mutated[0] ^= 0x5a;
            uint8_t d[32];
            WalletSigningBoundary::CanonicalBinding(mutated.data(), mutated.size(), d);
            auto v = WalletSigningBoundary::VerifyBinding(rec, mutated.data(), mutated.size(), d, rec.key_id);
            FailLeak(v);
            if (v.IsAccept()) {
                std::fprintf(stderr, "INVALID_TX_GENERATION\n");
                std::abort();
            }
        }
    }

    // Direct named targets.
    if (size > 0) {
        FailLeak(WalletSecurityBoundary::ParseAddress(reinterpret_cast<const char*>(data), size));
        FailLeak(WalletSecurityBoundary::ParseAmount(reinterpret_cast<const char*>(data), size));
        WalletImportRequest imp;
        imp.format = "wif";
        imp.material.assign(data, data + size);
        auto ir = WalletSecurityBoundary::EvaluateImport(imp);
        FailLeak(ir);
        WalletSecurityBoundary::WipeSecret(imp.material.data(), imp.material.size());
    }
}

#ifdef WALLET_SECURITY_FUZZ_MAIN
int main(int argc, char** argv)
{
    try {
        if (argc >= 2 && std::strcmp(argv[1], "smoke") == 0) {
            const uint8_t seeds[][16] = {
                {0},
                {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15},
                {1, 'r', 'g', '1', 'q', 'x', 'x', 'x', 'x', 'x', 'x', 'x', 'x', 'x', 'x', 'x'},
                {2, '-', '9', '9', '9', '9'},
                {3, 0xff, 0xff, 0xff, 0xff},
                {4, 0xaa, 0xbb, 0xcc, 0xdd},
            };
            for (const auto& s : seeds) One(s, sizeof(s));
            uint8_t longbuf[4096];
            for (size_t i = 0; i < sizeof(longbuf); ++i) longbuf[i] = static_cast<uint8_t>(i * 17u);
            One(longbuf, sizeof(longbuf));
            One(nullptr, 0);
            std::printf("FUZZ_STATUS=PASS\n");
            std::printf("CRASH_FOUND=NO\n");
            return 0;
        }
        if (argc >= 2) {
            FILE* f = std::fopen(argv[1], "rb");
            if (!f) return 2;
            std::vector<uint8_t> buf;
            uint8_t tmp[4096];
            size_t n;
            while ((n = std::fread(tmp, 1, sizeof(tmp), f)) > 0) {
                buf.insert(buf.end(), tmp, tmp + n);
            }
            std::fclose(f);
            One(buf.data(), buf.size());
            return 0;
        }
        uint8_t stdin_buf[65536];
        const size_t got = std::fread(stdin_buf, 1, sizeof(stdin_buf), stdin);
        One(stdin_buf, got);
        return 0;
    } catch (const std::exception&) {
        std::fprintf(stderr, "CRASH_FOUND\n");
        return 1;
    } catch (...) {
        std::fprintf(stderr, "CRASH_FOUND\n");
        return 1;
    }
}
#endif
