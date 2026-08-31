// Copyright (c) 2026 The RigorChain developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <script/rigor_bytes.h>
#include <script/rigor_opcode_namespace.h>
#include <script/script.h>
#include <test/util/setup_common.h>

#include <iomanip>
#include <map>
#include <sstream>

#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(rigor_opcode_namespace_tests, BasicTestingSetup)

//! Expected class of `b`, spelled out as byte ranges rather than by calling the
//! classifier under test. Changing ClassifyRigorOpcode without changing this
//! must fail.
static RigorOpcodeClass ExpectedClass(unsigned int b)
{
    if (b <= 0x4e || (b >= 0x51 && b <= 0x60)) return RigorOpcodeClass::PUSH_MARKER;
    if (b == 0x6a || b == 0xbb || b == 0xbd) return RigorOpcodeClass::ASSET_MARKER;
    if (b == 0xbc) return RigorOpcodeClass::BURNED;
    if (b >= 0xbe) return RigorOpcodeClass::INVALID;
    return RigorOpcodeClass::RESERVED_INERT;
}

BOOST_AUTO_TEST_CASE(byte_namespace_partition_is_frozen)
{
    std::map<RigorOpcodeClass, size_t> counts;
    for (unsigned int b = 0; b < 256; ++b) {
        const RigorOpcodeClass got = ClassifyRigorOpcode(b);
        std::ostringstream msg;
        msg << "byte 0x" << std::hex << b << " changed class";
        BOOST_CHECK_MESSAGE(got == ExpectedClass(b), msg.str());
        ++counts[got];
    }

    // Only 98 of 256 bytes mean anything: 95 data pushes, the three asset
    // markers, and nothing else. The remaining 158 are inert or invalid.
    BOOST_CHECK_EQUAL(counts[RigorOpcodeClass::PUSH_MARKER], 95U);
    BOOST_CHECK_EQUAL(counts[RigorOpcodeClass::ASSET_MARKER], 3U);
    BOOST_CHECK_EQUAL(counts[RigorOpcodeClass::BURNED], 1U);
    BOOST_CHECK_EQUAL(counts[RigorOpcodeClass::RESERVED_INERT], 91U);
    BOOST_CHECK_EQUAL(counts[RigorOpcodeClass::INVALID], 66U);
}

BOOST_AUTO_TEST_CASE(legacy_checksig_is_reserved_inert)
{
    BOOST_CHECK_EQUAL(static_cast<unsigned>(RIGOR_RESERVED_LEGACY_CHECKSIG), 0xacu);
    BOOST_CHECK(ClassifyRigorOpcode(0xac) == RigorOpcodeClass::RESERVED_INERT);
    BOOST_CHECK(ClassifyRigorByte(0xac) == RigorByteClass::RESERVED_INERT);
}

BOOST_AUTO_TEST_CASE(live_markers_are_asset_markers)
{
    BOOST_CHECK(ClassifyRigorOpcode(RIGOR_MARKER_RETURN) == RigorOpcodeClass::ASSET_MARKER);
    BOOST_CHECK(ClassifyRigorOpcode(RIGOR_MARKER_RES_MINT) == RigorOpcodeClass::ASSET_MARKER);
    BOOST_CHECK(ClassifyRigorOpcode(RIGOR_MARKER_CONVERGE) == RigorOpcodeClass::ASSET_MARKER);
}

BOOST_AUTO_TEST_CASE(burned_and_invalid)
{
    BOOST_CHECK(ClassifyRigorOpcode(0xbc) == RigorOpcodeClass::BURNED);
    BOOST_CHECK(ClassifyRigorOpcode(0xbe) == RigorOpcodeClass::INVALID);
    BOOST_CHECK(ClassifyRigorOpcode(0xff) == RigorOpcodeClass::INVALID);
}

BOOST_AUTO_TEST_CASE(pushes_are_markers_not_instructions)
{
    BOOST_CHECK(ClassifyRigorOpcode(RIGOR_PUSH_0) == RigorOpcodeClass::PUSH_MARKER);
    BOOST_CHECK(ClassifyRigorOpcode(RIGOR_PUSH_16) == RigorOpcodeClass::PUSH_MARKER);
    BOOST_CHECK(ClassifyRigorOpcode(RIGOR_PUSHDATA1) == RigorOpcodeClass::PUSH_MARKER);
}

BOOST_AUTO_TEST_SUITE_END()
