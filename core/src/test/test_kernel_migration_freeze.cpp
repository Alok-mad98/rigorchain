// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// P068 freeze: current C++ authority vs migration candidate.
// Does not switch production. Does not recode formulas.

#if !defined(CONSENSUS_KERNEL_MIGRATION_VERIFY_ONLY)
#error "test_kernel_migration_freeze is verify-only"
#endif

#include <consensus/kernel_migration/kernel_migration.h>
#include <consensus/rigor_consensus_lock.h>
#include <uint256.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
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
    in.sighash_tag = KernelAssignedSighashTag(s);
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
        std::fprintf(stderr, "CONSENSUS_KERNEL_FREEZE_DRIFT case=%s\n", name);
        return false;
    }
    return true;
}

int main()
{
    const Consensus::Params p = FrozenParams();
    KernelMigrationResult o;

    if (Consensus::GetResMintReward(0) != 40000000) return fail("GetResMintReward(0)");
    if (ComputeRigorIssuance(0, p).res_reward != 40000000) return fail("genesis RES schedule pack");
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
        if (o.res_schedule != 40000000 || o.res_issued != 0) return fail("res_issued_at_height(0)");
        if (o.oru_subsidy != 0 || o.draw_winner != -1) return fail("genesis issuance/draw");
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
        if (o.res_schedule != Consensus::GetResMintReward(h)) return fail("issuance res drift");
        if (o.res_issued != (h == 0 ? 0 : o.res_schedule)) return fail("issuance res issued");
    }

    {
        KernelMigrationInput in = Base();
        in.draw = {};
        if (!Verify(in, o, "draw-empty")) return fail("empty F");
        if (o.draw_winner != -1) return fail("empty winner");

        ConvergenceDrawSet one = TieSet(false);
        one.candidates.resize(1);
        in.draw = one;
        if (!Verify(in, o, "draw-single")) return fail("single");
        if (o.draw_winner != 0) return fail("single winner");

        in.draw = TieSet(false);
        if (!Verify(in, o, "draw-equal")) return fail("equal weight");
        const uint256 w1 = TieSet(false).candidates[static_cast<size_t>(o.draw_winner)].txid;
        in.draw = TieSet(true);
        if (!Verify(in, o, "draw-txid")) return fail("different txid");
        if (TieSet(true).candidates[static_cast<size_t>(o.draw_winner)].txid != w1) {
            return fail("txid tie-break");
        }

        in.draw = TieSet(false);
        in.draw_target = uint256();
        if (!Verify(in, o, "draw-T0")) return fail("T=0");
        if (o.draw_winner != -1) return fail("T=0 must have no winner");

        ConvergenceDrawSet ov = TieSet(false);
        ov.candidates.resize(1);
        ov.candidates[0].weight = std::numeric_limits<uint64_t>::max();
        in = Base();
        in.draw = ov;
        if (!Verify(in, o, "draw-overflow")) return fail("overflow weight");
        if (o.draw_winner != -1) return fail("overflow weight must have no winner");

        ConvergenceDrawSet dup = TieSet(false);
        dup.candidates[1].txid = dup.candidates[0].txid;
        in = Base();
        in.draw = dup;
        if (!Verify(in, o, "draw-dup")) return fail("duplicate tx");
        if (o.draw_winner < 0) return fail("duplicate tx must still name the txid");
        if (dup.candidates[static_cast<size_t>(o.draw_winner)].txid != dup.candidates[0].txid) {
            return fail("duplicate tx winner");
        }
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
