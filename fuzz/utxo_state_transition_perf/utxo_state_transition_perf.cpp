// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// P0-066 UTXO state-transition performance fuzz. Crash => FAIL.

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
    p.nBlockSubsidyBase = Consensus::ORU_INITIAL_BLOCK_SUBSIDY;
    p.nSubsidyHalvingInterval = Consensus::ORU_SUBSIDY_HALVING_INTERVAL;
    p.nBlockSubsidyMaxHalvingShifts = Consensus::ORU_SUBSIDY_MAX_SHIFTS;
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

void UtxoStateTransitionPerfOne(const uint8_t* data, size_t size)
{
    UTXOStateTransitionInput in;
    in.params = FrozenParams();
    if (data == nullptr || size == 0) {
        in.height = 0;
        (void)UTXOStateTransition(in);
        return;
    }

    const uint8_t kind = data[0];
    in.height = static_cast<int64_t>(static_cast<int8_t>(data[size > 1 ? 1 : 0]));
    if (in.height > 200 && in.height < 100000) in.height = 200;
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
    in.consensus.draw_target = Consensus::RES_PROBABILITY_TARGET_CONST;

    if (kind == 0xff) {
        in.tx.scriptPubKey.assign(data, data + (size < 128 ? size : 128));
        in.tx.crypto_accept = false;
        in.tx.signature.assign(size % 80, 0x30);
    }

    const size_t n = (kind == 3) ? 256 : (kind == 4 ? 64 : ((data[0] % 8) + 1));
    const bool valid_asset = kind != 5;
    if (kind == 3) {
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

    (void)UTXOStateTransition(in);
    (void)UTXOStateTransition(in);
}

static void Smoke()
{
    uint8_t empty[] = {0};
    UtxoStateTransitionPerfOne(empty, 1);
    uint8_t valid[] = {1, 50, 0, 0, 0, 2, 0, 0};
    UtxoStateTransitionPerfOne(valid, sizeof(valid));
    uint8_t invalid[] = {0xff, 0xde, 0xad, 0xbe, 0xef};
    UtxoStateTransitionPerfOne(invalid, sizeof(invalid));
    uint8_t large[] = {3, 40};
    UtxoStateTransitionPerfOne(large, sizeof(large));
    uint8_t many[256];
    for (int i = 0; i < 256; ++i) many[i] = static_cast<uint8_t>(i * 13 + 7);
    UtxoStateTransitionPerfOne(many, sizeof(many));
    UtxoStateTransitionPerfOne(nullptr, 0);
}

#if defined(UTXO_STATE_TRANSITION_PERF_MAIN)
int main(int argc, char** argv)
{
    try {
        if (argc >= 2 && std::strcmp(argv[1], "smoke") == 0) {
            Smoke();
            std::printf("FUZZ_SMOKE=PASS\nCRASH_FOUND=NO\n");
            return 0;
        }
        std::vector<uint8_t> buf;
        int c;
        while ((c = std::fgetc(stdin)) != EOF) buf.push_back(static_cast<uint8_t>(c));
        if (buf.empty()) Smoke();
        else UtxoStateTransitionPerfOne(buf.data(), buf.size());
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
        UtxoStateTransitionPerfOne(data, size);
    } catch (...) {
        std::abort();
    }
    return 0;
}
#endif
