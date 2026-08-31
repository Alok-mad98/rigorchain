// Copyright (c) 2026 The Rigor Chain developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <node/supply_monitor/observational_metrics.h>

#include <consensus/convergence_logic.h>
#include <primitives/block.h>
#include <chain.h>

void ObservationalMetrics::Update(const CBlock& block, const CBlockIndex* pindexPrev,
                                  CAmount chain_res_mint_total, CAmount chain_oru_total)
{
    // NON-CONSENSUS: Observational metric only. NEVER read during block validation or script evaluation.
    m_chain_res_mint_total = chain_res_mint_total;
    m_chain_oru_total = chain_oru_total;

    if (pindexPrev == nullptr || block.vtx.empty()) {
        m_cumulative_convergence = 0;
        return;
    }

    uint64_t block_new_convergence = 0;
    for (size_t i = 1; i < block.vtx.size(); ++i) {
        block_new_convergence += GetTransactionConvergenceAmount(*block.vtx[i]);
    }
    // Parent's Observational prefix is written at its own ConnectBlock. Recurrence is
    // O(1); walking pprev here was Θ(H) per block.
    m_cumulative_convergence = pindexPrev->Observational().GetCumulativeConvergence() + block_new_convergence;
}
