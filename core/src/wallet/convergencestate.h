// Copyright (c) The RigorChain / RingCore developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_WALLET_CONVERGENCESTATE_H
#define BITCOIN_WALLET_CONVERGENCESTATE_H

#include <consensus/amount.h>
#include <uint256.h>

#include <cstdint>
#include <map>
#include <optional>
#include <string>

class CWalletTx;

namespace wallet {

/*
CONVERGENCE_STATE_MACHINE_V2_LOCK

- Convergence can happen in any block
- RES is probabilistic (not per-block guaranteed)
- At most one RES per block
- RES requires 100 block maturity

Wallet must reflect:

- competition phase
- locked RES phase
- spendable RES phase

No shortcuts, no assumptions.
*/

/*
RES RULES LOCK

- Convergence can happen in any block
- RES is NOT guaranteed per block
- At most ONE RES per block
- RES requires 100 blocks maturity

WALLET MUST:

- NOT assume RES generation
- NOT assume winning
- NOT mark RES as spendable before maturity

UI MUST:

- distinguish WON vs AVAILABLE
- show locked RES
*/

/**
 * Legacy competition-confirmations counter for older status strings.
 * RES outcome is NOT known at inclusion: the candidate competes across
 * CONVERGENCE_DRAW_WINDOW (=32) subsequent draws (A-9C).
 */
static constexpr int CONVERGENCE_COMPETITION_CONFIRMATIONS = 1;

enum class ConvergenceState : uint8_t {
    CREATED = 0,
    BROADCASTED = 1,
    INCLUDED = 2,
    MATURING = 3,
    WON_PENDING = 4,     //!< won but RES locked
    WON_CONFIRMED = 5,   //!< RES matured and spendable
    LOST = 6,
};

struct ConvergenceRecord {
    ConvergenceState state{ConvergenceState::CREATED};
    int included_height{-1};       //!< block height where convergence tx was mined (-1 if unknown)
    int maturity_height{-1};     //!< chain height at which RES becomes spendable (included_height + maturity blocks)

    bool IsEmpty() const { return state == ConvergenceState::CREATED && included_height < 0 && maturity_height < 0; }
};

std::string ConvergenceStateToString(ConvergenceState s);
std::optional<ConvergenceState> ParseConvergenceStateString(const std::string& s);

/** Load convergence metadata from CWalletTx::mapValue (keys convergence_v2, convergence_state, ...). */
bool LoadConvergenceRecordFromMapValue(const CWalletTx& wtx, ConvergenceRecord& out);

/** Persist convergence metadata into mapValue (caller must Commit). */
void ApplyConvergenceRecordToMapValue(ConvergenceRecord rec, std::map<std::string, std::string>& map_value_out); //!< same key type as mapValue_t

/** UI: human-readable status line for convergence record at current chain height. */
std::string ConvergenceRecordUiStatus(const ConvergenceRecord& rec, int current_height);

/** UI: "confirmations / N" for competition phase. */
std::string ConvergenceCompetitionProgressText(const ConvergenceRecord& rec, int tx_depth);

/** UI: "current_height / maturity_height" for maturity phase. */
std::string ConvergenceMaturityProgressText(const ConvergenceRecord& rec, int current_height);

/** UI: "Matures in X blocks" when WON_PENDING, else empty. */
std::string ConvergenceMaturityCountdownText(const ConvergenceRecord& rec, int current_height);

} // namespace wallet

#endif // BITCOIN_WALLET_CONVERGENCESTATE_H
