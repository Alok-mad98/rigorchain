// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// P0-066 timing / RSS helpers. Not consensus.

#ifndef RIGOR_BENCHMARKS_PERF_UTIL_H
#define RIGOR_BENCHMARKS_PERF_UTIL_H

#include <chrono>
#include <cstdint>
#include <cstdio>
#include <string>
#include <sys/resource.h>
#include <unistd.h>

#ifdef __GLIBC__
#include <malloc.h>
#endif

struct PerfSample {
    std::string scenario;
    std::string path;
    uint64_t iters{0};
    double seconds{0};
    double ops_per_sec{0};
    double latency_ns{0};
    long rss_kb{0};
    long peak_rss_kb{0};
    long alloc_bytes{0};
    uint64_t fingerprint{0};
};

inline long ReadRssKb()
{
    struct rusage u {};
    getrusage(RUSAGE_SELF, &u);
    return u.ru_maxrss;
}

inline long ReadAllocBytes()
{
#ifdef __GLIBC__
    const struct mallinfo2 mi = mallinfo2();
    return static_cast<long>(mi.uordblks);
#else
    return 0;
#endif
}

template <typename Fn>
inline PerfSample RunTimed(const char* scenario, const char* path, uint64_t iters, Fn&& fn)
{
    volatile uint64_t sink = 0;
    for (uint64_t i = 0; i < 8 && i < iters; ++i) sink += static_cast<uint64_t>(fn());
    const long rss0 = ReadRssKb();
    const long alloc0 = ReadAllocBytes();
    const auto t0 = std::chrono::steady_clock::now();
    uint64_t fp = 0;
    for (uint64_t i = 0; i < iters; ++i) {
        const uint64_t v = static_cast<uint64_t>(fn());
        fp ^= v + 0x9e3779b97f4a7c15ULL + (fp << 6) + (fp >> 2);
        sink += v;
    }
    const auto t1 = std::chrono::steady_clock::now();
    (void)sink;
    const double sec = std::chrono::duration<double>(t1 - t0).count();
    PerfSample s;
    s.scenario = scenario;
    s.path = path;
    s.iters = iters;
    s.seconds = sec;
    s.ops_per_sec = sec > 0 ? static_cast<double>(iters) / sec : 0;
    s.latency_ns = sec > 0 ? (sec * 1e9) / static_cast<double>(iters) : 0;
    s.rss_kb = ReadRssKb();
    s.peak_rss_kb = s.rss_kb > rss0 ? s.rss_kb : rss0;
    const long alloc1 = ReadAllocBytes();
    s.alloc_bytes = alloc1 > alloc0 ? alloc1 - alloc0 : 0;
    s.fingerprint = fp;
    return s;
}

inline void PrintSample(const PerfSample& s)
{
    std::printf(
        "{\"scenario\":\"%s\",\"path\":\"%s\",\"iters\":%llu,"
        "\"seconds\":%.9f,\"ops_per_sec\":%.3f,\"latency_ns\":%.3f,"
        "\"rss_kb\":%ld,\"peak_rss_kb\":%ld,\"alloc_bytes\":%ld,"
        "\"fingerprint\":\"%016llx\"}\n",
        s.scenario.c_str(), s.path.c_str(),
        static_cast<unsigned long long>(s.iters), s.seconds, s.ops_per_sec,
        s.latency_ns, s.rss_kb, s.peak_rss_kb, s.alloc_bytes,
        static_cast<unsigned long long>(s.fingerprint));
}

#endif
