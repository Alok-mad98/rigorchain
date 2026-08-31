// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// P063 verify-only ledger apply. Does not recode issuance / RES / draw / PoW.

#include <consensus/ledger/ledger_transition.h>

#include <cstddef>
#include <limits>

namespace {

bool AmountOk(CAmount v)
{
    return v >= 0 && v <= MAX_MONEY;
}

bool AddMoney(CAmount a, CAmount b, CAmount& out)
{
    if (!AmountOk(a) || !AmountOk(b)) return false;
    if (b > MAX_MONEY - a) return false;
    out = a + b;
    return AmountOk(out);
}

bool OutpointIn(const LedgerOutpoint& o, const std::vector<LedgerOutpoint>& xs, std::size_t from)
{
    for (std::size_t i = from; i < xs.size(); ++i) {
        if (xs[i] == o) return true;
    }
    return false;
}

bool NoDupOutpoints(const std::vector<LedgerOutpoint>& xs)
{
    for (std::size_t i = 0; i < xs.size(); ++i) {
        if (OutpointIn(xs[i], xs, i + 1)) return false;
    }
    return true;
}

bool SumAsset(const std::vector<LedgerCoin>& coins, LedgerAsset asset, CAmount& out)
{
    out = 0;
    for (const auto& c : coins) {
        if (c.asset != asset) continue;
        if (!AddMoney(out, c.value, out)) return false;
    }
    return true;
}

bool AllAssetsKnown(const std::vector<LedgerCoin>& coins)
{
    for (const auto& c : coins) {
        if (c.asset != LedgerAsset::ORU && c.asset != LedgerAsset::RES) return false;
        if (!AmountOk(c.value)) return false;
    }
    return true;
}

bool Mature(const LedgerKernelInput& in)
{
    for (const auto& c : in.consumed) {
        if (c.coinbase_height < 0) continue;
        if (c.coinbase_height > in.height) return false;
        if (in.height - c.coinbase_height < in.maturity) return false;
    }
    return true;
}

bool GenesisOk(const LedgerKernelInput& in)
{
    if (in.height != 0) return true;
    return in.consensus.oru_issuance == 0 &&
           in.consensus.res_mint == 0 &&
           in.consensus.res_schedule == 40000000;
}

} // namespace

LedgerKernelOutput LedgerApply(const LedgerKernelInput& in)
{
    LedgerKernelOutput out;
    out.accept = false;
    out.value_ok = false;

    if (!in.tx.accept || !in.consensus.accept) return out;
    if (!AmountOk(in.fee) || !AmountOk(in.burn) || !AmountOk(in.converge)) return out;
    if (!AmountOk(in.consensus.oru_issuance) || !AmountOk(in.consensus.res_mint)) return out;
    if (!AllAssetsKnown(in.consumed) || !AllAssetsKnown(in.created)) return out;

    std::vector<LedgerOutpoint> spent;
    spent.reserve(in.consumed.size());
    for (const auto& c : in.consumed) spent.push_back(c.outpoint);
    std::vector<LedgerOutpoint> created_ops;
    created_ops.reserve(in.created.size());
    for (const auto& c : in.created) created_ops.push_back(c.outpoint);
    if (!NoDupOutpoints(spent) || !NoDupOutpoints(created_ops)) return out;
    if (!Mature(in) || !GenesisOk(in)) return out;

    CAmount in_all = 0;
    CAmount out_all = 0;
    CAmount in_oru = 0;
    CAmount out_oru = 0;
    CAmount in_res = 0;
    CAmount out_res = 0;
    if (!SumAsset(in.consumed, LedgerAsset::ORU, in_oru)) return out;
    if (!SumAsset(in.consumed, LedgerAsset::RES, in_res)) return out;
    if (!SumAsset(in.created, LedgerAsset::ORU, out_oru)) return out;
    if (!SumAsset(in.created, LedgerAsset::RES, out_res)) return out;
    if (!AddMoney(in_oru, in_res, in_all)) return out;
    if (!AddMoney(out_oru, out_res, out_all)) return out;

    CAmount credited = 0;
    if (!AddMoney(in_all, in.consensus.oru_issuance, credited)) return out;
    if (!AddMoney(credited, in.consensus.res_mint, credited)) return out;
    CAmount sinks = 0;
    if (!AddMoney(in.burn, in.fee, sinks)) return out;
    CAmount rhs = 0;
    if (!AddMoney(out_all, sinks, rhs)) return out;
    const bool value_ok = credited >= rhs;
    out.value_ok = value_ok;
    if (!value_ok) return out;

    CAmount oru_sinks = 0;
    if (!AddMoney(in.burn, in.converge, oru_sinks)) return out;
    if (!AddMoney(oru_sinks, in.fee, oru_sinks)) return out;
    CAmount oru_left = 0;
    if (!AddMoney(in_oru, in.consensus.oru_issuance, oru_left)) return out;
    if (oru_left < oru_sinks) return out;
    if (oru_left - oru_sinks != out_oru) return out;
    if (in_res > std::numeric_limits<CAmount>::max() - in.consensus.res_mint) return out;
    if (in_res + in.consensus.res_mint != out_res) return out;

    out.accept = true;
    out.spent_delta = std::move(spent);
    out.created_delta = in.created;
    return out;
}
