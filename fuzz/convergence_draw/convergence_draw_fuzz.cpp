// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// P0-079 Convergence Draw fuzz harness.
// Input: candidate set / weight / txid / threshold.
// Crash => CRASH_FOUND. Order-dependent winner => NONDETERMINISM.
// Winner index out of range or ineligible => INVALID_WINNER.

#include <consensus/amount.h>
#include <consensus/convergence_logic.h>
#include <consensus/params.h>
#include <consensus/rigor_consensus_lock.h>
#include <uint256.h>

#include <algorithm>
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

static uint256 ReadU256(const uint8_t* data, size_t size, size_t off)
{
    uint256 id;
    std::memset(id.begin(), 0, 32);
    if (off >= size) return id;
    const size_t take = std::min<size_t>(32, size - off);
    std::memcpy(id.begin(), data + off, take);
    return id;
}

static uint64_t ReadU64(const uint8_t* data, size_t size, size_t off)
{
    uint64_t v = 0;
    if (off >= size) return v;
    const size_t take = std::min<size_t>(8, size - off);
    std::memcpy(&v, data + off, take);
    return v;
}

static uint256 TargetFromMode(uint8_t mode)
{
    if (mode == 0) return uint256();
    if (mode == 0xff) {
        uint256 t;
        std::fill(t.begin(), t.end(), static_cast<unsigned char>(0xff));
        return t;
    }
    return uint256S("5555555555555555555555555555555555555555555555555555555555555555");
}

static void CheckOutcome(const ConvergenceDrawSet& set, const ConvergenceDrawOutcome& o)
{
    if (!o.HasWinner()) {
        if (o.winner_index != -1) Die("INVALID_WINNER");
        return;
    }
    if (o.winner_index < 0) Die("INVALID_WINNER");
    const size_t i = static_cast<size_t>(o.winner_index);
    if (i >= set.candidates.size()) Die("INVALID_WINNER");
    if (o.eligible.size() != set.candidates.size()) Die("INVALID_WINNER");
    if (!o.eligible[i]) Die("INVALID_WINNER");
    if (o.sigmas.size() != set.candidates.size()) Die("INVALID_WINNER");
}

static void One(const uint8_t* data, size_t size)
{
    ConvergenceDrawSet set;
    set.source_height = size > 2 ? static_cast<int>(data[2] % 200) + 1 : 100;
    set.source_block_hash = ReadU256(data, size, 3);
    set.convergence_root = ReadU256(data, size, 35);
    const uint8_t n = size > 0 ? static_cast<uint8_t>(data[0] % 17) : 0;
    const uint256 T = TargetFromMode(size > 1 ? data[1] : 1);
    set.candidates.reserve(n);
    size_t off = 67;
    for (uint8_t i = 0; i < n; ++i) {
        ConvergenceDrawCandidate c;
        c.txid = ReadU256(data, size, off);
        off += 32;
        c.weight = ReadU64(data, size, off);
        off += 8;
        c.source_height = set.source_height;
        set.candidates.push_back(c);
    }

    const ConvergenceDrawOutcome a = ResolveConvergenceDraw(set, T);
    CheckOutcome(set, a);
    const ConvergenceDrawOutcome a2 = ResolveConvergenceDraw(set, T);
    CheckOutcome(set, a2);
    if (a.HasWinner() != a2.HasWinner()) Die("NONDETERMINISM");
    if (a.winner_index != a2.winner_index) Die("NONDETERMINISM");

    if (set.candidates.size() >= 2) {
        const uint256 wa = a.HasWinner() ? set.candidates[static_cast<size_t>(a.winner_index)].txid : uint256();
        std::reverse(set.candidates.begin(), set.candidates.end());
        const ConvergenceDrawOutcome b = ResolveConvergenceDraw(set, T);
        CheckOutcome(set, b);
        if (a.HasWinner() != b.HasWinner()) Die("NONDETERMINISM");
        if (a.HasWinner()) {
            const uint256 wb = set.candidates[static_cast<size_t>(b.winner_index)].txid;
            if (wa != wb) Die("NONDETERMINISM");
        }
    }
}

#ifdef CONVERGENCE_DRAW_FUZZ_MAIN
int main(int argc, char** argv)
{
    try {
        if (argc >= 2 && std::strcmp(argv[1], "smoke") == 0) {
            const uint8_t empty[] = {0, 1};
            One(empty, sizeof(empty));
            uint8_t one[128]{};
            one[0] = 1;
            one[1] = 1;
            one[67 + 31] = 0x11;
            one[67 + 32] = 1;
            One(one, sizeof(one));
            uint8_t two[180]{};
            two[0] = 2;
            two[1] = 1;
            two[67 + 31] = 0x11;
            two[67 + 32] = 1;
            two[67 + 40 + 31] = 0x22;
            two[67 + 40 + 32] = 1;
            One(two, sizeof(two));
            uint8_t t0[128]{};
            t0[0] = 1;
            t0[1] = 0;
            t0[67 + 31] = 0x33;
            One(t0, sizeof(t0));
            uint8_t tmax[128]{};
            tmax[0] = 1;
            tmax[1] = 0xff;
            tmax[67 + 31] = 0x44;
            One(tmax, sizeof(tmax));
            uint8_t fat[128]{};
            fat[0] = 1;
            fat[1] = 1;
            std::memset(fat + 67 + 32, 0xff, 8);
            One(fat, sizeof(fat));
            uint8_t longbuf[2048];
            for (size_t i = 0; i < sizeof(longbuf); ++i) {
                longbuf[i] = static_cast<uint8_t>(i * 19u + 7u);
            }
            One(longbuf, sizeof(longbuf));
            One(nullptr, 0);
            std::printf("FUZZ_STATUS=PASS\n");
            std::printf("CRASH_FOUND=NO\n");
            std::printf("NONDETERMINISM=NO\n");
            std::printf("INVALID_WINNER=NO\n");
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
