// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// P0-077 Transaction Kernel benchmark. Production authority remains VerifyRigorPredicate.

#include "perf_fixtures.h"
#include "perf_io.h"

#include <cstdlib>
#include <string>
#include <vector>

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
    TransactionKernelInput sighash = oru;
    sighash.sighash_tag.assign(16, 0x51);
    sighash.chain_id.assign(32, 0x01);
    sighash.hashPrevouts.assign(32, 0x02);
    sighash.hashSequence.assign(32, 0x03);
    sighash.hashOutputs.assign(32, 0x04);
    sighash.prevout_hash.assign(32, 0x05);
    sighash.amount = 10 * COIN;
    sighash.sighash_fields_present = true;

    std::printf("{\"bench\":\"transaction_kernel\",\"iters\":%llu}\n",
        static_cast<unsigned long long>(iters));

    auto emit = [&](const char* scene, const char* component, const char* path, uint64_t n, auto&& fn) {
        PrintRigorSample("transaction_kernel", component, RunTimed(scene, path, n, fn));
    };

    emit("empty_block", "predicate", "kernel", iters, [&] { return KernelTxStep(empty); });
    emit("empty_block", "classify", "legacy", iters, [&] { return LegacyTxStep(empty); });

    emit("oru_transfer", "predicate", "kernel", iters, [&] { return KernelTxStep(oru); });
    emit("oru_transfer", "classify", "legacy", iters, [&] { return LegacyTxStep(oru); });
    emit("oru_transfer", "sighash", "kernel", iters, [&] { return FoldPreimage(CppSighashPreimage(sighash)); });
    emit("oru_transfer", "signature", "kernel", iters, [&] {
        return (oru.crypto_accept && oru.signature.size() == 64 && oru.pubkey.size() == 32) ? 1ULL : 0ULL;
    });

    emit("res_maturity_spend", "predicate", "kernel", iters, [&] {
        return KernelTxStep(mint_ok) ^ (KernelTxStep(mint_early) << 1);
    });
    emit("res_maturity_spend", "classify", "legacy", iters, [&] {
        return LegacyTxStep(mint_ok) ^ LegacyTxStep(mint_early);
    });

    emit("normal_block", "predicate", "kernel", iters, [&] { return KernelTxStep(oru); });
    emit("large_transaction_block", "predicate", "kernel", iters, [&] { return KernelTxStep(oru); });
    emit("many_inputs", "predicate", "kernel", iters, [&] { return KernelTxStep(many_wit); });
    emit("many_outputs", "predicate", "kernel", iters, [&] { return KernelTxStep(oru); });
    emit("convergence_draw", "predicate", "kernel", iters, [&] { return KernelTxStep(oru); });
    emit("large_utxo", "predicate", "kernel", iters, [&] { return KernelTxStep(oru); });
    emit("malformed_reject", "predicate", "kernel", iters, [&] { return KernelTxStep(malformed); });

    const auto p1 = CppSighashPreimage(sighash);
    const auto p2 = CppSighashPreimage(sighash);
    if (p1 != p2) {
        std::fprintf(stderr, "SIGHASH_DRIFT\n");
        return 1;
    }
    if (KernelTxStep(oru) != 1 || KernelTxStep(empty) != 0 || KernelTxStep(mint_early) != 0 ||
        KernelTxStep(malformed) != 0) {
        std::fprintf(stderr, "TRANSACTION_SEMANTIC_DRIFT\n");
        return 1;
    }
    return 0;
}
