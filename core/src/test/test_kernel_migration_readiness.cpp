// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// P067: dual-path migration verify. Does not switch production.

#if !defined(CONSENSUS_KERNEL_MIGRATION_VERIFY_ONLY)
#error "test_kernel_migration_readiness is verify-only"
#endif

#include <consensus/kernel_migration/kernel_migration.h>
#include <consensus/rigor_consensus_lock.h>
#include <uint256.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

static int fail(const char* msg)
{
    std::fprintf(stderr, "FAIL: %s\n", msg);
    return 1;
}

static Consensus::Params FrozenParams()
{
    Consensus::Params p;
    p.nBlockSubsidyBase = Consensus::ORU_INITIAL_BLOCK_SUBSIDY;
    p.nSubsidyHalvingInterval = Consensus::ORU_SUBSIDY_HALVING_INTERVAL;
    p.nBlockSubsidyMaxHalvingShifts = Consensus::ORU_SUBSIDY_MAX_SHIFTS;
    p.nCoinbaseMaturity = 100;
    return p;
}

static CScript Oru()
{
    return CScript() << RIGOR_PUSH_0 << std::vector<unsigned char>(WITNESS_V0_KEYHASH_SIZE, 0x11);
}

static CScript Res()
{
    return CScript() << RIGOR_PUSH_1 << std::vector<unsigned char>(WITNESS_V1_RES_SIZE, 0xaa);
}

static CScript Unknown()
{
    return CScript() << RIGOR_PUSH_2 << std::vector<unsigned char>(20, 0x00);
}

static LedgerCoin Coin(uint64_t id, CAmount v, LedgerAsset a, int64_t cb = -1)
{
    LedgerCoin c;
    c.outpoint.id = id;
    c.value = v;
    c.asset = a;
    c.coinbase_height = cb;
    return c;
}

static TransactionKernelInput ValidOruTx()
{
    TransactionKernelInput in;
    in.flags = TK_SCRIPT_VERIFY_WITNESS;
    in.scriptSig_push_only = true;
    in.stack_clean = true;
    in.signature.assign(64, 0x07);
    in.pubkey.assign(32, 0x09);
    const CScript s = Oru();
    in.scriptPubKey.assign(s.begin(), s.end());
    in.witness = {in.signature, in.pubkey};
    in.keyhash_match = true;
    in.crypto_accept = true;
    return in;
}

static uint256 Hex256(const char* hex)
{
    uint256 out;
    std::vector<unsigned char> raw;
    const std::string s(hex);
    for (size_t i = 0; i < s.size(); i += 2) {
        unsigned v = 0;
        std::sscanf(s.c_str() + static_cast<int>(i), "%2x", &v);
        raw.push_back(static_cast<unsigned char>(v));
    }
    std::memcpy(out.begin(), raw.data(), 32);
    return out;
}

static ConvergenceDrawSet TieSet(bool swap)
{
    const uint64_t sat = 209715200000000ULL;
    ConvergenceDrawSet set;
    set.source_height = 100;
    set.convergence_root = Hex256("1111111111111111111111111111111111111111111111111111111111111111");
    set.source_block_hash = Hex256("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
    ConvergenceDrawCandidate a, b;
    a.txid = Hex256("0000000000000000000000000000000000000000000000000000000000000001");
    a.weight = sat;
    b.txid = Hex256("0000000000000000000000000000000000000000000000000000000000000002");
    b.weight = sat;
    set.candidates = swap ? std::vector<ConvergenceDrawCandidate>{b, a}
                          : std::vector<ConvergenceDrawCandidate>{a, b};
    return set;
}

static KernelMigrationInput Base()
{
    KernelMigrationInput in;
    in.params = FrozenParams();
    in.height = 1;
    in.spend_height = 200;
    in.maturity = 100;
    in.draw_target = Consensus::RES_PROBABILITY_TARGET_CONST;
    in.tx = ValidOruTx();
    in.supply.height = 1;
    in.supply.oru_coinbase = GetBlockSubsidy(1, in.params);
    in.ledger.tx.accept = true;
    in.ledger.consensus.accept = true;
    return in;
}

static bool Verify(const KernelMigrationInput& in, KernelMigrationResult& out, const char* name)
{
    if (!KernelMigrationVerify(in, out)) {
        std::fprintf(stderr, "KERNEL_MIGRATION_MISMATCH case=%s\n", name);
        return false;
    }
    return true;
}

int main()
{
    const Consensus::Params p = FrozenParams();
    KernelMigrationResult o;

    {
        KernelMigrationInput in = Base();
        in.height = 0;
        in.supply = {};
        in.supply.height = 0;
        in.ledger = {};
        in.ledger.tx.accept = true;
        in.ledger.consensus.accept = true;
        in.tx = {};
        if (!Verify(in, o, "genesis")) return fail("genesis verify");
        if (o.oru_subsidy != 0) return fail("genesis ORU actual");
        if (o.res_schedule != 40000000) return fail("genesis RES schedule");
        if (o.res_issued != 0) return fail("genesis RES issued");
        if (o.draw_winner != -1) return fail("genesis draw");
        if (!o.supply_ok || !o.ledger_ok) return fail("genesis accept");
    }

    const int epochs[] = {1, 99, 100, 101, 209999, 210000, 210001, 419999};
    for (int h : epochs) {
        KernelMigrationInput in = Base();
        in.height = h;
        in.supply.height = h;
        in.supply.oru_coinbase = GetBlockSubsidy(h, p);
        char name[32];
        std::snprintf(name, sizeof(name), "epoch-%d", h);
        if (!Verify(in, o, name)) return fail(name);
        if (o.oru_subsidy != GetBlockSubsidy(h, p)) return fail("epoch oru");
        if (o.res_schedule != Consensus::GetResMintReward(h)) return fail("epoch res");
    }

    {
        KernelMigrationInput in = Base();
        in.classify_scripts = {Oru()};
        in.ledger.consumed = {Coin(1, 10 * COIN, LedgerAsset::ORU)};
        in.ledger.created = {Coin(2, 10 * COIN, LedgerAsset::ORU)};
        if (!Verify(in, o, "valid-spend")) return fail("valid spend");
        if (!o.spend_ok || !o.accept) return fail("valid spend accept");

        in.tx.crypto_accept = false;
        if (!Verify(in, o, "invalid-spend")) return fail("invalid spend");
        if (o.spend_ok || o.accept) return fail("invalid spend must reject");

        in = Base();
        in.classify_scripts = {Oru()};
        in.ledger.consumed = {Coin(1, 10 * COIN, LedgerAsset::ORU)};
        in.ledger.created = {Coin(2, 10 * COIN, LedgerAsset::ORU)};
        in.tx.signature.assign(8, 0x00);
        in.tx.crypto_accept = false;
        if (!Verify(in, o, "bad-signature")) return fail("bad signature");
        if (o.spend_ok) return fail("bad signature accepted");

        in = Base();
        in.classify_scripts = {Oru()};
        in.ledger.consumed = {Coin(1, 10 * COIN, LedgerAsset::ORU)};
        in.ledger.created = {Coin(2, 10 * COIN, LedgerAsset::ORU)};
        in.tx.witness = {std::vector<unsigned char>(521, 0x01), in.tx.pubkey};
        if (!Verify(in, o, "malformed-witness")) return fail("malformed witness");
        if (o.spend_ok) return fail("malformed witness accepted");

        in = Base();
        in.classify_scripts = {Unknown()};
        if (!Verify(in, o, "unknown-asset")) return fail("unknown asset");
        if (o.classification != ASSET_INVALID || o.ledger_ok) return fail("unknown asset must reject");
    }

    {
        KernelMigrationInput in = Base();
        ConvergenceDrawSet empty;
        if (ResolveConvergenceDraw(empty, in.draw_target).HasWinner()) return fail("empty F");
        in.draw = empty;
        if (!Verify(in, o, "draw-empty")) return fail("draw empty");
        if (o.draw_winner != -1) return fail("empty winner");

        ConvergenceDrawSet one = TieSet(false);
        one.candidates.resize(1);
        in.draw = one;
        if (!Verify(in, o, "draw-single")) return fail("draw single");
        if (o.draw_winner != 0) return fail("single winner");

        in.draw = TieSet(false);
        if (!Verify(in, o, "draw-equal")) return fail("draw equal");
        const int w1 = o.draw_winner;
        in.draw = TieSet(true);
        if (!Verify(in, o, "draw-tie")) return fail("draw tie");
        if (TieSet(false).candidates[static_cast<size_t>(w1)].txid !=
            TieSet(true).candidates[static_cast<size_t>(o.draw_winner)].txid) {
            return fail("txid tie-break");
        }
    }

    {
        KernelMigrationInput in = Base();
        in.height = 1;
        in.supply.oru_coinbase = GetBlockSubsidy(1, p);
        in.ledger.consensus.oru_issuance = GetBlockSubsidy(1, p);
        in.ledger.created = {Coin(1, GetBlockSubsidy(1, p), LedgerAsset::ORU)};
        if (!Verify(in, o, "ledger-mint")) return fail("mint");
        if (!o.ledger_ok) return fail("mint accept");

        in = Base();
        in.classify_scripts = {Oru()};
        in.ledger.consumed = {Coin(1, 10 * COIN, LedgerAsset::ORU)};
        in.ledger.created = {Coin(2, 8 * COIN, LedgerAsset::ORU)};
        in.ledger.fee = COIN;
        in.ledger.burn = COIN;
        if (!Verify(in, o, "ledger-burn-fee")) return fail("burn/fee");

        in = Base();
        in.classify_scripts = {Oru()};
        in.ledger.consumed = {Coin(1, 10 * COIN, LedgerAsset::ORU)};
        in.ledger.created = {Coin(2, 10 * COIN, LedgerAsset::ORU)};
        if (!Verify(in, o, "ledger-transfer")) return fail("transfer");

        in.ledger.consumed = {Coin(1, 10 * COIN, LedgerAsset::ORU), Coin(1, 10 * COIN, LedgerAsset::ORU)};
        if (!Verify(in, o, "double-spend")) return fail("double spend");
        if (o.ledger_ok) return fail("double spend accepted");

        in = Base();
        in.ledger.consumed = {Coin(1, MAX_MONEY, LedgerAsset::ORU), Coin(2, 1, LedgerAsset::ORU)};
        in.ledger.created = {Coin(3, 1, LedgerAsset::ORU)};
        if (!Verify(in, o, "overflow")) return fail("overflow");
        if (o.ledger_ok) return fail("overflow accepted");
    }

    {
        KernelMigrationInput in = Base();
        in.spend_height = 99;
        in.coin_height = 0;
        in.mint_lock = 100;
        if (!Verify(in, o, "maturity-99")) return fail("maturity 99");
        if (o.oru_mature || o.res_mature) return fail("immature");
        in.spend_height = 100;
        if (!Verify(in, o, "maturity-100")) return fail("maturity 100");
        if (!o.oru_mature || !o.res_mature) return fail("mature 100");
    }

    std::printf("PASS: kernel migration dual-path verify\n");
    std::printf("CONSENSUS_KERNEL_MIGRATION_READINESS=CLOSED/PASS\n");
    std::printf("PRODUCTION_KERNEL_SWITCHED=FALSE\n");
    return 0;
}
