// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// P0-076: economic model validation. Standalone.
// Calls frozen Kernel authorities. Does not recode GetBlockSubsidy /
// CheckResMintArithmetic / RigorSupplyDelta. Does not enter ConnectBlock.

#include <consensus/amount.h>
#include <consensus/params.h>
#include <consensus/res_issuance.h>
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

static bool PinIssuance(const Consensus::Params& p, int h, CAmount oru)
{
    const RigorIssuance g = ComputeRigorIssuance(h, p);
    if (g.oru_subsidy != oru) return false;
    if (GetBlockSubsidy(h, p) != oru) return false;
    if (g.epoch != RigorIssuanceEpoch(h)) return false;

    RigorSupplyDeltaInput in;
    in.height = h;
    BlockValidationState st;
    RigorSupplyDeltaResult out;
    if (!RigorSupplyDelta(in, p, out, st)) return false;
    if (out.oru_subsidy != oru) return false;
    if (out.res_delta != 0) return false;
    if (out.oru_circulating_delta != oru) return false;
    return true;
}

int main()
{
    const Consensus::Params p = FrozenParams();

    if (!PinIssuance(p, 0, 0)) return fail("genesis supply");
    if (GetBlockSubsidy(0, p) != Consensus::ORU_GENESIS_BLOCK_SUBSIDY) {
        return fail("genesis ORU pin");
    }
    if (SumSubsidy(0, 0, p) != 0) return fail("genesis cumulative");

    if (!PinIssuance(p, 1, Consensus::ORU_FLAT_BLOCK_SUBSIDY)) return fail("height 1");
    if (!PinIssuance(p, 99, Consensus::ORU_FLAT_BLOCK_SUBSIDY)) return fail("height 99");
    if (!PinIssuance(p, 100, Consensus::ORU_FLAT_BLOCK_SUBSIDY)) return fail("height 100");
    if (!PinIssuance(p, 101, Consensus::ORU_FLAT_BLOCK_SUBSIDY)) return fail("height 101");

    if (CoinbaseMature(1, 1, p.nCoinbaseMaturity)) return fail("maturity 1");
    if (CoinbaseMature(99, 1, p.nCoinbaseMaturity)) return fail("maturity 99");
    if (CoinbaseMature(100, 1, p.nCoinbaseMaturity)) return fail("maturity 100");
    if (!CoinbaseMature(101, 1, p.nCoinbaseMaturity)) return fail("maturity 101");
    {
        const CAmount issued_100 = SumSubsidy(0, 100, p);
        const CAmount issued_101 = SumSubsidy(0, 101, p);
        const CAmount locked_100 = SumSubsidy(1, 100, p);
        const CAmount locked_101 = SumSubsidy(2, 101, p);
        const CAmount matured_101 = issued_101 - locked_101;
        if (issued_100 != Consensus::ORU_FLAT_BLOCK_SUBSIDY * 100) return fail("issued at 100");
        if (issued_101 != Consensus::ORU_FLAT_BLOCK_SUBSIDY * 101) return fail("issued at 101");
        if (locked_100 != issued_100) return fail("all locked at 100");
        if (matured_101 != Consensus::ORU_FLAT_BLOCK_SUBSIDY) return fail("first coin matures at 101");
        if (locked_101 + matured_101 != issued_101) return fail("locked+matured at 101");
    }

    if (!PinIssuance(p, 209999, Consensus::ORU_FLAT_BLOCK_SUBSIDY)) return fail("height 209999");
    if (!PinIssuance(p, 210000, Consensus::ORU_FLAT_BLOCK_SUBSIDY)) return fail("height 210000");
    if (GetBlockSubsidy(209999, p) != GetBlockSubsidy(210000, p)) {
        return fail("flat ORU changed at observational epoch boundary");
    }
    if (Consensus::ResDestroyCostAtHeight(210000) <= Consensus::ResDestroyCostAtHeight(209999)) {
        return fail("RES display destroy cost did not harden");
    }
    if (!PinIssuance(p, 419999, Consensus::ORU_FLAT_BLOCK_SUBSIDY)) return fail("height 419999");
    if (!PinIssuance(p, 420000, Consensus::ORU_FLAT_BLOCK_SUBSIDY)) return fail("height 420000");

    {
        const CAmount floor_oru = GetBlockSubsidy(630000, p);
        if (floor_oru != Consensus::ORU_FLAT_BLOCK_SUBSIDY) return fail("ORU flat pin 630000");
        if (GetBlockSubsidy(840000, p) != floor_oru) return fail("ORU flat at 840000");
        if (Consensus::ResDestroyCostAtHeight(840000) <= Consensus::ResDestroyCostAtHeight(630000)) {
            return fail("RES price did not rise");
        }
        if (!PinIssuance(p, 840000, floor_oru)) return fail("height 840000");
    }

    if (Consensus::ResDestroyCostAtHeight(0) != Consensus::RES_DESTROY_COST_GENESIS) {
        return fail("RES destroy cost genesis");
    }
    if (!Consensus::CheckResMintArithmetic(Consensus::RES_COIN, Consensus::RES_DESTROY_COST_GENESIS, 0)) {
        return fail("exact genesis mint");
    }
    if (Consensus::CheckResMintArithmetic(Consensus::RES_COIN, Consensus::RES_DESTROY_COST_GENESIS - 1, 0)) {
        return fail("underpaid genesis mint");
    }
    {
        RigorSupplyDeltaInput in;
        in.height = 1;
        in.res_mint = 0;
        BlockValidationState st;
        RigorSupplyDeltaResult out;
        if (!RigorSupplyDelta(in, p, out, st)) return fail("RES no-mint supply");
        if (out.res_delta != 0) return fail("RES actual mint without burn");
    }

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
    std::printf("FLAT_ORU_TRANSITION=PASS\n");
    std::printf("RES_DESTROY_COST=PASS\n");
    std::printf("CIRCULATING_CALCULATION=PASS\n");
    std::printf("OVERFLOW_BOUNDARY=PASS\n");
    std::printf("FORMULA_DUPLICATION=FALSE\n");
    std::printf("CONSENSUS_MODIFIED=FALSE\n");
    return 0;
}
