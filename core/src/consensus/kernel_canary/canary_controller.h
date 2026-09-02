// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// P070 production-canary readiness controller. Compare-control only.
// ENABLE_CONSENSUS_KERNEL_CANARY remains the verify-only dual-path guard
// for KernelCanaryExecutor. This file does not recode authorities.
// EXTRACTED_KERNEL_PRODUCTION = FALSE; production authority is RigorConsensusKernel(C++)
// CANARY_AVAILABLE = TRUE
// CANARY_ENABLED default FALSE

#ifndef BITCOIN_CONSENSUS_KERNEL_CANARY_CANARY_CONTROLLER_H
#define BITCOIN_CONSENSUS_KERNEL_CANARY_CANARY_CONTROLLER_H

#include <atomic>
#include <mutex>
#include <string>

inline constexpr bool CANARY_AVAILABLE = true;
inline constexpr bool CANARY_ENABLED_DEFAULT = false;
/** Mainnet compare-only is not armed. Test-only canary, test chains only. */
inline constexpr bool PRODUCTION_MAINNET_CANARY_ARMED = false;

inline constexpr const char* KERNEL_CANARY_OPERATOR_EXPLICIT_REQUIRED = "OPERATOR_EXPLICIT_REQUIRED";
inline constexpr const char* KERNEL_CANARY_ROLLBACK_DISCREPANCY = "CONSENSUS_KERNEL_CANARY_RUNTIME_DISCREPANCY";
inline constexpr const char* KERNEL_CANARY_MAINNET_NOT_ARMED = "MAINNET_CANARY_NOT_ARMED";

/**
 * Rollout latch. Never changes block accept/reject.
 * Enable requires an explicit operator action. Disable stops compare.
 */
class ConsensusKernelCanaryController
{
    std::atomic<bool> m_compare_allowed{true};
    std::atomic<bool> m_operator_on{false};
    mutable std::mutex m_mu;
    std::string m_status{"OFF"};
    std::string m_last_reason;
    int m_last_height{0};
    std::string m_last_block_hash;
    std::string m_last_txid;
    std::string m_last_cpp;
    std::string m_last_candidate;
    std::string m_last_authority;

public:
    static ConsensusKernelCanaryController& Instance()
    {
        static ConsensusKernelCanaryController g;
        return g;
    }

    bool Available() const { return CANARY_AVAILABLE; }

    /** Operator-armed AND not latched off. Production default is FALSE. */
    bool Enabled() const { return m_operator_on.load() && m_compare_allowed.load(); }

    /** Discrepancy latch. Default true so testnet flag-only compare can run. */
    bool CompareAllowed() const { return m_compare_allowed.load(); }

    const char* Status() const
    {
        if (!m_compare_allowed.load()) return "DISABLED";
        if (m_operator_on.load()) return "ON";
        return "OFF";
    }

    std::string LastReason() const
    {
        std::lock_guard<std::mutex> lock(m_mu);
        return m_last_reason;
    }

    bool Enable(bool operator_explicit, bool mainnet, std::string& err)
    {
        err.clear();
        if (!operator_explicit) {
            err = KERNEL_CANARY_OPERATOR_EXPLICIT_REQUIRED;
            return false;
        }
        if (mainnet && !PRODUCTION_MAINNET_CANARY_ARMED) {
            err = KERNEL_CANARY_MAINNET_NOT_ARMED;
            return false;
        }
        m_operator_on.store(true);
        m_compare_allowed.store(true);
        std::lock_guard<std::mutex> lock(m_mu);
        m_status = "ON";
        m_last_reason.clear();
        return true;
    }

    void Disable(const char* reason)
    {
        m_operator_on.store(false);
        m_compare_allowed.store(false);
        std::lock_guard<std::mutex> lock(m_mu);
        m_status = "DISABLED";
        m_last_reason = reason ? reason : "disable";
    }

    void OnDiscrepancy(int height, const std::string& block_hash, const std::string& txid,
        const std::string& cpp_result, const std::string& candidate_result,
        const char* authority_name = nullptr, const char* token = nullptr)
    {
        {
            std::lock_guard<std::mutex> lock(m_mu);
            m_last_height = height;
            m_last_block_hash = block_hash;
            m_last_txid = txid;
            m_last_cpp = cpp_result;
            m_last_candidate = candidate_result;
            m_last_authority = authority_name ? authority_name : "";
        }
        Disable(token ? token : KERNEL_CANARY_ROLLBACK_DISCREPANCY);
    }

    int LastHeight() const
    {
        std::lock_guard<std::mutex> lock(m_mu);
        return m_last_height;
    }
    std::string LastBlockHash() const
    {
        std::lock_guard<std::mutex> lock(m_mu);
        return m_last_block_hash;
    }
    std::string LastTxid() const
    {
        std::lock_guard<std::mutex> lock(m_mu);
        return m_last_txid;
    }
    std::string LastCppResult() const
    {
        std::lock_guard<std::mutex> lock(m_mu);
        return m_last_cpp;
    }
    std::string LastCandidateResult() const
    {
        std::lock_guard<std::mutex> lock(m_mu);
        return m_last_candidate;
    }
    std::string LastAuthority() const
    {
        std::lock_guard<std::mutex> lock(m_mu);
        return m_last_authority;
    }

    void Reset()
    {
        m_operator_on.store(false);
        m_compare_allowed.store(true);
        std::lock_guard<std::mutex> lock(m_mu);
        m_status = "OFF";
        m_last_reason.clear();
        m_last_height = 0;
        m_last_block_hash.clear();
        m_last_txid.clear();
        m_last_cpp.clear();
        m_last_candidate.clear();
        m_last_authority.clear();
    }
};

#endif
