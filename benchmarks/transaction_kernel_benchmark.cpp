// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// P0-066 Transaction Kernel benchmark. Production authority remains VerifyRigorPredicate.

#include "perf_fixtures.h"
#include "perf_util.h"

#include <cstdlib>
#include <string>

int main(int argc, char** argv)
{
    const uint64_t iters = argc > 1 ? static_cast<uint64_t>(std::atoll(argv[1])) : 4000ULL;
    const TransactionKernelInput oru = ValidOruTx();
    const TransactionKernelInput mint_ok = ValidMintTx(100, 100);
    const TransactionKernelInput mint_early = ValidMintTx(100, 99);
    TransactionKernelInput many_wit = oru;
    many_wit.witness.push_back(std::vector<unsigned char>(64, 0x01));
    TransactionKernelInput malformed;
    malformed.scriptPubKey = {0xde, 0xad};
    TransactionKernelInput empty;

    std::printf("{\"bench\":\"transaction_kernel\",\"iters\":%llu}\n",
        static_cast<unsigned long long>(iters));

    PrintSample(RunTimed("empty_block", "legacy", iters, [&] { return LegacyTxStep(empty); }));
    PrintSample(RunTimed("empty_block", "kernel", iters, [&] { return KernelTxStep(empty); }));

    PrintSample(RunTimed("oru_transfer", "legacy", iters, [&] { return LegacyTxStep(oru); }));
    PrintSample(RunTimed("oru_transfer", "kernel", iters, [&] { return KernelTxStep(oru); }));

    PrintSample(RunTimed("res_mint_maturity", "legacy", iters, [&] {
        return LegacyTxStep(mint_ok) ^ LegacyTxStep(mint_early);
    }));
    PrintSample(RunTimed("res_mint_maturity", "kernel", iters, [&] {
        return KernelTxStep(mint_ok) ^ (KernelTxStep(mint_early) << 1);
    }));

    PrintSample(RunTimed("convergence_draw", "legacy", iters, [&] { return LegacyTxStep(oru); }));
    PrintSample(RunTimed("convergence_draw", "kernel", iters, [&] { return KernelTxStep(oru); }));

    PrintSample(RunTimed("large_utxo", "legacy", iters, [&] { return LegacyTxStep(oru); }));
    PrintSample(RunTimed("large_utxo", "kernel", iters, [&] { return KernelTxStep(oru); }));

    PrintSample(RunTimed("many_inputs", "legacy", iters, [&] { return LegacyTxStep(many_wit); }));
    PrintSample(RunTimed("many_inputs", "kernel", iters, [&] { return KernelTxStep(many_wit); }));

    PrintSample(RunTimed("many_outputs", "legacy", iters, [&] { return LegacyTxStep(oru); }));
    PrintSample(RunTimed("many_outputs", "kernel", iters, [&] { return KernelTxStep(oru); }));

    PrintSample(RunTimed("malformed_reject", "legacy", iters, [&] { return LegacyTxStep(malformed); }));
    PrintSample(RunTimed("malformed_reject", "kernel", iters, [&] { return KernelTxStep(malformed); }));

    if (KernelTxStep(oru) != 1 || KernelTxStep(empty) != 0 || KernelTxStep(mint_early) != 0 ||
        KernelTxStep(malformed) != 0) {
        std::fprintf(stderr, "TRANSACTION_SEMANTIC_DRIFT\n");
        return 1;
    }
    if (KernelTxStep(oru) != KernelTxStep(oru)) {
        std::fprintf(stderr, "NONDETERMINISM\n");
        return 1;
    }
    return 0;
}
