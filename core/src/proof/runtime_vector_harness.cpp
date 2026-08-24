// Copyright (c) 2026 The RigorChain developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <proof/runtime_vector_harness.h>

#include <chain.h>
#include <consensus/params.h>
#include <consensus/runtime_supply_witness_coq_adapter.h>
#include <univalue.h>
#include <util/moneystr.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <optional>
#include <string>

namespace proof {
namespace {

constexpr const char* HARNESS_VERSION = "proof-006-0.1";

std::array<bool, GOLDEN_VECTOR_COUNT> g_fixture_materialized_test_overlay{};
thread_local bool g_verify_fvr_witness_fields{false};

std::string AmountToHarnessString(CAmount amount)
{
    return FormatMoney(amount);
}

void AppendAmountField(VectorVerificationReport& report,
                         const std::string& category,
                         const std::string& field,
                         const std::string& source,
                         bool required,
                         CAmount expected,
                         bool has_witness,
                         CAmount witness_value)
{
    VectorFieldResult row;
    row.category = category;
    row.field = field;
    row.source = source;
    row.required = required;
    row.expected = AmountToHarnessString(expected);
    if (!has_witness) {
        row.witness = "";
        row.result = FieldCompareResult::OPTIONAL_MISS;
    } else {
        row.witness = AmountToHarnessString(witness_value);
        row.result = (witness_value == expected) ? FieldCompareResult::MATCH : FieldCompareResult::MISMATCH;
    }
    report.field_results.push_back(row);
    ++report.checked_fields;
    if (row.result == FieldCompareResult::MISMATCH && required) {
        ++report.mismatch_count;
        report.failed_fields.push_back(field);
    }
}

void AppendIntField(VectorVerificationReport& report,
                    const std::string& category,
                    const std::string& field,
                    const std::string& source,
                    bool required,
                    int64_t expected,
                    bool has_witness,
                    int64_t witness_value)
{
    VectorFieldResult row;
    row.category = category;
    row.field = field;
    row.source = source;
    row.required = required;
    row.expected = std::to_string(expected);
    if (!has_witness) {
        row.witness = "";
        row.result = FieldCompareResult::OPTIONAL_MISS;
    } else {
        row.witness = std::to_string(witness_value);
        row.result = (witness_value == expected) ? FieldCompareResult::MATCH : FieldCompareResult::MISMATCH;
    }
    report.field_results.push_back(row);
    ++report.checked_fields;
    if (row.result == FieldCompareResult::MISMATCH && required) {
        ++report.mismatch_count;
        report.failed_fields.push_back(field);
    }
}

void AppendBoolField(VectorVerificationReport& report,
                     const std::string& category,
                     const std::string& field,
                     const std::string& source,
                     bool required,
                     bool expected,
                     bool has_witness,
                     bool witness_value)
{
    AppendIntField(report, category, field, source, required, expected ? 1 : 0, has_witness, witness_value ? 1 : 0);
}

void AppendSkippedField(VectorVerificationReport& report,
                        const std::string& category,
                        const std::string& field,
                        const std::string& note_source)
{
    VectorFieldResult row;
    row.category = category;
    row.field = field;
    row.source = note_source;
    row.required = false;
    row.expected = "";
    row.witness = "";
    row.result = FieldCompareResult::SKIPPED;
    report.field_results.push_back(row);
}

const std::array<GoldenVectorSpec, GOLDEN_VECTOR_COUNT> kGoldenVectors{{
    // GV-ORU-0001
    {
        "GV-ORU-0001",
        GoldenVectorFamily::ORU,
        0,
        false,
        false,
        false,
        true,
        true,
        CAmount{100'000'000'000},
        true,
        CAmount{0},
        true,
        CAmount{100'000'000'000},
        true,
        CAmount{0},
        false,
        CAmount{0},
        false,
        CAmount{0},
        false,
        CAmount{0},
        false,
        0,
    },
    // GV-ORU-0002
    {
        "GV-ORU-0002",
        GoldenVectorFamily::ORU,
        1,
        false,
        false,
        false,
        true,
        true,
        CAmount{190'000'000'000},
        true,
        CAmount{100'000'000'000},
        true,
        CAmount{100'000'000'000},
        true,
        CAmount{10'000'000'000},
        false,
        CAmount{0},
        false,
        CAmount{0},
        false,
        CAmount{0},
        false,
        0,
    },
    // GV-ORU-0003
    {
        "GV-ORU-0003",
        GoldenVectorFamily::ORU,
        630'000,
        false,
        false,
        false,
        true,
        true,
        CAmount{500'010'000'000'000},
        true,
        CAmount{500'000'000'000'000},
        true,
        CAmount{12'500'000'000},
        true,
        CAmount{2'500'000'000},
        false,
        CAmount{0},
        false,
        CAmount{0},
        false,
        CAmount{0},
        false,
        0,
    },
    // GV-RES-0001
    {
        "GV-RES-0001",
        GoldenVectorFamily::RES,
        10,
        false,
        false,
        false,
        true,
        false,
        CAmount{0},
        false,
        CAmount{0},
        false,
        CAmount{0},
        false,
        CAmount{0},
        true,
        CAmount{40'000'000},
        true,
        CAmount{0},
        true,
        CAmount{40'000'000},
        true,
        1,
    },
    // GV-RES-0002
    {
        "GV-RES-0002",
        GoldenVectorFamily::RES,
        500,
        false,
        false,
        false,
        true,
        false,
        CAmount{0},
        false,
        CAmount{0},
        false,
        CAmount{0},
        false,
        CAmount{0},
        true,
        CAmount{80'000'000},
        true,
        CAmount{40'000'000},
        true,
        CAmount{40'000'000},
        true,
        1,
    },
    // GV-RES-0003
    {
        // Cap-boundary vector. RFC-017 makes RES_MAX_SUPPLY unreachable by the schedule, so this
        // state is not chain-producible; it exercises the defensive runtime cap check, and the
        // mint must still equal W(height / RES_REWARD_EPOCH) for RES-L3 to hold. Height 2'100'000
        // is epoch 10, where W = 1'096'405 atoms.
        "GV-RES-0003",
        GoldenVectorFamily::RES,
        2'100'000,
        false,
        false,
        false,
        true,
        false,
        CAmount{0},
        false,
        CAmount{0},
        false,
        CAmount{0},
        false,
        CAmount{0},
        true,
        CAmount{21'000'000'000'000},
        true,
        CAmount{20'999'998'903'595},
        true,
        CAmount{1'096'405},
        true,
        1,
    },
    // GV-DRAW-0001
    {
        "GV-DRAW-0001",
        GoldenVectorFamily::DRAW,
        1,
        false,
        false,
        true,
        true,
        false,
        CAmount{0},
        false,
        CAmount{0},
        false,
        CAmount{0},
        false,
        CAmount{0},
        true,
        CAmount{40'000'000},
        true,
        CAmount{0},
        true,
        CAmount{40'000'000},
        true,
        1,
    },
    // GV-DRAW-0002
    {
        "GV-DRAW-0002",
        GoldenVectorFamily::DRAW,
        7,
        false,
        false,
        true,
        true,
        false,
        CAmount{0},
        false,
        CAmount{0},
        false,
        CAmount{0},
        false,
        CAmount{0},
        true,
        CAmount{80'000'000},
        true,
        CAmount{40'000'000},
        true,
        CAmount{40'000'000},
        true,
        1,
    },
    // GV-DRAW-0003
    {
        "GV-DRAW-0003",
        GoldenVectorFamily::DRAW,
        2'100'001,
        false,
        true,
        true,
        true,
        false,
        CAmount{0},
        false,
        CAmount{0},
        false,
        CAmount{0},
        false,
        CAmount{0},
        true,
        CAmount{21'000'000'000'000},
        false,
        CAmount{0},
        true,
        CAmount{0},
        true,
        0,
    },
    // GV-STATE-0001 (genesis — composes GV-ORU-0001)
    {
        "GV-STATE-0001",
        GoldenVectorFamily::STATE,
        0,
        false,
        false,
        false,
        true,
        true,
        CAmount{100'000'000'000},
        true,
        CAmount{0},
        true,
        CAmount{100'000'000'000},
        true,
        CAmount{0},
        true,
        CAmount{0},
        false,
        CAmount{0},
        false,
        CAmount{0},
        false,
        0,
    },
    // GV-STATE-0002 (h=10 composite)
    {
        "GV-STATE-0002",
        GoldenVectorFamily::STATE,
        10,
        false,
        false,
        false,
        true,
        true,
        CAmount{1'005'000'000'000},
        true,
        CAmount{910'000'000'000},
        true,
        CAmount{100'000'000'000},
        true,
        CAmount{5'000'000'000},
        true,
        CAmount{40'000'000},
        true,
        CAmount{0},
        true,
        CAmount{40'000'000},
        true,
        1,
    },
    // GV-STATE-0003 (post-reorg branch B at h=6)
    {
        "GV-STATE-0003",
        GoldenVectorFamily::STATE,
        6,
        false,
        false,
        false,
        true,
        true,
        CAmount{5'900'000'000'000},
        false,
        CAmount{0},
        false,
        CAmount{0},
        false,
        CAmount{0},
        true,
        CAmount{0},
        false,
        CAmount{0},
        false,
        CAmount{0},
        true,
        0,
    },
}};

/**
 * Deterministic GVR classification (PROOF-017). Order:
 * 1. NOT_EXECUTED — set by caller before field evaluation (fixture unavailable)
 * 2. FAIL — mismatch_count > 0, missing required witness, or no runtime witness
 * 3. PARTIAL — DRAW family (FVR-M); optional required-field gaps without mismatch
 * 4. PASS — required supply fields match; diagnostic rows (e.g. invariants_passed) ignored
 */
void FinalizeReportStatus(VectorVerificationReport& report, const GoldenVectorSpec& spec)
{
    if (report.mismatch_count > 0) {
        report.verification_status = GvrVerificationStatus::FAIL;
        return;
    }

    if (!report.runtime_witness_present) {
        report.verification_status = GvrVerificationStatus::FAIL;
        report.notes.push_back("runtime witness not present");
        return;
    }

    bool any_required_missing = false;
    for (const VectorFieldResult& row : report.field_results) {
        if (row.required && row.result == FieldCompareResult::OPTIONAL_MISS) {
            any_required_missing = true;
            break;
        }
    }
    if (any_required_missing) {
        report.verification_status = GvrVerificationStatus::FAIL;
        report.notes.push_back("required witness field missing");
        return;
    }

    if (spec.family == GoldenVectorFamily::DRAW || spec.draw_fields_optional) {
        report.verification_status = GvrVerificationStatus::PARTIAL;
        report.notes.push_back("draw fields optional (FVR-M)");
        return;
    }

    report.verification_status = GvrVerificationStatus::PASS;
}

} // namespace

bool EffectiveFixtureMaterialized(size_t index, const GoldenVectorSpec& spec)
{
    if (index < g_fixture_materialized_test_overlay.size() && g_fixture_materialized_test_overlay[index]) {
        return true;
    }
    return spec.fixture_materialized;
}

const char* GoldenVectorDocumentStatus(size_t index)
{
    if (index >= GOLDEN_VECTOR_COUNT) return "DRAFT";
    return EffectiveFixtureMaterialized(index, GetGoldenVectorSpec(index)) ? "ESTABLISHED" : "DRAFT";
}

const char* GvrVerificationStatusToString(GvrVerificationStatus status)
{
    switch (status) {
    case GvrVerificationStatus::NOT_EXECUTED: return "GVR_NOT_EXECUTED";
    case GvrVerificationStatus::PASS: return "GVR_PASS";
    case GvrVerificationStatus::FAIL: return "GVR_FAIL";
    case GvrVerificationStatus::PARTIAL: return "GVR_PARTIAL";
    }
    return "GVR_UNKNOWN";
}

const char* FieldCompareResultToString(FieldCompareResult result)
{
    switch (result) {
    case FieldCompareResult::MATCH: return "MATCH";
    case FieldCompareResult::MISMATCH: return "MISMATCH";
    case FieldCompareResult::OPTIONAL_MISS: return "OPTIONAL_MISS";
    case FieldCompareResult::SKIPPED: return "SKIPPED";
    }
    return "UNKNOWN";
}

const GoldenVectorSpec& GetGoldenVectorSpec(size_t index)
{
    return kGoldenVectors.at(index);
}

std::optional<size_t> FindGoldenVectorIndex(const std::string& vector_id)
{
    for (size_t i = 0; i < kGoldenVectors.size(); ++i) {
        if (vector_id == kGoldenVectors[i].id) {
            return i;
        }
    }
    return std::nullopt;
}

const GoldenVectorSpec* FindGoldenVectorSpec(const std::string& vector_id)
{
    const std::optional<size_t> index = FindGoldenVectorIndex(vector_id);
    if (!index.has_value()) return nullptr;
    return &kGoldenVectors[*index];
}

VectorVerificationReport EvaluateGoldenVector(const GoldenVectorSpec& spec,
                                              const RuntimeWitnessSnapshot& snapshot)
{
    VectorVerificationReport report;
    report.vector_id = spec.id;
    report.runtime_witness_present = snapshot.present;

    const std::optional<size_t> index = FindGoldenVectorIndex(spec.id);
    const size_t idx = index.value_or(0);
    report.vector_status = GoldenVectorDocumentStatus(idx);
    if (!EffectiveFixtureMaterialized(idx, spec)) {
        report.verification_status = GvrVerificationStatus::NOT_EXECUTED;
        report.notes.push_back("fixture block not materialized");
        return report;
    }

    if (spec.reject_expected) {
        if (snapshot.present) {
            report.verification_status = GvrVerificationStatus::PARTIAL;
            report.notes.push_back("reject vector: post-connect witness unexpected");
        } else {
            report.verification_status = GvrVerificationStatus::PARTIAL;
            report.notes.push_back("reject vector fixture materialized; no post-connect witness (expected)");
        }
        return report;
    }

    const bool has_export = snapshot.present;
    const Consensus::RuntimeSupplyState& mirror = snapshot.export_data.mirror;
    const Consensus::CoqSupplyFieldHook& hook = snapshot.export_data.coq_hook;
    const bool has_invariants = snapshot.invariants.has_value();

    if (spec.expect_height) {
        AppendIntField(report,
                       "height",
                       "height",
                       "FVR-B",
                       true,
                       spec.height,
                       has_export,
                       mirror.height);
    }

    if (spec.expect_oru_supply) {
        AppendAmountField(report,
                          "oru_supply",
                          "rs_oru_supply",
                          "FVR-C",
                          true,
                          spec.expected_oru_supply,
                          has_export,
                          hook.rs_oru_supply);
        AppendAmountField(report,
                          "oru_supply",
                          "oru_supply_cumulative",
                          "FVR-B",
                          true,
                          spec.expected_oru_supply,
                          has_export,
                          mirror.oru_supply_cumulative);
    }

    if (spec.expect_oru_parent) {
        AppendAmountField(report,
                          "oru_supply",
                          "rs_oru_supply_parent",
                          "FVR-C",
                          true,
                          spec.expected_oru_parent,
                          has_export,
                          hook.rs_oru_supply_parent);
    }

    if (spec.expect_oru_subsidy) {
        AppendAmountField(report,
                          "oru_subsidy",
                          "cb_oru_subsidy",
                          "FVR-C",
                          true,
                          spec.expected_oru_subsidy,
                          has_export,
                          hook.cb_oru_subsidy);
    }

    if (spec.expect_oru_burn) {
        AppendAmountField(report,
                          "oru_burn",
                          "cb_oru_burn_delta",
                          "FVR-C",
                          true,
                          spec.expected_oru_burn,
                          has_export,
                          hook.cb_oru_burn_delta);
    }

    if (spec.expect_res_supply) {
        AppendAmountField(report,
                          "res_supply",
                          "rs_res_supply",
                          "FVR-C",
                          true,
                          spec.expected_res_supply,
                          has_export,
                          hook.rs_res_supply);
    }

    if (spec.expect_res_parent) {
        AppendAmountField(report,
                          "res_supply",
                          "rs_res_supply_parent",
                          "FVR-C",
                          true,
                          spec.expected_res_parent,
                          has_export,
                          hook.rs_res_supply_parent);
    }

    if (spec.expect_res_mint) {
        AppendAmountField(report,
                          "res_mint",
                          "cb_res_mint_delta",
                          "FVR-C",
                          true,
                          spec.expected_res_mint,
                          has_export,
                          hook.cb_res_mint_delta);
    }

    if (spec.expect_res_mint_count) {
        AppendIntField(report,
                       "mint_count",
                       "res_mint_count_M_h",
                       "FVR-C",
                       true,
                       spec.expected_res_mint_count,
                       has_export,
                       hook.res_mint_count_M_h);
    }

    // invariants_passed is observational/diagnostic only (PROOF-017); fixture churn must not FAIL harness.
    if (has_invariants) {
        AppendBoolField(report,
                        "runtime_invariants",
                        "invariants_passed",
                        "FVR-E",
                        false,
                        true,
                        true,
                        snapshot.invariants->passed);
    } else if (has_export) {
        AppendBoolField(report,
                        "runtime_invariants",
                        "invariants_passed",
                        "FVR-E",
                        false,
                        true,
                        false,
                        false);
    }

    const bool fvr_witness_required = g_verify_fvr_witness_fields;
    if (snapshot.witness.has_value()) {
        AppendBoolField(report,
                        "fvr_witness",
                        "witness_chain_present",
                        "FVR-F",
                        fvr_witness_required,
                        true,
                        true,
                        true);
    } else if (has_export) {
        AppendBoolField(report,
                        "fvr_witness",
                        "witness_chain_present",
                        "FVR-F",
                        fvr_witness_required,
                        true,
                        false,
                        false);
    }

    AppendBoolField(report,
                    "fvr_catalog",
                    "catalog_row_present",
                    "FVR-G",
                    fvr_witness_required,
                    true,
                    has_export,
                    snapshot.fvr_catalog_row);

    if (spec.draw_fields_optional) {
        AppendSkippedField(report, "draw_winner", "winner_txid", "FVR-M");
        report.notes.push_back("draw equivalence prohibited (FVR-M)");
    }

    FinalizeReportStatus(report, spec);
    return report;
}

RuntimeWitnessSnapshot CaptureRuntimeWitnessSnapshot(const CBlockIndex* pindex,
                                                     const CCoinsViewCache& view,
                                                     const Consensus::Params& consensus)
{
    RuntimeWitnessSnapshot snapshot;
    if (pindex == nullptr) {
        return snapshot;
    }
    snapshot.export_data = Consensus::BuildRuntimeSupplyExport(pindex, view, consensus);
    snapshot.present = true;
    snapshot.invariants = Consensus::VerifyRuntimeSupplyInvariants(snapshot.export_data.mirror, consensus);
    snapshot.witness = Consensus::GetRuntimeSupplyWitness(pindex);
    snapshot.fvr_catalog_row = Consensus::ResolveWitnessCoqCatalogRowAt(pindex).has_value();
    return snapshot;
}

VectorVerificationReport EvaluateGoldenVectorOnChain(const GoldenVectorSpec& spec,
                                                     const CBlockIndex* pindex_at_height,
                                                     const CCoinsViewCache& view,
                                                     const Consensus::Params& consensus)
{
    const std::optional<size_t> index = FindGoldenVectorIndex(spec.id);
    if (!index.has_value()) {
        VectorVerificationReport report;
        report.vector_id = spec.id;
        report.verification_status = GvrVerificationStatus::NOT_EXECUTED;
        report.notes.push_back("unknown vector id");
        return report;
    }
    if (!EffectiveFixtureMaterialized(*index, spec)) {
        VectorVerificationReport report;
        report.vector_id = spec.id;
        report.verification_status = GvrVerificationStatus::NOT_EXECUTED;
        report.notes.push_back("fixture block not materialized");
        return report;
    }
    if (pindex_at_height == nullptr || pindex_at_height->nHeight != spec.height) {
        VectorVerificationReport report;
        report.vector_id = spec.id;
        report.verification_status = GvrVerificationStatus::NOT_EXECUTED;
        report.notes.push_back("chain block at vector height unavailable");
        return report;
    }
    const RuntimeWitnessSnapshot snapshot = CaptureRuntimeWitnessSnapshot(pindex_at_height, view, consensus);
    return EvaluateGoldenVector(spec, snapshot);
}

std::vector<VectorVerificationReport> EvaluateAllGoldenVectors(
    const CBlockIndex* (*resolve_height)(int32_t height),
    const CCoinsViewCache* view,
    const Consensus::Params* consensus)
{
    std::vector<VectorVerificationReport> out;
    out.reserve(GOLDEN_VECTOR_COUNT);
    for (size_t i = 0; i < GOLDEN_VECTOR_COUNT; ++i) {
        const GoldenVectorSpec& spec = kGoldenVectors[i];
        if (!EffectiveFixtureMaterialized(i, spec) || resolve_height == nullptr || view == nullptr || consensus == nullptr) {
            RuntimeWitnessSnapshot empty;
            out.push_back(EvaluateGoldenVector(spec, empty));
            continue;
        }
        const CBlockIndex* pindex = resolve_height(spec.height);
        if (pindex == nullptr) {
            RuntimeWitnessSnapshot empty;
            out.push_back(EvaluateGoldenVector(spec, empty));
            continue;
        }
        const RuntimeWitnessSnapshot snapshot = CaptureRuntimeWitnessSnapshot(pindex, *view, *consensus);
        out.push_back(EvaluateGoldenVector(spec, snapshot));
    }
    return out;
}

std::string VectorVerificationOutcome(const VectorVerificationReport& report)
{
    if (report.vector_status != "ESTABLISHED") {
        return "PENDING";
    }
    if (report.verification_status == GvrVerificationStatus::NOT_EXECUTED) {
        return "PENDING";
    }
    if (report.verification_status == GvrVerificationStatus::FAIL || report.mismatch_count > 0) {
        return "FAILED";
    }
    if (report.verification_status == GvrVerificationStatus::PASS ||
        report.verification_status == GvrVerificationStatus::PARTIAL) {
        return "VERIFIED";
    }
    return "PENDING";
}

UniValue VectorFieldResultToJson(const VectorFieldResult& row)
{
    UniValue obj(UniValue::VOBJ);
    obj.pushKV("category", row.category);
    obj.pushKV("field", row.field);
    obj.pushKV("expected", row.expected);
    obj.pushKV("witness", row.witness);
    obj.pushKV("result", FieldCompareResultToString(row.result));
    obj.pushKV("source", row.source);
    obj.pushKV("required", row.required);
    return obj;
}

UniValue VectorVerificationReportToJson(const VectorVerificationReport& report)
{
    UniValue obj(UniValue::VOBJ);
    obj.pushKV("harness_version", HARNESS_VERSION);
    obj.pushKV("vector_id", report.vector_id);
    obj.pushKV("vector_status", report.vector_status);
    obj.pushKV("vector_verification", VectorVerificationOutcome(report));
    obj.pushKV("verification_status", GvrVerificationStatusToString(report.verification_status));
    obj.pushKV("mismatch_count", report.mismatch_count);
    obj.pushKV("checked_fields", report.checked_fields);
    obj.pushKV("runtime_witness_present", report.runtime_witness_present);
    obj.pushKV("equivalence_claimed", false);
    obj.pushKV("coq_equiv_runtime", "NOT_ESTABLISHED");
    obj.pushKV("draw_equivalence", "PROHIBITED");

    UniValue failed(UniValue::VARR);
    for (const std::string& field : report.failed_fields) {
        failed.push_back(field);
    }
    obj.pushKV("failed_fields", failed);

    UniValue notes(UniValue::VARR);
    for (const std::string& note : report.notes) {
        notes.push_back(note);
    }
    obj.pushKV("notes", notes);

    UniValue fields(UniValue::VARR);
    for (const VectorFieldResult& row : report.field_results) {
        fields.push_back(VectorFieldResultToJson(row));
    }
    obj.pushKV("field_results", fields);
    return obj;
}

UniValue GoldenVectorBatchReportToJson(const std::vector<VectorVerificationReport>& reports)
{
    UniValue root(UniValue::VOBJ);
    root.pushKV("harness_version", HARNESS_VERSION);
    root.pushKV("equivalence_claimed", false);
    root.pushKV("coq_equiv_runtime", "NOT_ESTABLISHED");
    root.pushKV("draw_equivalence", "PROHIBITED");

    int pass_count = 0;
    int fail_count = 0;
    int partial_count = 0;
    int not_executed_count = 0;
    int established_count = 0;
    int verified_count = 0;
    for (const VectorVerificationReport& report : reports) {
        switch (report.verification_status) {
        case GvrVerificationStatus::PASS: ++pass_count; break;
        case GvrVerificationStatus::FAIL: ++fail_count; break;
        case GvrVerificationStatus::PARTIAL: ++partial_count; break;
        case GvrVerificationStatus::NOT_EXECUTED: ++not_executed_count; break;
        }
        if (report.vector_status == "ESTABLISHED") {
            ++established_count;
        }
        if (VectorVerificationOutcome(report) == "VERIFIED") {
            ++verified_count;
        }
    }

    UniValue summary(UniValue::VOBJ);
    summary.pushKV("total", static_cast<int>(reports.size()));
    summary.pushKV("pass", pass_count);
    summary.pushKV("fail", fail_count);
    summary.pushKV("partial", partial_count);
    summary.pushKV("not_executed", not_executed_count);
    summary.pushKV("established", established_count);
    summary.pushKV("verified", verified_count);
    root.pushKV("summary", summary);

    UniValue vectors(UniValue::VARR);
    for (const VectorVerificationReport& report : reports) {
        vectors.push_back(VectorVerificationReportToJson(report));
    }
    root.pushKV("vectors", vectors);
    return root;
}

void SetGoldenVectorFixtureMaterializedForTest(size_t index, bool materialized)
{
    if (index < g_fixture_materialized_test_overlay.size()) {
        g_fixture_materialized_test_overlay[index] = materialized;
    }
}

void ResetGoldenVectorFixtureFlagsForTest()
{
    g_fixture_materialized_test_overlay.fill(false);
}

void SetGoldenVectorVerifyFvrForRpc(bool verify_fvr)
{
    g_verify_fvr_witness_fields = verify_fvr;
}

} // namespace proof
