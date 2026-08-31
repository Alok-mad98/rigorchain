// Copyright (c) 2026 The RigorChain developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <rigor_capability.h>
#include <test/util/setup_common.h>
#include <version.h>

#include <algorithm>
#include <string>

#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(rigor_capability_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(protocol_versions_are_separated_from_serialization)
{
    BOOST_CHECK_EQUAL(RIGOR_PROTOCOL_MIN_VERSION, 1);
    BOOST_CHECK_EQUAL(RIGOR_PROTOCOL_CURRENT_VERSION, 1);
    BOOST_CHECK(RIGOR_PROTOCOL_MIN_VERSION <= RIGOR_PROTOCOL_CURRENT_VERSION);
    BOOST_CHECK_EQUAL(SERIALIZATION_VERSION, PROTOCOL_VERSION);
    BOOST_CHECK(RIGOR_PROTOCOL_MIN_VERSION != SERIALIZATION_VERSION);
    BOOST_CHECK(RIGOR_PROTOCOL_CURRENT_VERSION != SERIALIZATION_VERSION);
    BOOST_CHECK_EQUAL(PROTOCOL_VERSION, 70016);
    BOOST_CHECK_EQUAL(RIGORCAP_PAYLOAD_BYTES, 12);
}

BOOST_AUTO_TEST_CASE(local_active_is_relay_only)
{
    BOOST_CHECK(RIGOR_LOCAL_ACTIVE_CAPABILITIES & RigorCapabilityBit(RigorCapability::CAP_TX_RELAY));
    BOOST_CHECK(RIGOR_LOCAL_ACTIVE_CAPABILITIES & RigorCapabilityBit(RigorCapability::CAP_BLOCK_RELAY));
    BOOST_CHECK(RIGOR_LOCAL_ACTIVE_CAPABILITIES & RigorCapabilityBit(RigorCapability::CAP_ADDR_RELAY));
    BOOST_CHECK(RIGOR_LOCAL_ACTIVE_CAPABILITIES & RigorCapabilityBit(RigorCapability::CAP_COMPACT_BLOCK));
    BOOST_CHECK(!(RIGOR_LOCAL_ACTIVE_CAPABILITIES & RigorCapabilityBit(RigorCapability::CAP_TX_SYNC)));
    BOOST_CHECK(!(RIGOR_LOCAL_ACTIVE_CAPABILITIES & RigorCapabilityBit(RigorCapability::CAP_RES_RELAY)));
    BOOST_CHECK(!(RIGOR_LOCAL_ACTIVE_CAPABILITIES & RigorCapabilityBit(RigorCapability::CAP_FUTURE_RELAY_0)));
    BOOST_CHECK(!(RIGOR_LOCAL_ACTIVE_CAPABILITIES & RigorCapabilityBit(RigorCapability::CAP_FUTURE_RELAY_1)));
}

BOOST_AUTO_TEST_CASE(empty_bitmap_enables_nothing)
{
    CNodeCapabilities caps;
    caps.ApplyAdvertisement(1, RigorCapabilityBit(RigorCapability::CAP_NONE));
    BOOST_CHECK(caps.Received());
    BOOST_CHECK_EQUAL(caps.EnabledBitmap(), 0);
    BOOST_CHECK(!caps.HasCapability(RigorCapability::CAP_TX_RELAY));
    BOOST_CHECK(!caps.HasCapability(RigorCapability::CAP_BLOCK_RELAY));
    BOOST_CHECK(!caps.HasCapability(RigorCapability::CAP_ADDR_RELAY));
    BOOST_CHECK(!caps.HasCapability(RigorCapability::CAP_COMPACT_BLOCK));
}

BOOST_AUTO_TEST_CASE(unknown_bits_are_ignored)
{
    CNodeCapabilities caps;
    const uint64_t unknown_high = uint64_t{1} << 63;
    const uint64_t mixed = RIGOR_LOCAL_ACTIVE_CAPABILITIES | unknown_high |
                           RigorCapabilityBit(RigorCapability::CAP_FUTURE_RELAY_0);
    caps.ApplyAdvertisement(/*protocol_version=*/2, mixed);

    BOOST_CHECK(caps.Received());
    BOOST_CHECK(caps.HasCapability(RigorCapability::CAP_TX_RELAY));
    BOOST_CHECK(caps.HasCapability(RigorCapability::CAP_BLOCK_RELAY));
    BOOST_CHECK(caps.HasCapability(RigorCapability::CAP_ADDR_RELAY));
    BOOST_CHECK(caps.HasCapability(RigorCapability::CAP_COMPACT_BLOCK));
    BOOST_CHECK(!caps.HasCapability(RigorCapability::CAP_FUTURE_RELAY_0));
    BOOST_CHECK(!caps.HasCapability(RigorCapability::CAP_FUTURE_RELAY_1));
    BOOST_CHECK_EQUAL(caps.EnabledBitmap() & unknown_high, 0);
    BOOST_CHECK_EQUAL(caps.AdvertisedBitmap() & unknown_high, unknown_high);
}

BOOST_AUTO_TEST_CASE(unimplemented_named_slots_are_not_enabled)
{
    CNodeCapabilities caps;
    caps.ApplyAdvertisement(1,
                            RigorCapabilityBit(RigorCapability::CAP_TX_SYNC) |
                                RigorCapabilityBit(RigorCapability::CAP_RES_RELAY) |
                                RigorCapabilityBit(RigorCapability::CAP_FUTURE_RELAY_0) |
                                RigorCapabilityBit(RigorCapability::CAP_FUTURE_RELAY_1));
    BOOST_CHECK_EQUAL(caps.EnabledBitmap(), 0);
    BOOST_CHECK(!caps.HasCapability(RigorCapability::CAP_TX_SYNC));
    BOOST_CHECK(!caps.HasCapability(RigorCapability::CAP_RES_RELAY));
}

BOOST_AUTO_TEST_CASE(consensus_upgrade_bits_are_not_in_the_namespace)
{
    // FUTURE_SIGNATURE_V2 / FUTURE_PREDICATE_V2 were consensus-leakage slots.
    // The known mask is communication-only: relay, unimplemented relay slots, future relay.
    const uint64_t known = RIGOR_KNOWN_CAPABILITY_MASK;
    BOOST_CHECK_EQUAL(known & ~(
        RigorCapabilityBit(RigorCapability::CAP_TX_RELAY) |
        RigorCapabilityBit(RigorCapability::CAP_BLOCK_RELAY) |
        RigorCapabilityBit(RigorCapability::CAP_ADDR_RELAY) |
        RigorCapabilityBit(RigorCapability::CAP_COMPACT_BLOCK) |
        RigorCapabilityBit(RigorCapability::CAP_TX_SYNC) |
        RigorCapabilityBit(RigorCapability::CAP_RES_RELAY) |
        RigorCapabilityBit(RigorCapability::CAP_FUTURE_RELAY_0) |
        RigorCapabilityBit(RigorCapability::CAP_FUTURE_RELAY_1)), 0);
}

BOOST_AUTO_TEST_CASE(unadvertised_enables_nothing)
{
    CNodeCapabilities caps;
    BOOST_CHECK(!caps.Received());
    BOOST_CHECK_EQUAL(caps.EnabledBitmap(), 0);
    BOOST_CHECK(!caps.HasCapability(RigorCapability::CAP_TX_RELAY));
    BOOST_CHECK(!caps.HasCapability(RigorCapability::CAP_BLOCK_RELAY));
}

BOOST_AUTO_TEST_CASE(advertisement_is_the_only_enablement_path)
{
    CNodeCapabilities caps;
    caps.ApplyAdvertisement(1, RigorCapabilityBit(RigorCapability::CAP_BLOCK_RELAY));
    BOOST_CHECK(caps.Received());
    BOOST_CHECK(caps.HasCapability(RigorCapability::CAP_BLOCK_RELAY));
    BOOST_CHECK(!caps.HasCapability(RigorCapability::CAP_TX_RELAY));
}

BOOST_AUTO_TEST_CASE(connection_mask_restricts_enablement)
{
    CNodeCapabilities caps;
    const uint64_t no_tx = RigorConnectionCapabilityMask(/*tx_ok=*/false, /*addr_ok=*/true);
    caps.ApplyAdvertisement(1, RIGOR_LOCAL_ACTIVE_CAPABILITIES, no_tx);
    BOOST_CHECK(!caps.HasCapability(RigorCapability::CAP_TX_RELAY));
    BOOST_CHECK(caps.HasCapability(RigorCapability::CAP_ADDR_RELAY));
    BOOST_CHECK(caps.HasCapability(RigorCapability::CAP_BLOCK_RELAY));
}

BOOST_AUTO_TEST_CASE(capability_does_not_bypass_predicate)
{
    // Isolation: HasCapability cannot mention VerifyRigorPredicate. This test
    // only checks the communication type of the enabled set.
    CNodeCapabilities caps;
    caps.ApplyAdvertisement(1, RIGOR_LOCAL_ACTIVE_CAPABILITIES);
    const auto names = RigorCapabilityNames(caps.EnabledBitmap());
    for (const auto& name : names) {
        BOOST_CHECK(name.find("SIGNATURE") == std::string::npos);
        BOOST_CHECK(name.find("PREDICATE") == std::string::npos);
        BOOST_CHECK(name.find("CONSENSUS") == std::string::npos);
    }
}

BOOST_AUTO_TEST_CASE(capability_names)
{
    BOOST_CHECK_EQUAL(std::string(RigorCapabilityName(RigorCapability::CAP_TX_RELAY)), "CAP_TX_RELAY");
    const auto names = RigorCapabilityNames(RIGOR_LOCAL_ACTIVE_CAPABILITIES);
    BOOST_CHECK_EQUAL(names.size(), 4);
}

BOOST_AUTO_TEST_SUITE_END()
