// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// Verify-only. Calls production Kernel and C ABI. Does not recode formulas.
//
// RFC-019: issuance surface is (oru_subsidy, RATE(h), epoch). Draw
// surfaces report no winner.

#ifndef RIGOR_KERNEL_SHADOW_VERIFY_ONLY
#error "consensus/shadow is verify-only; compile with -DRIGOR_KERNEL_SHADOW_VERIFY_ONLY"
#endif
#ifndef RIGOR_KERNEL_ABI_VERIFY_ONLY
#error "shadow executor needs the C ABI compile flag; not a production object"
#endif

#include <consensus/shadow/shadow_executor.h>
#include <consensus/shadow/ledger_delta_bridge.h>

#include <consensus/bridge/consensus_kernel_c_api.h>
#include <consensus/bridge/kernel_marshalling.h>
#include <consensus/params.h>
#include <consensus/res_issuance.h>
#include <consensus/rigor_issuance.h>
#include <consensus/rigor_supply.h>
#include <consensus/validation.h>
#include <primitives/transaction.h>

#include <cstdio>
#include <cstring>
#include <string>

namespace {

const char* AssetName(rigor_asset_type a)
{
    switch (a) {
    case RIGOR_ASSET_ORU: return "ORU";
    case RIGOR_ASSET_RES: return "RES";
    case RIGOR_ASSET_NONE: return "NONE";
    default: return "INVALID";
    }
}

rigor_asset_type MapAsset(AssetType a)
{
    switch (a) {
    case ASSET_ORU: return RIGOR_ASSET_ORU;
    case ASSET_RES: return RIGOR_ASSET_RES;
    case ASSET_NONE: return RIGOR_ASSET_NONE;
    default: return RIGOR_ASSET_INVALID;
    }
}

std::string Hex32(const rigor_u256& h)
{
    static const char* k = "0123456789abcdef";
    std::string s(64, '0');
    for (int i = 0; i < 32; ++i) {
        s[static_cast<size_t>(i) * 2] = k[(h.bytes[i] >> 4) & 0xf];
        s[static_cast<size_t>(i) * 2 + 1] = k[h.bytes[i] & 0xf];
    }
    return s;
}

std::string IssuanceJson(const rigor_issuance_result& r)
{
    char buf[256];
    std::snprintf(buf, sizeof(buf),
        "{\"epoch\":%u,\"oru_subsidy\":%llu,\"res_destroy_cost\":%llu}",
        r.epoch,
        static_cast<unsigned long long>(r.oru_subsidy),
        static_cast<unsigned long long>(r.res_destroy_cost));
    return buf;
}

std::string ClassifyJson(rigor_asset_type a)
{
    char buf[48];
    std::snprintf(buf, sizeof(buf), "{\"asset\":\"%s\"}", AssetName(a));
    return buf;
}

std::string DrawJson(const rigor_draw_result& r)
{
    if (!r.has_winner) return "{\"has_winner\":false}";
    char buf[128];
    std::snprintf(buf, sizeof(buf), "{\"has_winner\":true,\"winner\":\"%s\"}", Hex32(r.winner).c_str());
    return buf;
}

std::string SupplyJson(const ConsensusKernelShadowOutput& o)
{
    char buf[192];
    std::snprintf(buf, sizeof(buf),
        "{\"ok\":%s,\"delta\":%lld,\"ledger_oru_delta\":%lld}",
        o.supply_ok ? "true" : "false",
        static_cast<long long>(o.supply.delta),
        static_cast<long long>(o.ledger_oru_delta));
    return buf;
}

std::string OutputJson(ConsensusKernelSurface surface, const ConsensusKernelShadowOutput& o)
{
    switch (surface) {
    case CK_SURFACE_ISSUANCE: return IssuanceJson(o.issuance);
    case CK_SURFACE_CLASSIFY: return ClassifyJson(o.asset);
    case CK_SURFACE_DRAW: return DrawJson(o.draw);
    case CK_SURFACE_SUPPLY: return SupplyJson(o);
    }
    return "{}";
}

bool SameIssuance(const rigor_issuance_result& a, const rigor_issuance_result& b)
{
    return a.epoch == b.epoch && a.oru_subsidy == b.oru_subsidy &&
           a.res_destroy_cost == b.res_destroy_cost;
}

bool SameDraw(const rigor_draw_result& a, const rigor_draw_result& b)
{
    if (a.has_winner != b.has_winner) return false;
    if (!a.has_winner) return true;
    return std::memcmp(a.winner.bytes, b.winner.bytes, 32) == 0;
}

bool SameOut(ConsensusKernelSurface surface, const ConsensusKernelShadowOutput& a,
    const ConsensusKernelShadowOutput& b)
{
    switch (surface) {
    case CK_SURFACE_ISSUANCE: return SameIssuance(a.issuance, b.issuance);
    case CK_SURFACE_CLASSIFY: return a.asset == b.asset;
    case CK_SURFACE_DRAW: return SameDraw(a.draw, b.draw);
    case CK_SURFACE_SUPPLY:
        return a.supply_ok == b.supply_ok && a.supply.delta == b.supply.delta &&
               a.ledger_oru_delta == b.ledger_oru_delta;
    }
    return false;
}

const char* SurfaceName(ConsensusKernelSurface s)
{
    switch (s) {
    case CK_SURFACE_ISSUANCE: return "issuance";
    case CK_SURFACE_CLASSIFY: return "classify";
    case CK_SURFACE_DRAW: return "draw";
    case CK_SURFACE_SUPPLY: return "supply";
    }
    return "unknown";
}

} // namespace

bool ConsensusKernelShadowExecutor::RunCpp(const ConsensusKernelInput& in,
    ConsensusKernelShadowOutput& out) const
{
    out = ConsensusKernelShadowOutput{};
    switch (in.surface) {
    case CK_SURFACE_ISSUANCE: {
        const RigorIssuance g = ComputeRigorIssuance(static_cast<int>(in.height), KernelFrozenParams());
        out.issuance.oru_subsidy = static_cast<uint64_t>(g.oru_subsidy);
        out.issuance.res_destroy_cost = static_cast<uint64_t>(
            Consensus::ResDestroyCostAtHeight(static_cast<int>(in.height)));
        out.issuance.epoch = static_cast<uint32_t>(g.epoch);
        return true;
    }
    case CK_SURFACE_CLASSIFY: {
        const CScript spk = RigorScriptFromBytes(in.script, in.script_len);
        out.asset = MapAsset(ClassifyRigorAsset(spk));
        return true;
    }
    case CK_SURFACE_DRAW: {
        out.draw.has_winner = 0;
        return true;
    }
    case CK_SURFACE_SUPPLY: {
        RigorSupplyDeltaInput din;
        din.height = static_cast<int>(in.supply.height);
        din.fees_oru = static_cast<CAmount>(in.supply.fees_oru);
        din.fee_burn = static_cast<CAmount>(in.supply.burn);
        din.converge_sum = static_cast<CAmount>(in.supply.converge);
        din.res_mint = static_cast<CAmount>(in.supply.mint);
        din.oru_coinbase = static_cast<CAmount>(in.supply.oru_coinbase);
        din.res_bootstrap = 0;
        din.fees_res = 0;
        din.res_coinbase = RigorResCoinbaseExpected(din.fees_res, din.res_bootstrap);
        BlockValidationState state;
        RigorSupplyDeltaResult dout;
        out.supply_ok = RigorSupplyDelta(din, KernelFrozenParams(), dout, state);
        if (!out.supply_ok) {
            out.supply.delta = 0;
            out.ledger_oru_delta = 0;
            return true;
        }
        out.supply.delta = static_cast<int64_t>(dout.oru_circulating_delta);
        out.ledger_oru_delta = LedgerDeltaBridge(out.supply.delta);
        return true;
    }
    }
    return false;
}

bool ConsensusKernelShadowExecutor::RunAbi(const ConsensusKernelInput& in,
    ConsensusKernelShadowOutput& out) const
{
    out = ConsensusKernelShadowOutput{};
    switch (in.surface) {
    case CK_SURFACE_ISSUANCE:
        return rigor_compute_issuance(in.height, &out.issuance);
    case CK_SURFACE_CLASSIFY:
        return rigor_classify_script(in.script, in.script_len, &out.asset);
    case CK_SURFACE_DRAW:
        return rigor_resolve_draw(in.candidates, in.candidate_count, &in.draw_ctx, &out.draw);
    case CK_SURFACE_SUPPLY:
        out.supply_ok = rigor_supply_delta(&in.supply, &out.supply);
        out.ledger_oru_delta = out.supply_ok ? LedgerDeltaBridge(out.supply.delta) : 0;
        return true;
    }
    return false;
}

ConsensusKernelShadowDiscrepancy ConsensusKernelShadowExecutor::Compare(
    const ConsensusKernelInput& in) const
{
    ConsensusKernelShadowDiscrepancy d;
    ConsensusKernelShadowOutput cpp, abi;
    if (!RunCpp(in, cpp) || !RunAbi(in, abi)) {
        d.found = true;
        d.surface = SurfaceName(in.surface);
        d.cpp_json = "{\"ok\":false}";
        d.ext_json = "{\"ok\":false}";
        return d;
    }
    if (!SameOut(in.surface, cpp, abi)) {
        d.found = true;
        d.surface = SurfaceName(in.surface);
        d.cpp_json = OutputJson(in.surface, cpp);
        d.ext_json = OutputJson(in.surface, abi);
        return d;
    }
    return d;
}

int ConsensusKernelShadowExecutor::RunDeterministicBattery(int iterations,
    ConsensusKernelShadowDiscrepancy* first) const
{
    if (iterations <= 0) return 0;

    static const uint32_t kHeights[] = {0, 1, 99, 100, 209999, 210000, 419999};
    static const uint8_t kOru[] = {
        0x00, 0x14, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11,
        0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11};
    static const uint8_t kRes[] = {
        0x51, 0x20, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
        0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
        0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa};
    static const uint8_t kTrunc[] = {0x00, 0x14};
    static const uint8_t kUnknown[] = {
        0x76, 0xa9, 0x14, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x88, 0xac};
    static const uint8_t kV032[] = {
        0x00, 0x20, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

    rigor_u256 c{}, b{}, t0{}, tx1{}, tx2{};
    for (int i = 0; i < 32; ++i) {
        c.bytes[i] = 0x11;
        b.bytes[i] = 0xaa;
    }
    tx1.bytes[31] = 0x01;
    tx2.bytes[31] = 0x02;
    rigor_u256 tprod{};
    for (int i = 0; i < 32; ++i) tprod.bytes[i] = 0x55;
    const uint64_t sat = 209715200000000ULL;
    rigor_candidate one{tx1, sat};
    rigor_candidate two[2] = {{tx1, sat}, {tx2, sat}};
    rigor_candidate swap[2] = {{tx2, sat}, {tx1, sat}};
    rigor_candidate zero{tx1, 0};

    int mismatches = 0;
    for (int i = 0; i < iterations; ++i) {
        ConsensusKernelInput in{};
        const int lane = i % 16;
        if (lane < 7) {
            in.surface = CK_SURFACE_ISSUANCE;
            in.height = kHeights[lane];
        } else if (lane < 13) {
            in.surface = CK_SURFACE_CLASSIFY;
            switch (lane) {
            case 7:
                in.script = kOru;
                in.script_len = sizeof(kOru);
                break;
            case 8:
                in.script = kRes;
                in.script_len = sizeof(kRes);
                break;
            case 9:
                in.script = nullptr;
                in.script_len = 0;
                break;
            case 10:
                in.script = kTrunc;
                in.script_len = sizeof(kTrunc);
                break;
            case 11:
                in.script = kUnknown;
                in.script_len = sizeof(kUnknown);
                break;
            default:
                in.script = kV032;
                in.script_len = sizeof(kV032);
                break;
            }
        } else if (lane < 15) {
            in.surface = CK_SURFACE_DRAW;
            in.draw_ctx.payout_height = 101;
            in.draw_ctx.convergence_root = c;
            in.draw_ctx.source_block_hash = b;
            in.draw_ctx.target = (lane == 13) ? tprod : t0;
            if (i % 5 == 0) {
                in.candidates = nullptr;
                in.candidate_count = 0;
            } else if (i % 5 == 1) {
                in.candidates = &one;
                in.candidate_count = 1;
            } else if (i % 5 == 2) {
                in.candidates = two;
                in.candidate_count = 2;
            } else if (i % 5 == 3) {
                in.candidates = swap;
                in.candidate_count = 2;
            } else {
                in.candidates = &zero;
                in.candidate_count = 1;
            }
        } else {
            in.surface = CK_SURFACE_SUPPLY;
            in.supply.height = kHeights[i % 7];
            in.supply.fees_oru = static_cast<uint64_t>(i % 3) * 100;
            in.supply.burn = static_cast<uint64_t>(i % 2) * 5;
            in.supply.converge = static_cast<uint64_t>(i % 4);
            in.supply.mint = 0;
            in.supply.has_winner = 0;
            in.supply.card = 0;
            in.supply.oru_coinbase = 0;
        }

        const ConsensusKernelShadowDiscrepancy d = Compare(in);
        if (d.found) {
            ++mismatches;
            if (first && !first->found) *first = d;
            break;
        }

        ConsensusKernelShadowOutput a, b2;
        if (!RunCpp(in, a) || !RunCpp(in, b2) || !SameOut(in.surface, a, b2)) {
            ++mismatches;
            if (first && !first->found) {
                first->found = true;
                first->surface = SurfaceName(in.surface);
                first->cpp_json = OutputJson(in.surface, a);
                first->ext_json = "{\"repeat\":\"mismatch\"}";
            }
            break;
        }
    }
    return mismatches;
}
