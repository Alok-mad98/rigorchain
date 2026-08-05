// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_WALLET_FEE_DOMAIN_H
#define BITCOIN_WALLET_FEE_DOMAIN_H

#include <consensus/amount.h>
#include <consensus/res_amount.h>
#include <policy/feerate.h>
#include <primitives/transaction.h>
#include <univalue.h>
#include <wallet/wallet.h>

#include <set>

class CFeeRate;
class UniValue;

namespace wallet {
class CCoinControl;
class CWallet;

/** WALLET_SEMANTIC_FREEZE (P1): network fees are always ORU (witness v0, 8 decimals). */
constexpr AssetType NETWORK_FEE_ASSET = ASSET_ORU;

/**
 * ORU-only fee freeze: RES fee scaling forbidden.
 * @deprecated Do not use for feerate math; retained only as a named constant for migration notes.
 */
constexpr CAmount DEPRECATED_RES_FEE_UNIT_SCALE = COIN / RES_COIN;

/** Fee in ORU base units for a serialized transaction size at an ORU sat/vB feerate. */
CAmount OruNetworkFeeForVsize(const CFeeRate& feerate, int vbytes);

/** True when the wallet can fund the ORU fee pool (spendable ORU UTXOs exist). */
bool WalletHasSpendableOruForFee(const CWallet& wallet, const CCoinControl* coin_control);

/**
 * Wallet-core guard: RES transfers must not treat tx transfer asset as fee asset.
 * Returns a user-facing error if violated; empty optional if OK.
 */
std::optional<bilingual_str> CheckResTransferFeeSemantics(const std::vector<CRecipient>& vecSend);

/** Debug/wallet validation after tx assembly (dual-pool RES sends). */
void AssertWalletTxNetworkFeeOruOnly(const CTransaction& tx, const CWallet& wallet, CAmount reported_fee_oru);

/** ORU network fee paid by this wallet for a mapWallet entry (0 if not from us). */
CAmount WalletTxOruNetworkFee(const CWallet& wallet, const CWalletTx& wtx, isminefilter filter)
    EXCLUSIVE_LOCKS_REQUIRED(wallet.cs_wallet);

/** fundrawtransaction / walletcreatefundedpsbt: forbid subtracting ORU fee from RES outputs. */
std::optional<bilingual_str> CheckSubtractFeeFromResOutputs(const CMutableTransaction& tx,
    const std::set<int>& subtract_vout_indices);

/** RPC result: explicit ORU fee fields (`fee`, `fee_oru`, `fee_asset`). */
void PushRpcOruNetworkFeeFields(UniValue& obj, const std::string& amount_key, CAmount fee_oru);

/** Map wallet spend errors to canonical ORU-fee-domain messages where applicable. */
bilingual_str NormalizeWalletSpendError(const bilingual_str& err);

} // namespace wallet

#endif // BITCOIN_WALLET_FEE_DOMAIN_H
