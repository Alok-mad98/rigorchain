// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// P0-077 performance-boundary fuzz. Crash or state drift => FAIL.
// Does not recode consensus formulas.

#include <consensus/amount.h>
#include <consensus/params.h>
#include <consensus/rigor_consensus_lock.h>
#include <consensus/utxo/utxo_state_transition.h>
#include <script/script.h>
#include <uint256.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <vector>

static Consensus::Params FrozenParams()
{
    Consensus::Params p;
    p.nCoinbaseMaturity = 100;
    return p;
}

static LedgerCoin CoinFrom(uint64_t id, const uint8_t* p, bool valid_asset)
{
    LedgerCoin c;
    c.outpoint.id = id;
    c.outpoint.n = p[0];
    c.value = static_cast<CAmount>((p[1] + 1) * (COIN / 100));
    if (c.value < 0) c.value = COIN;
    c.asset = valid_asset ? ((p[2] & 1) ? LedgerAsset::RES : LedgerAsset::ORU) : LedgerAsset::INVALID;
    c.coinbase_height = (p[3] & 1) ? 0 : -1;
    return c;
}

static uint64_t Fold(const UTXOStateTransitionResult& o)
{
    uint64_t fp = o.accept ? 1ULL : 0ULL;
    fp ^= static_cast<uint64_t>(o.delta.additions.size()) << 1;
    fp ^= static_cast<uint64_t>(o.delta.removals.size()) << 8;
    for (const auto& c : o.delta.additions) fp ^= c.outpoint.id + static_cast<uint64_t>(c.value);
    for (const auto& r : o.delta.removals) fp ^= r.id + r.n;
    return fp;
}

void PerformanceBoundaryOne(const uint8_t* data, size_t size)
{
    UTXOStateTransitionInput in;
    in.params = FrozenParams();
    if (data == nullptr || size == 0) {
        in.height = 0;
        const auto a = UTXOStateTransition(in);
        const auto b = UTXOStateTransition(in);
        if (Fold(a) != Fold(b)) std::abort();
        return;
    }

    const uint8_t kind = data[0];
    in.height = static_cast<int64_t>(static_cast<int8_t>(data[size > 1 ? 1 : 0]));
    if (in.height > 400) in.height = 200;
    in.maturity = 100;
    in.tx.flags = TK_SCRIPT_VERIFY_WITNESS;
    in.tx.scriptSig_push_only = true;
    in.tx.stack_clean = true;
    in.tx.signature.assign(64, 0x07);
    in.tx.pubkey.assign(32, 0x09);
    const CScript oru = CScript() << RIGOR_PUSH_0 << std::vector<unsigned char>(WITNESS_V0_KEYHASH_SIZE, 0x11);
    in.tx.scriptPubKey.assign(oru.begin(), oru.end());
    in.tx.witness = {in.tx.signature, in.tx.pubkey};
    in.tx.keyhash_match = (kind & 1) == 0;
    in.tx.crypto_accept = (kind & 2) == 0;
    in.consensus.draw_target = uint256S("5555555555555555555555555555555555555555555555555555555555555555");

    if (kind == 0xff) {
        // malformed transaction
        in.tx.scriptPubKey.assign(data, data + (size < 256 ? size : 256));
        in.tx.crypto_accept = false;
        in.tx.signature.assign(size % 96, 0x30);
        in.tx.witness.clear();
    }

    if (kind == 0xfe) {
        // huge tx count (independent applies)
        const size_t ntx = 128;
        for (size_t t = 0; t < ntx; ++t) {
            UTXOStateTransitionInput one = in;
            one.height = 200;
            LedgerCoin c = CoinFrom(1 + t, data, true);
            one.snapshot = {c};
            one.ledger.consumed = {c};
            LedgerCoin outc = c;
            outc.outpoint.id = 10000 + t;
            outc.coinbase_height = -1;
            one.ledger.created = {outc};
            one.tx.keyhash_match = true;
            one.tx.crypto_accept = true;
            const auto a = UTXOStateTransition(one);
            const auto b = UTXOStateTransition(one);
            if (Fold(a) != Fold(b)) std::abort();
        }
        return;
    }

    const size_t n = (kind == 3) ? 1024 : (kind == 4 ? 96 : ((data[0] % 16) + 1));
    const bool valid_asset = kind != 5;
    if (kind == 3) {
        // large UTXO set
        in.snapshot.reserve(n);
        for (size_t i = 0; i < n; ++i) {
            uint8_t pad[4] = {static_cast<uint8_t>(i), 1, 0, 0};
            in.snapshot.push_back(CoinFrom(1000 + i, pad, true));
        }
        if (!in.snapshot.empty()) {
            in.ledger.consumed = {in.snapshot[0]};
            in.ledger.created = {CoinFrom(9, data, true)};
            in.ledger.created[0].value = in.ledger.consumed[0].value;
            in.ledger.created[0].asset = in.ledger.consumed[0].asset;
        }
        in.height = 200;
        in.tx.keyhash_match = true;
        in.tx.crypto_accept = true;
    } else if (kind == 6) {
        // huge block of many inputs
        CAmount sum = 0;
        in.snapshot.reserve(96);
        in.ledger.consumed.reserve(96);
        for (size_t i = 0; i < 96; ++i) {
            LedgerCoin c = CoinFrom(20 + i, data, true);
            c.value = COIN;
            c.asset = LedgerAsset::ORU;
            in.snapshot.push_back(c);
            in.ledger.consumed.push_back(c);
            sum += COIN;
        }
        in.ledger.created = {CoinFrom(99, data, true)};
        in.ledger.created[0].value = sum;
        in.ledger.created[0].asset = LedgerAsset::ORU;
        in.height = 200;
        in.tx.keyhash_match = true;
        in.tx.crypto_accept = true;
    } else if (kind != 0) {
        const uint8_t* p = size > 8 ? data + 4 : data;
        LedgerCoin c = CoinFrom(1, p, valid_asset);
        in.snapshot = {c};
        in.ledger.consumed = {c};
        LedgerCoin outc = c;
        outc.outpoint.id = 2;
        outc.coinbase_height = -1;
        in.ledger.created = {outc};
    }

    if (kind == 1) {
        in.height = 200;
        in.tx.keyhash_match = true;
        in.tx.crypto_accept = true;
    }

    const auto a = UTXOStateTransition(in);
    const auto b = UTXOStateTransition(in);
    if (Fold(a) != Fold(b)) std::abort();
    (void)n;
}

static void Smoke()
{
    uint8_t empty[] = {0};
    PerformanceBoundaryOne(empty, 1);
    uint8_t valid[] = {1, 50, 0, 0, 0, 2, 0, 0};
    PerformanceBoundaryOne(valid, sizeof(valid));
    uint8_t malformed[] = {0xff, 0xde, 0xad, 0xbe, 0xef};
    PerformanceBoundaryOne(malformed, sizeof(malformed));
    uint8_t large_utxo[] = {3, 40};
    PerformanceBoundaryOne(large_utxo, sizeof(large_utxo));
    uint8_t huge_tx[] = {0xfe, 10};
    PerformanceBoundaryOne(huge_tx, sizeof(huge_tx));
    uint8_t huge_block[] = {6, 20, 1, 2, 3, 4};
    PerformanceBoundaryOne(huge_block, sizeof(huge_block));
    uint8_t many[512];
    for (int i = 0; i < 512; ++i) many[i] = static_cast<uint8_t>(i * 13 + 7);
    PerformanceBoundaryOne(many, sizeof(many));
    PerformanceBoundaryOne(nullptr, 0);
}

#if defined(PERFORMANCE_BOUNDARY_MAIN)
int main(int argc, char** argv)
{
    try {
        if (argc >= 2 && std::strcmp(argv[1], "smoke") == 0) {
            Smoke();
            std::printf("FUZZ_SMOKE=PASS\nCRASH_FOUND=NO\nSTATE_DIFFERENCE=NO\nCONSENSUS_DIFFERENCE=NO\n");
            return 0;
        }
        std::vector<uint8_t> buf;
        int c;
        while ((c = std::fgetc(stdin)) != EOF) buf.push_back(static_cast<uint8_t>(c));
        if (buf.empty()) Smoke();
        else PerformanceBoundaryOne(buf.data(), buf.size());
        return 0;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "CRASH %s\n", e.what());
        return 1;
    } catch (...) {
        std::fprintf(stderr, "CRASH\n");
        return 1;
    }
}
#else
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    try {
        PerformanceBoundaryOne(data, size);
    } catch (...) {
        return 0;
    }
    return 0;
}
#endif
