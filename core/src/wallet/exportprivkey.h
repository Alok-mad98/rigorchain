// Copyright (c) 2026 The RIGOR developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_WALLET_EXPORTPRIVKEY_H
#define BITCOIN_WALLET_EXPORTPRIVKEY_H

#include <primitives/transaction.h>

#include <string>

namespace wallet {
class CWallet;

/** Result of descriptor-backed address private key export (ORU WIF or RES Ed25519 secret hex). */
struct AddressPrivKeyExport {
    std::string address;
    std::string asset;       //!< "ORU" or "RES"
    std::string private_key;
    std::string format;      //!< "WIF" or "ed25519"
};

/**
 * Export the private key for an owned wallet address via descriptor lookup.
 *
 * Path: Address → scriptPubKey → DescriptorScriptPubKeyMan → signing provider → key material.
 * Does not use legacy key databases or importprivkey semantics.
 *
 * Throws JSONRPCError on failure (locked / watch-only / not found / wrong asset / missing key).
 * Requires cs_wallet not held by caller (locks internally).
 */
AddressPrivKeyExport ExportAddressPrivateKey(const CWallet& wallet, const std::string& address,
                                             AssetType expected_asset);
} // namespace wallet

#endif // BITCOIN_WALLET_EXPORTPRIVKEY_H
