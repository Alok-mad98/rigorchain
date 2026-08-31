// Copyright (c) 2026 The RigorChain developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_SEED_MANIFEST_H
#define BITCOIN_SEED_MANIFEST_H

#include <netaddress.h>
#include <protocol.h>
#include <span.h>

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

/** Maximum size of rigor_seed_v1.json. Larger files are ignored, not truncated. */
static constexpr size_t MAX_SEED_MANIFEST_JSON_BYTES = 65536;
/** Maximum node entries. A larger array is rejected as a whole. */
static constexpr size_t MAX_SEED_MANIFEST_NODES = 512;
/** Maximum length of epoch or node token (printable ASCII). */
static constexpr size_t MAX_SEED_MANIFEST_TOKEN_BYTES = 128;

/**
 * Signed seed manifest (rigor_seed_v1.json).
 *
 * Fixed seeds cannot be encrypted: bootstrap IPs must be public.
 * Authenticity is Ed25519 over a length-prefixed canonical message, not encryption.
 * This is a P2P bootstrap check. It is not consensus.
 */
struct SeedManifest {
    std::string epoch;
    int64_t expiry{0};
    std::vector<std::string> nodes;
    std::vector<unsigned char> signature;
};

/**
 * Canonical bytes signed by the seed-operator Ed25519 key.
 *
 * Encoding (Bitcoin compact-size strings, injective):
 *   tag ‖ chain_id ‖ epoch ‖ expiry(uint64 LE) ‖ nodes[]
 * Nodes are sorted before encoding. chain_id is CChainParams::NetworkIDString().
 */
std::string SeedManifestSignedMessage(const SeedManifest& manifest, std::string_view chain_id);

/** Parse JSON. Never throws. Rejects floats, empty/non-positive expiry, oversized files of nodes. */
bool ParseSeedManifestJson(const std::string& json, SeedManifest& out, std::string& error);

bool SeedEpochAccepted(const std::string& epoch, const std::vector<std::string>& accepted_epochs);

bool VerifySeedManifestSignature(const SeedManifest& manifest,
                                 Span<const unsigned char> pubkey,
                                 std::string_view chain_id);

std::vector<CAddress> AddressesFromSeedManifest(const SeedManifest& manifest, uint16_t default_port);

#endif // BITCOIN_SEED_MANIFEST_H
