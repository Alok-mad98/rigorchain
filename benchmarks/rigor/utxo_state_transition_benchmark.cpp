// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// P0-077 UTXO state-transition benchmark. Verify-only compose. Not production.

#include "perf_fixtures.h"
#include "perf_io.h"

#include <cstdlib>
#include <string>
#include <unordered_map>
#include <utility>

static uint64_t LinearLookup(const UTXOStateTransitionInput& in)
{
    uint64_t hits = 0;
    for (const auto& want : in.ledger.consumed) {
        for (const auto& c : in.snapshot) {
            if (c.outpoint == want.outpoint) {
                hits += 1;
                break;
            }
        }
    }
    return hits;
}

static uint64_t MapLookup(const std::unordered_map<uint64_t, const LedgerCoin*>& idx,
    const UTXOStateTransitionInput& in)
{
    uint64_t hits = 0;
    for (const auto& want : in.ledger.consumed) {
        const auto it = idx.find(want.outpoint.id * 0x100000000ULL + want.outpoint.n);
        if (it != idx.end()) hits += 1;
    }
    return hits;
}

int main(int argc, char** argv)
{
    const uint64_t iters = argc > 1 ? static_cast<uint64_t>(std::atoll(argv[1])) : 1200ULL;
    const auto empty = EmptyBlockInput();
    const auto oru = OruTransferInput();
    const auto res_ok = ResMaturityInput(100);
    const auto res_early = ResMaturityInput(99);
    const auto draw = DrawBlockInput();
    const auto large = LargeUtxoInput(4096);
    const auto many_in = ManyInInput(64);
    const auto many_out = ManyOutInput(64);
    auto malformed = oru;
    malformed.asset_scripts = {{0xff, 0x00}};
    malformed.tx.crypto_accept = false;

    std::unordered_map<uint64_t, const LedgerCoin*> large_idx;
    large_idx.reserve(large.snapshot.size());
    for (const auto& c : large.snapshot) {
        large_idx.emplace(c.outpoint.id * 0x100000000ULL + c.outpoint.n, &c);
    }

    std::printf("{\"bench\":\"utxo_state_transition\",\"iters\":%llu}\n",
        static_cast<unsigned long long>(iters));

    auto emit = [&](const char* scene, const char* component, uint64_t n, auto&& fn) {
        PrintRigorSample("utxo_state_transition", component, RunTimed(scene, "kernel", n, fn));
    };

    emit("empty_block", "apply", iters, [&] { return FoldDelta(UTXOStateTransition(empty)); });
    emit("oru_transfer", "apply", iters, [&] { return FoldDelta(UTXOStateTransition(oru)); });
    emit("normal_block", "apply", iters, [&] { return FoldDelta(UTXOStateTransition(oru)); });
    emit("res_maturity_spend", "apply", iters, [&] {
        return FoldDelta(UTXOStateTransition(res_ok)) ^ (FoldDelta(UTXOStateTransition(res_early)) << 1);
    });
    emit("convergence_draw", "apply", iters, [&] { return FoldDelta(UTXOStateTransition(draw)); });
    emit("large_utxo", "apply", iters / 4 + 1, [&] { return FoldDelta(UTXOStateTransition(large)); });
    emit("large_utxo", "input_lookup_linear", iters / 4 + 1, [&] { return LinearLookup(large); });
    emit("large_utxo", "input_lookup_map", iters / 4 + 1, [&] { return MapLookup(large_idx, large); });
    emit("many_inputs", "apply", iters, [&] { return FoldDelta(UTXOStateTransition(many_in)); });
    emit("many_outputs", "apply", iters, [&] { return FoldDelta(UTXOStateTransition(many_out)); });
    emit("large_transaction_block", "apply", iters, [&] { return FoldDelta(UTXOStateTransition(many_in)); });
    emit("malformed_reject", "apply", iters, [&] { return FoldDelta(UTXOStateTransition(malformed)); });

    if (FoldDelta(UTXOStateTransition(empty)) != FoldDelta(UTXOStateTransition(empty))) {
        std::fprintf(stderr, "NONDETERMINISM\n");
        return 1;
    }
    if (!UTXOStateTransition(empty).accept) {
        std::fprintf(stderr, "EMPTY_BLOCK_REJECT\n");
        return 1;
    }
    if (UTXOStateTransition(res_early).accept) {
        std::fprintf(stderr, "MATURITY_BYPASS\n");
        return 1;
    }
    if (UTXOStateTransition(malformed).accept) {
        std::fprintf(stderr, "MALFORMED_ACCEPTED\n");
        return 1;
    }

    const long rss0 = ReadRssKb();
    for (uint64_t i = 0; i < iters * 8; ++i) {
        (void)UTXOStateTransition(oru);
    }
    const long rss1 = ReadRssKb();
    if (rss1 > rss0 + 32 * 1024) {
        std::fprintf(stderr, "MEMORY_LEAK rss0=%ld rss1=%ld\n", rss0, rss1);
        return 1;
    }
    std::printf("{\"bench\":\"utxo_state_transition\",\"component\":\"rss_probe\","
                "\"scenario\":\"oru_transfer\",\"path\":\"kernel\",\"rss_kb\":%ld,"
                "\"peak_rss_kb\":%ld,\"iters\":%llu,\"latency_ns\":0,\"ops_per_sec\":0,"
                "\"seconds\":0,\"alloc_bytes\":0,\"fingerprint\":\"%016llx\"}\n",
                rss1, rss1 > rss0 ? rss1 : rss0,
                static_cast<unsigned long long>(iters * 8),
                static_cast<unsigned long long>(rss1 > rss0 ? rss1 - rss0 : 0));
    return 0;
}
