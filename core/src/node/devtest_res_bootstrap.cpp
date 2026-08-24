// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <node/devtest_res_bootstrap.h>

#include <chainparams.h>

CAmount DevTestResWalletBootstrap(const CChainParams& chainparams)
{
    // FENCE-W11: amount is Consensus::Params::res_wallet_bootstrap_coinbase,
    // injected at CreateChainParams / CChainParams::RegTest. Do not read gArgs.
    if (!chainparams.IsMockableChain()) {
        return 0;
    }
    return chainparams.GetConsensus().res_wallet_bootstrap_coinbase;
}
