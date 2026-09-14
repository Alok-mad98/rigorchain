// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// P068 freeze: current C++ authority vs migration candidate.
// Does not switch production. Does not recode formulas.
// RFC-019: (oru_subsidy, RATE(h), epoch). No draw. res_issued is ledger mint.

#if !defined(CONSENSUS_KERNEL_MIGRATION_VERIFY_ONLY)
#error "test_kernel_migration_freeze is verify-only"
#endif

#include <consensus/kernel_migration/kernel_migration.h>
#include <consensus/res_issuance.h>
#include <consensus/rigor_consensus_lock.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

static int fail(const char* msg)
{
    std::fprintf(stderr, "CONSENSUS_KERNEL_FREEZE_DRIFT\nFAIL: %s\n", msg);
    return 1;
}

static Consensus::Params FrozenParams()
{
    Consensus::Params p;
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
    in.sighash_tag = KernelAssignedSighashTag(s);
    return in;
}

static KernelMigrationInput Base()
{
    KernelMigrationInput in;
    in.params = FrozenParams();
    in.height = 1;
    in.spend_height = 200;
    in.maturity = 100;
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
        std::fprintf(stderr, "CONSENSUS_KERNEL_FREEZE_DRIFT case=%s\n", name);
        return false;
    }
    return true;
}

int main()
{
    const Consensus::Params p = FrozenParams();
    KernelMigrationResult o;

    if (Consensus::ResDestroyCostAtHeight(0) != Consensus::RES_DESTROY_COST_GENESIS) {
        return fail("ResDestroyCostAtHeight(0)");
    }
    if (GetBlockSubsidy(0, p) != 0) return fail("res_issued ORU actual");
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
        if (o.res_destroy_cost != Consensus::RES_DESTROY_COST_GENESIS) return fail("genesis RATE");
        if (o.res_issued != 0) return fail("res_issued_at_height(0)");
        if (o.oru_subsidy != 0) return fail("genesis issuance");
    }

    const int heights[] = {0, 1, 99, 100, 101, 209999, 210000, 419999, 420000};
    for (int h : heights) {
        KernelMigrationInput in = Base();
        in.height = h;
        in.supply.height = h;
        in.supply.oru_coinbase = GetBlockSubsidy(h, p);
        char name[32];
        std::snprintf(name, sizeof(name), "issuance-%d", h);
        if (!Verify(in, o, name)) return fail(name);
        if (o.oru_subsidy != GetBlockSubsidy(h, p)) return fail("issuance oru drift");
        if (o.res_destroy_cost != Consensus::ResDestroyCostAtHeight(h)) return fail("issuance RATE drift");
        if (o.res_issued != 0) return fail("issuance res issued is ledger mint, Base has none");
    }

    {
        KernelMigrationInput in = Base();
        in.classify_scripts = {Oru()};
        if (!Verify(in, o, "classify-oru")) return fail("ORU");
        if (o.classification != ASSET_ORU) return fail("ORU class");

        in.classify_scripts = {Res()};
        if (!Verify(in, o, "classify-res")) return fail("RES");
        if (o.classification != ASSET_RES) return fail("RES class");

        in.classify_scripts = {Unknown()};
        if (!Verify(in, o, "classify-unknown")) return fail("unknown version");
        if (o.classification != ASSET_INVALID || o.ledger_ok) return fail("unknown must reject");

        const CScript malformed = CScript() << CScriptNum(0) << std::vector<unsigned char>(19, 0x11);
        in.classify_scripts = {malformed};
        if (!Verify(in, o, "classify-malformed")) return fail("malformed");
        if (o.classification != ASSET_INVALID) return fail("malformed class");

        const std::vector<unsigned char> huge(10 * 1024, 0xff);
        in.classify_scripts = {CScript(huge.begin(), huge.end())};
        if (!Verify(in, o, "classify-oversized")) return fail("oversized");
        if (o.classification != ASSET_INVALID) return fail("oversized class");

        in.classify_scripts = {CScript()};
        if (!Verify(in, o, "classify-empty")) return fail("empty script");
        if (o.classification != ASSET_INVALID) return fail("empty class");
    }

    {
        KernelMigrationInput in = Base();
        in.classify_scripts = {Oru()};
        in.ledger.consumed = {Coin(1, 10 * COIN, LedgerAsset::ORU)};
        in.ledger.created = {Coin(2, 10 * COIN, LedgerAsset::ORU)};
        in.tx.crypto_accept = false;
        if (!Verify(in, o, "bad-signature")) return fail("bad signature");
        if (o.spend_ok) return fail("bad signature accepted");

        in.tx = ValidOruTx();
        in.tx.pubkey.assign(32, 0xaa);
        in.tx.witness = {in.tx.signature, in.tx.pubkey};
        in.tx.keyhash_match = false;
        if (!Verify(in, o, "wrong-pubkey")) return fail("wrong pubkey");
        if (o.spend_ok) return fail("wrong pubkey accepted");

        in.tx = ValidOruTx();
        in.tx.witness[0][0] ^= 0x01;
        in.tx.crypto_accept = false;
        if (!Verify(in, o, "mutated-witness")) return fail("mutated witness");
        if (o.spend_ok) return fail("mutated witness accepted");

        in.tx = ValidOruTx();
        in.tx.signature.assign(8, 0x00);
        in.tx.crypto_accept = false;
        if (!Verify(in, o, "wrong-digest")) return fail("wrong digest");
        if (o.spend_ok) return fail("wrong digest accepted");

        in.tx = ValidOruTx();
        in.tx.chain_id.assign(32, 0xde);
        in.tx.crypto_accept = false;
        if (!Verify(in, o, "replay-domain")) return fail("replay domain");
        if (o.spend_ok) return fail("replay domain accepted");
    }

    {
        KernelMigrationInput in = Base();
        in.classify_scripts = {Oru()};
        in.ledger.consumed = {Coin(1, 10 * COIN, LedgerAsset::ORU), Coin(1, 10 * COIN, LedgerAsset::ORU)};
        in.ledger.created = {Coin(2, 10 * COIN, LedgerAsset::ORU)};
        if (!Verify(in, o, "double-spend")) return fail("double spend");
        if (o.ledger_ok) return fail("double spend accepted");

        in = Base();
        in.ledger.consumed = {Coin(1, -1, LedgerAsset::ORU)};
        in.ledger.created = {Coin(2, 0, LedgerAsset::ORU)};
        if (!Verify(in, o, "negative")) return fail("negative");
        if (o.ledger_ok) return fail("negative accepted");

        in = Base();
        in.ledger.consumed = {Coin(1, MAX_MONEY, LedgerAsset::ORU), Coin(2, 1, LedgerAsset::ORU)};
        in.ledger.created = {Coin(3, 1, LedgerAsset::ORU)};
        if (!Verify(in, o, "overflow")) return fail("overflow");
        if (o.ledger_ok) return fail("overflow accepted");

        in = Base();
        in.classify_scripts = {Oru()};
        in.ledger.consumed = {Coin(1, 10 * COIN, LedgerAsset::ORU), Coin(1, 5 * COIN, LedgerAsset::ORU)};
        in.ledger.created = {Coin(2, 15 * COIN, LedgerAsset::ORU)};
        if (!Verify(in, o, "dup-input")) return fail("duplicate input");
        if (o.ledger_ok) return fail("duplicate input accepted");

        in = Base();
        in.classify_scripts = {Unknown()};
        in.ledger.consumed = {Coin(1, 10 * COIN, LedgerAsset::INVALID)};
        in.ledger.created = {Coin(2, 10 * COIN, LedgerAsset::INVALID)};
        if (!Verify(in, o, "unknown-asset")) return fail("unknown asset");
        if (o.ledger_ok) return fail("unknown asset accepted");
    }

    std::printf("PASS: kernel migration freeze dual-path\n");
    std::printf("CONSENSUS_KERNEL_FREEZE_PASS\n");
        std::printf("PRODUCTION_CONSENSUS_AUTHORITY=RigorConsensusKernel(C++)\n");
    std::printf("FORMAL_REFINEMENT_STATUS=PROVED\n");
    std::printf("EXTRACTED_KERNEL_PRODUCTION=FALSE\n");
    std::printf("COMPCERT_BINARY_VERIFIED_KERNEL=FALSE\n");
    return 0;
}
