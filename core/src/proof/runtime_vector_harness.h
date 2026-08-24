// Copyright (c) 2026 The RigorChain developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_PROOF_RUNTIME_VECTOR_HARNESS_H
#define BITCOIN_PROOF_RUNTIME_VECTOR_HARNESS_H

#include <consensus/amount.h>
#include <consensus/runtime_supply_export.h>
#include <consensus/runtime_supply_invariant_checker.h>
#include <consensus/runtime_supply_witness_chain.h>


#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

class CBlockIndex;
class CCoinsViewCache;
class UniValue;

namespace Consensus {
struct Params;
} // namespace Consensus

namespace proof {

/** TASK-PROOF-007: Golden vector families (PROOF-000 registry). */
enum class GoldenVectorFamily : uint8_t {
    ORU,
    RES,
    DRAW,
    STATE,
};

/** Verification status (PROOF-005 §3). */
enum class GvrVerificationStatus : uint8_t {
    NOT_EXECUTED,
    PASS,
    FAIL,
    PARTIAL,
};

/** Per-field comparison outcome (PROOF-005 §7). */
enum class FieldCompareResult : uint8_t {
    MATCH,
    MISMATCH,
    OPTIONAL_MISS,
    SKIPPED,
};

/** Normative expected values for one golden vector (PROOF-001..004). */
struct GoldenVectorSpec {
    const char* id;
    GoldenVectorFamily family;
    int32_t height;
    /** When false, harness returns GVR_NOT_EXECUTED until fixture chain is wired. */
    bool fixture_materialized{false};
    /** GV-DRAW-0003: block must reject; no post-connect witness expected. */
    bool reject_expected{false};
    bool draw_fields_optional{false};

    bool expect_height{false};
    bool expect_oru_supply{false};
    CAmount expected_oru_supply{0};
    bool expect_oru_parent{false};
    CAmount expected_oru_parent{0};
    bool expect_oru_subsidy{false};
    CAmount expected_oru_subsidy{0};
    bool expect_oru_burn{false};
    CAmount expected_oru_burn{0};

    bool expect_res_supply{false};
    CAmount expected_res_supply{0};
    bool expect_res_parent{false};
    CAmount expected_res_parent{0};
    bool expect_res_mint{false};
    CAmount expected_res_mint{0};
    bool expect_res_mint_count{false};
    uint8_t expected_res_mint_count{0};
};

struct VectorFieldResult {
    std::string category;
    std::string field;
    std::string expected;
    std::string witness;
    FieldCompareResult result{FieldCompareResult::SKIPPED};
    std::string source;
    bool required{true};
};

struct VectorVerificationReport {
    std::string vector_id;
    std::string vector_status{"DRAFT"};
    GvrVerificationStatus verification_status{GvrVerificationStatus::NOT_EXECUTED};
    int mismatch_count{0};
    int checked_fields{0};
    std::vector<std::string> failed_fields;
    bool runtime_witness_present{false};
    std::vector<std::string> notes;
    std::vector<VectorFieldResult> field_results;
};

/**
 * Read-only witness bundle for harness comparison (FVR-B/C/E/F).
 * Does not modify consensus or FVR layers.
 */
struct RuntimeWitnessSnapshot {
    bool present{false};
    Consensus::RuntimeSupplyExport export_data{};
    std::optional<Consensus::RuntimeSupplyWitness> witness;
    std::optional<Consensus::RuntimeInvariantResult> invariants;
    bool fvr_catalog_row{false};
};

inline constexpr size_t GOLDEN_VECTOR_COUNT = 12;

const char* GvrVerificationStatusToString(GvrVerificationStatus status);
const char* FieldCompareResultToString(FieldCompareResult result);

/** Registry access (12 entries: GV-ORU/RES/DRAW/STATE × 0001..0003). */
const GoldenVectorSpec& GetGoldenVectorSpec(size_t index);
std::optional<size_t> FindGoldenVectorIndex(const std::string& vector_id);
const GoldenVectorSpec* FindGoldenVectorSpec(const std::string& vector_id);

/**
 * Compare vector expectations against observational runtime witness.
 * Vectors remain authoritative; witness disagreement does not amend vectors.
 */
VectorVerificationReport EvaluateGoldenVector(const GoldenVectorSpec& spec,
                                              const RuntimeWitnessSnapshot& snapshot);

/** Capture FVR export + witness + invariants at pindex (read-only). */
RuntimeWitnessSnapshot CaptureRuntimeWitnessSnapshot(const CBlockIndex* pindex,
                                                     const CCoinsViewCache& view,
                                                     const Consensus::Params& consensus);

/**
 * Evaluate using chain block at vector height when fixture is materialized.
 * Without materialized fixture or pindex, returns GVR_NOT_EXECUTED.
 *
 * The observational mirror this reads MUST come from chain state. There used to be a
 * `materializegoldenvector` RPC that built the mirror out of the vector's own expected values,
 * wrote them into ObservationalMetrics (W1 ledgers) and friends, stored the result as the block's
 * mirror, and then ran this comparison — so every field compared a number against itself and no
 * vector could fail. Worse, the poisoned mirror persisted, so an honest `getruntimevectorstatus`
 * afterwards reported PASS too. It has been deleted; do not reintroduce a path that lets the
 * expectation supply the observation.
 */
VectorVerificationReport EvaluateGoldenVectorOnChain(const GoldenVectorSpec& spec,
                                                     const CBlockIndex* pindex_at_height,
                                                     const CCoinsViewCache& view,
                                                     const Consensus::Params& consensus);

std::vector<VectorVerificationReport> EvaluateAllGoldenVectors(
    const CBlockIndex* (*resolve_height)(int32_t height) /*nullable*/,
    const CCoinsViewCache* view,
    const Consensus::Params* consensus);

UniValue VectorVerificationReportToJson(const VectorVerificationReport& report);
UniValue GoldenVectorBatchReportToJson(const std::vector<VectorVerificationReport>& reports);

/** Unit tests only: overlay fixture_materialized without changing normative registry. */
void SetGoldenVectorFixtureMaterializedForTest(size_t index, bool materialized);
void ResetGoldenVectorFixtureFlagsForTest();

/**
 * Report status: ESTABLISHED when the vector table declares a fixture for this vector.
 *
 * This used to also consult a runtime registry that `materializegoldenvector` wrote, which meant
 * an operator could promote a vector to ESTABLISHED by calling an RPC. See the comment on
 * EvaluateGoldenVectorOnChain for why that RPC is gone.
 */
const char* GoldenVectorDocumentStatus(size_t index);

/** RPC: when true, optional FVR-F/G witness rows become required. */
void SetGoldenVectorVerifyFvrForRpc(bool verify_fvr);

} // namespace proof

#endif // BITCOIN_PROOF_RUNTIME_VECTOR_HARNESS_H
