// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// P0-066 Consensus Kernel production-path benchmark. Does not recode rules.

#include "perf_fixtures.h"
#include "perf_util.h"

#include <cstdlib>
#include <string>
#include <vector>

int main(int argc, char** argv)
{
    const uint64_t iters = argc > 1 ? static_cast<uint64_t>(std::atoll(argv[1])) : 2000ULL;
    const uint64_t w = 209715200000000ULL;
    const ConvergenceDrawSet empty;
    const ConvergenceDrawSet single = MakeDraw(1, w);
    const ConvergenceDrawSet draw = MakeDraw(8, w);
    const ConvergenceDrawSet large = MakeDraw(64, w);
    const CScript oru = OruScript();
    const CScript mint = MintScript(100);
    const CScript bad;

    std::printf("{\"bench\":\"consensus_kernel\",\"iters\":%llu}\n",
        static_cast<unsigned long long>(iters));

    PrintSample(RunTimed("empty_block", "legacy", iters, [&] {
        return LegacyConsensusStep(0, empty);
    }));
    PrintSample(RunTimed("empty_block", "kernel", iters, [&] {
        return KernelConsensusStep(0, empty);
    }));

    PrintSample(RunTimed("oru_transfer", "legacy", iters, [&] {
        std::vector<std::vector<unsigned char>> sols;
        return static_cast<uint64_t>(ClassifyRigorTxout(oru, sols)) ^ LegacyConsensusStep(1, empty);
    }));
    PrintSample(RunTimed("oru_transfer", "kernel", iters, [&] {
        return static_cast<uint64_t>(ClassifyRigorAsset(oru)) ^ KernelConsensusStep(1, empty);
    }));

    PrintSample(RunTimed("res_mint_maturity", "legacy", iters, [&] {
        return static_cast<uint64_t>(IsResMintScript(mint)) ^ LegacyConsensusStep(101, empty);
    }));
    PrintSample(RunTimed("res_mint_maturity", "kernel", iters, [&] {
        return static_cast<uint64_t>(ClassifyRigorAsset(mint)) ^ KernelConsensusStep(101, empty);
    }));

    PrintSample(RunTimed("convergence_draw", "legacy", iters, [&] {
        return LegacyConsensusStep(1, draw);
    }));
    PrintSample(RunTimed("convergence_draw", "kernel", iters, [&] {
        return KernelConsensusStep(1, draw);
    }));

    PrintSample(RunTimed("large_utxo", "legacy", iters / 4 + 1, [&] {
        return LegacyConsensusStep(1, large);
    }));
    PrintSample(RunTimed("large_utxo", "kernel", iters / 4 + 1, [&] {
        return KernelConsensusStep(1, large);
    }));

    PrintSample(RunTimed("many_inputs", "legacy", iters, [&] {
        return LegacyConsensusStep(210000, single);
    }));
    PrintSample(RunTimed("many_inputs", "kernel", iters, [&] {
        return KernelConsensusStep(210000, single);
    }));

    PrintSample(RunTimed("many_outputs", "legacy", iters, [&] {
        return LegacyConsensusStep(420000, single);
    }));
    PrintSample(RunTimed("many_outputs", "kernel", iters, [&] {
        return KernelConsensusStep(420000, single);
    }));

    PrintSample(RunTimed("malformed_reject", "legacy", iters, [&] {
        std::vector<std::vector<unsigned char>> sols;
        return static_cast<uint64_t>(ClassifyRigorTxout(bad, sols));
    }));
    PrintSample(RunTimed("malformed_reject", "kernel", iters, [&] {
        return static_cast<uint64_t>(ClassifyRigorAsset(bad));
    }));

    const auto a = KernelConsensusStep(1, draw);
    const auto b = KernelConsensusStep(1, draw);
    if (a != b) {
        std::fprintf(stderr, "NONDETERMINISM\n");
        return 1;
    }
    return 0;
}
