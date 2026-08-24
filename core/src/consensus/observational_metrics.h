// Copyright (c) 2026 The Rigor Chain developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_CONSENSUS_OBSERVATIONAL_METRICS_H
#define BITCOIN_CONSENSUS_OBSERVATIONAL_METRICS_H

#include <consensus/amount.h>

#include <cstdint>
#include <memory>

class CBlock;
class CBlockIndex;

/**
 * Post-validation index / explorer projection (W1, W2).
 *
 * NON-CONSENSUS: Observational metric only. NEVER read during block validation
 * or script execution.
 *
 * Not a spendable balance, not circulating supply, and not an input to T, f_i,
 * mint authorization, or EvalScript. ConnectBlock writes via Update() only after
 * the block has been accepted. RPC, explorer, and runtime-supply export may read
 * the getters.
 *
 * W1: path-fold issuance ledgers (RES mint total, RFC-002 S_ORU).
 * W2: cached copy of the coinbase CVGE cumulative-convergence commitment.
 * W8: RFC-018 fee-burn has no chain cumulative here. Do not fold fee burns into
 *     GetChainOruTotal().
 *
 * No disk-format fallbacks, no missing-CVGE reconstruction, no consensus rejects.
 */
class ObservationalMetrics
{
public:
    ObservationalMetrics() = default;
    ObservationalMetrics(const ObservationalMetrics&) = default;
    ObservationalMetrics& operator=(const ObservationalMetrics&) = default;
    ObservationalMetrics(ObservationalMetrics&&) noexcept = default;
    ObservationalMetrics& operator=(ObservationalMetrics&&) noexcept = default;

    // NON-CONSENSUS: Observational metric only. NEVER read during block validation or script execution.
    CAmount GetChainResMintTotal() const { return m_chain_res_mint_total; }
    // NON-CONSENSUS: Observational metric only. NEVER read during block validation or script execution.
    CAmount GetChainOruTotal() const { return m_chain_oru_total; }
    // NON-CONSENSUS: Observational metric only. NEVER read during block validation or script execution.
    uint64_t GetCumulativeConvergence() const { return m_cumulative_convergence; }

    bool HasResMintCache() const { return m_chain_res_mint_total >= 0; }
    bool HasOruCache() const { return m_chain_oru_total >= 0; }

    /**
     * Write path after consensus validation has accepted the block.
     *
     * Ledger totals are supplied by the caller (indexer). Cumulative convergence
     * is copied from the coinbase CVGE payload when present; otherwise 0.
     */
    void Update(const CBlock& block, const CBlockIndex* pindexPrev,
                CAmount chain_res_mint_total, CAmount chain_oru_total);

    /** DisconnectBlock: drop memory-only W1 ledgers so reorgs recompute from ancestors. */
    void InvalidateLedgerCaches()
    {
        m_chain_res_mint_total = -1;
        m_chain_oru_total = -1;
    }

    /** Path-fold cache fill / test fixture. Not a consensus write path. */
    void WriteResMintCache(CAmount value) { m_chain_res_mint_total = value; }
    /** Path-fold cache fill / test fixture. Not a consensus write path. */
    void WriteOruCache(CAmount value) { m_chain_oru_total = value; }
    /** CDiskBlockIndex load and test fixture. Not a consensus write path. */
    void SetCumulativeConvergence(uint64_t value) { m_cumulative_convergence = value; }

    /** Combined fixture helper for unit tests. */
    void LoadCachesForTest(CAmount res_mint_total, CAmount oru_total, uint64_t cumulative = 0)
    {
        m_chain_res_mint_total = res_mint_total;
        m_chain_oru_total = oru_total;
        m_cumulative_convergence = cumulative;
    }

private:
    // NON-CONSENSUS: Observational metric only. NEVER read during block validation or script execution.
    CAmount m_chain_res_mint_total{-1};
    // NON-CONSENSUS: Observational metric only. NEVER read during block validation or script execution.
    CAmount m_chain_oru_total{-1};
    // NON-CONSENSUS: Observational metric only. NEVER read during block validation or script execution.
    uint64_t m_cumulative_convergence{0};
};

/**
 * Copyable unique_ptr owner so CBlockIndex's protected default copy remains valid.
 * Each index holds one heap ObservationalMetrics; copies clone the metrics.
 */
class ObservationalMetricsHandle
{
    std::unique_ptr<ObservationalMetrics> m_ptr{std::make_unique<ObservationalMetrics>()};

public:
    ObservationalMetricsHandle() = default;
    ObservationalMetricsHandle(const ObservationalMetricsHandle& other)
        : m_ptr(std::make_unique<ObservationalMetrics>(*other.m_ptr))
    {
    }
    ObservationalMetricsHandle& operator=(const ObservationalMetricsHandle& other)
    {
        if (this != &other) {
            m_ptr = std::make_unique<ObservationalMetrics>(*other.m_ptr);
        }
        return *this;
    }
    ObservationalMetricsHandle(ObservationalMetricsHandle&&) noexcept = default;
    ObservationalMetricsHandle& operator=(ObservationalMetricsHandle&&) noexcept = default;

    ObservationalMetrics& Get() { return *m_ptr; }
    const ObservationalMetrics& Get() const { return *m_ptr; }
};

#endif // BITCOIN_CONSENSUS_OBSERVATIONAL_METRICS_H
