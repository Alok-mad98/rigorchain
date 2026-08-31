// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// P070 optional canary telemetry. Does not write a consensus log by default.
// ENABLE_CONSENSUS_KERNEL_CANARY is the verify-only dual-path guard.
// PRODUCTION_KERNEL_SWITCHED = FALSE

#ifndef BITCOIN_CONSENSUS_KERNEL_CANARY_CANARY_METRICS_H
#define BITCOIN_CONSENSUS_KERNEL_CANARY_CANARY_METRICS_H

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <unistd.h>

struct ConsensusKernelCanaryMetrics {
    std::atomic<uint64_t> blocks_checked{0};
    std::atomic<uint64_t> comparisons{0};
    std::atomic<uint64_t> discrepancies{0};
    std::atomic<uint64_t> runtime_ns_cpp{0};
    std::atomic<uint64_t> runtime_ns_candidate{0};
    std::atomic<uint64_t> memory_usage{0};

    static ConsensusKernelCanaryMetrics& Instance()
    {
        static ConsensusKernelCanaryMetrics g;
        return g;
    }

    void Reset()
    {
        blocks_checked.store(0);
        comparisons.store(0);
        discrepancies.store(0);
        runtime_ns_cpp.store(0);
        runtime_ns_candidate.store(0);
        memory_usage.store(0);
    }

    double runtime_ms_cpp() const { return static_cast<double>(runtime_ns_cpp.load()) / 1.0e6; }
    double runtime_ms_candidate() const { return static_cast<double>(runtime_ns_candidate.load()) / 1.0e6; }

    static uint64_t SampleRssBytes()
    {
        FILE* f = std::fopen("/proc/self/statm", "r");
        if (!f) return 0;
        unsigned long size = 0, rss = 0;
        const int n = std::fscanf(f, "%lu %lu", &size, &rss);
        std::fclose(f);
        if (n != 2) return 0;
        long page = sysconf(_SC_PAGESIZE);
        if (page <= 0) page = 4096;
        return rss * static_cast<uint64_t>(page);
    }
};

#endif
