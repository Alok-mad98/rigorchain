// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// P0-066 Ledger + UTXO State Transition benchmark. Does not switch production.

#include "perf_fixtures.h"
#include "perf_util.h"

#include <cstdlib>
#include <string>

int main(int argc, char** argv)
{
    const uint64_t iters = argc > 1 ? static_cast<uint64_t>(std::atoll(argv[1])) : 1500ULL;
    const auto empty = EmptyBlockInput();
    const auto oru = OruTransferInput();
    const auto res_ok = ResMaturityInput(100);
    const auto res_early = ResMaturityInput(99);
    const auto draw = DrawBlockInput();
    const auto large = LargeUtxoInput(4096);
    const auto many_in = ManyInInput(32);
    const auto many_out = ManyOutInput(32);
    auto malformed = oru;
    malformed.asset_scripts = {{0xff, 0x00}};
    malformed.tx.crypto_accept = false;

    std::printf("{\"bench\":\"ledger_utxo_transition\",\"iters\":%llu}\n",
        static_cast<unsigned long long>(iters));

    auto pair = [&](const char* name, const UTXOStateTransitionInput& in, uint64_t n) {
        PrintSample(RunTimed(name, "legacy", n, [&] { return LegacyLedgerStep(in); }));
        PrintSample(RunTimed(name, "kernel", n, [&] { return KernelUtxoStep(in); }));
    };

    pair("empty_block", empty, iters);
    pair("oru_transfer", oru, iters);
    PrintSample(RunTimed("res_mint_maturity", "legacy", iters, [&] {
        return LegacyLedgerStep(res_ok) ^ LegacyLedgerStep(res_early);
    }));
    PrintSample(RunTimed("res_mint_maturity", "kernel", iters, [&] {
        return KernelUtxoStep(res_ok) ^ (KernelUtxoStep(res_early) << 1);
    }));
    pair("convergence_draw", draw, iters);
    pair("large_utxo", large, iters / 4 + 1);
    pair("many_inputs", many_in, iters);
    pair("many_outputs", many_out, iters);
    pair("malformed_reject", malformed, iters);

    if (KernelUtxoStep(empty) != KernelUtxoStep(empty) || KernelUtxoStep(oru) != KernelUtxoStep(oru)) {
        std::fprintf(stderr, "NONDETERMINISM\n");
        return 1;
    }
    if (KernelUtxoStep(empty) == 0) {
        std::fprintf(stderr, "EMPTY_BLOCK_REJECT\n");
        return 1;
    }
    if (KernelUtxoStep(res_early) != 0) {
        std::fprintf(stderr, "MATURITY_BYPASS\n");
        return 1;
    }
    if (KernelUtxoStep(malformed) != 0) {
        std::fprintf(stderr, "MALFORMED_ACCEPTED\n");
        return 1;
    }

    const long rss0 = ReadRssKb();
    for (uint64_t i = 0; i < iters * 8; ++i) {
        (void)KernelUtxoStep(oru);
    }
    const long rss1 = ReadRssKb();
    if (rss1 > rss0 + 32 * 1024) {
        std::fprintf(stderr, "MEMORY_LEAK rss0=%ld rss1=%ld\n", rss0, rss1);
        return 1;
    }
    return 0;
}
