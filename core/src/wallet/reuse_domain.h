// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_WALLET_REUSE_DOMAIN_H
#define BITCOIN_WALLET_REUSE_DOMAIN_H

#include <primitives/transaction.h>

class CScript;

namespace wallet {
class CWallet;

/** Bitcoin-style address reuse tracking applies only to ORU (witness v0) outputs. */
bool OruReuseSemanticsApply(const CScript& script_pub_key);
bool OruReuseSemanticsApply(AssetType asset);

/** avoid_reuse applies to ORU balances/coin selection only; RES is always fully visible. */
bool EffectiveAvoidReuseForAsset(bool avoid_reuse, AssetType asset);

/** True when an output should be hidden from spendable balance due to ORU reuse heuristics. */
bool OutputBlockedByOruReuse(const CWallet& wallet, const CScript& script_pub_key, bool allow_used_addresses);

} // namespace wallet

#endif // BITCOIN_WALLET_REUSE_DOMAIN_H
