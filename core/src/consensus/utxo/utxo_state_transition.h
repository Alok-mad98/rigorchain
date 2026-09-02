// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// P065 verify-only UTXO State Transition. Composes existing kernels.
// Not compiled into production (absent from Makefile.am).
// Does not recode issuance / RES / draw / classify / supply / sighash.

#ifndef BITCOIN_CONSENSUS_UTXO_UTXO_STATE_TRANSITION_H
#define BITCOIN_CONSENSUS_UTXO_UTXO_STATE_TRANSITION_H

#include <consensus/convergence_logic.h>
#include <consensus/ledger/ledger_transition.h>
#include <consensus/params.h>
#include <consensus/rigor_consensus_lock.h>
#include <consensus/rigor_issuance.h>
#include <consensus/txkernel/transaction_kernel_input.h>
#include <primitives/transaction.h>
#include <script/script.h>

#include <cstdint>
#include <vector>

struct ConsensusKernelInput {
    int64_t height{0};
    ConvergenceDrawSet draw;
    uint256 draw_target{};
};

struct UTXOStateTransitionInput {
    std::vector<LedgerCoin> snapshot;
    TransactionKernelInput tx;
    ConsensusKernelInput consensus;
    LedgerKernelInput ledger;
    CAmount fee{0};
    CAmount burn{0};
    CAmount converge{0};
    std::vector<std::vector<unsigned char>> asset_scripts;
    int64_t height{0};
    int64_t maturity{100};
    Consensus::Params params{};
};

struct LedgerDelta {
    std::vector<LedgerOutpoint> removals;
    std::vector<LedgerCoin> additions;
};

struct UTXOStateTransitionResult {
    bool accept{false};
    LedgerDelta delta;
};

/** Unique entry. Calls Transaction / Consensus / Ledger kernels only. */
inline UTXOStateTransitionResult UTXOStateTransition(const UTXOStateTransitionInput& in)
{
    UTXOStateTransitionResult out;

    if (in.height < 0) return out;

    const RigorIssuance iss = ComputeRigorIssuance(static_cast<int>(in.height), in.params);
    const ConvergenceDrawOutcome draw =
        ResolveConvergenceDraw(in.consensus.draw, in.consensus.draw_target);

    for (const auto& raw : in.asset_scripts) {
        const CScript spk(raw.begin(), raw.end());
        const AssetType classified = ClassifyRigorAsset(spk);
        if (classified != ASSET_ORU && classified != ASSET_RES) return out;
    }

    for (const auto& c : in.snapshot) {
        if (c.asset == LedgerAsset::INVALID) return out;
    }
    for (const auto& c : in.ledger.created) {
        if (c.asset == LedgerAsset::INVALID) return out;
    }

    const bool tx_ok = in.ledger.consumed.empty() || VerifyRigorPredicateBoundary(in.tx);

    LedgerKernelInput ledger = in.ledger;
    ledger.height = in.height;
    ledger.maturity = in.maturity;
    ledger.fee = in.fee;
    ledger.burn = in.burn;
    ledger.converge = in.converge;
    ledger.tx.accept = tx_ok;
    ledger.consensus.accept = true;
    if (in.ledger.consensus.oru_issuance != 0 &&
        in.ledger.consensus.oru_issuance != iss.oru_subsidy) {
        return out;
    }
    if (in.ledger.consensus.res_mint != 0 &&
        in.ledger.consensus.res_mint != iss.res_reward) {
        return out;
    }
    ledger.consensus.oru_issuance = (in.height == 0) ? 0 : in.ledger.consensus.oru_issuance;
    ledger.consensus.res_mint = (in.height == 0) ? 0 : in.ledger.consensus.res_mint;
    ledger.consensus.res_schedule = iss.res_reward;
    if (in.height == 0 && draw.HasWinner()) {
        return out;
    }

    LedgerKernelOutput applied = LedgerApply(ledger);
    out.accept = applied.accept;
    if (applied.accept) {
        out.delta.removals = std::move(applied.spent_delta);
        out.delta.additions = std::move(applied.created_delta);
    }
    return out;
}

#endif // BITCOIN_CONSENSUS_UTXO_UTXO_STATE_TRANSITION_H
