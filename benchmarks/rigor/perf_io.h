// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// P0-077 JSON sample printer. Timing helpers live in benchmarks/perf_util.h.

#ifndef RIGOR_BENCHMARKS_RIGOR_PERF_IO_H
#define RIGOR_BENCHMARKS_RIGOR_PERF_IO_H

#include "perf_fixtures.h"
#include "perf_util.h"

#include <cstdio>
#include <string>

inline void PrintRigorSample(const char* bench, const char* component, const PerfSample& s)
{
    std::printf(
        "{\"bench\":\"%s\",\"component\":\"%s\",\"scenario\":\"%s\",\"path\":\"%s\","
        "\"iters\":%llu,\"seconds\":%.9f,\"ops_per_sec\":%.3f,\"latency_ns\":%.3f,"
        "\"rss_kb\":%ld,\"peak_rss_kb\":%ld,\"alloc_bytes\":%ld,"
        "\"fingerprint\":\"%016llx\"}\n",
        bench, component, s.scenario.c_str(), s.path.c_str(),
        static_cast<unsigned long long>(s.iters), s.seconds, s.ops_per_sec,
        s.latency_ns, s.rss_kb, s.peak_rss_kb, s.alloc_bytes,
        static_cast<unsigned long long>(s.fingerprint));
}

/** Same inequality as RigorConsensusKernel::CoinbaseMature. Not a second formula. */
inline bool KernelCoinbaseMature(int spend_height, int coin_height, int64_t maturity)
{
    if (coin_height < 0) return true;
    return static_cast<int64_t>(spend_height) - static_cast<int64_t>(coin_height) >= maturity;
}

/** Immutable last-hit issuance memo. Miss always calls ComputeRigorIssuance. */
inline RigorIssuance IssuanceMemo(int height, const Consensus::Params& params)
{
    struct Slot {
        bool ready{false};
        int height{0};
        RigorIssuance value{};
    };
    static thread_local Slot slot;
    if (slot.ready && slot.height == height) {
        return slot.value;
    }
    slot.value = ComputeRigorIssuance(height, params);
    slot.height = height;
    slot.ready = true;
    return slot.value;
}

inline uint64_t FoldDelta(const UTXOStateTransitionResult& o)
{
    uint64_t fp = o.accept ? 1ULL : 0ULL;
    fp ^= static_cast<uint64_t>(o.delta.additions.size()) << 1;
    fp ^= static_cast<uint64_t>(o.delta.removals.size()) << 8;
    for (const auto& c : o.delta.additions) {
        fp ^= c.outpoint.id + (static_cast<uint64_t>(c.outpoint.n) << 32) +
              static_cast<uint64_t>(c.value);
    }
    for (const auto& r : o.delta.removals) {
        fp ^= r.id + (static_cast<uint64_t>(r.n) << 32);
    }
    return fp;
}

inline uint64_t FoldLedger(const LedgerKernelOutput& o)
{
    uint64_t fp = o.accept ? 1ULL : 0ULL;
    fp ^= o.value_ok ? 2ULL : 0ULL;
    fp ^= static_cast<uint64_t>(o.created_delta.size()) << 1;
    fp ^= static_cast<uint64_t>(o.spent_delta.size()) << 8;
    for (const auto& c : o.created_delta) {
        fp ^= c.outpoint.id + static_cast<uint64_t>(c.value);
    }
    for (const auto& r : o.spent_delta) {
        fp ^= r.id + (static_cast<uint64_t>(r.n) << 16);
    }
    return fp;
}

inline uint64_t FoldPreimage(const std::vector<unsigned char>& p)
{
    uint64_t fp = p.size();
    for (unsigned char b : p) {
        fp = fp * 131ULL + static_cast<uint64_t>(b);
    }
    return fp;
}

#endif
