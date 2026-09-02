// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// P071 mainnet canary monitor. Compare-only. ENABLE_CONSENSUS_KERNEL_CANARY
// remains the verify-only dual-path guard for KernelCanaryExecutor.
// Never mutates block accept/reject. EXTRACTED_KERNEL_PRODUCTION = FALSE; production authority is RigorConsensusKernel(C++)

#ifndef BITCOIN_CONSENSUS_KERNEL_CANARY_MAINNETCANARYMONITOR_H
#define BITCOIN_CONSENSUS_KERNEL_CANARY_MAINNETCANARYMONITOR_H

#include <consensus/kernel_canary/canary_controller.h>
#include <consensus/kernel_canary/canary_health.h>
#include <consensus/kernel_canary/canary_metrics.h>

#include <atomic>
#include <cstdint>
#include <string>

inline constexpr const char* KERNEL_MAINNET_CANARY_NOT_ARMED = "MAINNET_CANARY_NOT_ARMED";
inline constexpr const char* KERNEL_MAINNET_CANARY_DISCREPANCY = "CONSENSUS_KERNEL_MAINNET_CANARY_DISCREPANCY";
/** Production never auto-injects. Tests call FaultInject* explicitly. */
inline constexpr bool CANARY_FAULT_INJECTION_TEST = false;
inline constexpr int MAINNET_CANARY_LONGRUN_BLOCKS = 100000;

class MainnetCanaryMonitor
{
    std::atomic<bool> m_started{false};
    std::atomic<bool> m_enabled{false};
    std::atomic<uint64_t> m_blocks{0};
    std::atomic<uint64_t> m_compares{0};
    std::atomic<uint64_t> m_discrepancies{0};
    std::atomic<uint64_t> m_runtime_ns_cpp{0};
    std::atomic<uint64_t> m_runtime_ns_candidate{0};
    std::atomic<uint64_t> m_rss_start{0};
    std::atomic<uint64_t> m_rss_end{0};
    std::atomic<uint64_t> m_rss_peak{0};
    std::atomic<int> m_start_height{0};
    std::atomic<int> m_end_height{0};
    std::atomic<uint64_t> m_rollback_events{0};
    std::atomic<bool> m_have_height{false};

public:
    static MainnetCanaryMonitor& Instance()
    {
        static MainnetCanaryMonitor g;
        return g;
    }

    bool Start()
    {
        if (!PRODUCTION_MAINNET_CANARY_ARMED) return false;
        m_started.store(true);
        m_enabled.store(true);
        ConsensusKernelCanaryHealth::Instance().SetActive();
        const uint64_t rss = ConsensusKernelCanaryMetrics::SampleRssBytes();
        m_rss_start.store(rss);
        m_rss_peak.store(rss);
        m_rss_end.store(rss);
        return true;
    }

    void Stop()
    {
        m_enabled.store(false);
        m_started.store(false);
        ConsensusKernelCanaryHealth::Instance().SetPaused();
    }

    void Pause()
    {
        m_enabled.store(false);
        ConsensusKernelCanaryHealth::Instance().SetPaused();
    }

    void DisableOnFailure()
    {
        m_enabled.store(false);
        m_rollback_events.fetch_add(1);
        ConsensusKernelCanaryHealth::Instance().SetDisabled();
        ConsensusKernelCanaryController::Instance().Disable(KERNEL_MAINNET_CANARY_DISCREPANCY);
    }

    ConsensusKernelCanaryHealthState Health() const { return ConsensusKernelCanaryHealth::Instance().State(); }
    const char* HealthStatus() const { return ConsensusKernelCanaryHealth::Instance().Status(); }

    const char* Status() const
    {
        if (!m_started.load()) return "OFF";
        if (!m_enabled.load()) return "DISABLED";
        return "ON";
    }

    bool IsStarted() const { return m_started.load(); }
    bool enabled() const { return m_enabled.load(); }
    uint64_t blocks_checked() const { return m_blocks.load(); }
    uint64_t comparison_count() const { return m_compares.load(); }
    uint64_t discrepancy_count() const { return m_discrepancies.load(); }

    uint64_t mainnet_canary_blocks_total() const { return m_blocks.load(); }
    uint64_t mainnet_canary_compare_total() const { return m_compares.load(); }
    uint64_t mainnet_canary_discrepancy_total() const { return m_discrepancies.load(); }
    double mainnet_canary_runtime_cpp() const { return static_cast<double>(m_runtime_ns_cpp.load()) / 1.0e6; }
    double mainnet_canary_runtime_candidate() const
    {
        return static_cast<double>(m_runtime_ns_candidate.load()) / 1.0e6;
    }

    void NoteCompare(uint64_t cpp_ns, uint64_t cand_ns, bool same, int height = 0)
    {
        m_blocks.fetch_add(1);
        m_compares.fetch_add(1);
        m_runtime_ns_cpp.fetch_add(cpp_ns);
        m_runtime_ns_candidate.fetch_add(cand_ns);
        if (!m_have_height.load()) {
            m_start_height.store(height);
            m_have_height.store(true);
        }
        m_end_height.store(height);
        const uint64_t rss = ConsensusKernelCanaryMetrics::SampleRssBytes();
        m_rss_end.store(rss);
        uint64_t peak = m_rss_peak.load();
        while (rss > peak && !m_rss_peak.compare_exchange_weak(peak, rss)) {}
        if (!same) {
            m_discrepancies.fetch_add(1);
            DisableOnFailure();
        }
    }

    void InjectDiscrepancyForTest(int height, const std::string& block_hash, const std::string& txid,
        const std::string& cpp_result, const std::string& candidate_result, const char* authority_name)
    {
        ConsensusKernelCanaryController::Instance().OnDiscrepancy(
            height, block_hash, txid, cpp_result, candidate_result, authority_name,
            KERNEL_MAINNET_CANARY_DISCREPANCY);
        m_discrepancies.fetch_add(1);
        DisableOnFailure();
    }

    /** Controlled test-only faults. Never accepts the candidate. */
    bool FaultInject(const char* authority_name)
    {
        (void)CANARY_FAULT_INJECTION_TEST;
        InjectDiscrepancyForTest(101, "fault-hash", "fault-txid", "cpp", "candidate-mismatch", authority_name);
        return !enabled() && Health() == ConsensusKernelCanaryHealthState::DISABLED
            && discrepancy_count() >= 1;
    }

    bool RunSoak(int blocks, std::string& err);
    bool ExportObservation(const std::string& path) const;

    uint64_t rss_delta() const
    {
        const uint64_t a = m_rss_start.load();
        const uint64_t b = m_rss_end.load();
        return b > a ? b - a : 0;
    }
    uint64_t memory_growth() const
    {
        const uint64_t a = m_rss_start.load();
        const uint64_t b = m_rss_peak.load();
        return b > a ? b - a : 0;
    }
    int start_height() const { return m_start_height.load(); }
    int end_height() const { return m_end_height.load(); }
    uint64_t rollback_events() const { return m_rollback_events.load(); }

    void Reset()
    {
        m_started.store(false);
        m_enabled.store(false);
        m_blocks.store(0);
        m_compares.store(0);
        m_discrepancies.store(0);
        m_runtime_ns_cpp.store(0);
        m_runtime_ns_candidate.store(0);
        m_rss_start.store(0);
        m_rss_end.store(0);
        m_rss_peak.store(0);
        m_start_height.store(0);
        m_end_height.store(0);
        m_rollback_events.store(0);
        m_have_height.store(false);
        ConsensusKernelCanaryHealth::Instance().Reset();
    }
};

#endif
