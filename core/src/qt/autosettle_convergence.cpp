// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <qt/autosettle_convergence.h>

#include <qt/clientmodel.h>
#include <qt/optionsmodel.h>
#include <qt/walletmodel.h>

#include <chainparams.h>
#include <interfaces/node.h>
#include <interfaces/wallet.h>
#include <key_io.h>
#include <logging.h>
#include <outputtype.h>
#include <util/result.h>
#include <util/settings.h>
#include <util/system.h>
#include <univalue.h>

#include <variant>

namespace {

bool IsWitnessV1ResAddress(const std::string& addr, const CChainParams& params)
{
    std::string err;
    const CTxDestination dest = DecodeDestination(addr, params, err, nullptr);
    return IsValidDestination(dest) && std::holds_alternative<WitnessV1Res>(dest);
}

} // namespace

bool EnsureConvergenceMintArmed(WalletModel* wallet_model)
{
    if (!wallet_model) return false;
    OptionsModel* options = wallet_model->getOptionsModel();
    if (!options || !options->getAutoSettleConvergenceMining()) {
        return false;
    }
    if (!wallet_model->wallet().canGetAddresses()) {
        LogPrint(BCLog::QT, "Auto-settle Convergence: wallet cannot get addresses yet\n");
        return false;
    }

    interfaces::Node& node = options->node();
    const CChainParams& params = Params();

    // Prefer an already-configured valid arming address from settings.json.
    const util::SettingsValue existing = node.getPersistentSetting("miningresfeeaddr");
    if (existing.isStr() && IsWitnessV1ResAddress(existing.get_str(), params)) {
        node.forceSetting("miningresfeeaddr", existing);
        LogPrint(BCLog::QT, "Auto-settle Convergence: armed from settings.json (%s)\n", existing.get_str());
        return true;
    }

    // Honor rigor.conf / CLI if already armed (do not replace with a new wallet address).
    if (gArgs.IsArgSet("-miningresfeeaddr")) {
        const std::string cur = gArgs.GetArg("-miningresfeeaddr", "");
        if (IsWitnessV1ResAddress(cur, params)) {
            const util::SettingsValue cur_val{cur};
            node.forceSetting("miningresfeeaddr", cur_val);
            if (!existing.isStr()) {
                node.updateRwSetting("miningresfeeaddr", cur_val);
            }
            LogPrint(BCLog::QT, "Auto-settle Convergence: armed from existing -miningresfeeaddr (%s)\n", cur);
            return true;
        }
    }

    auto dest = wallet_model->wallet().getNewDestination(OutputType::BECH32M, "auto-settle-convergence");
    if (!dest) {
        LogPrintf("Auto-settle Convergence: getNewDestination(BECH32M) failed: %s\n",
                  util::ErrorString(dest).original);
        return false;
    }
    if (!std::holds_alternative<WitnessV1Res>(*dest)) {
        LogPrintf("Auto-settle Convergence: destination is not witness-v1 RES\n");
        return false;
    }

    const std::string addr = EncodeDestination(*dest);
    const util::SettingsValue addr_val{addr};
    node.updateRwSetting("miningresfeeaddr", addr_val);
    node.forceSetting("miningresfeeaddr", addr_val);
    LogPrintf("Auto-settle Convergence: armed miningresfeeaddr=%s (wallet=%s); "
              "prize still pays Convergence winner; network fees remain ORU-only\n",
              addr, wallet_model->wallet().getWalletName());
    return true;
}
