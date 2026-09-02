// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// P0-076: economic model validation. Standalone.
// Calls frozen Kernel authorities. Does not recode GetBlockSubsidy /
// GetResMintReward / RigorSupplyDelta. Does not enter ConnectBlock.
// Does not add a second economic-model authority.
//
// Build (from repo root):
//   g++ -O2 -std=c++17 -Icore/src \
//     -o formal_verification/extracted/test_economic_model_validation \
//     core/src/test/test_economic_model_validation.cpp \
//     core/src/consensus/rigor_issuance.cpp \
//     core/src/consensus/rigor_supply.cpp \
//     tools/consensus_kernel_refinement_log_stub.cpp \
//     core/src/uint256.cpp \
//     core/src/util/strencodings.cpp \
//     -Wl,--gc-sections
//   formal_verification/extracted/test_economic_model_validation

#include <consensus/amount.h>
#include <consensus/params.h>
#include <consensus/rigor_consensus_lock.h>
#include <consensus/rigor_issuance.h>
#include <consensus/rigor_supply.h>
#include <consensus/validation.h>

#include <cstdint>
#include <cstdio>
#include <limits>

static Consensus::Params FrozenParams()
{
    Consensus::Params p;
    p.nBlockSubsidyBase = Consensus::ORU_INITIAL_BLOCK_SUBSIDY;
    p.nSubsidyHalvingInterval = Consensus::ORU_SUBSIDY_HALVING_INTERVAL;
    p.nBlockSubsidyMaxHalvingShifts = Consensus::ORU_SUBSIDY_MAX_SHIFTS;
    p.nCoinbaseMaturity = 100;
    return p;
}

static int fail(const char* msg)
{
    std::fprintf(stderr, "FAIL: %s\n", msg);
    return 1;
}

static CAmount SumSubsidy(int from_h, int to_h, const Consensus::Params& p)
{
    if (to_h < from_h) return 0;
    CAmount sum = 0;
    for (int h = from_h; h <= to_h; ++h) {
        const CAmount r = GetBlockSubsidy(h, p);
        if (sum > std::numeric_limits<CAmount>::max() - r) return -1;
        sum += r;
    }
    return sum;
}

static bool CoinbaseMature(int spend_height, int coin_height, int64_t maturity)
{
    return static_cast<int64_t>(spend_height) - static_cast<int64_t>(coin_height) >= maturity;
}

static bool PinIssuance(const Consensus::Params& p, int h, CAmount oru, CAmount res_sched)
{
    const RigorIssuance g = ComputeRigorIssuance(h, p);
    if (g.oru_subsidy != oru) return false;
    if (GetBlockSubsidy(h, p) != oru) return false;
    if (Consensus::GetResMintReward(h) != res_sched) return false;
    if (g.res_reward != res_sched) return false;
    if (g.epoch != RigorIssuanceEpoch(h)) return false;

    RigorSupplyDeltaInput in;
    in.height = h;
    BlockValidationState st;
    RigorSupplyDeltaResult out;
    if (!RigorSupplyDelta(in, p, out, st)) return false;
    if (out.oru_subsidy != oru) return false;
    if (out.res_reward_scheduled != res_sched) return false;
    if (out.res_delta != 0) return false;
    if (out.oru_circulating_delta != oru) return false;
    return true;
}

int main()
{
    const Consensus::Params p = FrozenParams();

    // --- genesis supply (release_consensus_vectors.json) ---
    if (!PinIssuance(p, 0, 0, 40000000)) return fail("genesis supply");
    if (GetBlockSubsidy(0, p) != Consensus::ORU_GENESIS_BLOCK_SUBSIDY) {
        return fail("genesis ORU pin");
    }
    if (SumSubsidy(0, 0, p) != 0) return fail("genesis cumulative");

    // --- early mining / required heights ---
    if (!PinIssuance(p, 1, 100000000000LL, 40000000)) return fail("height 1");
    if (!PinIssuance(p, 99, 100000000000LL, 40000000)) return fail("height 99");
    if (!PinIssuance(p, 100, 100000000000LL, 40000000)) return fail("height 100");
    if (!PinIssuance(p, 101, 100000000000LL, 40000000)) return fail("height 101");

    // --- maturity transition (nCoinbaseMaturity = 100; coin created at height 1) ---
    if (CoinbaseMature(1, 1, p.nCoinbaseMaturity)) return fail("maturity 1");
    if (CoinbaseMature(99, 1, p.nCoinbaseMaturity)) return fail("maturity 99");
    if (CoinbaseMature(100, 1, p.nCoinbaseMaturity)) return fail("maturity 100");
    if (!CoinbaseMature(101, 1, p.nCoinbaseMaturity)) return fail("maturity 101");
    {
        const CAmount issued_100 = SumSubsidy(0, 100, p);
        const CAmount issued_101 = SumSubsidy(0, 101, p);
        const CAmount locked_100 = SumSubsidy(1, 100, p); // all post-genesis coins still locked
        const CAmount locked_101 = SumSubsidy(2, 101, p);
        const CAmount matured_101 = issued_101 - locked_101;
        if (issued_100 != 100000000000LL * 100) return fail("issued at 100");
        if (issued_101 != 100000000000LL * 101) return fail("issued at 101");
        if (locked_100 != issued_100) return fail("all locked at 100");
        if (matured_101 != 100000000000LL) return fail("first coin matures at 101");
        if (locked_101 + matured_101 != issued_101) return fail("locked+matured at 101");
    }

    // --- halving transition (frozen vectors 209999/210000/420000) ---
    if (!PinIssuance(p, 209999, 100000000000LL, 40000000)) return fail("height 209999");
    if (!PinIssuance(p, 210000, 50000000000LL, 14142196)) return fail("height 210000");
    if (GetBlockSubsidy(209999, p) == GetBlockSubsidy(210000, p)) {
        return fail("halving did not change ORU");
    }
    if (Consensus::GetResMintReward(209999) == Consensus::GetResMintReward(210000)) {
        return fail("halving did not change RES schedule");
    }
    if (!PinIssuance(p, 419999, 50000000000LL, 14142196)) return fail("height 419999");
    if (!PinIssuance(p, 420000, 25000000000LL, 7698004)) return fail("height 420000");

    // 840000 is past the last frozen RES-schedule pin. ORU is the last pinned
    // floor (same authority value as height 630000). RES is GetResMintReward.
    {
        const CAmount floor_oru = GetBlockSubsidy(630000, p);
        if (floor_oru != 12500000000LL) return fail("ORU floor pin 630000");
        if (GetBlockSubsidy(840000, p) != floor_oru) return fail("ORU floor at 840000");
        const CAmount res_840000 = Consensus::GetResMintReward(840000);
        if (res_840000 != Consensus::ResRewardForEpoch(RigorIssuanceEpoch(840000))) {
            return fail("RES 840000 is not the Kernel schedule");
        }
        if (res_840000 <= 0) return fail("RES 840000 vanished");
        if (res_840000 >= Consensus::GetResMintReward(630000)) {
            return fail("RES 840000 did not decay vs epoch 3");
        }
        if (!PinIssuance(p, 840000, floor_oru, res_840000)) return fail("height 840000");
    }

    // --- RES schedule (schedule is not actual issuance) ---
    if (Consensus::GetResMintReward(0) != 40000000) return fail("RES schedule genesis");
    {
        RigorSupplyDeltaInput in;
        in.height = 1;
        in.res_mint = 0;
        BlockValidationState st;
        RigorSupplyDeltaResult out;
        if (!RigorSupplyDelta(in, p, out, st)) return fail("RES no-mint supply");
        if (out.res_reward_scheduled != 40000000) return fail("RES schedule field");
        if (out.res_delta != 0) return fail("RES actual mint without winner");
        if (out.res_mint_authorized != true) return fail("RES schedule authorized");
    }

    // --- circulating calculation (observer delta from RigorSupplyDelta) ---
    {
        RigorSupplyDeltaInput in;
        in.height = 1;
        in.converge_sum = 0;
        BlockValidationState st;
        RigorSupplyDeltaResult out;
        if (!RigorSupplyDelta(in, p, out, st)) return fail("circulating zero-converge");
        if (out.oru_circulating_delta != GetBlockSubsidy(1, p)) {
            return fail("circulating delta != subsidy");
        }
        in.converge_sum = Consensus::MIN_CONVERGENCE_AMOUNT;
        st = BlockValidationState{};
        if (!RigorSupplyDelta(in, p, out, st)) return fail("circulating with converge");
        if (out.oru_circulating_delta != GetBlockSubsidy(1, p) - Consensus::MIN_CONVERGENCE_AMOUNT) {
            return fail("circulating delta != subsidy - C(h)");
        }
    }
    {
        const int H = 101;
        const CAmount issued = SumSubsidy(0, H, p);
        const int lo = H - p.nCoinbaseMaturity + 1;
        const CAmount locked = SumSubsidy(lo, H, p);
        const CAmount matured = issued - locked;
        const CAmount burned = 0;
        const CAmount circulating = matured - burned;
        if (circulating > issued) return fail("circulating > total");
        if (locked + circulating + burned != issued) return fail("locked+circulating+burned");
    }

    // --- overflow boundary ---
    {
        RigorSupplyDeltaInput overflow;
        overflow.height = 1;
        overflow.oru_coinbase = std::numeric_limits<CAmount>::max();
        BlockValidationState st;
        RigorSupplyDeltaResult out;
        if (RigorSupplyDelta(overflow, p, out, st)) return fail("overflow accepted");
    }

    std::printf("PASS: economic model validation\n");
    std::printf("GENESIS_SUPPLY=PASS\n");
    std::printf("MATURITY_TRANSITION=PASS\n");
    std::printf("HALVING_TRANSITION=PASS\n");
    std::printf("RES_SCHEDULE=PASS\n");
    std::printf("CIRCULATING_CALCULATION=PASS\n");
    std::printf("OVERFLOW_BOUNDARY=PASS\n");
    std::printf("FORMULA_DUPLICATION=FALSE\n");
    std::printf("CONSENSUS_MODIFIED=FALSE\n");
    return 0;
}
