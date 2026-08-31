// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <connect_block_rigor_checks.h>

#include <consensus/params.h>
#include <consensus/rigor_supply.h>
#include <consensus/validation.h>
#include <kernel/chainparams.h>
#include <logging.h>

namespace ConnectBlockRigor {

bool VerifyCoinbaseConvergenceAndResLayout(BlockValidationState& state,
    const CChainParams& params,
    const CAmount nFeesRes,
    const CAmount res_coinbase_value,
    const CAmount res_mint_coinbase)
{
    const CAmount res_bootstrap_coinbase =
        params.IsMockableChain() ? params.GetConsensus().res_wallet_bootstrap_coinbase : 0;
    const CAmount expected_res_coinbase =
        RigorResCoinbaseExpected(nFeesRes, res_bootstrap_coinbase, res_mint_coinbase);
    if (res_coinbase_value != expected_res_coinbase) {
        LogPrintf("ERROR: ConnectBlock(): RES coinbase mismatch (actual=%d expected=%d nFeesRes=%d bootstrap=%d mint=%d)\n",
            res_coinbase_value, expected_res_coinbase, nFeesRes, res_bootstrap_coinbase, res_mint_coinbase);
        return state.Invalid(BlockValidationResult::BLOCK_CONSENSUS, "bad-cb-res-amount");
    }

    return true;
}

} // namespace ConnectBlockRigor
