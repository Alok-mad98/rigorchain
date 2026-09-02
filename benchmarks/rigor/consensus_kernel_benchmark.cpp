// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// P0-077 Consensus Kernel sub-step benchmark. Calls unique authorities only.

#include "perf_fixtures.h"
#include "perf_io.h"

#include <cstdlib>
#include <string>
#include <vector>

int main(int argc, char** argv)
{
    const uint64_t iters = argc > 1 ? static_cast<uint64_t>(std::atoll(argv[1])) : 2000ULL;
    const uint64_t w = 209715200000000ULL;
    const Consensus::Params p = FrozenParams();
    const ConvergenceDrawSet empty;
    const ConvergenceDrawSet draw = MakeDraw(8, w);
    const ConvergenceDrawSet large = MakeDraw(64, w);
    const CScript oru = OruScript();
    const CScript mint = MintScript(100);
    const CScript bad;

    std::printf("{\"bench\":\"consensus_kernel\",\"iters\":%llu}\n",
        static_cast<unsigned long long>(iters));

    auto emit = [&](const char* scene, const char* component, const char* path, uint64_t n, auto&& fn) {
        PrintRigorSample("consensus_kernel", component, RunTimed(scene, path, n, fn));
    };

    emit("empty_block", "issuance", "kernel", iters, [&] {
        return static_cast<uint64_t>(ComputeRigorIssuance(0, p).oru_subsidy);
    });
    emit("empty_block", "issuance", "memo", iters, [&] {
        return static_cast<uint64_t>(IssuanceMemo(0, p).oru_subsidy);
    });
    emit("empty_block", "maturity", "kernel", iters, [&] {
        return KernelCoinbaseMature(0, -1, 100) ? 1ULL : 0ULL;
    });
    emit("empty_block", "classification", "kernel", iters, [&] {
        return static_cast<uint64_t>(ClassifyRigorAsset(bad));
    });
    emit("empty_block", "draw", "kernel", iters, [&] {
        return static_cast<uint64_t>(ResolveConvergenceDraw(empty, Consensus::RES_PROBABILITY_TARGET_CONST).winner_index + 2);
    });
    emit("empty_block", "supply", "kernel", iters, [&] {
        RigorSupplyDeltaInput sin;
        sin.height = 0;
        sin.oru_coinbase = 0;
        BlockValidationState state;
        RigorSupplyDeltaResult out;
        return RigorSupplyDelta(sin, p, out, state) ? 1ULL : 0ULL;
    });
    emit("empty_block", "full", "kernel", iters, [&] { return KernelConsensusStep(0, empty); });

    emit("oru_transfer", "classification", "kernel", iters, [&] {
        return static_cast<uint64_t>(ClassifyRigorAsset(oru));
    });
    emit("oru_transfer", "full", "kernel", iters, [&] { return KernelConsensusStep(1, empty); });

    emit("res_maturity_spend", "classification", "kernel", iters, [&] {
        return static_cast<uint64_t>(ClassifyRigorAsset(mint));
    });
    emit("res_maturity_spend", "maturity", "kernel", iters, [&] {
        return (KernelCoinbaseMature(100, 0, 100) ? 1ULL : 0ULL) ^
               (KernelCoinbaseMature(99, 0, 100) ? 2ULL : 0ULL);
    });
    emit("res_maturity_spend", "full", "kernel", iters, [&] { return KernelConsensusStep(101, empty); });

    emit("convergence_draw", "draw", "kernel", iters, [&] {
        return static_cast<uint64_t>(ResolveConvergenceDraw(draw, Consensus::RES_PROBABILITY_TARGET_CONST).winner_index + 2);
    });
    emit("convergence_draw", "full", "kernel", iters, [&] { return KernelConsensusStep(1, draw); });

    emit("large_utxo", "draw", "kernel", iters / 4 + 1, [&] {
        return static_cast<uint64_t>(ResolveConvergenceDraw(large, Consensus::RES_PROBABILITY_TARGET_CONST).winner_index + 2);
    });
    emit("large_utxo", "full", "kernel", iters / 4 + 1, [&] { return KernelConsensusStep(1, large); });

    emit("many_inputs", "issuance", "kernel", iters, [&] {
        return static_cast<uint64_t>(ComputeRigorIssuance(210000, p).oru_subsidy);
    });
    emit("many_inputs", "full", "kernel", iters, [&] { return KernelConsensusStep(210000, MakeDraw(1, w)); });

    emit("many_outputs", "issuance", "kernel", iters, [&] {
        return static_cast<uint64_t>(ComputeRigorIssuance(420000, p).epoch);
    });
    emit("many_outputs", "full", "kernel", iters, [&] { return KernelConsensusStep(420000, MakeDraw(1, w)); });

    emit("normal_block", "full", "kernel", iters, [&] { return KernelConsensusStep(200, empty); });
    emit("large_transaction_block", "full", "kernel", iters, [&] { return KernelConsensusStep(200, draw); });

    const auto a = KernelConsensusStep(1, draw);
    const auto b = KernelConsensusStep(1, draw);
    const auto c = IssuanceMemo(1, p);
    const auto d = ComputeRigorIssuance(1, p);
    if (a != b || c.oru_subsidy != d.oru_subsidy || c.res_reward != d.res_reward) {
        std::fprintf(stderr, "NONDETERMINISM_OR_MEMO_DRIFT\n");
        return 1;
    }
    return 0;
}
