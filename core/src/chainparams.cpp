// Copyright (c) 2010 Satoshi Nakamoto
// Copyright (c) 2009-2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <chainparams.h>

#if defined(HAVE_CONFIG_H)
#include <config/rigor-config.h>
#endif

#include <consensus/merkle.h>
#include <consensus/res_amount.h>
#include <deploymentinfo.h>
#include <script/predicate_verify.h>
#include <util/string.h>
#include <util/system.h>

#include <assert.h>

namespace {
/** FENCE-W11: CLI → Consensus::Params at chain construction. Not a ConnectBlock gArgs read. */
CAmount ResWalletBootstrapFromArgs(const ArgsManager& args)
{
    return args.GetBoolArg("-reswalletbootstrap", false) ? RES_COIN : 0;
}
} // namespace

void ReadRegTestArgs(const ArgsManager& args, CChainParams::RegTestOptions& options)
{
    if (auto value = args.GetBoolArg("-fastprune")) options.fastprune = *value;
    options.res_wallet_bootstrap_coinbase = ResWalletBootstrapFromArgs(args);

    // Buried softforks and BIP9 windows are fixed in chainparams. -testactivationheight
    // and -vbparams are not registered (see SetupChainParamsBaseOptions), so ParseParameters
    // rejects them on every chain before chainparams are built.
}

static std::unique_ptr<const CChainParams> globalChainParams;

const CChainParams &Params() {
    assert(globalChainParams);
    return *globalChainParams;
}

std::unique_ptr<const CChainParams> CreateChainParams(const ArgsManager& args, const std::string& chain)
{
    // Production builds still construct test/regtest params here so
    // SetupServerArgs can fill help-text defaults. Runtime selection is gated
    // in SelectParams / GetChainName.
    if (chain == CBaseChainParams::MAIN) {
        return CChainParams::Main();
    } else if (chain == CBaseChainParams::TESTNET) {
        return CChainParams::TestNet();
    } else if (chain == CBaseChainParams::DEVTEST) {
        CChainParams::MockableChainOptions opts;
        opts.res_wallet_bootstrap_coinbase = ResWalletBootstrapFromArgs(args);
        return CChainParams::DevTest(opts);
    } else if (chain == CBaseChainParams::SLOWTEST) {
        CChainParams::MockableChainOptions opts;
        opts.res_wallet_bootstrap_coinbase = ResWalletBootstrapFromArgs(args);
        return CChainParams::SlowTest(opts);
    } else if (chain == CBaseChainParams::REGTEST) {
        auto opts = CChainParams::RegTestOptions{};
        ReadRegTestArgs(args, opts);
        return CChainParams::RegTest(opts);
    }
    throw std::runtime_error(strprintf("%s: Unknown chain %s.", __func__, chain));
}

void SelectParams(const std::string& network)
{
#ifdef RIGOR_PRODUCTION_MAINNET
    if (network != CBaseChainParams::MAIN) {
        throw std::runtime_error("ERROR: RIGOR production artifacts are mainnet-only. Testnet/regtest/devtest are available only in development builds.");
    }
#endif
    SelectBaseParams(network);
    globalChainParams = CreateChainParams(gArgs, network);
}
