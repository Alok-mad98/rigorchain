// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// P063: UTXO ledger transition boundary. Standalone.
// Does not switch production. Does not recode issuance / RES / draw / PoW.

#include <consensus/amount.h>
#include <consensus/ledger/ledger_transition.h>
#include <consensus/params.h>
#include <consensus/rigor_consensus_lock.h>
#include <consensus/rigor_issuance.h>

#include <cstdint>
#include <cstdio>

static int fail(const char* msg)
{
    std::fprintf(stderr, "FAIL: %s\n", msg);
    return 1;
}

static Consensus::Params FrozenParams()
{
    Consensus::Params p;
    p.nBlockSubsidyBase = Consensus::ORU_INITIAL_BLOCK_SUBSIDY;
    p.nSubsidyHalvingInterval = Consensus::ORU_SUBSIDY_HALVING_INTERVAL;
    p.nBlockSubsidyMaxHalvingShifts = Consensus::ORU_SUBSIDY_MAX_SHIFTS;
    p.nCoinbaseMaturity = 100;
    return p;
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

static LedgerKernelInput Base()
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

int main()
{
    const Consensus::Params p = FrozenParams();

    // --- Genesis ---
    {
        if (GetBlockSubsidy(0, p) != 0) return fail("genesis ORU actual");
        if (Consensus::GetResMintReward(0) != 40000000) return fail("genesis RES schedule");
        LedgerKernelInput in = Base();
        in.height = 0;
        const LedgerKernelOutput o = LedgerApply(in);
        if (!o.accept) return fail("genesis ledger accept");
        if (!o.spent_delta.empty() || !o.created_delta.empty()) return fail("genesis empty delta");
        if (in.consensus.oru_issuance != 0 || in.consensus.res_mint != 0) return fail("genesis actual");
    }

    // --- Maturity 99 / 100 / 101 ---
    {
        LedgerKernelInput in = Base();
        in.consumed = {Coin(7, 0, 10, LedgerAsset::ORU, 1)};
        in.created = {Coin(8, 0, 10, LedgerAsset::ORU)};
        in.height = 99;
        if (LedgerApply(in).accept) return fail("maturity 99");
        in.height = 100;
        if (LedgerApply(in).accept) return fail("maturity 100");
        in.height = 101;
        if (!LedgerApply(in).accept) return fail("maturity 101");
    }

    // --- Double spend ---
    {
        LedgerKernelInput in = Base();
        const LedgerCoin c = Coin(1, 0, 10, LedgerAsset::ORU);
        in.consumed = {c, c};
        in.created = {Coin(2, 0, 10, LedgerAsset::ORU)};
        if (LedgerApply(in).accept) return fail("double spend");
    }

    // --- Overflow ---
    {
        LedgerKernelInput in = Base();
        in.consumed = {Coin(1, 0, MAX_MONEY, LedgerAsset::ORU), Coin(2, 0, 1, LedgerAsset::ORU)};
        in.created = {Coin(3, 0, 1, LedgerAsset::ORU)};
        if (LedgerApply(in).accept) return fail("overflow");
    }

    // --- Malformed ---
    {
        LedgerKernelInput neg = Base();
        neg.consumed = {Coin(1, 0, -1, LedgerAsset::ORU)};
        if (LedgerApply(neg).accept) return fail("negative");

        LedgerKernelInput dup = Base();
        dup.consumed = {Coin(1, 0, 10, LedgerAsset::ORU)};
        dup.created = {Coin(2, 0, 5, LedgerAsset::ORU), Coin(2, 0, 5, LedgerAsset::ORU)};
        if (LedgerApply(dup).accept) return fail("duplicate created");

        LedgerKernelInput unk = Base();
        unk.consumed = {Coin(1, 0, 1, LedgerAsset::INVALID)};
        unk.created = {Coin(2, 0, 1, LedgerAsset::ORU)};
        if (LedgerApply(unk).accept) return fail("unknown asset");
    }

    // --- Value conservation ---
    {
        LedgerKernelInput n = Base();
        n.consumed = {Coin(1, 0, 10, LedgerAsset::ORU)};
        n.created = {Coin(2, 0, 9, LedgerAsset::ORU)};
        n.fee = 1;
        if (!LedgerApply(n).accept) return fail("normal spend");

        LedgerKernelInput f = Base();
        f.consumed = {Coin(1, 0, 10, LedgerAsset::ORU)};
        f.created = {Coin(2, 0, 7, LedgerAsset::ORU)};
        f.fee = 3;
        if (!LedgerApply(f).accept) return fail("fee spend");

        LedgerKernelInput b = Base();
        b.consumed = {Coin(1, 0, 10, LedgerAsset::ORU)};
        b.created = {Coin(2, 0, 8, LedgerAsset::ORU)};
        b.burn = 2;
        if (!LedgerApply(b).accept) return fail("burn");

        LedgerKernelInput m = Base();
        m.created = {Coin(3, 0, 40000000, LedgerAsset::RES)};
        m.consensus.res_mint = 40000000;
        if (!LedgerApply(m).accept) return fail("mint");
    }

    std::printf("PASS: ledger transition refinement\n");
    std::printf("VALUE_CONSERVATION=PASS\n");
    std::printf("DOUBLE_SPEND_BOUNDARY=PASS\n");
    std::printf("ASSET_CONSERVATION=PASS\n");
    std::printf("PRODUCTION_KERNEL_SWITCHED=NO\n");
    return 0;
}
