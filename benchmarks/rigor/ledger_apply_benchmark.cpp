// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// P0-077 LedgerApply benchmark. Verify-only kernel. Does not switch production.

#include "perf_fixtures.h"
#include "perf_io.h"

#include <cstdlib>
#include <string>

static LedgerKernelInput LedgerOf(const UTXOStateTransitionInput& in)
{
    const RigorIssuance iss = ComputeRigorIssuance(static_cast<int>(in.height), in.params);
    LedgerKernelInput ledger = in.ledger;
    ledger.height = in.height;
    ledger.maturity = in.maturity;
    ledger.fee = in.fee;
    ledger.burn = in.burn;
    ledger.converge = in.converge;
    ledger.tx.accept = in.ledger.consumed.empty() || VerifyRigorPredicateBoundary(in.tx);
    ledger.consensus.accept = true;
    ledger.consensus.oru_issuance = (in.height == 0) ? 0 : in.ledger.consensus.oru_issuance;
    ledger.consensus.res_mint = (in.height == 0) ? 0 : in.ledger.consensus.res_mint;
    ledger.consensus.res_schedule = iss.res_reward;
    return ledger;
}

int main(int argc, char** argv)
{
    const uint64_t iters = argc > 1 ? static_cast<uint64_t>(std::atoll(argv[1])) : 1500ULL;
    const auto empty = LedgerOf(EmptyBlockInput());
    const auto oru = LedgerOf(OruTransferInput());
    const auto res_ok = LedgerOf(ResMaturityInput(200));
    const auto res_early = LedgerOf(ResMaturityInput(50));
    const auto draw = LedgerOf(DrawBlockInput());
    const auto large = LedgerOf(LargeUtxoInput(4096));
    const auto many_in = LedgerOf(ManyInInput(64));
    const auto many_out = LedgerOf(ManyOutInput(64));
    auto malformed = oru;
    malformed.tx.accept = false;

    std::printf("{\"bench\":\"ledger_apply\",\"iters\":%llu}\n",
        static_cast<unsigned long long>(iters));

    auto emit = [&](const char* scene, uint64_t n, const LedgerKernelInput& in) {
        PrintRigorSample("ledger_apply", "apply", RunTimed(scene, "kernel", n, [&] {
            return FoldLedger(LedgerApply(in));
        }));
    };

    emit("empty_block", iters, empty);
    emit("oru_transfer", iters, oru);
    emit("normal_block", iters, oru);
    emit("res_maturity_spend", iters, res_ok);
    PrintRigorSample("ledger_apply", "apply", RunTimed("res_maturity_spend", "immature", iters, [&] {
        return FoldLedger(LedgerApply(res_early));
    }));
    emit("convergence_draw", iters, draw);
    emit("large_utxo", iters / 4 + 1, large);
    emit("many_inputs", iters, many_in);
    emit("many_outputs", iters, many_out);
    emit("large_transaction_block", iters, many_in);
    emit("malformed_reject", iters, malformed);

    if (FoldLedger(LedgerApply(empty)) != FoldLedger(LedgerApply(empty))) {
        std::fprintf(stderr, "NONDETERMINISM\n");
        return 1;
    }
    if (!LedgerApply(empty).accept) {
        std::fprintf(stderr, "EMPTY_BLOCK_REJECT\n");
        return 1;
    }
    if (LedgerApply(res_early).accept) {
        std::fprintf(stderr, "MATURITY_BYPASS\n");
        return 1;
    }
    if (LedgerApply(malformed).accept) {
        std::fprintf(stderr, "MALFORMED_ACCEPTED\n");
        return 1;
    }
    return 0;
}
