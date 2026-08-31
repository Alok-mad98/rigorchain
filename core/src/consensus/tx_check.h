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
