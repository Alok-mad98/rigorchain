// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_WALLET_PSBT_DOMAIN_H
#define BITCOIN_WALLET_PSBT_DOMAIN_H

#include <consensus/amount.h>
#include <node/psbt.h>
#include <primitives/transaction.h>
#include <psbt.h>
#include <wallet/fee_domain.h>

#include <optional>
#include <set>
#include <string>
#include <vector>

namespace wallet {

/** Transfer asset for a PSBT output (script-derived; not display unit). */
AssetType PsbtTransferAssetFromScript(const CScript& script_pub_key);

/** Transfer asset for PSBT input `i` when UTXO data is present in the PSBT. */
std::optional<AssetType> PsbtInputTransferAsset(const PartiallySignedTransaction& psbt, size_t input_index);

/**
 * Input asset domain for fee/semantic authority (RFC-003 / Consensus::GetTransactionInputAsset).
 * Returns empty when inputs disagree or UTXO data is missing.
 */
std::optional<AssetType> PsbtUnifiedInputDomain(const PartiallySignedTransaction& psbt);

/** True when known input UTXOs span more than one transfer asset domain. */
bool PsbtHasMixedInputDomains(const PartiallySignedTransaction& psbt);

/**
 * ORU network fee for PSBT presentation (POOL-F).
 * Never applies RES_FEE_UNIT_SCALE. Returns nullopt when fee cannot be shown safely
 * (missing UTXOs or mixed input value domains).
 */
std::optional<CAmount> PsbtOruNetworkFeeForDisplay(const PartiallySignedTransaction& psbt,
    const node::PSBTAnalysis& analysis);

/** Human-readable address/lock summary for external-signer UI (V0=ORU, V1=RES container). */
std::string PsbtOutputDestinationSummary(const CScript& script_pub_key);

struct PsbtDomainSummary {
    std::vector<AssetType> input_domains;
    std::optional<AssetType> unified_input_domain;
    bool mixed_input_domains{false};
    AssetType fee_asset{NETWORK_FEE_ASSET};
    std::optional<CAmount> fee_oru;
    std::vector<std::pair<AssetType, CAmount>> outputs_by_asset;
};

PsbtDomainSummary BuildPsbtDomainSummary(const PartiallySignedTransaction& psbt,
    const node::PSBTAnalysis& analysis);

} // namespace wallet

#endif // BITCOIN_WALLET_PSBT_DOMAIN_H
