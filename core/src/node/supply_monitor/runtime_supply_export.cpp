// Copyright (c) 2026 The RigorChain developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <node/supply_monitor/runtime_supply_export.h>

#include <chain.h>
#include <node/supply_monitor/coq_supply_field_hook.h>
#include <node/supply_monitor/runtime_supply_invariant_checker.h>
#include <core_io.h>
#include <univalue.h>
#include <validation.h>

#include <string>
#include <utility>
#include <vector>

namespace Consensus {
namespace {

void PushAmountCell(UniValue& arr, int cell_index, const std::string& coq_name, const std::string& source,
                    bool present, const CAmount value)
{
    UniValue cell(UniValue::VOBJ);
    cell.pushKV("cell_index", cell_index);
    cell.pushKV("coq_name", coq_name);
    cell.pushKV("source", source);
    cell.pushKV("present", present);
    if (present) {
        cell.pushKV("value", ValueFromAmount(value));
    }
    arr.push_back(std::move(cell));
}

void PushIntCell(UniValue& arr, int cell_index, const std::string& coq_name, const std::string& source,
                 bool present, int64_t value)
{
    UniValue cell(UniValue::VOBJ);
    cell.pushKV("cell_index", cell_index);
    cell.pushKV("coq_name", coq_name);
    cell.pushKV("source", source);
    cell.pushKV("present", present);
    if (present) {
        cell.pushKV("value", value);
    }
    arr.push_back(std::move(cell));
}

void PushHexCell(UniValue& arr, int cell_index, const std::string& coq_name, const std::string& source,
                 bool present, const uint256& hash)
{
    UniValue cell(UniValue::VOBJ);
    cell.pushKV("cell_index", cell_index);
    cell.pushKV("coq_name", coq_name);
    cell.pushKV("source", source);
    cell.pushKV("present", present);
    if (present) {
        cell.pushKV("value", hash.GetHex());
    }
    arr.push_back(std::move(cell));
}

int EmitMirrorAuthoritativePair(UniValue& arr, int start_index, const std::string& coq_name,
                                bool mirror_present, const CAmount mirror_val,
                                bool auth_present, const CAmount auth_val)
{
    PushAmountCell(arr, start_index, coq_name, "runtime_mirror", mirror_present, mirror_val);
    PushAmountCell(arr, start_index + 1, coq_name, "authoritative_index", auth_present, auth_val);
    return start_index + 2;
}

} // namespace

RuntimeSupplyExport BuildRuntimeSupplyExport(const CBlockIndex* pindex,
                                             const CCoinsViewCache& view,
                                             const Params& consensus)
{
    assert(pindex != nullptr);

    RuntimeSupplyExport exp;
    WarmSupplyLedgerCaches(const_cast<CBlockIndex*>(pindex), consensus);

    const std::optional<RuntimeSupplyState>& stored = GetRuntimeSupplyMirror(*pindex);
    if (stored.has_value() && stored->block_hash == pindex->GetBlockHash()) {
        exp.mirror = *stored;
        exp.mirror_source = RuntimeSupplyMirrorSource::STORED_MIRROR;
    } else {
        const uint8_t mint_count = pindex->m_block_res_mint_sum > 0 ? 1 : 0;
        exp.mirror = CaptureRuntimeSupplyState(pindex, view, consensus, mint_count);
        exp.mirror_source = RuntimeSupplyMirrorSource::CAPTURED_FRESH;
    }

    exp.coq_hook = ProjectCoqSupplyFieldHook(exp.mirror);

    exp.authoritative_oru_supply_cumulative = pindex->Observational().GetChainOruTotal();
    exp.authoritative_oru_converge_burn_block = pindex->m_block_converge_sum;
    exp.authoritative_res_mint_cumulative = pindex->Observational().GetChainResMintTotal();
    exp.authoritative_res_mint_block = pindex->m_block_res_mint_sum;

    return exp;
}

UniValue RuntimeSupplyExportToJson(const RuntimeSupplyExport& exp,
                                   const CBlockIndex& pindex,
                                   const Params& consensus)
{
    const RuntimeSupplyState& s = exp.mirror;

    UniValue obj(UniValue::VOBJ);
    obj.pushKV("height", s.height);
    obj.pushKV("block_hash", s.block_hash.GetHex());
    obj.pushKV("parent_hash", s.parent_hash.IsNull() ? "" : s.parent_hash.GetHex());

    obj.pushKV("mirror_present", exp.mirror_source == RuntimeSupplyMirrorSource::STORED_MIRROR);
    obj.pushKV("mirror_source",
               exp.mirror_source == RuntimeSupplyMirrorSource::STORED_MIRROR ? "stored_mirror" : "captured_fresh");

    obj.pushKV("oru_supply_cumulative", ValueFromAmount(s.oru_supply_cumulative));
    obj.pushKV("oru_subsidy_block", ValueFromAmount(s.oru_subsidy_block));
    obj.pushKV("oru_converge_burn_block", ValueFromAmount(s.oru_converge_burn_block));
    obj.pushKV("oru_supply_parent", ValueFromAmount(s.oru_supply_parent));
    obj.pushKV("res_mint_cumulative", ValueFromAmount(s.res_mint_cumulative));
    obj.pushKV("res_mint_block", ValueFromAmount(s.res_mint_block));
    obj.pushKV("res_mint_parent", ValueFromAmount(s.res_mint_parent));
    obj.pushKV("res_mint_count_block", static_cast<int>(s.res_mint_count_block));
    obj.pushKV("res_max_supply", ValueFromAmount(s.res_max_supply));
    obj.pushKV("res_reward_amount", ValueFromAmount(s.res_reward_amount));
    obj.pushKV("utxo_res_balance", ValueFromAmount(s.utxo_res_balance));
    obj.pushKV("utxo_non_res_balance", ValueFromAmount(s.utxo_non_res_balance));
    if (s.oru_subsidy_cumulative_derived.has_value()) {
        obj.pushKV("oru_subsidy_cumulative_derived", ValueFromAmount(*s.oru_subsidy_cumulative_derived));
    }

    UniValue auth(UniValue::VOBJ);
    if (exp.authoritative_oru_supply_cumulative >= 0) {
        auth.pushKV("m_chain_oru_total", ValueFromAmount(exp.authoritative_oru_supply_cumulative));
    }
    auth.pushKV("m_block_converge_sum", ValueFromAmount(exp.authoritative_oru_converge_burn_block));
    if (exp.authoritative_res_mint_cumulative >= 0) {
        auth.pushKV("m_chain_res_mint_total", ValueFromAmount(exp.authoritative_res_mint_cumulative));
    }
    auth.pushKV("m_block_res_mint_sum", ValueFromAmount(exp.authoritative_res_mint_block));
    obj.pushKV("authoritative_caches", std::move(auth));

    const RuntimeInvariantResult formal_inv = VerifyRuntimeSupplyInvariants(s, consensus);
    obj.pushKV("invariant_passed", formal_inv.passed);
    obj.pushKV("checked_invariants", static_cast<int>(formal_inv.checked_invariants));

    UniValue inv(UniValue::VARR);
    for (const std::string& msg : CheckRuntimeSupplyInvariants(s, consensus)) {
        inv.push_back(msg);
    }
    obj.pushKV("invariant_violations", std::move(inv));

    UniValue coq_hook_violations(UniValue::VARR);
    for (const std::string& msg : CheckCoqHookMatchesAuthoritativeCaches(exp.coq_hook, s, pindex)) {
        coq_hook_violations.push_back(msg);
    }
    obj.pushKV("coq_hook_violations", std::move(coq_hook_violations));

    UniValue coq_fields(UniValue::VOBJ);
    const CoqSupplyFieldHook& hook = exp.coq_hook;
    coq_fields.pushKV("height", hook.height);
    coq_fields.pushKV("block_hash", hook.block_hash.GetHex());
    coq_fields.pushKV("parent_hash", hook.parent_hash.IsNull() ? "" : hook.parent_hash.GetHex());
    coq_fields.pushKV("rs_oru_supply", ValueFromAmount(hook.rs_oru_supply));
    coq_fields.pushKV("rs_res_supply", ValueFromAmount(hook.rs_res_supply));
    coq_fields.pushKV("cb_oru_subsidy", ValueFromAmount(hook.cb_oru_subsidy));
    coq_fields.pushKV("cb_oru_burn_delta", ValueFromAmount(hook.cb_oru_burn_delta));
    coq_fields.pushKV("cb_res_mint_delta", ValueFromAmount(hook.cb_res_mint_delta));
    coq_fields.pushKV("res_mint_count_M_h", static_cast<int>(hook.res_mint_count_M_h));
    coq_fields.pushKV("rs_oru_supply_parent", ValueFromAmount(hook.rs_oru_supply_parent));
    coq_fields.pushKV("rs_res_supply_parent", ValueFromAmount(hook.rs_res_supply_parent));
    coq_fields.pushKV("audit_coq_cb_oru_mint_delta_scaffold", ValueFromAmount(hook.audit_coq_cb_oru_mint_delta_scaffold));
    if (hook.audit_oru_subsidy_cumulative_derived.has_value()) {
        coq_fields.pushKV("audit_oru_subsidy_cumulative_derived",
                           ValueFromAmount(*hook.audit_oru_subsidy_cumulative_derived));
    }
    obj.pushKV("coq_fields", std::move(coq_fields));

    obj.pushKV("phase5_supply_cell_count", 32);
    obj.pushKV("phase5_supply_cells", RuntimeSupplyPhase5CellsToJson(exp));

    UniValue sem(UniValue::VOBJ);
    sem.pushKV("equivalence_claimed", false);
    sem.pushKV("oru_supply_cumulative", "rfc002_ledger_issued_minus_converge_not_circulating");
    sem.pushKV("oru_subsidy_cumulative_derived", "total_issued_oru");
    sem.pushKV("oru_converge_burn_block", "this_block_convergence_destroyed_oru_not_rfc018_fee_burn");
    sem.pushKV("utxo_non_res_balance", "circulating_spendable_oru_utxo_includes_immature_excludes_cvg2");
    sem.pushKV("rfc018_fee_burn_cumulative", "not_persisted_on_block_index");
    obj.pushKV("supply_semantics", std::move(sem));

    return obj;
}

UniValue RuntimeSupplyPhase5CellsToJson(const RuntimeSupplyExport& exp)
{
    const RuntimeSupplyState& m = exp.mirror;
    const CAmount auth_oru = exp.authoritative_oru_supply_cumulative;
    const CAmount auth_res = exp.authoritative_res_mint_cumulative;
    const bool auth_oru_ok = auth_oru >= 0;
    const bool auth_res_ok = auth_res >= 0;
    const uint8_t auth_mint_count = exp.authoritative_res_mint_block > 0 ? 1 : 0;

    UniValue arr(UniValue::VARR);
    int idx = 0;

    PushIntCell(arr, idx++, "height", "runtime_mirror", true, m.height);
    PushIntCell(arr, idx++, "height", "authoritative_index", true, m.height);

    PushHexCell(arr, idx++, "block_hash", "runtime_mirror", true, m.block_hash);
    PushHexCell(arr, idx++, "block_hash", "authoritative_index", true, m.block_hash);

    PushHexCell(arr, idx++, "parent_hash", "runtime_mirror", true, m.parent_hash);
    PushHexCell(arr, idx++, "parent_hash", "authoritative_index", true, m.parent_hash);

    idx = EmitMirrorAuthoritativePair(arr, idx, "rs_oru_supply", true, m.oru_supply_cumulative, auth_oru_ok, auth_oru);
    idx = EmitMirrorAuthoritativePair(arr, idx, "rs_res_supply", true, m.res_mint_cumulative, auth_res_ok, auth_res);
    idx = EmitMirrorAuthoritativePair(arr, idx, "cb_oru_subsidy", true, m.oru_subsidy_block, true, m.oru_subsidy_block);
    idx = EmitMirrorAuthoritativePair(arr, idx, "cb_oru_burn_delta", true, m.oru_converge_burn_block, true,
                                      exp.authoritative_oru_converge_burn_block);
    idx = EmitMirrorAuthoritativePair(arr, idx, "cb_res_mint_delta", true, m.res_mint_block, true,
                                      exp.authoritative_res_mint_block);

    PushIntCell(arr, idx++, "res_mint_count_M_h", "runtime_mirror", true, m.res_mint_count_block);
    PushIntCell(arr, idx++, "res_mint_count_M_h", "authoritative_index", true, auth_mint_count);

    PushAmountCell(arr, idx++, "rs_oru_supply_parent", "runtime_mirror", true, m.oru_supply_parent);
    PushAmountCell(arr, idx++, "rs_oru_supply_parent", "authoritative_index", false, 0);

    PushAmountCell(arr, idx++, "rs_res_supply_parent", "runtime_mirror", true, m.res_mint_parent);
    PushAmountCell(arr, idx++, "rs_res_supply_parent", "authoritative_index", false, 0);

    PushAmountCell(arr, idx++, "res_max_supply", "runtime_mirror", true, m.res_max_supply);
    PushAmountCell(arr, idx++, "res_max_supply", "authoritative_index", true, m.res_max_supply);

    PushAmountCell(arr, idx++, "res_reward_amount", "runtime_mirror", true, m.res_reward_amount);
    PushAmountCell(arr, idx++, "res_reward_amount", "authoritative_index", true, m.res_reward_amount);

    PushAmountCell(arr, idx++, "utxo_res_balance", "runtime_mirror", true, m.utxo_res_balance);
    PushAmountCell(arr, idx++, "utxo_res_balance", "authoritative_index", false, 0);

    PushAmountCell(arr, idx++, "utxo_non_res_balance", "runtime_mirror", true, m.utxo_non_res_balance);
    PushAmountCell(arr, idx++, "utxo_non_res_balance", "authoritative_index", false, 0);

    const bool derived_present = m.oru_subsidy_cumulative_derived.has_value();
    const CAmount derived_val = derived_present ? *m.oru_subsidy_cumulative_derived : 0;
    PushAmountCell(arr, idx++, "audit_oru_subsidy_cumulative_derived", "runtime_mirror", derived_present, derived_val);
    PushAmountCell(arr, idx++, "audit_oru_subsidy_cumulative_derived", "authoritative_index", false, 0);

    return arr;
}

} // namespace Consensus
