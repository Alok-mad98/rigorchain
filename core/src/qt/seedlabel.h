// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_QT_SEEDLABEL_H
#define BITCOIN_QT_SEEDLABEL_H

#include <string>
#include <vector>

/** One configured DNS seed after a lookup attempt. Addresses are IP strings, not ports. */
struct ConfiguredSeedLookup {
    std::string name;
    std::vector<std::string> addresses;
    bool lookup_finished{false};
};

enum class SeedNameMatch {
    None,
    Unique,
    Ambiguous,
};

struct SeedNameResult {
    SeedNameMatch match{SeedNameMatch::None};
    std::string name;
};

/**
 * Attribute ip to a configured seed only when exactly one finished lookup
 * contains that address. An unfinished lookup, an empty lookup, or two
 * matching names produces no hostname.
 */
inline SeedNameResult UniqueConfiguredSeedName(const std::string& ip, const std::vector<ConfiguredSeedLookup>& seeds)
{
    SeedNameResult result;
    if (ip.empty()) return result;
    for (const ConfiguredSeedLookup& seed : seeds) {
        if (!seed.lookup_finished) continue;
        bool found = false;
        for (const std::string& address : seed.addresses) {
            if (address == ip) {
                found = true;
                break;
            }
        }
        if (!found) continue;
        if (result.match == SeedNameMatch::Unique) {
            result.match = SeedNameMatch::Ambiguous;
            result.name.clear();
            return result;
        }
        result.match = SeedNameMatch::Unique;
        result.name = seed.name;
    }
    return result;
}

#endif // BITCOIN_QT_SEEDLABEL_H
