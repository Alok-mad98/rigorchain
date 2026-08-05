// Copyright (c) The RigorChain / RingCore developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <wallet/convergencestate.h>

#include <wallet/transaction.h>

#include <tinyformat.h>
#include <util/strencodings.h>

#include <algorithm>

namespace wallet {

static const char* KEY_CONVERGENCE_V2 = "convergence_v2";
static const char* KEY_CONVERGENCE_STATE = "convergence_state";
static const char* KEY_CONVERGENCE_INCLUDED_H = "convergence_included_height";
static const char* KEY_CONVERGENCE_MATURITY_H = "convergence_maturity_height";

std::string ConvergenceStateToString(ConvergenceState s)
{
    switch (s) {
    case ConvergenceState::CREATED: return "CREATED";
    case ConvergenceState::BROADCASTED: return "BROADCASTED";
    case ConvergenceState::INCLUDED: return "INCLUDED";
    case ConvergenceState::MATURING: return "MATURING";
    case ConvergenceState::WON_PENDING: return "WON_PENDING";
    case ConvergenceState::WON_CONFIRMED: return "WON_CONFIRMED";
    case ConvergenceState::LOST: return "LOST";
    }
    return "UNKNOWN";
}

std::optional<ConvergenceState> ParseConvergenceStateString(const std::string& s)
{
    if (s == "CREATED") return ConvergenceState::CREATED;
    if (s == "BROADCASTED") return ConvergenceState::BROADCASTED;
    if (s == "INCLUDED") return ConvergenceState::INCLUDED;
    if (s == "MATURING") return ConvergenceState::MATURING;
    if (s == "WON_PENDING") return ConvergenceState::WON_PENDING;
    if (s == "WON_CONFIRMED") return ConvergenceState::WON_CONFIRMED;
    if (s == "LOST") return ConvergenceState::LOST;
    return std::nullopt;
}

bool LoadConvergenceRecordFromMapValue(const CWalletTx& wtx, ConvergenceRecord& out)
{
    const auto& m = wtx.mapValue;
    const auto it_v2 = m.find(KEY_CONVERGENCE_V2);
    if (it_v2 == m.end() || it_v2->second != "1") {
        return false;
    }
    out = ConvergenceRecord{};
    const auto it_st = m.find(KEY_CONVERGENCE_STATE);
    if (it_st != m.end()) {
        if (auto op = ParseConvergenceStateString(it_st->second)) {
            out.state = *op;
        }
    }
    const auto it_inc = m.find(KEY_CONVERGENCE_INCLUDED_H);
    if (it_inc != m.end()) {
        out.included_height = LocaleIndependentAtoi<int>(it_inc->second);
    }
    const auto it_mat = m.find(KEY_CONVERGENCE_MATURITY_H);
    if (it_mat != m.end()) {
        out.maturity_height = LocaleIndependentAtoi<int>(it_mat->second);
    }
    return true;
}

void ApplyConvergenceRecordToMapValue(ConvergenceRecord rec, std::map<std::string, std::string>& map_value_out)
{
    map_value_out[KEY_CONVERGENCE_V2] = "1";
    map_value_out[KEY_CONVERGENCE_STATE] = ConvergenceStateToString(rec.state);
    if (rec.included_height >= 0) {
        map_value_out[KEY_CONVERGENCE_INCLUDED_H] = strprintf("%d", rec.included_height);
    } else {
        map_value_out.erase(KEY_CONVERGENCE_INCLUDED_H);
    }
    if (rec.maturity_height >= 0) {
        map_value_out[KEY_CONVERGENCE_MATURITY_H] = strprintf("%d", rec.maturity_height);
    } else {
        map_value_out.erase(KEY_CONVERGENCE_MATURITY_H);
    }
}

std::string ConvergenceRecordUiStatus(const ConvergenceRecord& rec, int current_height)
{
    (void)current_height;
    switch (rec.state) {
    case ConvergenceState::WON_PENDING:
        return "Won RES (Locked)";
    case ConvergenceState::WON_CONFIRMED:
        return "RES Available";
    case ConvergenceState::LOST:
        return "Not Selected";
    case ConvergenceState::CREATED:
        return "Created";
    case ConvergenceState::BROADCASTED:
        return "Broadcast";
    case ConvergenceState::INCLUDED:
        return "Included";
    case ConvergenceState::MATURING:
        return "Maturing";
    }
    return "";
}

std::string ConvergenceCompetitionProgressText(const ConvergenceRecord& rec, int tx_depth)
{
    const int conf = std::max(0, tx_depth);
    return strprintf("%d / %d", conf, CONVERGENCE_COMPETITION_CONFIRMATIONS);
}

std::string ConvergenceMaturityProgressText(const ConvergenceRecord& rec, int current_height)
{
    if (rec.maturity_height < 0) {
        return "— / —";
    }
    return strprintf("%d / %d", current_height, rec.maturity_height);
}

std::string ConvergenceMaturityCountdownText(const ConvergenceRecord& rec, int current_height)
{
    if (rec.state != ConvergenceState::WON_PENDING || rec.maturity_height < 0) {
        return {};
    }
    const int left = std::max(0, rec.maturity_height - current_height);
    return strprintf("Matures in %d blocks", left);
}

} // namespace wallet
