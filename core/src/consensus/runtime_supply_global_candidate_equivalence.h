// Copyright (c) 2026 The RigorChain developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_CONSENSUS_RUNTIME_SUPPLY_GLOBAL_CANDIDATE_EQUIVALENCE_H
#define BITCOIN_CONSENSUS_RUNTIME_SUPPLY_GLOBAL_CANDIDATE_EQUIVALENCE_H

#include <consensus/runtime_supply_coq_chas_adapter.h>
#include <consensus/runtime_supply_witness_equivalence_candidate.h>
#include <uint256.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

class CBlockIndex;
class UniValue;

namespace Consensus {

/**
 * TASK-FVR-K: global aggregation of CHAS (FVR-H) + candidate (FVR-I/J).
 *
 * Per-block theoretical equivalence potential; `equivalence_claimed` always false.
 */
struct GlobalCandidateEquivalenceObservation {
    int32_t height{-1};
    uint256 block_hash;

    CandidateEquivalenceStatus oru_global_status{CandidateEquivalenceStatus::NotComparable};
    CandidateEquivalenceStatus res_global_status{CandidateEquivalenceStatus::NotComparable};

    std::vector<std::string> blocking_reasons;

    bool equivalence_claimed{false};
};

GlobalCandidateEquivalenceObservation BuildGlobalCandidateEquivalence(
    const RuntimeSupplyChasAlignmentReport& chas,
    const WitnessEquivalenceCandidate& candidate);

std::optional<GlobalCandidateEquivalenceObservation> ResolveGlobalCandidateEquivalenceAt(
    const CBlockIndex* pindex);

struct GlobalCandidateEquivalenceExport {
    GlobalCandidateEquivalenceObservation global;
    RuntimeSupplyChasAlignmentReport chas;
    WitnessEquivalenceCandidate candidate;
};

std::optional<GlobalCandidateEquivalenceExport> ResolveGlobalCandidateEquivalenceExportAt(
    const CBlockIndex* pindex);

UniValue GlobalCandidateEquivalenceExportToJson(const GlobalCandidateEquivalenceExport& exp);

} // namespace Consensus

#endif // BITCOIN_CONSENSUS_RUNTIME_SUPPLY_GLOBAL_CANDIDATE_EQUIVALENCE_H
