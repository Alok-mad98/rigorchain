// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// P0-077 block-validation-shaped compose. Sequential UTXOStateTransition over a
// fixture "block". Not ConnectBlock. Not a second consensus executor.

#include "perf_fixtures.h"
#include "perf_io.h"

#include <cstdlib>
#include <string>
#include <vector>

static uint64_t ApplyBlock(const std::vector<UTXOStateTransitionInput>& txs)
{
    uint64_t fp = txs.size();
    for (const auto& tx : txs) {
        fp ^= FoldDelta(UTXOStateTransition(tx)) + 0x9e3779b97f4a7c15ULL;
    }
    return fp;
}

int main(int argc, char** argv)
{
    const uint64_t iters = argc > 1 ? static_cast<uint64_t>(std::atoll(argv[1])) : 400ULL;

    std::vector<UTXOStateTransitionInput> empty_block = {EmptyBlockInput()};
    std::vector<UTXOStateTransitionInput> normal;
    normal.reserve(8);
    for (int i = 0; i < 8; ++i) normal.push_back(OruTransferInput());

    std::vector<UTXOStateTransitionInput> large_tx;
    large_tx.reserve(64);
    for (int i = 0; i < 64; ++i) large_tx.push_back(OruTransferInput());

    std::vector<UTXOStateTransitionInput> many_in = {ManyInInput(64)};
    std::vector<UTXOStateTransitionInput> many_out = {ManyOutInput(64)};
    std::vector<UTXOStateTransitionInput> oru = {OruTransferInput()};
    std::vector<UTXOStateTransitionInput> res = {ResMaturityInput(100)};
    std::vector<UTXOStateTransitionInput> subsidy = {SubsidyBlockInput()};
    std::vector<UTXOStateTransitionInput> large_utxo = {LargeUtxoInput(4096)};

    std::printf("{\"bench\":\"block_validation\",\"iters\":%llu}\n",
        static_cast<unsigned long long>(iters));

    auto emit = [&](const char* scene, uint64_t n, const std::vector<UTXOStateTransitionInput>& block) {
        PrintRigorSample("block_validation", "compose", RunTimed(scene, "kernel", n, [&] {
            return ApplyBlock(block);
        }));
    };

    emit("empty_block", iters, empty_block);
    emit("normal_block", iters, normal);
    emit("large_transaction_block", iters / 4 + 1, large_tx);
    emit("many_inputs", iters, many_in);
    emit("many_outputs", iters, many_out);
    emit("oru_transfer", iters, oru);
    emit("res_maturity_spend", iters, res);
    emit("subsidy_block", iters, subsidy);
    emit("large_utxo", iters / 8 + 1, large_utxo);

    if (ApplyBlock(empty_block) != ApplyBlock(empty_block)) {
        std::fprintf(stderr, "NONDETERMINISM\n");
        return 1;
    }
    return 0;
}
