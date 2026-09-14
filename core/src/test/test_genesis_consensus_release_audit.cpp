// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// P075: genesis consensus release audit pins. Standalone.
// Does not recode formulas. Does not add migration or canary.

#include <consensus/amount.h>
#include <consensus/consensus.h>
#include <consensus/ledger/ledger_transition.h>
#include <consensus/params.h>
#include <consensus/res_issuance.h>
#include <consensus/rigor_consensus_lock.h>
#include <consensus/rigor_issuance.h>
#include <consensus/rigor_supply.h>
#include <consensus/validation.h>
#include <script/script.h>
#include <script/standard.h>

#include <cstdint>
#include <cstdio>
#include <limits>
#include <vector>

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
    in.consensus.res_schedule = 0;
    in.height = 1;
    in.maturity = 100;
    return in;
}

static bool PinHeight(const Consensus::Params& p, int h, CAmount oru, CAmount rate)
{
    const RigorIssuance g = ComputeRigorIssuance(h, p);
    if (g.oru_subsidy != oru) return false;
    if (Consensus::ResDestroyCostAtHeight(h) != rate) return false;
    RigorSupplyDeltaInput in;
    in.height = h;
    BlockValidationState st;
    RigorSupplyDeltaResult out;
    if (!RigorSupplyDelta(in, p, out, st)) return false;
    if (out.res_delta != 0) return false;
    return true;
}

int main()
{
    const Consensus::Params p = FrozenParams();

    if (!PinHeight(p, 0, 0, Consensus::RES_DESTROY_COST_GENESIS)) return fail("height 0");
    if (GetBlockSubsidy(0, p) != 0) return fail("height 0 ORU actual");
    if (Consensus::ResDestroyCostAtHeight(0) != Consensus::RES_DESTROY_COST_GENESIS) {
        return fail("height 0 RATE");
    }

    if (!PinHeight(p, 1, Consensus::ORU_FLAT_BLOCK_SUBSIDY, 102400006103LL)) return fail("height 1");
    if (!PinHeight(p, 99, Consensus::ORU_FLAT_BLOCK_SUBSIDY, 102400604248LL)) return fail("height 99");
    if (!PinHeight(p, 100, Consensus::ORU_FLAT_BLOCK_SUBSIDY, 102400610352LL)) return fail("height 100");
    if (!PinHeight(p, 101, Consensus::ORU_FLAT_BLOCK_SUBSIDY, 102400616456LL)) return fail("height 101");
    if (!PinHeight(p, 210000, Consensus::ORU_FLAT_BLOCK_SUBSIDY, 103685749152LL)) return fail("height 210000");

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
    std::printf("REFINEMENT_PROVED=FALSE\n");
    std::printf("EQUIVALENCE_PROVED=FALSE\n");
    return 0;
}
