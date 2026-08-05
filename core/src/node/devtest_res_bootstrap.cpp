// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <node/devtest_res_bootstrap.h>

#include <chainparams.h>
#include <consensus/res_amount.h>
#include <util/system.h>

CAmount DevTestResWalletBootstrap(const CChainParams& chainparams)
{
    if (!chainparams.IsMockableChain()) {
        return 0;
    }
    if (!gArgs.GetBoolArg("-reswalletbootstrap", false)) {
        return 0;
    }
    return RES_COIN;
}
