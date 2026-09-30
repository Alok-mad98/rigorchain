// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// Pins the documentation constants in
// docs/security/RIGOR_SECURITY_PATCH_BOUNDARY.md.
// Literals are intentional: changing a consensus lock and its caller together
// must still fail this file. This suite does not define a consensus rule.

#include <chainparams.h>
#include <chainparamsbase.h>
#include <consensus/amount.h>
#include <consensus/rigor_consensus_lock.h>
#include <consensus/rigor_issuance.h>
#include <primitives/transaction.h>
#include <rigor_kernel/pow_digest.h>
#include <script/predicate_verify.h>
#include <script/rigor_bytes.h>
#include <script/rigor_tags.h>
#include <script/script.h>
#include <script/standard.h>
#include <test/util/setup_common.h>

#include <boost/test/unit_test.hpp>

#include <array>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

BOOST_FIXTURE_TEST_SUITE(security_boundary_tests, BasicTestingSetup)

namespace {

bool TagIs(Span<const unsigned char> tag, const char* literal)
{
    return tag.size() == 8 && std::memcmp(tag.data(), literal, 8) == 0;
}

bool TagIs(const std::array<unsigned char, 8>& tag, const char* literal)
{
    return std::memcmp(tag.data(), literal, 8) == 0;
}

} // namespace

BOOST_AUTO_TEST_CASE(oru_subsidy_constant_is_unchanged)
{
    BOOST_CHECK_EQUAL(COIN, 100000000);
    BOOST_CHECK_EQUAL(Consensus::ORU_GENESIS_BLOCK_SUBSIDY, 0);
    BOOST_CHECK_EQUAL(Consensus::ORU_FLAT_BLOCK_SUBSIDY, 128 * COIN);
    BOOST_CHECK_EQUAL(Consensus::ORU_FLAT_BLOCK_SUBSIDY, 12800000000LL);

    const auto main = CreateChainParams(*m_node.args, CBaseChainParams::MAIN);
    const auto& params = main->GetConsensus();
    BOOST_CHECK_EQUAL(GetBlockSubsidy(0, params), 0);
    const int heights[] = {1, 2, 100, 209999, 210000, 420000, 10000000};
    for (int h : heights) {
        BOOST_CHECK_EQUAL(GetBlockSubsidy(h, params), 12800000000LL);
    }
}

BOOST_AUTO_TEST_CASE(res_max_supply_constant_is_unchanged)
{
    BOOST_CHECK_EQUAL(Consensus::RES_COIN, 100000000);
    BOOST_CHECK_EQUAL(Consensus::RES_MAX_SUPPLY, 419430400000000LL);
    BOOST_CHECK_EQUAL(Consensus::RES_MAX_SUPPLY, (CAmount{1} << 22) * Consensus::RES_COIN);
}

BOOST_AUTO_TEST_CASE(op_converge_byte_is_unchanged)
{
    BOOST_CHECK_EQUAL(static_cast<unsigned>(RIGOR_MARKER_CONVERGE), 0xbdu);
    const CScript converge = CScript() << std::vector<unsigned char>(20, 0x11) << RIGOR_MARKER_CONVERGE;
    BOOST_CHECK_EQUAL(converge.size(), 22U);
    BOOST_CHECK_EQUAL(static_cast<unsigned>(converge.back()), 0xbdu);
    std::vector<std::vector<unsigned char>> solutions;
    BOOST_CHECK(ClassifyRigorTxout(converge, solutions) == TxoutType::CONVERGE);
}

BOOST_AUTO_TEST_CASE(witness_versions_are_unchanged)
{
    BOOST_CHECK_EQUAL(static_cast<unsigned>(RIGOR_PUSH_0), 0x00u);
    BOOST_CHECK_EQUAL(static_cast<unsigned>(RIGOR_PUSH_1), 0x51u);
    BOOST_CHECK_EQUAL(CScript::DecodeOP_N(RIGOR_PUSH_0), 0);
    BOOST_CHECK_EQUAL(CScript::DecodeOP_N(RIGOR_PUSH_1), 1);
    BOOST_CHECK_EQUAL(WITNESS_V0_KEYHASH_SIZE, 20U);
    BOOST_CHECK_EQUAL(WITNESS_V1_RES_SIZE, 32U);

    const CScript oru = CScript() << RIGOR_PUSH_0 << std::vector<unsigned char>(20, 0x11);
    int oru_version = -1;
    std::vector<unsigned char> oru_program;
    BOOST_CHECK(oru.IsWitnessProgram(oru_version, oru_program));
    BOOST_CHECK_EQUAL(oru_version, 0);
    BOOST_CHECK_EQUAL(oru_program.size(), 20U);
    BOOST_CHECK_EQUAL(static_cast<unsigned>(oru[0]), 0x00u);
    std::vector<std::vector<unsigned char>> solutions;
    BOOST_CHECK(ClassifyRigorTxout(oru, solutions) == TxoutType::WITNESS_V0_KEYHASH);

    const CScript res = CScript() << RIGOR_PUSH_1 << std::vector<unsigned char>(32, 0xaa);
    int res_version = -1;
    std::vector<unsigned char> res_program;
    BOOST_CHECK(res.IsWitnessProgram(res_version, res_program));
    BOOST_CHECK_EQUAL(res_version, 1);
    BOOST_CHECK_EQUAL(res_program.size(), 32U);
    BOOST_CHECK_EQUAL(static_cast<unsigned>(res[0]), 0x51u);
    solutions.clear();
    BOOST_CHECK(ClassifyRigorTxout(res, solutions) == TxoutType::WITNESS_V1_RES);
}

BOOST_AUTO_TEST_CASE(signature_domains_are_unchanged)
{
    BOOST_CHECK(TagIs(RigorTags::SighashOruV1, "RGRORU01"));
    BOOST_CHECK(TagIs(RigorTags::SighashResV1, "RGRRES01"));
    BOOST_CHECK(TagIs(GetSighashDomainTag(SigVersion::WITNESS_V0), "RGRORU01"));
    BOOST_CHECK(TagIs(GetSighashDomainTag(SigVersion::WITNESS_V1_RES), "RGRRES01"));
}

BOOST_AUTO_TEST_CASE(pow_domain_tag_is_unchanged)
{
    BOOST_CHECK_EQUAL(std::string(rigor_kernel::POW_DOMAIN_TAG, rigor_kernel::POW_DOMAIN_TAG_LEN), "RGRPOW01");
    BOOST_CHECK_EQUAL(rigor_kernel::POW_DOMAIN_TAG_LEN, 8U);
    BOOST_CHECK_EQUAL(rigor_kernel::POW_HEADER_LEN, 80U);
    BOOST_CHECK_EQUAL(rigor_kernel::POW_PREIMAGE_LEN, 88U);
}

BOOST_AUTO_TEST_SUITE_END()
