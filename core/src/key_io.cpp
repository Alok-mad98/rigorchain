// Copyright (c) 2014-2021 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <key_io.h>

#include <base58.h>
#include <bech32.h>
#include <util/strencodings.h>

#include <algorithm>
#include <assert.h>
#include <string.h>

namespace {
class DestinationEncoder
{
private:
    const CChainParams& m_params;

public:
    explicit DestinationEncoder(const CChainParams& params) : m_params(params) {}

    std::string operator()(const PKHash&) const
    {
        return {};
    }

    std::string operator()(const ScriptHash&) const
    {
        return {};
    }

    std::string operator()(const WitnessV0KeyHash& id) const
    {
        std::vector<unsigned char> data = {0};
        data.reserve(33);
        ConvertBits<8, 5, true>([&](unsigned char c) { data.push_back(c); }, id.begin(), id.end());
        return bech32::Encode(bech32::Encoding::BECH32, m_params.Bech32HRP(), data);
    }

    std::string operator()(const WitnessV0ScriptHash& id) const
    {
        std::vector<unsigned char> data = {0};
        data.reserve(53);
        ConvertBits<8, 5, true>([&](unsigned char c) { data.push_back(c); }, id.begin(), id.end());
        return bech32::Encode(bech32::Encoding::BECH32, m_params.Bech32HRP(), data);
    }

    std::string operator()(const WitnessV1Res& tap) const
    {
        std::vector<unsigned char> data = {1};
        data.reserve(53);
        ConvertBits<8, 5, true>([&](unsigned char c) { data.push_back(c); }, tap.begin(), tap.end());
        return bech32::Encode(bech32::Encoding::BECH32M, m_params.Bech32HRP(), data);
    }

    std::string operator()(const WitnessUnknown&) const
    {
        return {};
    }

    std::string operator()(const CNoDestination& no) const { return {}; }
};
} // namespace

CTxDestination DecodeDestination(const std::string& str, const CChainParams& params, std::string& error_str, std::vector<int>* error_locations)
{
    std::vector<unsigned char> data;
    error_str = "";

    // Note this will be false if it is a valid Bech32 address for a different network
    bool is_bech32 = (ToLower(str.substr(0, params.Bech32HRP().size())) == params.Bech32HRP());

    if (!is_bech32) {
        error_str = "Only witness addresses are supported on this chain (v0 Bech32 or v1 Bech32m).";
        return CNoDestination();
    }

    data.clear();
    const auto dec = bech32::Decode(str);
    if ((dec.encoding == bech32::Encoding::BECH32 || dec.encoding == bech32::Encoding::BECH32M) && dec.data.size() > 0) {
        // Bech32 decoding
        if (dec.hrp != params.Bech32HRP()) {
            error_str = strprintf("Invalid or unsupported prefix for Segwit (Bech32) address (expected %s, got %s).", params.Bech32HRP(), dec.hrp);
            return CNoDestination();
        }
        int version = dec.data[0]; // The first 5 bit symbol is the witness version (0-16)
        // W1.1D FREEZE INVARIANT (INV-P-1): Witness version <-> Bech32/Bech32m checksum
        // is the first line of defense against witness-version spoofing. V0 (ORU) MUST
        // use Bech32; V1+ (RES) MUST use Bech32m. Any future upstream merge that relaxes
        // this binding MUST be rejected at PR review.
        if (version == 0 && dec.encoding != bech32::Encoding::BECH32) {
            error_str = "Version 0 witness address must use Bech32 checksum";
            return CNoDestination();
        }
        if (version != 0 && dec.encoding != bech32::Encoding::BECH32M) {
            error_str = "Version 1+ witness address must use Bech32m checksum";
            return CNoDestination();
        }
        // The rest of the symbols are converted witness program bytes.
        data.reserve(((dec.data.size() - 1) * 5) / 8);
        if (ConvertBits<5, 8, false>([&](unsigned char c) { data.push_back(c); }, dec.data.begin() + 1, dec.data.end())) {
            if (version == 0) {
                {
                    WitnessV0KeyHash keyid;
                    if (data.size() == keyid.size()) {
                        std::copy(data.begin(), data.end(), keyid.begin());
                        return keyid;
                    }
                }
                {
                    WitnessV0ScriptHash scriptid;
                    if (data.size() == scriptid.size()) {
                        std::copy(data.begin(), data.end(), scriptid.begin());
                        return scriptid;
                    }
                }

                error_str = "Invalid Bech32 v0 address data size";
                return CNoDestination();
            }

            if (version == 1 && data.size() == WITNESS_V1_RES_SIZE) {
                static_assert(WITNESS_V1_RES_SIZE == WitnessV1Res::size());
                WitnessV1Res tap;
                std::copy(data.begin(), data.end(), tap.begin());
                return tap;
            }

            if (version == 1 && data.size() == WITNESS_V1_RES2_CLAIM_PROGRAM_SIZE) {
                WitnessUnknown unk;
                unk.version = 1;
                unk.length = static_cast<unsigned int>(data.size());
                std::copy(data.begin(), data.end(), unk.program);
                return unk;
            }

            if (version != 1) {
                error_str = "Unsupported witness version; only v0 and v1 are allowed";
                return CNoDestination();
            }
            error_str = "Invalid Bech32m v1 address data size";
            return CNoDestination();
        }
    }

    // Perform Bech32 error location
    auto res = bech32::LocateErrors(str);
    error_str = res.first;
    if (error_locations) *error_locations = std::move(res.second);
    return CNoDestination();
}

CKey DecodeSecret(const std::string& str)
{
    CKey key;
    std::vector<unsigned char> data;
    if (DecodeBase58Check(str, data, 34)) {
        const std::vector<unsigned char>& privkey_prefix = Params().Base58Prefix(CChainParams::SECRET_KEY);
        if ((data.size() == 32 + privkey_prefix.size() || (data.size() == 33 + privkey_prefix.size() && data.back() == 1)) &&
            std::equal(privkey_prefix.begin(), privkey_prefix.end(), data.begin())) {
            // ED25519 uses a fixed 32-byte private key; compression marker is ignored for legacy WIF compatibility.
            key.Set(data.begin() + privkey_prefix.size(), data.begin() + privkey_prefix.size() + 32, true);
        }
    }
    if (!data.empty()) {
        memory_cleanse(data.data(), data.size());
    }
    return key;
}

std::string EncodeSecret(const CKey& key)
{
    assert(key.IsValid());
    std::vector<unsigned char> data = Params().Base58Prefix(CChainParams::SECRET_KEY);
    data.insert(data.end(), key.begin(), key.end());
    // Keep WIF payload fixed to prefix + 32-byte ED25519 secret.
    std::string ret = EncodeBase58Check(data);
    memory_cleanse(data.data(), data.size());
    return ret;
}

CExtPubKey DecodeExtPubKey(const std::string& str)
{
    CExtPubKey key;
    std::vector<unsigned char> data;
    if (DecodeBase58Check(str, data, 78)) {
        const std::vector<unsigned char>& prefix = Params().Base58Prefix(CChainParams::EXT_PUBLIC_KEY);
        if (data.size() == BIP32_EXTKEY_SIZE + prefix.size() && std::equal(prefix.begin(), prefix.end(), data.begin())) {
            key.Decode(data.data() + prefix.size());
        }
    }
    return key;
}

std::string EncodeExtPubKey(const CExtPubKey& key)
{
    std::vector<unsigned char> data = Params().Base58Prefix(CChainParams::EXT_PUBLIC_KEY);
    size_t size = data.size();
    data.resize(size + BIP32_EXTKEY_SIZE);
    key.Encode(data.data() + size);
    std::string ret = EncodeBase58Check(data);
    return ret;
}

CExtKey DecodeExtKey(const std::string& str)
{
    CExtKey key;
    std::vector<unsigned char> data;
    if (DecodeBase58Check(str, data, 78)) {
        const std::vector<unsigned char>& prefix = Params().Base58Prefix(CChainParams::EXT_SECRET_KEY);
        if (data.size() == BIP32_EXTKEY_SIZE + prefix.size() && std::equal(prefix.begin(), prefix.end(), data.begin())) {
            key.Decode(data.data() + prefix.size());
        }
    }
    if (!data.empty()) {
        memory_cleanse(data.data(), data.size());
    }
    return key;
}

std::string EncodeExtKey(const CExtKey& key)
{
    std::vector<unsigned char> data = Params().Base58Prefix(CChainParams::EXT_SECRET_KEY);
    size_t size = data.size();
    data.resize(size + BIP32_EXTKEY_SIZE);
    key.Encode(data.data() + size);
    std::string ret = EncodeBase58Check(data);
    memory_cleanse(data.data(), data.size());
    return ret;
}

std::string EncodeDestination(const CTxDestination& dest)
{
    return std::visit(DestinationEncoder(Params()), dest);
}

CTxDestination DecodeDestination(const std::string& str, std::string& error_msg, std::vector<int>* error_locations)
{
    return DecodeDestination(str, Params(), error_msg, error_locations);
}

CTxDestination DecodeDestination(const std::string& str)
{
    std::string error_msg;
    return DecodeDestination(str, error_msg);
}

bool IsValidDestinationString(const std::string& str, const CChainParams& params)
{
    std::string error_msg;
    return IsValidDestination(DecodeDestination(str, params, error_msg, nullptr));
}

bool IsValidDestinationString(const std::string& str)
{
    return IsValidDestinationString(str, Params());
}

/**
 * RFC-WALLET-002: RG1P/RG1Q address distinction for asset type detection.
 * RG1P (Witness V0) → ORU asset
 * RG1Q (Witness V1) → RES asset
 */

std::optional<AssetType> GetAssetTypeFromDestination(const CTxDestination& dest)
{
    if (std::holds_alternative<WitnessV0KeyHash>(dest)) {
        return ASSET_ORU;
    }
    if (std::holds_alternative<WitnessV1Res>(dest)) {
        return ASSET_RES;
    }
    return std::nullopt;
}

std::optional<AssetType> GetAssetTypeFromAddress(const std::string& str, const CChainParams& params)
{
    std::string error_msg;
    CTxDestination dest = DecodeDestination(str, params, error_msg, nullptr);
    if (!IsValidDestination(dest)) {
        return std::nullopt;
    }
    return GetAssetTypeFromDestination(dest);
}
