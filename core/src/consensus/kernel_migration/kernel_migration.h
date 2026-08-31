// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// P067 verify-only migration shim. Calls existing authorities only.
// Not compiled into production. Does not recode formulas.
// PRODUCTION_KERNEL_SWITCHED = FALSE

#ifndef BITCOIN_CONSENSUS_KERNEL_MIGRATION_KERNEL_MIGRATION_H
#define BITCOIN_CONSENSUS_KERNEL_MIGRATION_KERNEL_MIGRATION_H

#if !defined(CONSENSUS_KERNEL_MIGRATION_VERIFY_ONLY)
#error "consensus/kernel_migration is verify-only; compile with -DCONSENSUS_KERNEL_MIGRATION_VERIFY_ONLY"
#endif

#ifndef KERNEL_MIGRATION_LOG
#define KERNEL_MIGRATION_LOG 0
#endif

#include <consensus/amount.h>
#include <consensus/convergence_logic.h>
#include <consensus/ledger/ledger_transition.h>
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

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#if KERNEL_MIGRATION_LOG
#define KM_LOG(...) std::fprintf(stderr, __VA_ARGS__)
#else
#define KM_LOG(...)
#endif

/** ConnectBlock-shaped explicit input. No CCoinsView / CBlockIndex / DB. */
struct KernelMigrationInput {
    int height{0};
    int spend_height{0};
    int coin_height{-1};
    int64_t mint_lock{-1};
    int64_t maturity{100};
    ConvergenceDrawSet draw;
    uint256 draw_target{};
    RigorSupplyDeltaInput supply;
    TransactionKernelInput tx;
    LedgerKernelInput ledger;
    std::vector<CScript> classify_scripts;
    Consensus::Params params{};
};

struct KernelMigrationResult {
    CAmount oru_subsidy{0};
    CAmount res_schedule{0};
    CAmount res_issued{0};
    bool oru_mature{true};
    bool res_mature{true};
    int draw_winner{-1};
    AssetType classification{ASSET_INVALID};
    bool spend_ok{false};
    bool supply_ok{false};
    std::string supply_reject;
    bool ledger_ok{false};
    bool accept{false};
};

inline bool KernelMigrationMatureOru(int spend_height, int coin_height, int64_t maturity)
{
    if (coin_height < 0) return true;
    return spend_height - coin_height >= maturity;
}

inline bool KernelMigrationMatureRes(int spend_height, int64_t mint_lock)
{
    if (mint_lock < 0) return true;
    return static_cast<int64_t>(spend_height) >= mint_lock;
}

inline AssetType KernelMigrationPrimaryAsset(const KernelMigrationInput& in)
{
    if (in.classify_scripts.empty()) return ASSET_INVALID;
    return ClassifyRigorAsset(in.classify_scripts[0]);
}

/** Path A — current production authorities (ConnectBlock-shaped). */
inline KernelMigrationResult RunProductionAuthorityPath(const KernelMigrationInput& in)
{
    KernelMigrationResult out;
    out.oru_subsidy = GetBlockSubsidy(in.height, in.params);
    out.res_schedule = Consensus::GetResMintReward(in.height);
    const RigorIssuance pack = ComputeRigorIssuance(in.height, in.params);
    if (pack.oru_subsidy != out.oru_subsidy || pack.res_reward != out.res_schedule) {
        KM_LOG("migration: production issuance pack mismatch\n");
        return out;
    }
    out.res_issued = (in.height == 0) ? 0 : out.res_schedule;
    out.oru_mature = KernelMigrationMatureOru(in.spend_height, in.coin_height, in.maturity);
    out.res_mature = KernelMigrationMatureRes(in.spend_height, in.mint_lock);

    const ConvergenceDrawOutcome draw = ResolveConvergenceDraw(in.draw, in.draw_target);
    out.draw_winner = draw.winner_index;

    out.classification = KernelMigrationPrimaryAsset(in);
    bool assets_ok = true;
    for (const CScript& s : in.classify_scripts) {
        std::vector<std::vector<unsigned char>> sols;
        (void)ClassifyRigorTxout(s, sols);
        const AssetType a = ClassifyRigorAsset(s);
        if (a != ASSET_ORU && a != ASSET_RES) assets_ok = false;
    }

    out.spend_ok = in.ledger.consumed.empty() || VerifyRigorPredicateBoundary(in.tx);

    RigorSupplyDeltaInput supply = in.supply;
    supply.height = in.height;
    BlockValidationState state;
    RigorSupplyDeltaResult sdelta;
    out.supply_ok = RigorSupplyDelta(supply, in.params, sdelta, state);
    if (!out.supply_ok) out.supply_reject = state.GetRejectReason();

    LedgerKernelInput ledger = in.ledger;
    ledger.height = in.height;
    ledger.maturity = in.maturity;
    ledger.tx.accept = out.spend_ok;
    ledger.consensus.accept = true;
    ledger.consensus.oru_issuance = (in.height == 0) ? 0 : ledger.consensus.oru_issuance;
    ledger.consensus.res_mint = (in.height == 0) ? 0 : ledger.consensus.res_mint;
    ledger.consensus.res_schedule = out.res_schedule;
    out.ledger_ok = assets_ok && LedgerApply(ledger).accept;
    out.accept = out.supply_ok && out.ledger_ok && out.oru_mature && out.res_mature;
    if (in.height == 0 && draw.HasWinner()) out.accept = false;
    return out;
}

/** Path B — closed kernel boundary composition. */
inline KernelMigrationResult RunKernelBoundaryPath(const KernelMigrationInput& in)
{
    KernelMigrationResult out;
    const RigorIssuance iss = ComputeRigorIssuance(in.height, in.params);
    out.oru_subsidy = iss.oru_subsidy;
    out.res_schedule = iss.res_reward;
    out.res_issued = (in.height == 0) ? 0 : iss.res_reward;
    out.oru_mature = KernelMigrationMatureOru(in.spend_height, in.coin_height, in.maturity);
    out.res_mature = KernelMigrationMatureRes(in.spend_height, in.mint_lock);

    const ConvergenceDrawOutcome draw = ResolveConvergenceDraw(in.draw, in.draw_target);
    out.draw_winner = draw.winner_index;
    out.classification = KernelMigrationPrimaryAsset(in);
    out.spend_ok = in.ledger.consumed.empty() || VerifyRigorPredicateBoundary(in.tx);

    RigorSupplyDeltaInput supply = in.supply;
    supply.height = in.height;
    BlockValidationState state;
    RigorSupplyDeltaResult sdelta;
    out.supply_ok = RigorSupplyDelta(supply, in.params, sdelta, state);
    if (!out.supply_ok) out.supply_reject = state.GetRejectReason();

    UTXOStateTransitionInput u;
    u.params = in.params;
    u.height = in.height;
    u.maturity = in.maturity;
    u.tx = in.tx;
    u.ledger = in.ledger;
    u.consensus.draw = in.draw;
    u.consensus.draw_target = in.draw_target;
    u.consensus.height = in.height;
    u.fee = in.ledger.fee;
    u.burn = in.ledger.burn;
    u.converge = in.ledger.converge;
    for (const CScript& s : in.classify_scripts) {
        u.asset_scripts.emplace_back(s.begin(), s.end());
    }
    const UTXOStateTransitionResult applied = UTXOStateTransition(u);
    out.ledger_ok = applied.accept;
    out.accept = out.supply_ok && applied.accept && out.oru_mature && out.res_mature;
    return out;
}

inline bool KernelMigrationResultsEqual(const KernelMigrationResult& a, const KernelMigrationResult& b, const char** field)
{
    auto fail = [&](const char* f) {
        *field = f;
        return false;
    };
    if (a.oru_subsidy != b.oru_subsidy) return fail("issuance.oru");
    if (a.res_schedule != b.res_schedule) return fail("issuance.res_schedule");
    if (a.res_issued != b.res_issued) return fail("issuance.res_issued");
    if (a.oru_mature != b.oru_mature) return fail("maturity.oru");
    if (a.res_mature != b.res_mature) return fail("maturity.res");
    if (a.draw_winner != b.draw_winner) return fail("draw.winner");
    if (a.classification != b.classification) return fail("classification");
    if (a.spend_ok != b.spend_ok) return fail("spend");
    if (a.supply_ok != b.supply_ok) return fail("supply");
    if (a.supply_reject != b.supply_reject) return fail("supply.reject");
    if (a.ledger_ok != b.ledger_ok) return fail("ledger");
    return true;
}

/** Dual execution. FAIL HARD on mismatch. Never auto-correct. */
inline bool KernelMigrationVerify(const KernelMigrationInput& in, KernelMigrationResult& out)
{
    const KernelMigrationResult a = RunProductionAuthorityPath(in);
    const KernelMigrationResult b = RunKernelBoundaryPath(in);
    const char* field = nullptr;
    if (!KernelMigrationResultsEqual(a, b, &field)) {
        std::fprintf(stderr, "KERNEL_MIGRATION_MISMATCH field=%s\n", field ? field : "?");
        KM_LOG("migration verify mismatch (no auto-correct)\n");
        return false;
    }
    out = a;
    return true;
}

#endif
