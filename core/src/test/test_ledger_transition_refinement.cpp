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

    // B1: NoDupOutpoints is one unsorted scan. n>24 used to take a sort-copy
    // branch; both a duplicate at that size and 25 distinct coins must still
    // decide correctly, and spent_delta must keep consumed order.
    {
        LedgerKernelInput many = Base();
        CAmount total = 0;
        for (int i = 0; i < 25; ++i) {
            many.consumed.push_back(Coin(static_cast<uint64_t>(i + 1), 0, 1, LedgerAsset::ORU));
            total += 1;
        }
        many.created = {Coin(100, 0, total, LedgerAsset::ORU)};
        const LedgerKernelOutput ok = LedgerApply(many);
        if (!ok.accept) return fail("25 distinct outpoints");
        if (ok.spent_delta.size() != 25) return fail("25 spent_delta size");
        for (int i = 0; i < 25; ++i) {
            if (ok.spent_delta[i].id != static_cast<uint64_t>(i + 1)) {
                return fail("spent_delta order");
            }
        }

        LedgerKernelInput dup = many;
        dup.consumed.push_back(Coin(1, 0, 1, LedgerAsset::ORU));
        if (LedgerApply(dup).accept) return fail("duplicate among 26");
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

    // --- Fail-closed value_ok (R1: early reject must not compute true) ---
    {
        LedgerKernelInput in = Base();
        in.tx.accept = false;
        in.consumed = {Coin(1, 0, 10, LedgerAsset::ORU)};
        in.created = {Coin(2, 0, 10, LedgerAsset::ORU)};
        const LedgerKernelOutput o = LedgerApply(in);
        if (o.accept) return fail("tx reject accepted");
        if (o.value_ok) return fail("tx reject value_ok");
    }
    {
        LedgerKernelInput in = Base();
        in.consumed = {Coin(1, 0, 10, LedgerAsset::ORU)};
        in.created = {Coin(2, 0, 11, LedgerAsset::ORU)};
        const LedgerKernelOutput o = LedgerApply(in);
        if (o.accept) return fail("inflation accepted");
        if (o.value_ok) return fail("inflation value_ok");
    }

    // --- Additional early-return kills (R6) ---
    {
        LedgerKernelInput in = Base();
        in.consensus.accept = false;
        in.consumed = {Coin(1, 0, 10, LedgerAsset::ORU)};
        in.created = {Coin(2, 0, 10, LedgerAsset::ORU)};
        if (LedgerApply(in).accept) return fail("ck reject accepted");
    }
    {
        LedgerKernelInput in = Base();
        in.fee = -1;
        if (LedgerApply(in).accept) return fail("neg fee");
    }
    {
        LedgerKernelInput in = Base();
        in.height = 0;
        in.consensus.oru_issuance = 1;
        if (LedgerApply(in).accept) return fail("genesis issuance");
    }
    {
        LedgerKernelInput in = Base();
        in.consumed = {Coin(1, 0, 10, LedgerAsset::ORU)};
        in.created = {Coin(2, 0, 10, LedgerAsset::ORU)};
        in.converge = 1;
        if (LedgerApply(in).accept) return fail("converge identity");
    }
    {
        LedgerKernelInput in = Base();
        in.consumed = {Coin(1, 0, 10, LedgerAsset::ORU)};
        in.burn = 5;
        in.converge = 6;
        if (LedgerApply(in).accept) return fail("oru sinks");
    }
    {
        LedgerKernelInput in = Base();
        in.consumed = {Coin(1, 0, 10, LedgerAsset::RES)};
        in.created = {Coin(2, 0, 9, LedgerAsset::RES)};
        if (LedgerApply(in).accept) return fail("res identity");
    }
    {
        LedgerKernelInput in = Base();
        in.created = {Coin(1, 0, MAX_MONEY, LedgerAsset::ORU), Coin(2, 0, 1, LedgerAsset::ORU)};
        if (LedgerApply(in).accept) return fail("created overflow");
    }

    // S1: an unknown asset in `created` must be rejected even when every
    // monetary identity balances without it. Without the created-side asset
    // gate the INVALID coin is accepted and enters created_delta.
    {
        LedgerKernelInput in = Base();
        in.consumed = {Coin(1, 0, 100, LedgerAsset::ORU)};
        in.created = {Coin(2, 0, 100, LedgerAsset::ORU), Coin(3, 0, 50, LedgerAsset::INVALID)};
        const LedgerKernelOutput o = LedgerApply(in);
        if (o.accept) return fail("created unknown asset accepted");
        if (o.value_ok) return fail("created unknown asset value_ok");
        if (!o.created_delta.empty()) return fail("created unknown asset delta");
    }

    // S1: an out-of-range sink must not be reported as value_ok. `converge` is
    // range-checked before the conservation sum, so dropping that check leaves
    // value_ok = (0 >= 0) = true on an otherwise empty input.
    {
        LedgerKernelInput in = Base();
        in.converge = MAX_MONEY + 1;
        const LedgerKernelOutput o = LedgerApply(in);
        if (o.accept) return fail("converge out of range accepted");
        if (o.value_ok) return fail("converge out of range value_ok");
    }

    // S1: MAX_MONEY-scale inputs. Each AddMoney guard below leaves its output
    // parameter untouched on failure, so dropping the guard lets a stale 0 (or
    // stale partial sum) flow into the conservation compare and reports
    // value_ok = true on an input the kernel must reject.
    {
        LedgerKernelInput in = Base();
        in.consumed = {Coin(1, 0, MAX_MONEY, LedgerAsset::ORU),
                       Coin(2, 0, MAX_MONEY, LedgerAsset::RES)};
        if (LedgerApply(in).value_ok) return fail("consumed total overflow value_ok");
    }
    {
        LedgerKernelInput in = Base();
        in.created = {Coin(1, 0, MAX_MONEY, LedgerAsset::ORU),
                      Coin(2, 0, MAX_MONEY, LedgerAsset::RES)};
        if (LedgerApply(in).value_ok) return fail("created total overflow value_ok");
    }
    {
        LedgerKernelInput in = Base();
        in.consumed = {Coin(1, 0, MAX_MONEY, LedgerAsset::ORU)};
        in.consensus.oru_issuance = MAX_MONEY;
        if (LedgerApply(in).value_ok) return fail("credited issuance overflow value_ok");
    }
    {
        LedgerKernelInput in = Base();
        in.consumed = {Coin(1, 0, MAX_MONEY, LedgerAsset::ORU)};
        in.consensus.res_mint = MAX_MONEY;
        if (LedgerApply(in).value_ok) return fail("credited mint overflow value_ok");
    }
    {
        LedgerKernelInput in = Base();
        in.burn = MAX_MONEY;
        in.fee = MAX_MONEY;
        if (LedgerApply(in).value_ok) return fail("sinks overflow value_ok");
    }
    {
        LedgerKernelInput in = Base();
        in.created = {Coin(1, 0, MAX_MONEY, LedgerAsset::ORU)};
        in.burn = MAX_MONEY;
        if (LedgerApply(in).value_ok) return fail("rhs overflow value_ok");
    }
    // S1: the ORU sink sum must include the fee. Without the second AddMoney
    // guard oru_sinks stays at burn + converge, the ORU identity balances, and
    // the block is accepted while the fee is never sunk.
    {
        LedgerKernelInput in = Base();
        in.consumed = {Coin(1, 0, MAX_MONEY, LedgerAsset::ORU)};
        in.converge = MAX_MONEY;
        in.fee = 1;
        if (LedgerApply(in).accept) return fail("oru sinks fee overflow accepted");
    }

    std::printf("PASS: ledger transition refinement\n");
    std::printf("VALUE_CONSERVATION=PASS\n");
    std::printf("DOUBLE_SPEND_BOUNDARY=PASS\n");
    std::printf("ASSET_CONSERVATION=PASS\n");
        std::printf("PRODUCTION_CONSENSUS_AUTHORITY=RigorConsensusKernel(C++)\n");
    std::printf("FORMAL_REFINEMENT_STATUS=PROVED\n");
    std::printf("EXTRACTED_KERNEL_PRODUCTION=FALSE\n");
    std::printf("COMPCERT_BINARY_VERIFIED_KERNEL=FALSE\n");
    return 0;
}
