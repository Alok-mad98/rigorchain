// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <wallet/psbt_domain.h>

#include <key_io.h>
#include <script/standard.h>

#include <map>

namespace wallet {

AssetType PsbtTransferAssetFromScript(const CScript& script_pub_key)
{
    return AssetTypeFromScriptPubKey(script_pub_key);
}

std::optional<AssetType> PsbtInputTransferAsset(const PartiallySignedTransaction& psbt, size_t input_index)
{
    CTxOut utxo;
    if (!psbt.GetInputUTXO(utxo, input_index)) {
        return std::nullopt;
    }
    return PsbtTransferAssetFromScript(utxo.scriptPubKey);
}

std::optional<AssetType> PsbtUnifiedInputDomain(const PartiallySignedTransaction& psbt)
{
    std::optional<AssetType> domain;
    for (size_t i = 0; i < psbt.tx->vin.size(); ++i) {
        const std::optional<AssetType> in_asset = PsbtInputTransferAsset(psbt, i);
        if (!in_asset.has_value()) {
            continue;
        }
        if (!domain.has_value()) {
            domain = *in_asset;
        } else if (*domain != *in_asset) {
            return std::nullopt;
        }
    }
    return domain;
}

bool PsbtHasMixedInputDomains(const PartiallySignedTransaction& psbt)
{
    std::set<AssetType> seen;
    for (size_t i = 0; i < psbt.tx->vin.size(); ++i) {
        const std::optional<AssetType> in_asset = PsbtInputTransferAsset(psbt, i);
        if (!in_asset.has_value()) {
            continue;
        }
        seen.insert(*in_asset);
        if (seen.size() > 1) {
            return true;
        }
    }
    return false;
}

std::optional<CAmount> PsbtOruNetworkFeeForDisplay(const PartiallySignedTransaction& psbt,
    const node::PSBTAnalysis& analysis)
{
    if (!analysis.fee.has_value()) {
        return std::nullopt;
    }
    if (PsbtHasMixedInputDomains(psbt)) {
        return std::nullopt;
    }
    for (size_t i = 0; i < psbt.tx->vin.size(); ++i) {
        if (!PsbtInputTransferAsset(psbt, i).has_value()) {
            return std::nullopt;
        }
    }
    return *analysis.fee;
}

std::string PsbtOutputDestinationSummary(const CScript& script_pub_key)
{
    CTxDestination dest;
    if (!ExtractDestination(script_pub_key, dest)) {
        return "non-address output";
    }
    if (std::holds_alternative<WitnessV0KeyHash>(dest) || std::holds_alternative<WitnessV0ScriptHash>(dest)) {
        return EncodeDestination(dest) + " (witness v0, ORU transfer domain)";
    }
    if (std::holds_alternative<WitnessV1Res>(dest)) {
        return EncodeDestination(dest) + " (witness v1, RES resource-lock container; no script-path spend)";
    }
    return EncodeDestination(dest);
}

PsbtDomainSummary BuildPsbtDomainSummary(const PartiallySignedTransaction& psbt,
    const node::PSBTAnalysis& analysis)
{
    PsbtDomainSummary summary;
    summary.fee_asset = NETWORK_FEE_ASSET;
    summary.fee_oru = PsbtOruNetworkFeeForDisplay(psbt, analysis);
    summary.mixed_input_domains = PsbtHasMixedInputDomains(psbt);
    summary.unified_input_domain = PsbtUnifiedInputDomain(psbt);

    std::set<AssetType> input_set;
    for (size_t i = 0; i < psbt.tx->vin.size(); ++i) {
        if (const std::optional<AssetType> a = PsbtInputTransferAsset(psbt, i)) {
            input_set.insert(*a);
        }
    }
    summary.input_domains.assign(input_set.begin(), input_set.end());

    std::map<AssetType, CAmount> out_totals;
    for (const CTxOut& out : psbt.tx->vout) {
        const AssetType a = PsbtTransferAssetFromScript(out.scriptPubKey);
        out_totals[a] += out.nValue;
    }
    for (const auto& [asset, amount] : out_totals) {
        summary.outputs_by_asset.emplace_back(asset, amount);
    }
    return summary;
}

} // namespace wallet
