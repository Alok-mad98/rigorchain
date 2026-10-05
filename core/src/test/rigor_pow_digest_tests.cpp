// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// SPEC-008 §7. These vectors are shared with formal_verification/coq/PowDigest.v;
// the zero-header digest and its little-endian reading are the same constants
// that module discharges by vm_compute. If one side changes, both must.

#include <arith_uint256.h>
#include <primitives/block.h>
#include <rigor_kernel/keccak.h>
#include <rigor_kernel/pow_digest.h>
#include <streams.h>
#include <test/util/setup_common.h>
#include <uint256.h>
#include <version.h>

#include <boost/test/unit_test.hpp>

#include <algorithm>
#include <cstddef>
#include <numeric>
#include <vector>

BOOST_FIXTURE_TEST_SUITE(rigor_pow_digest_tests, BasicTestingSetup)

namespace {

std::vector<unsigned char> Sha3(const std::vector<unsigned char>& msg)
{
    std::vector<unsigned char> out(32);
    // An empty vector's data() is null. Length 0 still needs a valid pointer.
    const unsigned char empty_src = 0;
    const unsigned char* src = msg.empty() ? &empty_src : msg.data();
    rigor_kernel::Sha3_256Short(src, msg.size(), out.data());
    return out;
}

//! A header with every field distinct. A zero header cannot catch a field
//! ordering or endianness bug, because every permutation of zeros is zero.
CBlockHeader TypedHeader()
{
    CBlockHeader h;
    h.nVersion = 0x20000000;
    h.hashPrevBlock = uint256S("0x00000000000000000001a2b3c4d5e6f7890a0b0c0d0e0f101112131415161718");
    h.hashMerkleRoot = uint256S("0x9d3fbd207af927fc8e2c57e975d8d5845913bf059d956592e8a0bcf38b3515dd");
    h.nTime = 1786125600;
    h.nBits = 0x1d008000;
    h.nNonce = 0x89abcdef;
    return h;
}

} // namespace

BOOST_AUTO_TEST_CASE(sha3_256_fips202_vectors)
{
    const std::vector<unsigned char> empty_expected{
        0xa7, 0xff, 0xc6, 0xf8, 0xbf, 0x1e, 0xd7, 0x66, 0x51, 0xc1, 0x47,
        0x56, 0xa0, 0x61, 0xd6, 0x62, 0xf5, 0x80, 0xff, 0x4d, 0xe4, 0x3b,
        0x49, 0xfa, 0x82, 0xd8, 0x0a, 0x4b, 0x80, 0xf8, 0x43, 0x4a};
    BOOST_CHECK(Sha3({}) == empty_expected);

    const std::vector<unsigned char> abc{'a', 'b', 'c'};
    const std::vector<unsigned char> abc_expected{
        0x3a, 0x98, 0x5d, 0xa7, 0x4f, 0xe2, 0x25, 0xb2, 0x04, 0x5c, 0x17,
        0x2d, 0x6b, 0xd3, 0x90, 0xbd, 0x85, 0x5f, 0x08, 0x6e, 0x3e, 0x9d,
        0x52, 0x5b, 0x46, 0xbf, 0xe2, 0x45, 0x11, 0x43, 0x15, 0x32};
    BOOST_CHECK(Sha3(abc) == abc_expected);
}

BOOST_AUTO_TEST_CASE(sha3_256_block_boundary)
{
    // 134 bytes is the longest message the single-block construction accepts.
    // RIGOR's own preimage is 88, so this guards an edge the protocol never
    // reaches but an implementer could reach by generalising the function.
    std::vector<unsigned char> msg(rigor_kernel::SHA3_256_MAX_SHORT_MSG);
    std::iota(msg.begin(), msg.end(), static_cast<unsigned char>(0));

    const std::vector<unsigned char> expected{
        0x64, 0x4e, 0x15, 0x22, 0x4f, 0x55, 0x97, 0x35, 0x1a, 0xef, 0x5c,
        0x4b, 0xdd, 0x22, 0xb2, 0x7c, 0xa0, 0xc1, 0x9d, 0xb2, 0x24, 0x44,
        0x31, 0x53, 0x4c, 0x2a, 0x4a, 0x0b, 0xeb, 0xfd, 0xf3, 0x9c};
    BOOST_CHECK(Sha3(msg) == expected);
}

BOOST_AUTO_TEST_CASE(pow_preimage_matches_canonical_serialization)
{
    const CBlockHeader header = TypedHeader();

    unsigned char preimage[rigor_kernel::POW_PREIMAGE_LEN];
    rigor_kernel::BuildPowPreimage(header, preimage);

    // The hand-rolled hot path must agree with the consensus serializer.
    CDataStream ss(SER_NETWORK, PROTOCOL_VERSION);
    ss << header;
    BOOST_CHECK_EQUAL(ss.size(), rigor_kernel::POW_HEADER_LEN);
    BOOST_CHECK(std::equal(ss.begin(), ss.end(), preimage + rigor_kernel::POW_DOMAIN_TAG_LEN,
                           [](std::byte a, unsigned char b) {
                               return static_cast<unsigned char>(a) == b;
                           }));

    BOOST_CHECK(std::equal(rigor_kernel::POW_DOMAIN_TAG,
                           rigor_kernel::POW_DOMAIN_TAG + rigor_kernel::POW_DOMAIN_TAG_LEN,
                           preimage));

    // 88 <= 134, so the padded message is one rate block: exactly one
    // Keccak-f[1600] (SPEC-008 §3.3).
    static_assert(rigor_kernel::POW_PREIMAGE_LEN <= rigor_kernel::SHA3_256_MAX_SHORT_MSG,
                  "SPEC-008 preimage must fit one rate block");
}

BOOST_AUTO_TEST_CASE(pow_digest_agrees_with_coq_model_zero_header)
{
    // A default header serializes to 80 zero bytes, which is exactly the Coq
    // model's zero_header. PowDigest.v proves this digest by vm_compute.
    const CBlockHeader zero_header;
    const uint256 digest = rigor_kernel::GetRigorPowHash(zero_header);

    const unsigned char expected[32] = {
        0xdd, 0x05, 0x7f, 0xc1, 0xdf, 0x8f, 0x5b, 0x94, 0x66, 0xe8, 0x34,
        0xcc, 0x9d, 0x7b, 0x81, 0x9e, 0x2d, 0x51, 0x1c, 0x75, 0xb4, 0x36,
        0x75, 0xb5, 0x77, 0xf6, 0xd9, 0xf5, 0xd6, 0xde, 0x04, 0xfc};
    BOOST_CHECK(std::equal(expected, expected + 32, digest.begin()));

    // Byte 0 of the digest is the least significant limb of the value that is
    // compared against the target. This is the convention that silently
    // diverges between a model and an implementation, so it is pinned twice.
    BOOST_CHECK_EQUAL(digest.begin()[0], 0xdd);
    BOOST_CHECK_EQUAL(digest.ToString(),
                      "fc04ded6f5d9f677b57536b4751c512d9e817b9dcc34e866945b8fdfc17f05dd");
}

BOOST_AUTO_TEST_CASE(pow_digest_typed_header_vector)
{
    const uint256 digest = rigor_kernel::GetRigorPowHash(TypedHeader());
    BOOST_CHECK_EQUAL(digest.ToString(),
                      "f041f0b4e75543c50a2fbffd6b0e19f7f1ee8b1c4bfcc75c1002d29a8271358b");
}

BOOST_AUTO_TEST_CASE(pow_domain_tag_is_load_bearing)
{
    const CBlockHeader zero_header;
    const uint256 tagged = rigor_kernel::GetRigorPowHash(zero_header);

    std::vector<unsigned char> untagged_msg(rigor_kernel::POW_HEADER_LEN, 0);
    const std::vector<unsigned char> untagged = Sha3(untagged_msg);

    BOOST_CHECK(!std::equal(untagged.begin(), untagged.end(), tagged.begin()));
}

BOOST_AUTO_TEST_CASE(pow_digest_is_deterministic_and_nonce_sensitive)
{
    const CBlockHeader header = TypedHeader();
    BOOST_CHECK(rigor_kernel::GetRigorPowHash(header) == rigor_kernel::GetRigorPowHash(header));

    CBlockHeader bumped = header;
    bumped.nNonce = header.nNonce + 1;
    BOOST_CHECK(rigor_kernel::GetRigorPowHash(header) != rigor_kernel::GetRigorPowHash(bumped));

    // Every header field must reach the digest, not just the nonce.
    CBlockHeader retimed = header;
    retimed.nTime = header.nTime + 1;
    BOOST_CHECK(rigor_kernel::GetRigorPowHash(header) != rigor_kernel::GetRigorPowHash(retimed));

    CBlockHeader rebitted = header;
    rebitted.nBits = header.nBits + 1;
    BOOST_CHECK(rigor_kernel::GetRigorPowHash(header) != rigor_kernel::GetRigorPowHash(rebitted));
}

BOOST_AUTO_TEST_SUITE_END()
