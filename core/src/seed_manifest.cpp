// Copyright (c) 2026 The RigorChain developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <seed_manifest.h>

#include <crypto/ed25519_provider.h>
#include <logging.h>
#include <netbase.h>
#include <random.h>
#include <serialize.h>
#include <span.h>
#include <streams.h>
#include <tinyformat.h>
#include <univalue.h>
#include <util/strencodings.h>
#include <util/time.h>
#include <version.h>

#include <algorithm>
#include <cstdint>
#include <stdexcept>
#include <string_view>

using namespace std::chrono_literals;

static bool IsManifestToken(const std::string& s)
{
    if (s.empty() || s.size() > MAX_SEED_MANIFEST_TOKEN_BYTES) return false;
    for (unsigned char c : s) {
        if (c < 0x20 || c > 0x7E) return false;
    }
    return true;
}

std::string SeedManifestSignedMessage(const SeedManifest& manifest, std::string_view chain_id)
{
    std::vector<std::string> nodes = manifest.nodes;
    std::sort(nodes.begin(), nodes.end());
    DataStream ss{};
    ss << std::string{"RIGOR_SEED_MANIFEST_V1"};
    ss << std::string{chain_id};
    ss << manifest.epoch;
    ss << uint64_t(manifest.expiry);
    ss << nodes;
    return ss.str();
}

bool ParseSeedManifestJson(const std::string& json, SeedManifest& out, std::string& error)
{
    try {
        if (json.size() > MAX_SEED_MANIFEST_JSON_BYTES) {
            error = "seed manifest exceeds size limit";
            return false;
        }
        UniValue uv;
        if (!uv.read(json) || !uv.isObject()) {
            error = "seed manifest is not a JSON object";
            return false;
        }
        if (!uv.exists("epoch") || !uv["epoch"].isStr()) {
            error = "seed manifest missing epoch";
            return false;
        }
        if (!uv.exists("expiry") || !uv["expiry"].isNum()) {
            error = "seed manifest missing expiry";
            return false;
        }
        if (!uv.exists("nodes") || !uv["nodes"].isArray()) {
            error = "seed manifest missing nodes";
            return false;
        }
        if (!uv.exists("signature") || !uv["signature"].isStr()) {
            error = "seed manifest missing signature";
            return false;
        }

        const std::string expiry_str = uv["expiry"].getValStr();
        if (expiry_str.find('.') != std::string::npos ||
            expiry_str.find('e') != std::string::npos ||
            expiry_str.find('E') != std::string::npos) {
            error = "seed manifest expiry must be an integer";
            return false;
        }

        SeedManifest parsed;
        parsed.epoch = uv["epoch"].get_str();
        if (!IsManifestToken(parsed.epoch)) {
            error = "seed manifest epoch is not printable ASCII";
            return false;
        }
        parsed.expiry = uv["expiry"].getInt<int64_t>();
        if (parsed.expiry <= 0) {
            error = "seed manifest expiry must be a positive unix time";
            return false;
        }

        const auto& nodes = uv["nodes"].getValues();
        if (nodes.size() > MAX_SEED_MANIFEST_NODES) {
            error = "seed manifest has too many nodes";
            return false;
        }
        parsed.nodes.reserve(nodes.size());
        for (const auto& node : nodes) {
            if (!node.isStr()) {
                error = "seed manifest node is not a string";
                return false;
            }
            const std::string s = node.get_str();
            if (!IsManifestToken(s)) {
                error = "seed manifest node is not printable ASCII";
                return false;
            }
            parsed.nodes.push_back(s);
        }
        auto sorted = parsed.nodes;
        std::sort(sorted.begin(), sorted.end());
        if (std::adjacent_find(sorted.begin(), sorted.end()) != sorted.end()) {
            error = "seed manifest has duplicate nodes";
            return false;
        }

        parsed.signature = ParseHex(uv["signature"].get_str());
        if (parsed.signature.size() != ed25519::SIGNATURE_SIZE) {
            error = "seed manifest signature must be 64 bytes";
            return false;
        }
        out = std::move(parsed);
        return true;
    } catch (const std::exception& e) {
        error = strprintf("seed manifest parse failed: %s", e.what());
        return false;
    }
}

bool SeedEpochAccepted(const std::string& epoch, const std::vector<std::string>& accepted_epochs)
{
    return std::find(accepted_epochs.begin(), accepted_epochs.end(), epoch) != accepted_epochs.end();
}

bool VerifySeedManifestSignature(const SeedManifest& manifest,
                                 Span<const unsigned char> pubkey,
                                 std::string_view chain_id)
{
    if (pubkey.size() != ed25519::PUBKEY_SIZE) return false;
    if (chain_id.empty()) return false;
    const std::string msg = SeedManifestSignedMessage(manifest, chain_id);
    return ed25519::VerifyDetached(manifest.signature,
                                   MakeUCharSpan(msg),
                                   pubkey);
}

std::vector<CAddress> AddressesFromSeedManifest(const SeedManifest& manifest, uint16_t default_port)
{
    std::vector<CAddress> out;
    const auto one_week{7 * 24h};
    FastRandomContext rng;
    for (const auto& node : manifest.nodes) {
        CService service;
        if (!Lookup(node, service, default_port, /*fAllowLookup=*/false)) {
            LogPrint(BCLog::NET, "Signed seed manifest skipped unparsable node %s\n", node);
            continue;
        }
        CAddress addr{service, static_cast<ServiceFlags>(NODE_NETWORK | NODE_WITNESS)};
        addr.nTime = rng.rand_uniform_delay(Now<NodeSeconds>() - one_week, -one_week);
        out.push_back(addr);
    }
    return out;
}
