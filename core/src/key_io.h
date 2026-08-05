// Copyright (c) 2009-2010 Satoshi Nakamoto
// Copyright (c) 2009-2021 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_KEY_IO_H
#define BITCOIN_KEY_IO_H

#include <chainparams.h>
#include <key.h>
#include <pubkey.h>
#include <script/standard.h>

#include <string>

CKey DecodeSecret(const std::string& str);
std::string EncodeSecret(const CKey& key);

CExtKey DecodeExtKey(const std::string& str);
std::string EncodeExtKey(const CExtKey& extkey);
CExtPubKey DecodeExtPubKey(const std::string& str);
std::string EncodeExtPubKey(const CExtPubKey& extpubkey);

std::string EncodeDestination(const CTxDestination& dest);
CTxDestination DecodeDestination(const std::string& str);
CTxDestination DecodeDestination(const std::string& str, std::string& error_msg, std::vector<int>* error_locations = nullptr);
CTxDestination DecodeDestination(const std::string& str, const CChainParams& params, std::string& error_msg, std::vector<int>* error_locations = nullptr);
bool IsValidDestinationString(const std::string& str);
bool IsValidDestinationString(const std::string& str, const CChainParams& params);

/**
 * RFC-WALLET-002: RG1P/RG1Q address distinction for asset type detection.
 * RG1P (Witness V0) → ORU asset
 * RG1Q (Witness V1) → RES asset
 */

/** Detect asset type from CTxDestination. Returns std::nullopt if destination does not encode asset type. */
std::optional<AssetType> GetAssetTypeFromDestination(const CTxDestination& dest);

/** Detect asset type from address string. Returns std::nullopt if address is invalid or does not encode asset type. */
std::optional<AssetType> GetAssetTypeFromAddress(const std::string& str, const CChainParams& params);

#endif // BITCOIN_KEY_IO_H
