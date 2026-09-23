// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// P055: production shadow battery. Verify-only. Not a Boost test_rigor object.
// Does not switch ConnectBlock. Does not recode formulas.

#ifndef RIGOR_KERNEL_SHADOW_VERIFY_ONLY
#define RIGOR_KERNEL_SHADOW_VERIFY_ONLY
#endif
#ifndef RIGOR_KERNEL_ABI_VERIFY_ONLY
#define RIGOR_KERNEL_ABI_VERIFY_ONLY
#endif

#include <consensus/bridge/kernel_marshalling.h>
#include <consensus/params.h>
#include <consensus/res_issuance.h>
#include <consensus/rigor_consensus_lock.h>
#include <consensus/rigor_issuance.h>
#include <consensus/shadow/ledger_delta_bridge.h>
#include <consensus/shadow/shadow_executor.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

static void DumpDiscrepancy(const char* input, const ConsensusKernelShadowDiscrepancy& d)
{
    std::fprintf(stderr, "CONSENSUS_KERNEL_SHADOW_DISCREPANCY_FOUND\n");
    std::fprintf(stderr, "surface: %s\n", d.surface.c_str());
    std::fprintf(stderr, "input: %s\n", input);
    std::fprintf(stderr, "cpp: %s\n", d.cpp_json.c_str());
    std::fprintf(stderr, "extracted: %s\n", d.ext_json.c_str());
}

static int hex_nibble(char c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
    if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
    return -1;
}

static std::vector<uint8_t> parse_hex(const std::string& s)
{
    if (s == "-" || s.empty()) return {};
    if (s.size() % 2 != 0) std::exit(2);
    std::vector<uint8_t> out;
    for (size_t i = 0; i < s.size(); i += 2) {
        const int hi = hex_nibble(s[i]);
        const int lo = hex_nibble(s[i + 1]);
        if (hi < 0 || lo < 0) std::exit(2);
        out.push_back(static_cast<uint8_t>((hi << 4) | lo));
    }
    return out;
}

static rigor_u256 u256_from_hex(const std::string& hex)
{
    rigor_u256 out{};
    const auto raw = parse_hex(hex);
    if (raw.size() != 32) std::exit(2);
    std::memcpy(out.bytes, raw.data(), 32);
    return out;
}

static bool expect_issuance(const ConsensusKernelShadowExecutor& ex, uint32_t h,
    uint32_t epoch, uint64_t oru, uint64_t res_sched)
{
    ConsensusKernelInput in{};
    in.surface = CK_SURFACE_ISSUANCE;
    in.height = h;
    ConsensusKernelShadowOutput o;
    if (!ex.RunCpp(in, o)) return false;
    if (o.issuance.epoch != epoch || o.issuance.oru_subsidy != oru ||
        o.issuance.res_destroy_cost != res_sched) {
        ConsensusKernelShadowDiscrepancy d;
        d.found = true;
        d.surface = "issuance";
        d.cpp_json = "{\"epoch\":" + std::to_string(o.issuance.epoch) + "}";
        d.ext_json = "{\"pin_mismatch\":true}";
        char buf[96];
        std::snprintf(buf, sizeof(buf), "{\"height\":%u}", h);
        DumpDiscrepancy(buf, d);
        return false;
    }
    if (h == 0 && res_sched == 0) {
        ConsensusKernelShadowDiscrepancy d;
        d.found = true;
        d.surface = "issuance";
        d.cpp_json = "{\"res_destroy_cost\":0}";
        d.ext_json = "{\"price_collapsed\":true}";
        DumpDiscrepancy("{\"height\":0}", d);
        return false;
    }
    const ConsensusKernelShadowDiscrepancy d = ex.Compare(in);
    if (d.found) {
        char buf[96];
        std::snprintf(buf, sizeof(buf), "{\"height\":%u}", h);
        DumpDiscrepancy(buf, d);
        return false;
    }
    return true;
}

int main()
{
    const ConsensusKernelShadowExecutor ex;

    if (Consensus::ResDestroyCostAtHeight(0) != Consensus::RES_DESTROY_COST_GENESIS ||
        Consensus::ResDestroyCostAtHeight(0) != 102400000000LL) {
        std::fprintf(stderr, "CONSENSUS_KERNEL_SHADOW_DISCREPANCY_FOUND\n");
        std::fprintf(stderr, "ResDestroyCostAtHeight(0) pin failed\n");
        return 1;
    }
    {
        ConsensusKernelInput in{};
        in.surface = CK_SURFACE_ISSUANCE;
        in.height = 0;
        ConsensusKernelShadowOutput o;
        ex.RunCpp(in, o);
        if (o.issuance.res_destroy_cost == 0) {
            std::fprintf(stderr, "CONSENSUS_KERNEL_SHADOW_DISCREPANCY_FOUND\n");
            std::fprintf(stderr, "FORBIDDEN: schedule at 0 collapsed to 0\n");
            return 1;
        }
        if (RigorIssuanceEpoch(0) != 0 || ComputeRigorIssuance(0, KernelFrozenParams()).oru_subsidy != 0) {
            std::fprintf(stderr, "CONSENSUS_KERNEL_SHADOW_DISCREPANCY_FOUND\n");
            std::fprintf(stderr, "height0 ORU/epoch pin failed\n");
            return 1;
        }
    }

    struct IssPin {
        uint32_t h, epoch;
        uint64_t oru, res;
    };
    const IssPin pins[] = {
        {0, 0, 0, 102400000000ULL},
        {1, 0, 12800000000ULL, 102400006103ULL},
        {99, 0, 12800000000ULL, 102400604248ULL},
        {100, 0, 12800000000ULL, 102400610352ULL},
        {209999, 0, 12800000000ULL, 103685743011ULL},
        {210000, 1, 12800000000ULL, 103685749152ULL},
        {419999, 1, 12800000000ULL, 104979513869ULL},
    };
    for (const IssPin& p : pins) {
        if (!expect_issuance(ex, p.h, p.epoch, p.oru, p.res)) return 1;
    }

    struct ClPin {
        const char* hex;
        rigor_asset_type want;
        const char* name;
    };
    const ClPin cls[] = {
        {"00141111111111111111111111111111111111111111", RIGOR_ASSET_ORU, "ORU"},
        {"5120aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa", RIGOR_ASSET_RES, "RES"},
        {"-", RIGOR_ASSET_INVALID, "invalid_empty"},
        {"deadbeef", RIGOR_ASSET_INVALID, "invalid_unknown"},
        {"0014", RIGOR_ASSET_INVALID, "malformed_witness"},
        {"00200000000000000000000000000000000000000000000000000000000000000000", RIGOR_ASSET_INVALID, "unknown_v0_32"},
        {"76a914000000000000000000000000000000000000000088ac", RIGOR_ASSET_INVALID, "unknown_p2pkh"},
    };
    for (const ClPin& c : cls) {
        auto raw = parse_hex(c.hex);
        ConsensusKernelInput in{};
        in.surface = CK_SURFACE_CLASSIFY;
        in.script = raw.empty() ? nullptr : raw.data();
        in.script_len = raw.size();
        ConsensusKernelShadowOutput o;
        if (!ex.RunCpp(in, o) || o.asset != c.want) {
            ConsensusKernelShadowDiscrepancy d;
            d.found = true;
            d.surface = "classify";
            d.cpp_json = std::string("{\"asset\":") + std::to_string(static_cast<int>(o.asset)) + "}";
            d.ext_json = std::string("{\"want\":") + c.name + "}";
            DumpDiscrepancy(c.name, d);
            return 1;
        }
        const auto d = ex.Compare(in);
        if (d.found) {
            DumpDiscrepancy(c.name, d);
            return 1;
        }
    }

    {
        ConsensusKernelInput in{};
        in.surface = CK_SURFACE_SUPPLY;
        in.supply.height = 1;
        in.supply.fees_oru = 100;
        in.supply.burn = 5;
        in.supply.converge = 10;
        ConsensusKernelShadowOutput o;
        if (!ex.RunCpp(in, o) || !o.supply_ok) {
            DumpDiscrepancy("{\"supply\":\"burn_converge\"}",
                ConsensusKernelShadowDiscrepancy{true, "supply", "{}", "{\"ok\":false}"});
            return 1;
        }
        if (o.ledger_oru_delta != LedgerDeltaBridge(o.supply.delta)) {
            DumpDiscrepancy("{\"supply\":\"ledger_bridge\"}",
                ConsensusKernelShadowDiscrepancy{true, "supply", std::to_string(o.supply.delta),
                    std::to_string(o.ledger_oru_delta)});
            return 1;
        }
        const auto d = ex.Compare(in);
        if (d.found) {
            DumpDiscrepancy("{\"supply\":\"burn_converge\"}", d);
            return 1;
        }
    }

    ConsensusKernelShadowDiscrepancy first;
    const int bad = ex.RunDeterministicBattery(10000, &first);
    if (bad != 0 || first.found) {
        DumpDiscrepancy("{\"battery\":10000}", first);
        return 1;
    }

    std::printf("PASS: consensus kernel shadow battery\n");
    std::printf("CPP_KERNEL_EXECUTION=PASS\n");
    std::printf("SHADOW_COMPARE=PASS\n");
    std::printf("DISCREPANCY=NONE\n");
    std::printf("iterations=10000\n");
    return 0;
}
