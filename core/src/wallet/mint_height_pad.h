// Copyright (c) The RigorChain / RingCore developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_WALLET_MINT_HEIGHT_PAD_H
#define BITCOIN_WALLET_MINT_HEIGHT_PAD_H

#include <algorithm>
#include <limits>

namespace wallet {

/**
 * Wallet-only confirmation-height margin for RES convergence construction.
 *
 * RFC-020 destroy_cost(h) is strictly increasing. Consensus still prices a
 * mint at the actual confirming block height. An exact-fit mint at tip+1 is
 * consensus-invalid in the following block and is evicted from every mempool
 * (`removeForReorg`). The wallet therefore prices R at
 * current_height + CONVERGENCE_HEIGHT_PAD so the transaction stays valid
 * across mempool delay. This constant is not a consensus parameter.
 *
 * RFC-020 §5 already allows the wallet to construct at confirm height plus
 * a margin. 1024 blocks is enough lifetime on both testnet and mainnet.
 */
static constexpr int CONVERGENCE_HEIGHT_PAD = 1024;
static constexpr int DEFAULT_CONVERGENCE_HEIGHT_PAD = CONVERGENCE_HEIGHT_PAD;

[[nodiscard]] inline int ConvergenceTargetHeight(int current_height, int height_pad = CONVERGENCE_HEIGHT_PAD)
{
    if (current_height < 0) return current_height;
    const int pad = std::max(0, height_pad);
    if (pad == 0) return current_height;
    if (current_height > std::numeric_limits<int>::max() - pad) {
        return std::numeric_limits<int>::max();
    }
    return current_height + pad;
}

} // namespace wallet

#endif // BITCOIN_WALLET_MINT_HEIGHT_PAD_H
