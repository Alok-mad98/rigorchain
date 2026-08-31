// Copyright (c) 2026 The RigorChain developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <node/supply_monitor/runtime_supply_coq_chas_adapter.h>

#include <chain.h>
#include <core_io.h>
#include <univalue.h>

#include <utility>

namespace Consensus {

namespace {

CanonicalHashAlignmentStatus ClassifySupplyChasField(
    const RuntimeSupplyWitnessCoqCatalogRow& row,
    const std::vector<std::string>& delta_audit)
{
    if (row.height < 0) {
        return CanonicalHashAlignmentStatus::Missing;
    }
    if (!row.invariants_passed) {
        return CanonicalHashAlignmentStatus::FundamentallyBlocked;
    }
    if (row.height > 0 && row.delta.parent_witness_present && !row.delta.witness_chain_step_valid) {
        return CanonicalHashAlignmentStatus::FundamentallyBlocked;
    }
    if (row.height > 0 && !row.delta.parent_witness_present) {
        return CanonicalHashAlignmentStatus::Partial;
    }
    for (const std::string& id : delta_audit) {
        if (id == "WITNESS-COQ-DELTA-ORU" || id == "WITNESS-COQ-DELTA-RES") {
            return CanonicalHashAlignmentStatus::Partial;
        }
    }
    return CanonicalHashAlignmentStatus::Partial;
}

RuntimeSupplyChasFieldAlignment MakeFieldAlignment(
    const std::string& chas_field,
    const std::string& coq_field,
    CAmount runtime_value,
    CanonicalHashAlignmentStatus status,
    bool witness_present,
    std::vector<std::string> audit_notes)
{
    RuntimeSupplyChasFieldAlignment field;
    field.chas_field = chas_field;
    field.coq_catalog_field = coq_field;
    field.status = status;
    field.runtime_value = runtime_value;
    field.runtime_witness_present = witness_present;
    field.maps_to_canonical_hash = true;
    field.theoretical_alignment_potential = true;
    field.audit_notes = std::move(audit_notes);
    return field;
}

std::vector<std::string> BuildOruAuditNotes(const RuntimeSupplyWitnessCoqCatalogRow& row,
                                            const std::vector<std::string>& delta_audit)
{
    std::vector<std::string> notes;
    if (!row.mirror_present) {
        notes.emplace_back("MIRROR-ABSENT-ORU");
    }
    if (row.coq_hook.cb_oru_burn_delta > 0) {
        notes.emplace_back("COQ-BURN-DELTA-GAP");
    }
    for (const std::string& id : delta_audit) {
        if (id == "WITNESS-COQ-DELTA-ORU" || id == "WITNESS-CHAIN-BROKEN") {
            notes.push_back(id);
        }
    }
    return notes;
}

std::vector<std::string> BuildResAuditNotes(const RuntimeSupplyWitnessCoqCatalogRow& row,
                                            const std::vector<std::string>& delta_audit)
{
    std::vector<std::string> notes;
    if (!row.mirror_present) {
        notes.emplace_back("MIRROR-ABSENT-RES");
    }
    for (const std::string& id : delta_audit) {
        if (id == "WITNESS-COQ-DELTA-RES" || id == "WITNESS-CHAIN-BROKEN") {
            notes.push_back(id);
        }
    }
    return notes;
}

UniValue ChasFieldAlignmentToJson(const RuntimeSupplyChasFieldAlignment& field)
{
    UniValue obj(UniValue::VOBJ);
    obj.pushKV("chas_field", field.chas_field);
    obj.pushKV("coq_catalog_field", field.coq_catalog_field);
    obj.pushKV("chas_status", ChasAlignmentStatusToString(field.status));
    obj.pushKV("runtime_value", ValueFromAmount(field.runtime_value));
    obj.pushKV("runtime_witness_present", field.runtime_witness_present);
    obj.pushKV("maps_to_canonical_hash", field.maps_to_canonical_hash);
    obj.pushKV("theoretical_alignment_potential", field.theoretical_alignment_potential);
    UniValue notes(UniValue::VARR);
    for (const std::string& note : field.audit_notes) {
        notes.push_back(note);
    }
    obj.pushKV("audit_notes", std::move(notes));
    return obj;
}

} // namespace

const char* ChasAlignmentStatusToString(const CanonicalHashAlignmentStatus status)
{
    switch (status) {
    case CanonicalHashAlignmentStatus::Aligned:
        return "CHAS_Aligned";
    case CanonicalHashAlignmentStatus::Partial:
        return "CHAS_Partial";
    case CanonicalHashAlignmentStatus::Missing:
        return "CHAS_Missing";
    case CanonicalHashAlignmentStatus::FundamentallyBlocked:
        return "CHAS_FundamentallyBlocked";
    }
    return "CHAS_Missing";
}

RuntimeSupplyChasAlignmentReport BuildChasAlignmentFromCatalogRow(
    const RuntimeSupplyWitnessCoqCatalogRow& row)
{
    RuntimeSupplyChasAlignmentReport report;
    report.height = row.height;
    report.block_hash = row.block_hash;
    report.parent_hash = row.parent_hash;
    report.mirror_present = row.mirror_present;
    report.mirror_source = row.mirror_present ? "stored_mirror" : "witness_only";
    report.invariants_passed = row.invariants_passed;
    report.checked_invariants = row.checked_invariants;
    report.violations = row.violations;
    report.delta = row.delta;
    report.delta_audit_violations = CheckWitnessCoqDeltaMatchesHook(row);
    report.equivalence_claimed = false;
    report.coq_equiv_runtime = "NOT_ESTABLISHED";

    const CanonicalHashAlignmentStatus oru_status =
        ClassifySupplyChasField(row, report.delta_audit_violations);
    const CanonicalHashAlignmentStatus res_status =
        ClassifySupplyChasField(row, report.delta_audit_violations);

    const bool witness_present = row.height >= 0;
    report.chas_oru_state = MakeFieldAlignment(
        "chas_oru_state",
        "rs_oru_supply",
        row.rs_oru_supply,
        oru_status,
        witness_present,
        BuildOruAuditNotes(row, report.delta_audit_violations));
    report.chas_res_state = MakeFieldAlignment(
        "chas_res_state",
        "rs_res_supply",
        row.rs_res_supply,
        res_status,
        witness_present,
        BuildResAuditNotes(row, report.delta_audit_violations));

    return report;
}

std::optional<RuntimeSupplyChasAlignmentReport> ResolveChasAlignmentAt(const CBlockIndex* pindex)
{
    const std::optional<RuntimeSupplyWitnessCoqCatalogRow> row = ResolveWitnessCoqCatalogRowAt(pindex);
    if (!row.has_value()) {
        return std::nullopt;
    }
    return BuildChasAlignmentFromCatalogRow(*row);
}

UniValue ChasAlignmentReportToJson(const RuntimeSupplyChasAlignmentReport& report)
{
    UniValue obj(UniValue::VOBJ);
    obj.pushKV("height", report.height);
    obj.pushKV("block_hash", report.block_hash.GetHex());
    obj.pushKV("parent_hash", report.parent_hash.IsNull() ? "" : report.parent_hash.GetHex());

    obj.pushKV("mirror_present", report.mirror_present);
    obj.pushKV("mirror_source", report.mirror_source);
    obj.pushKV("invariants_passed", report.invariants_passed);
    obj.pushKV("checked_invariants", static_cast<int>(report.checked_invariants));
    UniValue violations(UniValue::VARR);
    for (const std::string& id : report.violations) {
        violations.push_back(id);
    }
    obj.pushKV("violations", std::move(violations));

    obj.pushKV("equivalence_claimed", report.equivalence_claimed);
    obj.pushKV("coq_equiv_runtime", report.coq_equiv_runtime);

    UniValue delta(UniValue::VOBJ);
    delta.pushKV("parent_witness_present", report.delta.parent_witness_present);
    if (report.delta.parent_height.has_value()) {
        delta.pushKV("parent_height", *report.delta.parent_height);
    }
    if (report.delta.parent_block_hash.has_value()) {
        delta.pushKV("parent_block_hash", report.delta.parent_block_hash->GetHex());
    }
    delta.pushKV("rs_oru_supply_delta", ValueFromAmount(report.delta.rs_oru_supply_delta));
    delta.pushKV("rs_res_supply_delta", ValueFromAmount(report.delta.rs_res_supply_delta));
    delta.pushKV("witness_chain_step_valid", report.delta.witness_chain_step_valid);
    UniValue delta_audit(UniValue::VARR);
    for (const std::string& id : report.delta_audit_violations) {
        delta_audit.push_back(id);
    }
    delta.pushKV("delta_audit_violations", std::move(delta_audit));
    obj.pushKV("parent_delta", std::move(delta));

    UniValue fields(UniValue::VARR);
    fields.push_back(ChasFieldAlignmentToJson(report.chas_oru_state));
    fields.push_back(ChasFieldAlignmentToJson(report.chas_res_state));
    obj.pushKV("chas_field_alignments", std::move(fields));

    UniValue summary(UniValue::VOBJ);
    summary.pushKV("chas_oru_status", ChasAlignmentStatusToString(report.chas_oru_state.status));
    summary.pushKV("chas_res_status", ChasAlignmentStatusToString(report.chas_res_state.status));
    summary.pushKV("pd_4f_ob_02", "OPEN");
    summary.pushKV("pd_4f_cpp_01", "OPEN");
    obj.pushKV("chas_summary", std::move(summary));

    return obj;
}

} // namespace Consensus
