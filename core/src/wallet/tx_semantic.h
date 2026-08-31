// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_WALLET_TX_SEMANTIC_H
#define BITCOIN_WALLET_TX_SEMANTIC_H

#include <wallet/transaction.h>
#include <wallet/wallet.h>

#include <string>

class CCoinsViewCache;

namespace wallet {

/**
 * RFC-003 canonical transaction semantic (FREEZE-RFC-STATE-01).
 * Derived ONLY from input asset domain, StateTag rules, and coinbase status — never witness layout alone.
 */
enum class TxSemanticKind : uint8_t {
    Unknown = 0,
    OruCoinbase,
    OruTransfer,
    OruConvergence,
    ResTransfer,
    ResMint,
    MixedInputDomain,
};

/** Classify a wallet transaction when all input prevouts are available in the wallet. */
TxSemanticKind ClassifyWalletTxSemantic(const CWallet& wallet, const CWalletTx& wtx)
    EXCLUSIVE_LOCKS_REQUIRED(wallet.cs_wallet);

/** Classify from an input UTXO view (consensus StateTag helpers). Returns Unknown when prevouts are missing. */
TxSemanticKind ClassifyTxSemantic(const CTransaction& tx, const CCoinsViewCache& inputs);

/**
 * Fallback when wallet lacks input prevouts (typical incoming payment).
 * RES-bearing outputs → ResTransfer (never ResMint: mint needs input view for maturity).
 */
TxSemanticKind ClassifyTxSemanticFromOutputs(const CTransaction& tx);

/** Canonical English presentation label (Qt/RPC). */
std::string TxSemanticCanonicalLabel(TxSemanticKind kind);

/** Stable machine identifier for RPC (`semantic` field). */
std::string TxSemanticRpcId(TxSemanticKind kind);

} // namespace wallet

#endif // BITCOIN_WALLET_TX_SEMANTIC_H
