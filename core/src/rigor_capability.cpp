// Copyright (c) 2026 The RigorChain developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <rigor_capability.h>

const char* RigorCapabilityName(RigorCapability cap)
{
    switch (cap) {
    case RigorCapability::CAP_NONE: return "CAP_NONE";
    case RigorCapability::CAP_TX_RELAY: return "CAP_TX_RELAY";
    case RigorCapability::CAP_BLOCK_RELAY: return "CAP_BLOCK_RELAY";
    case RigorCapability::CAP_ADDR_RELAY: return "CAP_ADDR_RELAY";
    case RigorCapability::CAP_COMPACT_BLOCK: return "CAP_COMPACT_BLOCK";
    case RigorCapability::CAP_TX_SYNC: return "CAP_TX_SYNC";
    case RigorCapability::CAP_RES_RELAY: return "CAP_RES_RELAY";
    case RigorCapability::CAP_FUTURE_RELAY_0: return "CAP_FUTURE_RELAY_0";
    case RigorCapability::CAP_FUTURE_RELAY_1: return "CAP_FUTURE_RELAY_1";
    }
    return "UNKNOWN";
}

std::vector<std::string> RigorCapabilityNames(uint64_t bitmap)
{
    std::vector<std::string> names;
    static constexpr RigorCapability kAll[] = {
        RigorCapability::CAP_TX_RELAY,
        RigorCapability::CAP_BLOCK_RELAY,
        RigorCapability::CAP_ADDR_RELAY,
        RigorCapability::CAP_COMPACT_BLOCK,
        RigorCapability::CAP_TX_SYNC,
        RigorCapability::CAP_RES_RELAY,
        RigorCapability::CAP_FUTURE_RELAY_0,
        RigorCapability::CAP_FUTURE_RELAY_1,
    };
    for (RigorCapability cap : kAll) {
        if (bitmap & RigorCapabilityBit(cap)) {
            names.emplace_back(RigorCapabilityName(cap));
        }
    }
    return names;
}
