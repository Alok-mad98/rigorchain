// Copyright (c) 2026 The RigorChain developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <consensus/runtime_supply_witness_coq_adapter.h>

#include <chain.h>
#include <consensus/runtime_supply_state.h>
#include <core_io.h>
#include <univalue.h>

#include <utility>

namespace Consensus {

namespace {

CoqSupplyFieldHook HookFromWitnessOnly(const RuntimeSupplyWitness& witness)
{
    CoqSupplyFieldHook hook;
    hook.height = witness.height;
    hook.block_hash = witness.block_hash;
    hook.parent_hash = witness.parent_hash;
    hook.rs_oru_supply = witness.oru_supply_cumulative;
    hook.rs_res_supply = witness.res_mint_cumulative;
    return hook;
}

void FillDelta(RuntimeSupplyWitnessCoqDelta& delta,
               const RuntimeSupplyWitness& witness,
               const std::optional<RuntimeSupplyWitness>& parent_witness)
{
    if (!parent_witness.has_value()) {
        delta.parent_witness_present = false;
        delta.witness_chain_step_valid = witness.height == 0;
        return;
    }

    delta.parent_witness_present = true;
    delta.parent_height = parent_witness->height;
    delta.parent_block_hash = parent_witness->block_hash;
    delta.rs_oru_supply_delta =
        witness.oru_supply_cumulative - parent_witness->oru_supply_cumulative;
    delta.rs_res_supply_delta =
        witness.res_mint_cumulative - parent_witness->res_mint_cumulative;
    delta.witness_chain_step_valid =
        VerifyRuntimeWitnessChainStep(*parent_witness, witness);
}

} // namespace

RuntimeSupplyWitnessCoqCatalogRow BuildWitnessCoqCatalogRow(
    const RuntimeSupplyWitness& witness,
    const std::optional<RuntimeSupplyState>& mirror,
    const bool mirror_present,
    const RuntimeSupplyMirrorSource mirror_source,
    const std::optional<RuntimeSupplyWitness>& parent_witness)
{
    RuntimeSupplyWitnessCoqCatalogRow row;
    row.height = witness.height;
    row.block_hash = witness.block_hash;
    row.parent_hash = witness.parent_hash;

    row.rs_oru_supply = witness.oru_supply_cumulative;
    row.rs_res_supply = witness.res_mint_cumulative;

    row.invariants_passed = witness.invariants_passed;
    row.checked_invariants = witness.checked_invariants;
    row.violations = witness.violations;

    row.mirror_present = mirror_present;
    row.mirror_source = mirror_source;
    row.coq_hook = mirror_present ? ProjectCoqSupplyFieldHook(*mirror) : HookFromWitnessOnly(witness);

    FillDelta(row.delta, witness, parent_witness);
    row.equivalence_claimed = false;
    return row;
}

std::optional<RuntimeSupplyWitnessCoqCatalogRow> ResolveWitnessCoqCatalogRowAt(const CBlockIndex* pindex)
{
    if (pindex == nullptr) {
        return std::nullopt;
    }

    const std::optional<RuntimeSupplyWitness> witness = GetRuntimeSupplyWitness(pindex->nHeight);
    if (!witness.has_value()) {
        return std::nullopt;
    }

    std::optional<RuntimeSupplyWitness> parent_witness;
    if (pindex->nHeight > 0) {
        parent_witness = GetRuntimeSupplyWitness(pindex->nHeight - 1);
    }

    const std::optional<RuntimeSupplyState>& stored = GetRuntimeSupplyMirror(*pindex);
    std::optional<RuntimeSupplyState> mirror;
    bool mirror_present = false;
    RuntimeSupplyMirrorSource mirror_source = RuntimeSupplyMirrorSource::CAPTURED_FRESH;
    if (stored.has_value() && stored->block_hash == pindex->GetBlockHash()) {
        mirror = *stored;
        mirror_present = true;
        mirror_source = RuntimeSupplyMirrorSource::STORED_MIRROR;
    }

    return BuildWitnessCoqCatalogRow(*witness, mirror, mirror_present, mirror_source, parent_witness);
}

std::vector<std::string> CheckWitnessCoqDeltaMatchesHook(const RuntimeSupplyWitnessCoqCatalogRow& row)
{
    std::vector<std::string> errors;

    if (row.delta.parent_witness_present && !row.delta.witness_chain_step_valid) {
        errors.emplace_back("WITNESS-CHAIN-BROKEN");
    }

    if (!row.mirror_present) {
        return errors;
    }

    const CAmount expected_oru =
        row.coq_hook.cb_oru_subsidy - row.coq_hook.cb_oru_burn_delta;
    if (row.delta.parent_witness_present &&
        row.delta.rs_oru_supply_delta != expected_oru) {
        errors.emplace_back("WITNESS-COQ-DELTA-ORU");
    }

    const CAmount expected_res = row.coq_hook.cb_res_mint_delta;
    if (row.delta.parent_witness_present &&
        row.delta.rs_res_supply_delta != expected_res) {
        errors.emplace_back("WITNESS-COQ-DELTA-RES");
    }

    return errors;
}

UniValue WitnessCoqCatalogRowToJson(const RuntimeSupplyWitnessCoqCatalogRow& row)
{
    UniValue obj(UniValue::VOBJ);
    obj.pushKV("height", row.height);
    obj.pushKV("block_hash", row.block_hash.GetHex());
    obj.pushKV("parent_hash", row.parent_hash.IsNull() ? "" : row.parent_hash.GetHex());

    obj.pushKV("invariants_passed", row.invariants_passed);
    obj.pushKV("checked_invariants", static_cast<int>(row.checked_invariants));
    UniValue violations(UniValue::VARR);
    for (const std::string& id : row.violations) {
        violations.push_back(id);
    }
    obj.pushKV("violations", std::move(violations));

    obj.pushKV("mirror_present", row.mirror_present);
    obj.pushKV("mirror_source",
               row.mirror_present ? "stored_mirror" : "witness_only");

    obj.pushKV("equivalence_claimed", row.equivalence_claimed);
    obj.pushKV("coq_equiv_runtime", "NOT_ESTABLISHED");

    UniValue catalog(UniValue::VOBJ);
    catalog.pushKV("rs_oru_supply", ValueFromAmount(row.rs_oru_supply));
    catalog.pushKV("rs_res_supply", ValueFromAmount(row.rs_res_supply));

    const CoqSupplyFieldHook& hook = row.coq_hook;
    catalog.pushKV("cb_oru_subsidy", ValueFromAmount(hook.cb_oru_subsidy));
    catalog.pushKV("cb_oru_burn_delta", ValueFromAmount(hook.cb_oru_burn_delta));
    catalog.pushKV("cb_res_mint_delta", ValueFromAmount(hook.cb_res_mint_delta));
    catalog.pushKV("res_mint_count_M_h", static_cast<int>(hook.res_mint_count_M_h));
    catalog.pushKV("rs_oru_supply_parent", ValueFromAmount(hook.rs_oru_supply_parent));
    catalog.pushKV("rs_res_supply_parent", ValueFromAmount(hook.rs_res_supply_parent));
    catalog.pushKV("audit_coq_cb_oru_mint_delta_scaffold",
                   ValueFromAmount(hook.audit_coq_cb_oru_mint_delta_scaffold));
    if (hook.audit_oru_subsidy_cumulative_derived.has_value()) {
        catalog.pushKV("audit_oru_subsidy_cumulative_derived",
                       ValueFromAmount(*hook.audit_oru_subsidy_cumulative_derived));
    }
    obj.pushKV("coq_catalog", std::move(catalog));

    UniValue delta(UniValue::VOBJ);
    delta.pushKV("parent_witness_present", row.delta.parent_witness_present);
    if (row.delta.parent_height.has_value()) {
        delta.pushKV("parent_height", *row.delta.parent_height);
    }
    if (row.delta.parent_block_hash.has_value()) {
        delta.pushKV("parent_block_hash", row.delta.parent_block_hash->GetHex());
    }
    delta.pushKV("rs_oru_supply_delta", ValueFromAmount(row.delta.rs_oru_supply_delta));
    delta.pushKV("rs_res_supply_delta", ValueFromAmount(row.delta.rs_res_supply_delta));
    delta.pushKV("witness_chain_step_valid", row.delta.witness_chain_step_valid);

    UniValue delta_audit(UniValue::VARR);
    for (const std::string& msg : CheckWitnessCoqDeltaMatchesHook(row)) {
        delta_audit.push_back(msg);
    }
    delta.pushKV("delta_audit_violations", std::move(delta_audit));
    obj.pushKV("parent_delta", std::move(delta));

    return obj;
}

} // namespace Consensus
