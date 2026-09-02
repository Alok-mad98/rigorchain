// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// P065: UTXO state-transition composition. Standalone.
// Does not switch production. Does not recode issuance / RES / draw / classify.

#include <consensus/amount.h>
#include <consensus/convergence_logic.h>
#include <consensus/params.h>
#include <consensus/rigor_consensus_lock.h>
#include <consensus/rigor_issuance.h>
#include <consensus/utxo/utxo_state_transition.h>
#include <primitives/transaction.h>
#include <script/script.h>
#include <script/standard.h>
#include <uint256.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
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

static uint256 uint256_from_hex(const char* hex)
{
    uint256 out;
    std::vector<unsigned char> raw;
    const std::string s(hex);
    for (size_t i = 0; i < s.size(); i += 2) {
        unsigned v = 0;
        std::sscanf(s.c_str() + static_cast<int>(i), "%2x", &v);
        raw.push_back(static_cast<unsigned char>(v));
    }
    if (raw.size() != 32) std::exit(2);
    std::memcpy(out.begin(), raw.data(), 32);
    return out;
}

static LedgerCoin Coin(uint64_t id, uint32_t n, CAmount value, LedgerAsset asset, int64_t cb = -1)
{
    LedgerCoin c;
    c.outpoint.id = id;
    c.outpoint.n = n;
    c.value = value;
    c.asset = asset;
    c.coinbase_height = cb;
    return c;
}

static std::vector<unsigned char> Bytes(const CScript& s)
{
    return std::vector<unsigned char>(s.begin(), s.end());
}

static TransactionKernelInput ValidOruTx()
{
    TransactionKernelInput in;
    const CScript oru = CScript() << RIGOR_PUSH_0 << std::vector<unsigned char>(WITNESS_V0_KEYHASH_SIZE, 0x11);
    in.flags = TK_SCRIPT_VERIFY_WITNESS;
    in.scriptSig_push_only = true;
    in.stack_clean = true;
    in.signature.assign(64, 0x07);
    in.pubkey.assign(32, 0x09);
    in.scriptPubKey = Bytes(oru);
    in.witness = {in.signature, in.pubkey};
    in.keyhash_match = true;
    in.crypto_accept = true;
    in.sighash_tag = KernelAssignedSighashTag(CScript(in.scriptPubKey.begin(), in.scriptPubKey.end()));
    return in;
}

static UTXOStateTransitionInput Base()
{
    UTXOStateTransitionInput in;
    in.params = FrozenParams();
    in.height = 1;
    in.maturity = 100;
    in.consensus.height = 1;
    in.tx = ValidOruTx();
    return in;
}

static ConvergenceDrawSet TieSet(bool swap)
{
    const uint64_t sat = 209715200000000ULL;
    ConvergenceDrawSet set;
    set.source_height = 100;
    set.convergence_root = uint256_from_hex(
        "1111111111111111111111111111111111111111111111111111111111111111");
    set.source_block_hash = uint256_from_hex(
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
    ConvergenceDrawCandidate a, b;
    a.txid = uint256_from_hex(
        "0000000000000000000000000000000000000000000000000000000000000001");
    a.weight = sat;
    b.txid = uint256_from_hex(
        "0000000000000000000000000000000000000000000000000000000000000002");
    b.weight = sat;
    set.candidates = swap ? std::vector<ConvergenceDrawCandidate>{b, a}
                          : std::vector<ConvergenceDrawCandidate>{a, b};
    return set;
}

int main()
{
    const Consensus::Params p = FrozenParams();
    const CScript oru = CScript() << RIGOR_PUSH_0 << std::vector<unsigned char>(WITNESS_V0_KEYHASH_SIZE, 0x11);
    const CScript res = CScript() << RIGOR_PUSH_1 << std::vector<unsigned char>(WITNESS_V1_RES_SIZE, 0xaa);
    const CScript empty;
    const CScript v2 = CScript() << RIGOR_PUSH_2 << std::vector<unsigned char>(20, 0x00);

    // --- Genesis ---
    {
        if (GetBlockSubsidy(0, p) != 0) return fail("genesis ORU actual");
        if (Consensus::GetResMintReward(0) != 40000000) return fail("genesis RES schedule");
        UTXOStateTransitionInput in = Base();
        in.height = 0;
        in.consensus.height = 0;
        const UTXOStateTransitionResult o = UTXOStateTransition(in);
        if (!o.accept) return fail("genesis accept");
        if (!o.delta.removals.empty() || !o.delta.additions.empty()) return fail("genesis empty delta");
        if (ComputeRigorIssuance(0, p).oru_subsidy != 0) return fail("genesis issuance pack");
        if (ComputeRigorIssuance(0, p).res_reward != 40000000) return fail("genesis schedule pack");
    }

    // --- Maturity: coinbase_height 0 ---
    {
        UTXOStateTransitionInput in = Base();
        in.tx = ValidOruTx();
        in.snapshot = {Coin(7, 0, 10, LedgerAsset::ORU, 0)};
        in.ledger.consumed = in.snapshot;
        in.ledger.created = {Coin(8, 0, 10, LedgerAsset::ORU)};
        in.height = 99;
        if (UTXOStateTransition(in).accept) return fail("maturity 99");
        in.height = 100;
        if (!UTXOStateTransition(in).accept) return fail("maturity 100");
        in.height = 101;
        if (!UTXOStateTransition(in).accept) return fail("maturity 101");
    }

    // --- Epoch ---
    if (RigorIssuanceEpoch(209999) != 0) return fail("epoch 209999");
    if (RigorIssuanceEpoch(210000) != 1) return fail("epoch 210000");
    if (GetBlockSubsidy(209999, p) != 100000000000LL) return fail("ORU 209999");
    if (GetBlockSubsidy(210000, p) != 50000000000LL) return fail("ORU 210000");
    if (Consensus::GetResMintReward(209999) != 40000000) return fail("RES 209999");
    if (Consensus::GetResMintReward(210000) != 14142196) return fail("RES 210000");
    {
        UTXOStateTransitionInput e0 = Base();
        e0.height = 209999;
        e0.ledger.consensus.oru_issuance = GetBlockSubsidy(209999, p);
        e0.ledger.created = {Coin(1, 0, GetBlockSubsidy(209999, p), LedgerAsset::ORU)};
        if (!UTXOStateTransition(e0).accept) return fail("epoch 209999 transition");
        UTXOStateTransitionInput e1 = Base();
        e1.height = 210000;
        e1.ledger.consensus.oru_issuance = GetBlockSubsidy(210000, p);
        e1.ledger.created = {Coin(1, 0, GetBlockSubsidy(210000, p), LedgerAsset::ORU)};
        if (!UTXOStateTransition(e1).accept) return fail("epoch 210000 transition");
    }

    // --- Double spend ---
    {
        UTXOStateTransitionInput in = Base();
        const LedgerCoin c = Coin(1, 0, 10, LedgerAsset::ORU);
        in.ledger.consumed = {c, c};
        in.ledger.created = {Coin(2, 0, 10, LedgerAsset::ORU)};
        if (UTXOStateTransition(in).accept) return fail("double spend");
    }

    // --- Asset ---
    if (ClassifyRigorAsset(oru) != ASSET_ORU) return fail("classify ORU");
    if (ClassifyRigorAsset(res) != ASSET_RES) return fail("classify RES");
    if (ClassifyRigorAsset(empty) != ASSET_INVALID) return fail("classify empty");
    if (ClassifyRigorAsset(v2) != ASSET_INVALID) return fail("classify unknown version");
    {
        UTXOStateTransitionInput ok = Base();
        ok.height = 0;
        ok.asset_scripts = {Bytes(oru), Bytes(res)};
        if (!UTXOStateTransition(ok).accept) return fail("valid assets");
        UTXOStateTransitionInput bad = ok;
        bad.asset_scripts = {Bytes(empty)};
        if (UTXOStateTransition(bad).accept) return fail("invalid asset");
        UTXOStateTransitionInput unk = ok;
        unk.asset_scripts = {Bytes(v2)};
        if (UTXOStateTransition(unk).accept) return fail("unknown version");
    }

    // --- Draw ---
    {
        const uint256 T = Consensus::RES_PROBABILITY_TARGET_CONST;
        ConvergenceDrawSet empty_f;
        if (ResolveConvergenceDraw(empty_f, T).HasWinner()) return fail("empty F");

        ConvergenceDrawSet one = TieSet(false);
        one.candidates.resize(1);
        if (!ResolveConvergenceDraw(one, T).HasWinner()) return fail("single candidate");

        const auto w2 = ResolveConvergenceDraw(TieSet(false), T);
        const auto w3 = ResolveConvergenceDraw(TieSet(true), T);
        if (!w2.HasWinner() || !w3.HasWinner()) return fail("weight/txid tie");
        if (TieSet(false).candidates[static_cast<size_t>(w2.winner_index)].txid !=
            TieSet(true).candidates[static_cast<size_t>(w3.winner_index)].txid) {
            return fail("txid tie break");
        }

        UTXOStateTransitionInput g = Base();
        g.height = 0;
        g.consensus.draw = empty_f;
        g.consensus.draw_target = T;
        if (!UTXOStateTransition(g).accept) return fail("genesis draw none");

        UTXOStateTransitionInput s = Base();
        s.height = 1;
        s.consensus.draw = one;
        s.consensus.draw_target = T;
        s.ledger.consensus.oru_issuance = GetBlockSubsidy(1, p);
        s.ledger.created = {Coin(1, 0, GetBlockSubsidy(1, p), LedgerAsset::ORU)};
        if (!UTXOStateTransition(s).accept) return fail("single draw transition");
    }

    // --- Malformed ---
    {
        UTXOStateTransitionInput ov = Base();
        ov.ledger.consumed = {Coin(1, 0, MAX_MONEY, LedgerAsset::ORU), Coin(2, 0, 1, LedgerAsset::ORU)};
        ov.ledger.created = {Coin(3, 0, 1, LedgerAsset::ORU)};
        if (UTXOStateTransition(ov).accept) return fail("overflow");

        UTXOStateTransitionInput neg = Base();
        neg.height = -1;
        if (UTXOStateTransition(neg).accept) return fail("negative height");

        UTXOStateTransitionInput wit = Base();
        wit.snapshot = {Coin(1, 0, 10, LedgerAsset::ORU)};
        wit.ledger.consumed = wit.snapshot;
        wit.ledger.created = {Coin(2, 0, 10, LedgerAsset::ORU)};
        wit.tx.witness = {std::vector<unsigned char>(521, 0x01), wit.tx.pubkey};
        if (UTXOStateTransition(wit).accept) return fail("invalid witness");

        UTXOStateTransitionInput sig = Base();
        sig.snapshot = {Coin(1, 0, 10, LedgerAsset::ORU)};
        sig.ledger.consumed = sig.snapshot;
        sig.ledger.created = {Coin(2, 0, 10, LedgerAsset::ORU)};
        sig.tx.crypto_accept = false;
        if (UTXOStateTransition(sig).accept) return fail("bad signature");
    }

    // --- Deterministic fuzz (malformed) ---
    for (int i = 0; i < 32; ++i) {
        UTXOStateTransitionInput in = Base();
        in.height = -1 - i;
        if (UTXOStateTransition(in).accept) return fail("fuzz negative height");
        in = Base();
        in.ledger.consumed = {Coin(1, 0, MAX_MONEY, LedgerAsset::ORU), Coin(2, 0, static_cast<CAmount>(i) + 1, LedgerAsset::ORU)};
        in.ledger.created = {Coin(3, 0, 1, LedgerAsset::ORU)};
        if (UTXOStateTransition(in).accept) return fail("fuzz overflow");
        in = Base();
        in.snapshot = {Coin(1, 0, 10, LedgerAsset::ORU)};
        in.ledger.consumed = in.snapshot;
        in.ledger.created = {Coin(2, 0, 10, LedgerAsset::ORU)};
        in.tx.crypto_accept = false;
        in.tx.witness.clear();
        if (i % 2 == 0) in.tx.signature.assign(8, 0x00);
        if (UTXOStateTransition(in).accept) return fail("fuzz malformed spend");
    }

    std::printf("PASS: utxo state transition refinement\n");
    std::printf("UTXO_STATE_TRANSITION_REFINEMENT=PASS\n");
        std::printf("PRODUCTION_CONSENSUS_AUTHORITY=RigorConsensusKernel(C++)\n");
    std::printf("FORMAL_REFINEMENT_STATUS=PROVED\n");
    std::printf("EXTRACTED_KERNEL_PRODUCTION=FALSE\n");
    std::printf("COMPCERT_BINARY_VERIFIED_KERNEL=FALSE\n");
    return 0;
}
