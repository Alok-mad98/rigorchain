// Copyright (c) 2026 The RigorChain developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#ifndef BITCOIN_RIGOR_CAPABILITY_H
#define BITCOIN_RIGOR_CAPABILITY_H

#include <cstdint>
#include <string>
#include <vector>

/**
 * RIGOR P2P capability negotiation.
 *
 * Capabilities are communication flags only:
 *   - they are not consensus
 *   - they do not change state transition
 *   - they do not change transaction validity
 *   - they never enter ConnectBlock or the Script predicate
 *
 * Feature enablement is peer.HasCapability(), never `if (peerVersion >= X)`.
 *
 * Unknown bits MUST be ignored (not enabled, not a disconnect).
 * Named future slots are communication-only. Consensus upgrades do not
 * travel through this bitmap.
 */

enum class RigorCapability : uint64_t {
    CAP_NONE = 0,
    CAP_TX_RELAY = uint64_t{1} << 0,
    CAP_BLOCK_RELAY = uint64_t{1} << 1,
    CAP_ADDR_RELAY = uint64_t{1} << 2,
    CAP_COMPACT_BLOCK = uint64_t{1} << 3,
    /** Named communication slot. Not implemented: no salt transport without a per-feature command. */
    CAP_TX_SYNC = uint64_t{1} << 4,
    /** Named communication slot. Not implemented: RES inventory uses CAP_TX_RELAY. */
    CAP_RES_RELAY = uint64_t{1} << 5,
    /** Communication-only future slot. Never local-active. */
    CAP_FUTURE_RELAY_0 = uint64_t{1} << 16,
    /** Communication-only future slot. Never local-active. */
    CAP_FUTURE_RELAY_1 = uint64_t{1} << 17,
};

inline constexpr uint64_t RigorCapabilityBit(RigorCapability cap)
{
    return static_cast<uint64_t>(cap);
}

/** Capabilities this node advertises and may enable. Unimplemented and FUTURE_* excluded. */
inline constexpr uint64_t RIGOR_LOCAL_ACTIVE_CAPABILITIES =
    RigorCapabilityBit(RigorCapability::CAP_TX_RELAY) |
    RigorCapabilityBit(RigorCapability::CAP_BLOCK_RELAY) |
    RigorCapabilityBit(RigorCapability::CAP_ADDR_RELAY) |
    RigorCapabilityBit(RigorCapability::CAP_COMPACT_BLOCK);

/** Named bits we parse. Bits outside this mask are unknown and dropped. */
inline constexpr uint64_t RIGOR_KNOWN_CAPABILITY_MASK =
    RIGOR_LOCAL_ACTIVE_CAPABILITIES |
    RigorCapabilityBit(RigorCapability::CAP_TX_SYNC) |
    RigorCapabilityBit(RigorCapability::CAP_RES_RELAY) |
    RigorCapabilityBit(RigorCapability::CAP_FUTURE_RELAY_0) |
    RigorCapabilityBit(RigorCapability::CAP_FUTURE_RELAY_1);

/** Exact `rigorcap` payload size: uint32 protocol + uint64 bitmap. */
inline constexpr size_t RIGORCAP_PAYLOAD_BYTES = 12;

inline constexpr uint64_t RigorConnectionCapabilityMask(bool tx_ok, bool addr_ok)
{
    uint64_t bits = RigorCapabilityBit(RigorCapability::CAP_BLOCK_RELAY) |
                    RigorCapabilityBit(RigorCapability::CAP_COMPACT_BLOCK);
    if (addr_ok) bits |= RigorCapabilityBit(RigorCapability::CAP_ADDR_RELAY);
    if (tx_ok) bits |= RigorCapabilityBit(RigorCapability::CAP_TX_RELAY);
    return bits;
}

/**
 * Per-peer capability state. Intersection of
 * (peer advertisement ∩ known ∩ local-active ∩ connection mask).
 * Unimplemented named slots and FUTURE_* are recorded as advertised and are not enabled.
 */
class CNodeCapabilities
{
    uint64_t m_peer_advertised{0};
    uint64_t m_enabled{0};
    uint32_t m_peer_protocol_version{0};
    bool m_received{false};

public:
    bool HasCapability(RigorCapability cap) const
    {
        if (cap == RigorCapability::CAP_NONE) return false;
        return (m_enabled & RigorCapabilityBit(cap)) != 0;
    }

    bool Received() const { return m_received; }
    uint64_t AdvertisedBitmap() const { return m_peer_advertised; }
    uint64_t EnabledBitmap() const { return m_enabled; }
    uint32_t PeerProtocolVersion() const { return m_peer_protocol_version; }

    /**
     * Apply a RIGORCAP advertisement.
     * Unknown bits are discarded. Known-but-not-local-active bits are recorded
     * as advertised and are not enabled. connection_mask further restricts
     * enablement to what this connection may use.
     */
    void ApplyAdvertisement(uint32_t protocol_version, uint64_t bitmap,
                            uint64_t connection_mask = ~uint64_t{0})
    {
        m_received = true;
        m_peer_protocol_version = protocol_version;
        m_peer_advertised = bitmap;
        const uint64_t known = bitmap & RIGOR_KNOWN_CAPABILITY_MASK;
        m_enabled = known & RIGOR_LOCAL_ACTIVE_CAPABILITIES & connection_mask;
    }
};

const char* RigorCapabilityName(RigorCapability cap);
std::vector<std::string> RigorCapabilityNames(uint64_t bitmap);

#endif // BITCOIN_RIGOR_CAPABILITY_H
