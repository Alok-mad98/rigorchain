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
/** Enforce ORU ↔ witness v0 / legacy, RES ↔ witness v1 (32-byte program); reject unknown witness versions. */
[[nodiscard]] bool CheckTxOutAssetWitnessBinding(const CTxOut& out, TxValidationState& state);

/**
 * RIGOR consensus: Only witness v0 (ORU) and witness v1 with 32-byte program (RES) are allowed.
 * All other script types (P2PKH, P2SH, bare multisig, Taproot with control block, etc.) are rejected.
 * This is enforced at consensus level - parsing layer remains compatible for historical data.
 */
[[nodiscard]] bool IsAllowedScript(const CScript& scriptPubKey);
} // namespace Consensus

bool CheckTransaction(const CTransaction& tx, TxValidationState& state);

#endif // BITCOIN_CONSENSUS_TX_CHECK_H
