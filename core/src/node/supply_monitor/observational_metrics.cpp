// Copyright (c) 2026 The Rigor Chain developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <node/supply_monitor/observational_metrics.h>

#include <chain.h>
#include <consensus/convergenceres.h>
#include <consensus/params.h>
#include <consensus/rigor_consensus_lock.h>
#include <consensus/rigor_issuance.h>
#include <primitives/block.h>

#include <vector>

void ObservationalMetrics::Update(const CBlock& block, const CBlockIndex* pindexPrev,
                                  std::optional<CAmount> chain_res_mint_total,
                                  std::optional<CAmount> chain_oru_total)
{
    // NON-CONSENSUS: Observational metric only. NEVER read during block validation or script evaluation.
    m_chain_res_mint_total = chain_res_mint_total.value_or(-1);
    m_chain_oru_total = chain_oru_total.value_or(-1);

    if (pindexPrev == nullptr || block.vtx.empty()) {
        m_cumulative_convergence = 0;
        return;
    }

    uint64_t block_new_convergence = 0;
    for (size_t i = 1; i < block.vtx.size(); ++i) {
        block_new_convergence += Consensus::GetTransactionConvergenceAmount(*block.vtx[i]);
    }
    // Parent's Observational prefix is written at its own ConnectBlock. Recurrence is
    // O(1); walking pprev here was Θ(H) per block.
    m_cumulative_convergence = pindexPrev->Observational().GetCumulativeConvergence() + block_new_convergence;
}

ObservationalFoldResult FoldChainResMintTotal(const CBlockIndex* tip)
{
    if (tip == nullptr) return ObservationalFoldResult::Ok(0);

    std::vector<const CBlockIndex*> pending;
    const CBlockIndex* cursor = tip;
    while (cursor != nullptr && !cursor->Observational().HasResMintCache()) {
        pending.push_back(cursor);
        cursor = cursor->pprev;
    }

    CAmount running = (cursor != nullptr) ? cursor->Observational().GetChainResMintTotal() : 0;
    if (running < 0 || running > Consensus::RES_MAX_SUPPLY) {
        return ObservationalFoldResult::Invalid();
    }

    for (auto it = pending.rbegin(); it != pending.rend(); ++it) {
        const CBlockIndex* block = *it;
        if (block->m_block_res_mint_sum < 0 || block->m_block_res_mint_sum > Consensus::RES_MAX_SUPPLY) {
            return ObservationalFoldResult::Invalid();
        }
        if (running > Consensus::RES_MAX_SUPPLY - block->m_block_res_mint_sum) {
            return ObservationalFoldResult::Invalid();
        }
        running += block->m_block_res_mint_sum;
        block->Observational().WriteResMintCache(running);
    }
    return ObservationalFoldResult::Ok(running);
}

ObservationalFoldResult FoldChainOruTotal(const CBlockIndex* tip, const Consensus::Params& consensus)
{
    if (tip == nullptr) return ObservationalFoldResult::Ok(0);

    std::vector<const CBlockIndex*> pending;
    const CBlockIndex* cursor = tip;
    while (cursor != nullptr && !cursor->Observational().HasOruCache()) {
        pending.push_back(cursor);
        cursor = cursor->pprev;
    }

    CAmount running = (cursor != nullptr) ? cursor->Observational().GetChainOruTotal() : 0;
    if (running < 0) return ObservationalFoldResult::Invalid();

    for (auto it = pending.rbegin(); it != pending.rend(); ++it) {
        const CBlockIndex* block = *it;
        const CAmount subsidy = GetBlockSubsidy(block->nHeight, consensus);
        const CAmount converge = block->m_block_converge_sum;
        if (converge < 0) return ObservationalFoldResult::Invalid();
        running = running + subsidy - converge;
        if (running < 0) return ObservationalFoldResult::Invalid();
        block->Observational().WriteOruCache(running);
    }
    return ObservationalFoldResult::Ok(running);
}
