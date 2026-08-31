// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// P0-066 fixtures. Calls production / verify-only kernels. Does not recode rules.

#ifndef RIGOR_BENCHMARKS_PERF_FIXTURES_H
#define RIGOR_BENCHMARKS_PERF_FIXTURES_H

#include <consensus/amount.h>
#include <consensus/convergence_logic.h>
#include <consensus/params.h>
#include <consensus/rigor_consensus_lock.h>
#include <consensus/rigor_issuance.h>
#include <consensus/rigor_supply.h>
#include <consensus/txkernel/transaction_kernel_input.h>
#include <consensus/utxo/utxo_state_transition.h>
#include <consensus/validation.h>
#include <primitives/transaction.h>
#include <script/script.h>
#include <script/standard.h>
#include <uint256.h>

#include <cstdint>
#include <cstring>
#include <vector>

inline Consensus::Params FrozenParams()
{
    Consensus::Params p;
    p.nBlockSubsidyBase = Consensus::ORU_INITIAL_BLOCK_SUBSIDY;
    p.nSubsidyHalvingInterval = Consensus::ORU_SUBSIDY_HALVING_INTERVAL;
    p.nBlockSubsidyMaxHalvingShifts = Consensus::ORU_SUBSIDY_MAX_SHIFTS;
    p.nCoinbaseMaturity = 100;
    return p;
}

inline std::vector<unsigned char> ScriptBytes(const CScript& s)
{
    return std::vector<unsigned char>(s.begin(), s.end());
}

inline CScript OruScript()
{
    return CScript() << RIGOR_PUSH_0 << std::vector<unsigned char>(WITNESS_V0_KEYHASH_SIZE, 0x11);
}

inline CScript ResScript()
{
    return CScript() << RIGOR_PUSH_1 << std::vector<unsigned char>(WITNESS_V1_RES_SIZE, 0xaa);
}

inline CScript MintScript(int64_t lock)
{
    return GetResMintScript(lock, uint160());
}

inline TransactionKernelInput ValidOruTx()
{
    TransactionKernelInput in;
    in.flags = TK_SCRIPT_VERIFY_WITNESS;
    in.scriptSig_push_only = true;
    in.stack_clean = true;
    in.signature.assign(64, 0x07);
    in.pubkey.assign(32, 0x09);
    in.scriptPubKey = ScriptBytes(OruScript());
    in.witness = {in.signature, in.pubkey};
    in.keyhash_match = true;
    in.crypto_accept = true;
    return in;
}

inline TransactionKernelInput ValidMintTx(int64_t lock, int64_t spend)
{
    TransactionKernelInput in;
    in.flags = TK_SCRIPT_VERIFY_SIGPUSHONLY | TK_SCRIPT_VERIFY_CLEANSTACK;
    in.scriptSig_push_only = true;
    in.stack_clean = true;
    in.signature.assign(64, 0x07);
    in.pubkey.assign(32, 0x09);
    in.scriptPubKey = ScriptBytes(MintScript(lock));
    in.spend_height = spend;
    in.keyhash_match = true;
    in.crypto_accept = true;
    return in;
}

inline LedgerCoin LCoin(uint64_t id, uint32_t n, CAmount value, LedgerAsset asset, int64_t cb = -1)
{
    LedgerCoin c;
    c.outpoint.id = id;
    c.outpoint.n = n;
    c.value = value;
    c.asset = asset;
    c.coinbase_height = cb;
    return c;
}

inline uint256 ByteTxid(unsigned char b)
{
    uint256 id;
    std::memset(id.begin(), 0, 32);
    *(id.begin() + 31) = b;
    return id;
}

inline ConvergenceDrawSet MakeDraw(int n, uint64_t weight)
{
    ConvergenceDrawSet set;
    set.source_height = 500;
    set.source_nbits = 0x207fffff;
    set.source_block_hash = uint256S("0xaa");
    set.convergence_root = uint256S("0xcc");
    for (int i = 0; i < n; ++i) {
        ConvergenceDrawCandidate c;
        c.txid = ByteTxid(static_cast<unsigned char>(i + 1));
        c.weight = weight;
        set.candidates.push_back(c);
    }
    return set;
}

inline UTXOStateTransitionInput EmptyBlockInput()
{
    UTXOStateTransitionInput in;
    in.params = FrozenParams();
    in.height = 0;
    in.maturity = 100;
    in.consensus.height = 0;
    in.consensus.draw_target = Consensus::RES_PROBABILITY_TARGET_CONST;
    return in;
}

inline UTXOStateTransitionInput OruTransferInput()
{
    UTXOStateTransitionInput in;
    in.params = FrozenParams();
    in.height = 200;
    in.maturity = 100;
    in.tx = ValidOruTx();
    in.snapshot = {LCoin(1, 0, 10 * COIN, LedgerAsset::ORU)};
    in.ledger.consumed = in.snapshot;
    in.ledger.created = {LCoin(2, 0, 10 * COIN, LedgerAsset::ORU)};
    in.asset_scripts = {ScriptBytes(OruScript())};
    in.consensus.draw_target = Consensus::RES_PROBABILITY_TARGET_CONST;
    return in;
}

inline UTXOStateTransitionInput ResMaturityInput(int64_t spend)
{
    UTXOStateTransitionInput in = OruTransferInput();
    in.height = spend;
    in.tx = ValidMintTx(100, spend);
    in.snapshot = {LCoin(3, 0, 40 * Consensus::RES_COIN, LedgerAsset::RES, 0)};
    in.ledger.consumed = in.snapshot;
    in.ledger.created = {LCoin(4, 0, 40 * Consensus::RES_COIN, LedgerAsset::RES)};
    in.asset_scripts = {ScriptBytes(MintScript(100))};
    return in;
}

inline UTXOStateTransitionInput DrawBlockInput()
{
    const auto& p = FrozenParams();
    UTXOStateTransitionInput in;
    in.params = p;
    in.height = 1;
    in.tx = ValidOruTx();
    in.consensus.draw = MakeDraw(2, 209715200000000ULL);
    in.consensus.draw_target = Consensus::RES_PROBABILITY_TARGET_CONST;
    in.ledger.consensus.oru_issuance = GetBlockSubsidy(1, p);
    in.ledger.created = {LCoin(1, 0, GetBlockSubsidy(1, p), LedgerAsset::ORU)};
    return in;
}

inline UTXOStateTransitionInput LargeUtxoInput(size_t n)
{
    UTXOStateTransitionInput in = OruTransferInput();
    in.snapshot.clear();
    in.snapshot.reserve(n);
    for (size_t i = 0; i < n; ++i) {
        in.snapshot.push_back(LCoin(1000 + i, 0, COIN, LedgerAsset::ORU));
    }
    in.ledger.consumed = {in.snapshot[0]};
    in.ledger.created = {LCoin(9, 0, COIN, LedgerAsset::ORU)};
    return in;
}

inline UTXOStateTransitionInput ManyInInput(size_t n)
{
    UTXOStateTransitionInput in = OruTransferInput();
    in.snapshot.clear();
    in.ledger.consumed.clear();
    CAmount sum = 0;
    for (size_t i = 0; i < n; ++i) {
        const LedgerCoin c = LCoin(20 + i, 0, COIN, LedgerAsset::ORU);
        in.snapshot.push_back(c);
        in.ledger.consumed.push_back(c);
        sum += COIN;
    }
    in.ledger.created = {LCoin(99, 0, sum, LedgerAsset::ORU)};
    return in;
}

inline UTXOStateTransitionInput ManyOutInput(size_t n)
{
    UTXOStateTransitionInput in = OruTransferInput();
    const CAmount each = COIN;
    in.snapshot = {LCoin(1, 0, each * static_cast<CAmount>(n), LedgerAsset::ORU)};
    in.ledger.consumed = in.snapshot;
    in.ledger.created.clear();
    for (size_t i = 0; i < n; ++i) {
        in.ledger.created.push_back(LCoin(50 + i, 0, each, LedgerAsset::ORU));
    }
    return in;
}

inline uint64_t LegacyConsensusStep(int height, const ConvergenceDrawSet& draw)
{
    const Consensus::Params p = FrozenParams();
    const CAmount oru = GetBlockSubsidy(height, p);
    const CAmount res = Consensus::GetResMintReward(height);
    const ConvergenceDrawOutcome o = ResolveConvergenceDraw(draw, Consensus::RES_PROBABILITY_TARGET_CONST);
    RigorSupplyDeltaInput sin;
    sin.height = height;
    sin.oru_coinbase = oru;
    BlockValidationState state;
    RigorSupplyDeltaResult out;
    const bool ok = RigorSupplyDelta(sin, p, out, state);
    return static_cast<uint64_t>(oru) ^ static_cast<uint64_t>(res) ^
           static_cast<uint64_t>(o.winner_index + 2) ^ (ok ? 1ULL : 0ULL);
}

inline uint64_t KernelConsensusStep(int height, const ConvergenceDrawSet& draw)
{
    const Consensus::Params p = FrozenParams();
    const RigorIssuance iss = ComputeRigorIssuance(height, p);
    const ConvergenceDrawOutcome o = ResolveConvergenceDraw(draw, Consensus::RES_PROBABILITY_TARGET_CONST);
    RigorSupplyDeltaInput sin;
    sin.height = height;
    sin.oru_coinbase = iss.oru_subsidy;
    BlockValidationState state;
    RigorSupplyDeltaResult out;
    const bool ok = RigorSupplyDelta(sin, p, out, state);
    return static_cast<uint64_t>(iss.oru_subsidy) ^ static_cast<uint64_t>(iss.res_reward) ^
           static_cast<uint64_t>(o.winner_index + 2) ^ (ok ? 1ULL : 0ULL);
}

inline uint64_t LegacyTxStep(const TransactionKernelInput& in)
{
    const CScript spk(in.scriptPubKey.begin(), in.scriptPubKey.end());
    std::vector<std::vector<unsigned char>> sols;
    const TxoutType t = ClassifyRigorTxout(spk, sols);
    const AssetType a = ClassifyRigorAsset(spk);
    const bool mint = IsResMintScript(spk);
    int wv = 0;
    std::vector<unsigned char> prog;
    const bool wit = spk.IsWitnessProgram(wv, prog);
    return static_cast<uint64_t>(t) ^ (static_cast<uint64_t>(a) << 8) ^ (mint ? 1ULL : 0) ^ (wit ? 2ULL : 0);
}

inline uint64_t KernelTxStep(const TransactionKernelInput& in)
{
    return VerifyRigorPredicateBoundary(in) ? 1ULL : 0ULL;
}

inline uint64_t LegacyLedgerStep(const UTXOStateTransitionInput& in)
{
    const RigorIssuance iss = ComputeRigorIssuance(static_cast<int>(in.height), in.params);
    (void)ResolveConvergenceDraw(in.consensus.draw, in.consensus.draw_target);
    const bool tx_ok = in.ledger.consumed.empty() || VerifyRigorPredicateBoundary(in.tx);
    LedgerKernelInput ledger = in.ledger;
    ledger.height = in.height;
    ledger.maturity = in.maturity;
    ledger.tx.accept = tx_ok;
    ledger.consensus.accept = true;
    ledger.consensus.oru_issuance = (in.height == 0) ? 0 : ledger.consensus.oru_issuance;
    ledger.consensus.res_mint = (in.height == 0) ? 0 : ledger.consensus.res_mint;
    ledger.consensus.res_schedule = iss.res_reward;
    const LedgerKernelOutput o = LedgerApply(ledger);
    return (o.accept ? 1ULL : 0ULL) ^ (static_cast<uint64_t>(o.created_delta.size()) << 1);
}

inline uint64_t KernelUtxoStep(const UTXOStateTransitionInput& in)
{
    const UTXOStateTransitionResult o = UTXOStateTransition(in);
    return (o.accept ? 1ULL : 0ULL) ^ (static_cast<uint64_t>(o.delta.additions.size()) << 1) ^
           (static_cast<uint64_t>(o.delta.removals.size()) << 8);
}

#endif
