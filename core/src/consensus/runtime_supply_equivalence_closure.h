// Copyright (c) 2026 The RigorChain developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_CONSENSUS_RUNTIME_SUPPLY_EQUIVALENCE_CLOSURE_H
#define BITCOIN_CONSENSUS_RUNTIME_SUPPLY_EQUIVALENCE_CLOSURE_H

#include <consensus/runtime_supply_coq_chas_adapter.h>
#include <consensus/runtime_supply_global_candidate_equivalence.h>
#include <consensus/runtime_supply_witness_chain.h>
#include <consensus/runtime_supply_witness_coq_adapter.h>
#include <consensus/runtime_supply_witness_equivalence_candidate.h>
#include <uint256.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

class CBlockIndex;
class UniValue;

namespace Consensus {

/** TASK-FVR-L: proof-permission boundary (does NOT prove equivalence). */
enum class EquivalenceClosureStatus {
    Open,
    PartiallyClosable,
    Closable,
    Prohibited,
};

struct EquivalenceClosureCondition {
    bool has_runtime_witness_chain{false};
    bool has_invariant_pass{false};
    bool has_candidate_equivalence_layer{false};
    bool has_global_candidate_equivalence_layer{false};
    bool has_chas_alignment_layer{false};
    bool has_coq_catalog_alignment_layer{false};
    bool no_fundamentally_blocked_draw{false};
};

struct EquivalenceClosureResult {
    EquivalenceClosureStatus status{EquivalenceClosureStatus::Open};
    bool closure_allowed{false};
    EquivalenceClosureCondition condition;
    std::vector<std::string> missing_layers;
    std::vector<std::string> active_layers;
    bool equivalence_claimed{false};
};

const char* EquivalenceClosureStatusToString(EquivalenceClosureStatus status);

bool ClosureAllowed(const EquivalenceClosureCondition& condition);

EquivalenceClosureResult BuildEquivalenceClosure(
    const std::optional<RuntimeSupplyWitness>& witness,
    const std::optional<RuntimeSupplyWitnessCoqCatalogRow>& catalog,
    const std::optional<RuntimeSupplyChasAlignmentReport>& chas,
    const std::optional<WitnessEquivalenceCandidate>& candidate,
    const std::optional<GlobalCandidateEquivalenceObservation>& global);

std::optional<EquivalenceClosureResult> ResolveEquivalenceClosureAt(const CBlockIndex* pindex);

UniValue EquivalenceClosureResultToJson(const EquivalenceClosureResult& result);

} // namespace Consensus

#endif // BITCOIN_CONSENSUS_RUNTIME_SUPPLY_EQUIVALENCE_CLOSURE_H
