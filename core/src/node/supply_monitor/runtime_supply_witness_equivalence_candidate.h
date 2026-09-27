// Copyright (c) 2026 The RigorChain developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_NODE_SUPPLY_MONITOR_RUNTIME_SUPPLY_WITNESS_EQUIVALENCE_CANDIDATE_H
#define BITCOIN_NODE_SUPPLY_MONITOR_RUNTIME_SUPPLY_WITNESS_EQUIVALENCE_CANDIDATE_H

#include <node/supply_monitor/runtime_supply_coq_chas_adapter.h>
#include <node/supply_monitor/runtime_supply_witness_coq_adapter.h>
#include <node/supply_monitor/runtime_supply_witness_chain.h>
#include <uint256.h>

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

class CBlockIndex;
class UniValue;

namespace Consensus {

/**
 * candidate-equivalence status (theoretical comparability only).
 *
 * Does NOT assert Coq ≡ Runtime. Identifies which witness fields could
 * theoretically be compared in a future proof attempt.
 */
enum class CandidateEquivalenceStatus {
    Comparable,
    PartiallyComparable,
    NotComparable,
    FundamentallyBlocked,
};

const char* CandidateEquivalenceStatusToString(CandidateEquivalenceStatus status);

/**
 * Per-block candidate-equivalence record from runtime witness + Coq catalog + CHAS.
 *
 * `equivalence_claimed` is always false in this phase.
 */
struct WitnessEquivalenceCandidate {
    int32_t height{-1};
    uint256 block_hash;

    CandidateEquivalenceStatus oru_supply_status{CandidateEquivalenceStatus::NotComparable};
    CandidateEquivalenceStatus res_supply_status{CandidateEquivalenceStatus::NotComparable};
    CandidateEquivalenceStatus invariant_status{CandidateEquivalenceStatus::FundamentallyBlocked};
    CandidateEquivalenceStatus witness_chain_status{CandidateEquivalenceStatus::NotComparable};

    std::vector<std::string> blocking_reasons;

    bool equivalence_claimed{false};
};

/** Build candidate from FVR-F witness + FVR-G catalog row + FVR-H CHAS report. */
WitnessEquivalenceCandidate BuildWitnessEquivalenceCandidate(
    const RuntimeSupplyWitness& witness,
    const RuntimeSupplyWitnessCoqCatalogRow& catalog,
    const RuntimeSupplyChasAlignmentReport& chas);

/** Resolve candidate at `pindex` (in-memory witnesses + mirrors only). */
std::optional<WitnessEquivalenceCandidate> ResolveWitnessEquivalenceCandidateAt(const CBlockIndex* pindex);

/** JSON for equivalence-candidate RPCs. */
UniValue WitnessEquivalenceCandidateToJson(const WitnessEquivalenceCandidate& candidate);

/** FVR-J: full export bundle for Coq audit scripts (witness + catalog + CHAS + candidate). */
struct CoqEquivalenceCandidateExport {
    WitnessEquivalenceCandidate candidate;
    RuntimeSupplyWitness witness;
    RuntimeSupplyWitnessCoqCatalogRow catalog;
    RuntimeSupplyChasAlignmentReport chas;
};

std::optional<CoqEquivalenceCandidateExport> ResolveCoqEquivalenceCandidateExportAt(const CBlockIndex* pindex);

/** Coq-readable JSON (mirrors FVR-I + formal layer field names for ). */
UniValue CoqEquivalenceCandidateExportToJson(const CoqEquivalenceCandidateExport& exp);

} // namespace Consensus

#endif // BITCOIN_NODE_SUPPLY_MONITOR_RUNTIME_SUPPLY_WITNESS_EQUIVALENCE_CANDIDATE_H
