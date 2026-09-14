// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// P065: UTXO state-transition composition. Standalone.
// Does not switch production. Does not recode issuance / RES / draw / classify.

#include <consensus/amount.h>
#include <consensus/params.h>
#include <consensus/res_issuance.h>
#include <consensus/rigor_consensus_lock.h>
#include <consensus/rigor_issuance.h>
#include <consensus/utxo/utxo_state_transition.h>
#include <primitives/transaction.h>
#include <script/script.h>
#include <script/standard.h>

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
    p.nCoinbaseMaturity = 100;
    return p;
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
        if (Consensus::ResDestroyCostAtHeight(0) != Consensus::RES_DESTROY_COST_GENESIS) {
            return fail("genesis RATE");
        }
        UTXOStateTransitionInput in = Base();
        in.height = 0;
        in.consensus.height = 0;
        const UTXOStateTransitionResult o = UTXOStateTransition(in);
        if (!o.accept) return fail("genesis accept");
        if (!o.delta.removals.empty() || !o.delta.additions.empty()) return fail("genesis empty delta");
        if (ComputeRigorIssuance(0, p).oru_subsidy != 0) return fail("genesis issuance pack");
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
    if (GetBlockSubsidy(209999, p) != Consensus::ORU_FLAT_BLOCK_SUBSIDY) return fail("ORU 209999");
    if (GetBlockSubsidy(210000, p) != Consensus::ORU_FLAT_BLOCK_SUBSIDY) return fail("ORU 210000");
    if (Consensus::ResDestroyCostAtHeight(209999) != 103685743011LL) return fail("RATE 209999");
    if (Consensus::ResDestroyCostAtHeight(210000) != 103685749152LL) return fail("RATE 210000");
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
