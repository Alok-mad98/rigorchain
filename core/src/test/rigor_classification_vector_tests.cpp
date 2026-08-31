// Copyright (c) 2026 The RigorChain developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <test/data/rigor_classification_vectors.json.h>

#include <primitives/transaction.h>
#include <script/predicate_verify.h>
#include <script/script.h>
#include <script/standard.h>
#include <test/util/json.h>
#include <test/util/setup_common.h>
#include <util/strencodings.h>

#include <boost/test/unit_test.hpp>

#include <string>
#include <vector>

BOOST_FIXTURE_TEST_SUITE(rigor_classification_vector_tests, BasicTestingSetup)

static AssetType ParseExpectedAsset(const std::string& s)
{
    if (s == "ORU") return ASSET_ORU;
    if (s == "RES") return ASSET_RES;
    if (s == "NONE") return ASSET_NONE;
    BOOST_REQUIRE_EQUAL(s, "INVALID");
    return ASSET_INVALID;
}

BOOST_AUTO_TEST_CASE(classification_golden_vectors_p0_024)
{
    UniValue root = read_json(std::string(
        json_tests::rigor_classification_vectors,
        json_tests::rigor_classification_vectors + sizeof(json_tests::rigor_classification_vectors)));
    BOOST_REQUIRE(root.isArray());
    BOOST_REQUIRE(root.size() >= 6);

    for (size_t i = 0; i < root.size(); ++i) {
        const UniValue& v = root[i];
        const std::string name = v["name"].get_str();
        const std::vector<unsigned char> raw = ParseHex(v["scriptPubKey"].get_str());
        const CScript spk(raw.begin(), raw.end());
        const AssetType expect = ParseExpectedAsset(v["expected"].get_str());
        BOOST_CHECK_MESSAGE(AssetTypeFromScriptPubKey(spk) == expect, name + " AssetTypeFromScriptPubKey");
        BOOST_CHECK_MESSAGE(ClassifyRigorAsset(spk) == expect, name + " ClassifyRigorAsset");
        BOOST_CHECK_MESSAGE(ClassifyRigorAsset(spk) == AssetTypeFromScriptPubKey(spk),
                            name + " ClassifyRigorAsset == AssetTypeFromScriptPubKey");

        std::vector<std::vector<unsigned char>> sols;
        const TxoutType got_t = ClassifyRigorTxout(spk, sols);
        if (v.exists("txout_type")) {
            BOOST_CHECK_MESSAGE(GetTxnOutputType(got_t) == v["txout_type"].get_str(),
                                name + " ClassifyRigorTxout");
        }
    }
}

static bool ByteIsClassifyGate(unsigned char b)
{
    // Gate set matches ClassifierGate.v / AssetClassification.byte_is_classify_gate.
    // 20 is the PUSHBYTES_20 length prefix (Coq RIGOR_PUSHBYTES_20); not an enum name in C++.
    return b == RIGOR_PUSH_0 || (b >= 1 && b <= 4) || b == 20
        || (b >= RIGOR_PUSH_1 && b <= RIGOR_PUSH_16) || b == RIGOR_MARKER_RETURN;
}

BOOST_AUTO_TEST_CASE(classifier_nongate_first_byte_forall_tails)
{
    const std::vector<std::vector<unsigned char>> tails = {
        {},
        {0x00},
        {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff},
        {0x14, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11,
         0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0xbd},
    };
    for (int b = 0; b < 256; ++b) {
        if (ByteIsClassifyGate(static_cast<unsigned char>(b))) continue;
        for (const auto& tail : tails) {
            std::vector<unsigned char> raw;
            raw.push_back(static_cast<unsigned char>(b));
            raw.insert(raw.end(), tail.begin(), tail.end());
            const CScript spk(raw.begin(), raw.end());
            BOOST_CHECK_EQUAL(AssetTypeFromScriptPubKey(spk), ASSET_INVALID);
        }
    }
}

BOOST_AUTO_TEST_CASE(classifier_length_lt_4_never_res)
{
    BOOST_CHECK_NE(AssetTypeFromScriptPubKey(CScript()), ASSET_RES);
    for (int a = 0; a < 256; ++a) {
        const std::vector<unsigned char> one_raw{static_cast<unsigned char>(a)};
        const CScript one(one_raw.begin(), one_raw.end());
        BOOST_CHECK_NE(AssetTypeFromScriptPubKey(one), ASSET_RES);
        for (int c = 0; c < 256; c += 17) {
            const std::vector<unsigned char> two_raw{
                static_cast<unsigned char>(a), static_cast<unsigned char>(c)};
            const CScript two(two_raw.begin(), two_raw.end());
            BOOST_CHECK_NE(AssetTypeFromScriptPubKey(two), ASSET_RES);
        }
        const std::vector<unsigned char> three_raw{static_cast<unsigned char>(a), 0x00, 0x14};
        const CScript three(three_raw.begin(), three_raw.end());
        BOOST_CHECK_NE(AssetTypeFromScriptPubKey(three), ASSET_RES);
    }
}

BOOST_AUTO_TEST_SUITE_END()
