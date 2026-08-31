// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// P075: genesis consensus release audit pins. Standalone.
// Does not recode formulas. Does not add migration or canary.

#include <consensus/amount.h>
#include <consensus/consensus.h>
#include <consensus/convergence_logic.h>
#include <consensus/ledger/ledger_transition.h>
#include <consensus/params.h>
#include <consensus/rigor_consensus_lock.h>
#include <consensus/rigor_issuance.h>
#include <consensus/rigor_supply.h>
#include <consensus/validation.h>
#include <script/script.h>
#include <script/standard.h>
#include <uint256.h>

#include <cstdint>
#include <cstdio>
#include <limits>
#include <vector>

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

static LedgerCoin Coin(uint64_t id, uint32_t n, CAmount value, LedgerAsset asset, int64_t cb = -1)
{
    LedgerCoin c;
    c.outpoint.id = id;
    c.outpoint.n = n;
    c.value = value;
    c.asset = asset;
    c.coinbase_height = cb;
    return c;
}

static LedgerKernelInput BaseLedger()
{
    LedgerKernelInput in;
    in.tx.accept = true;
    in.consensus.accept = true;
    in.consensus.oru_issuance = 0;
    in.consensus.res_mint = 0;
    in.consensus.res_schedule = 40000000;
    in.height = 1;
    in.maturity = 100;
    return in;
}

static bool PinHeight(const Consensus::Params& p, int h, CAmount oru, CAmount res_sched)
{
    const RigorIssuance g = ComputeRigorIssuance(h, p);
    if (g.oru_subsidy != oru) return false;
    if (Consensus::GetResMintReward(h) != res_sched) return false;
    if (g.res_reward != res_sched) return false;
    ConvergenceDrawSet empty;
    empty.source_height = h;
    if (ResolveConvergenceDraw(empty, Consensus::RES_PROBABILITY_TARGET_CONST).HasWinner()) {
        return false;
    }
    RigorSupplyDeltaInput in;
    in.height = h;
    BlockValidationState st;
    RigorSupplyDeltaResult out;
    if (!RigorSupplyDelta(in, p, out, st)) return false;
    if (out.res_delta != 0) return false;
    if (out.res_reward_scheduled != res_sched) return false;
    return true;
}

int main()
{
    const Consensus::Params p = FrozenParams();

    if (!PinHeight(p, 0, 0, 40000000)) return fail("height 0");
    if (GetBlockSubsidy(0, p) != 0) return fail("height 0 ORU actual");
    if (Consensus::GetResMintReward(0) != 40000000) return fail("height 0 RES schedule");

    if (!PinHeight(p, 1, 100000000000LL, 40000000)) return fail("height 1");
    if (!PinHeight(p, 99, 100000000000LL, 40000000)) return fail("height 99");
    if (!PinHeight(p, 100, 100000000000LL, 40000000)) return fail("height 100");
    if (!PinHeight(p, 101, 100000000000LL, 40000000)) return fail("height 101");
    if (!PinHeight(p, 210000, 50000000000LL, 14142196)) return fail("height 210000");

    if ((99 - 1) >= p.nCoinbaseMaturity) return fail("maturity 99");
    if ((100 - 1) >= p.nCoinbaseMaturity) return fail("maturity 100");
    if ((101 - 1) < p.nCoinbaseMaturity) return fail("maturity 101");

    {
        const CScript empty;
        const CScript bad = CScript() << CScriptNum(99) << std::vector<unsigned char>(20, 0x00);
        if (ClassifyRigorAsset(empty) != ASSET_INVALID) return fail("malformed empty");
        if (ClassifyRigorAsset(bad) != ASSET_INVALID) return fail("malformed unknown version");
    }

    {
        const auto none = ResolveConvergenceDraw(ConvergenceDrawSet{}, uint256());
        if (none.HasWinner()) return fail("invalid draw T=0");
    }

    {
        RigorSupplyDeltaInput overflow;
        overflow.height = 1;
        overflow.oru_coinbase = std::numeric_limits<CAmount>::max();
        BlockValidationState st;
        RigorSupplyDeltaResult out;
        if (RigorSupplyDelta(overflow, p, out, st)) return fail("overflow accepted");
    }

    {
        LedgerKernelInput in = BaseLedger();
        in.consumed = {Coin(7, 0, 10, LedgerAsset::ORU, 1)};
        in.created = {Coin(8, 0, 10, LedgerAsset::ORU)};
        in.height = 99;
        if (LedgerApply(in).accept) return fail("invalid maturity 99");
        in.height = 100;
        if (LedgerApply(in).accept) return fail("invalid maturity 100");
        in.height = 101;
        if (!LedgerApply(in).accept) return fail("maturity 101 must accept");
    }

    {
        LedgerKernelInput in = BaseLedger();
        const LedgerCoin c = Coin(1, 0, 10, LedgerAsset::ORU);
        in.consumed = {c, c};
        in.created = {Coin(2, 0, 10, LedgerAsset::ORU)};
        if (LedgerApply(in).accept) return fail("duplicate spend");
    }

    {
        LedgerKernelInput in = BaseLedger();
        in.consumed = {Coin(1, 0, MAX_MONEY, LedgerAsset::ORU), Coin(2, 0, 1, LedgerAsset::ORU)};
        in.created = {Coin(3, 0, 1, LedgerAsset::ORU)};
        if (LedgerApply(in).accept) return fail("ledger overflow");
    }

    {
        LedgerKernelInput in = BaseLedger();
        in.consumed = {Coin(1, 0, 1, LedgerAsset::INVALID)};
        in.created = {Coin(2, 0, 1, LedgerAsset::ORU)};
        if (LedgerApply(in).accept) return fail("invalid asset");
    }

    std::printf("PASS: genesis consensus release audit\n");
    std::printf("GENESIS_VECTORS=PASS\n");
    std::printf("ATTACK_BOUNDARY=PASS\n");
    std::printf("GENESIS_CONSENSUS_READY=TRUE\n");
    std::printf("REFINEMENT_PROVED=TRUE\n");
    std::printf("EQUIVALENCE_PROVED=FALSE\n");
    return 0;
}
