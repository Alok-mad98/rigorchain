// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2021 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <chainparamsbase.h>

#if defined(HAVE_CONFIG_H)
#include <config/rigor-config.h>
#endif

#include <tinyformat.h>
#include <util/system.h>

#include <assert.h>

const std::string CBaseChainParams::MAIN = "main";
const std::string CBaseChainParams::TESTNET = "test";
const std::string CBaseChainParams::SIGNET = "signet";
const std::string CBaseChainParams::REGTEST = "regtest";
const std::string CBaseChainParams::DEVTEST = "devtest";
const std::string CBaseChainParams::SLOWTEST = "slowtest";

void SetupChainParamsBaseOptions(ArgsManager& argsman)
{
#ifndef BOOST_TEST_MODULE
#ifdef RIGOR_PRODUCTION_MAINNET
    argsman.AddArg("-chain=<chain>", "Use the chain <chain> (default: main). AlphaPlus production artifacts accept only main.", ArgsManager::ALLOW_ANY, OptionsCategory::CHAINPARAMS);
#else
    argsman.AddArg("-chain=<chain>", "Use the chain <chain> (default: main). Allowed values: main, test, regtest, devtest, slowtest", ArgsManager::ALLOW_ANY, OptionsCategory::CHAINPARAMS);
    argsman.AddArg("-regtest", "Enter regression test mode, which uses a special chain in which blocks can be solved instantly. "
                 "This is intended for regression testing tools and app development. Equivalent to -chain=regtest.", ArgsManager::ALLOW_ANY | ArgsManager::DEBUG_ONLY, OptionsCategory::CHAINPARAMS);
    // -testactivationheight is deliberately not registered: buried softforks are genesis-active
    // on every RIGOR network, so an unregistered argument makes ParseParameters reject it at
    // startup ("Invalid parameter -testactivationheight") on all chains, not just regtest.
    argsman.AddArg("-testnet", "Use the test chain. Equivalent to -chain=test.", ArgsManager::ALLOW_ANY, OptionsCategory::CHAINPARAMS);
    argsman.AddArg("-devtest", "Use the devtest chain for fast local PoW/economics validation. Equivalent to -chain=devtest.", ArgsManager::ALLOW_ANY | ArgsManager::DEBUG_ONLY, OptionsCategory::CHAINPARAMS);
    // -vbparams is deliberately not registered: BIP9 windows are fixed in chainparams.
    // An unregistered argument makes ParseParameters reject it ("Invalid parameter -vbparams").
#endif // RIGOR_PRODUCTION_MAINNET
#endif // BOOST_TEST_MODULE
}

static std::unique_ptr<CBaseChainParams> globalChainBaseParams;

const CBaseChainParams& BaseParams()
{
    assert(globalChainBaseParams);
    return *globalChainBaseParams;
}

/**
 * Port numbers for incoming Tor connections (8334, 18334, 38334, 18445) have
 * been chosen arbitrarily to keep ranges of used ports tight.
 */
std::unique_ptr<CBaseChainParams> CreateBaseChainParams(const std::string& chain)
{
    // Do not throw here for non-main chains: SetupServerArgs needs testnet /
    // regtest port defaults for help text even in production builds.
    // Runtime network selection is gated in SelectBaseParams / GetChainName.
    if (chain == CBaseChainParams::MAIN) {
        return std::make_unique<CBaseChainParams>("", 8761, 8334);
    } else if (chain == CBaseChainParams::TESTNET) {
        return std::make_unique<CBaseChainParams>("testnet3", 18761, 18334);
    } else if (chain == CBaseChainParams::REGTEST) {
        return std::make_unique<CBaseChainParams>("regtest", 28761, 18445);
    } else if (chain == CBaseChainParams::DEVTEST) {
        return std::make_unique<CBaseChainParams>("devtest", 48761, 48760);
    } else if (chain == CBaseChainParams::SLOWTEST) {
        return std::make_unique<CBaseChainParams>("slowtest", 49761, 49760);
    }
    throw std::runtime_error(strprintf("%s: Unknown chain %s.", __func__, chain));
}

void SelectBaseParams(const std::string& chain)
{
#ifdef RIGOR_PRODUCTION_MAINNET
    if (chain != CBaseChainParams::MAIN) {
        throw std::runtime_error("ERROR: AlphaPlus production artifacts are mainnet-only. Testnet/regtest/devtest are available only in development builds.");
    }
#endif
    globalChainBaseParams = CreateBaseChainParams(chain);
    gArgs.SelectConfigNetwork(chain);
}
