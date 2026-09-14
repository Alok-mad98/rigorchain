// Copyright (c) 2017-2019 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_CONSENSUS_TX_CHECK_H
#define BITCOIN_CONSENSUS_TX_CHECK_H

/**
 * Context-independent transaction checking code that can be called outside the
 * bitcoin server and doesn't depend on chain or mempool state. Transaction
 * verification code that does call server functions or depend on server state
 * belongs in tx_verify.h/cpp instead.
 */

#include <script/script.h>

class CTransaction;
class CTxOut;
class TxValidationState;

namespace Consensus {
/** Coinbase scriptSig bounds enforced by CheckTransaction as `bad-cb-length`. */
inline constexpr size_t COINBASE_SCRIPTSIG_MIN_SIZE = 2;
inline constexpr size_t COINBASE_SCRIPTSIG_MAX_SIZE = 100;

/**
 * The coinbase scriptSig for `height`: the BIP34 height push plus one padding byte.
 *
 * The padding is not decorative. `CScript() << height` is a single OP_N byte for
 * heights 1..16, which is below COINBASE_SCRIPTSIG_MIN_SIZE, so the height alone is
 * not a legal coinbase scriptSig during the first sixteen blocks of any chain — every
 * regtest fixture starts there.
 *
 * Before RFC-019 the height was the first of three pushes (height, convergence root,
 * paid winner) and the triple cleared the minimum on its own. RFC-019 issues RES in the
 * body against the epoch price, so the coinbase commits to nothing and the height stands
 * alone; this restores the minimum without reintroducing a commitment.
 *
 * ConnectBlock compares only the height prefix, so the trailing byte is consensus-neutral
 * and a miner may replace it with a longer extra-nonce field up to the maximum.
 */
CScript BuildCoinbaseScriptSig(int height);

/** Enforce allowed script forms via ClassifyRigorTxout (script-as-asset). */
[[nodiscard]] bool CheckTxOutAssetWitnessBinding(const CTxOut& out, TxValidationState& state);

/**
 * RIGOR consensus: only witness v0/20 (ORU), native witness v1/32 (RES),
 * RIGOR_MARKER_RES_MINT, and RIGOR_MARKER_CONVERGE are allowed. P2PKH, P2SH, P2WSH, CVG2, RES2,
 * and unknown scripts are rejected. No historical compatibility path.
 */
[[nodiscard]] bool IsAllowedScript(const CScript& scriptPubKey);
} // namespace Consensus

bool CheckTransaction(const CTransaction& tx, TxValidationState& state);

#endif // BITCOIN_CONSENSUS_TX_CHECK_H
