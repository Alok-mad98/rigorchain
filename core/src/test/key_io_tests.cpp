// Copyright (c) 2011-2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <test/data/key_io_invalid.json.h>
#include <test/data/key_io_valid.json.h>

#include <crypto/ed25519_provider.h>
#include <key.h>
#include <key_io.h>
#include <primitives/transaction.h>
#include <script/predicate_verify.h>
#include <script/rigor_tags.h>
#include <script/script.h>
#include <script/standard.h>
#include <span.h>
#include <test/util/json.h>
#include <test/util/script.h>
#include <test/util/setup_common.h>
#include <util/strencodings.h>

#include <algorithm>
#include <boost/test/unit_test.hpp>

#include <univalue.h>

BOOST_FIXTURE_TEST_SUITE(key_io_tests, BasicTestingSetup)

static bool IsRigorAddressScript(const CScript& script)
{
    int witver{0};
    std::vector<unsigned char> witprog;
    if (!script.IsWitnessProgram(witver, witprog)) return false;
    if (witver == 0 && witprog.size() == WITNESS_V0_KEYHASH_SIZE) return true;
    if (witver == 1 && witprog.size() == WITNESS_V1_RES_SIZE &&
        IsRigorResNativeWitnessProgram(witprog) &&
        ed25519::IsCanonicalOnCurvePubkey(witprog)) {
        return true;
    }
    return false;
}

// Goal: check that parsed keys match test payload
BOOST_AUTO_TEST_CASE(key_io_valid_parse)
{
    UniValue tests = read_json(std::string(json_tests::key_io_valid, json_tests::key_io_valid + sizeof(json_tests::key_io_valid)));
    CKey privkey;
    CTxDestination destination;
    SelectParams(CBaseChainParams::MAIN);

    for (unsigned int idx = 0; idx < tests.size(); idx++) {
        const UniValue& test = tests[idx];
        std::string strTest = test.write();
        if (test.size() < 3) { // Allow for extra stuff (useful for comments)
            BOOST_ERROR("Bad test: " << strTest);
            continue;
        }
        std::string exp_base58string = test[0].get_str();
        const std::vector<std::byte> exp_payload{ParseHex<std::byte>(test[1].get_str())};
        const UniValue &metadata = test[2].get_obj();
        bool isPrivkey = find_value(metadata, "isPrivkey").get_bool();
        const std::string chain = find_value(metadata, "chain").get_str();
        if (chain == CBaseChainParams::SIGNET) continue; // signet chain params are removed
        SelectParams(chain);
        bool try_case_flip = find_value(metadata, "tryCaseFlip").isNull() ? false : find_value(metadata, "tryCaseFlip").get_bool();
        if (isPrivkey) {
            // Must be valid private key
            privkey = DecodeSecret(exp_base58string);
            BOOST_CHECK_MESSAGE(privkey.IsValid(), "!IsValid:" + strTest);
            // ED25519 secret keys are always interpreted as compressed in-memory.
            BOOST_CHECK_MESSAGE(privkey.IsCompressed(), "compressed mismatch:" + strTest);
            BOOST_CHECK_MESSAGE(Span{privkey} == Span{exp_payload}, "key mismatch:" + strTest);

            // Private key must be invalid public key
            destination = DecodeDestination(exp_base58string);
            BOOST_CHECK_MESSAGE(!IsValidDestination(destination), "IsValid privkey as pubkey:" + strTest);
        } else {
            const std::vector<unsigned char> payload_bytes{
                reinterpret_cast<const unsigned char*>(exp_payload.data()),
                reinterpret_cast<const unsigned char*>(exp_payload.data()) + exp_payload.size()};
            CScript exp_script(payload_bytes.begin(), payload_bytes.end());
            destination = DecodeDestination(exp_base58string);
            if (!IsRigorAddressScript(exp_script)) {
                BOOST_CHECK_MESSAGE(!IsValidDestination(destination), "non-RIGOR payload decoded as destination: " + strTest);
                continue;
            }
            CScript script = GetScriptForDestination(destination);
            BOOST_CHECK_MESSAGE(IsValidDestination(destination), "!IsValid:" + strTest);
            BOOST_CHECK_EQUAL(HexStr(script), HexStr(exp_payload));

            // Try flipped case version
            for (char& c : exp_base58string) {
                if (c >= 'a' && c <= 'z') {
                    c = (c - 'a') + 'A';
                } else if (c >= 'A' && c <= 'Z') {
                    c = (c - 'A') + 'a';
                }
            }
            destination = DecodeDestination(exp_base58string);
            BOOST_CHECK_MESSAGE(IsValidDestination(destination) == try_case_flip, "!IsValid case flipped:" + strTest);
            if (IsValidDestination(destination)) {
                script = GetScriptForDestination(destination);
                BOOST_CHECK_EQUAL(HexStr(script), HexStr(exp_payload));
            }

            // Public key must be invalid private key
            privkey = DecodeSecret(exp_base58string);
            BOOST_CHECK_MESSAGE(!privkey.IsValid(), "IsValid pubkey as privkey:" + strTest);
        }
    }
}

// Goal: check that generated keys match test vectors
BOOST_AUTO_TEST_CASE(key_io_valid_gen)
{
    UniValue tests = read_json(std::string(json_tests::key_io_valid, json_tests::key_io_valid + sizeof(json_tests::key_io_valid)));

    for (unsigned int idx = 0; idx < tests.size(); idx++) {
        const UniValue& test = tests[idx];
        std::string strTest = test.write();
        if (test.size() < 3) // Allow for extra stuff (useful for comments)
        {
            BOOST_ERROR("Bad test: " << strTest);
            continue;
        }
        std::string exp_base58string = test[0].get_str();
        std::vector<unsigned char> exp_payload = ParseHex(test[1].get_str());
        const UniValue &metadata = test[2].get_obj();
        bool isPrivkey = find_value(metadata, "isPrivkey").get_bool();
        const std::string chain = find_value(metadata, "chain").get_str();
        if (chain == CBaseChainParams::SIGNET) continue; // signet chain params are removed
        SelectParams(chain);
        if (isPrivkey) {
            CKey key;
            key.Set(exp_payload.begin(), exp_payload.end(), true);
            assert(key.IsValid());
            const std::string encoded = EncodeSecret(key);
            const CKey decoded = DecodeSecret(encoded);
            BOOST_CHECK_MESSAGE(decoded.IsValid(), "roundtrip decode invalid: " + strTest);
            BOOST_CHECK_MESSAGE(decoded.IsCompressed(), "roundtrip compressed mismatch: " + strTest);
            BOOST_CHECK_MESSAGE(std::equal(decoded.begin(), decoded.end(), exp_payload.begin(), exp_payload.end()),
                                "roundtrip key mismatch: " + strTest);
        } else {
            CTxDestination dest;
            CScript exp_script(exp_payload.begin(), exp_payload.end());
            if (!IsRigorAddressScript(exp_script)) {
                // Classification is byte-shape, so a v1/32 that is not a public key
 // still extracts. Encoding is where the curve check lives.
                if (ExtractDestination(exp_script, dest)) {
                    BOOST_CHECK(EncodeDestination(dest).empty());
                    BOOST_CHECK(!IsValidDestination(dest));
                }
                continue;
            }
            BOOST_CHECK(ExtractDestination(exp_script, dest));
            std::string address = EncodeDestination(dest);

            BOOST_CHECK_EQUAL(address, exp_base58string);
        }
    }

    SelectParams(CBaseChainParams::MAIN);
}


// Goal: check that base58 parsing code is robust against a variety of corrupted data
BOOST_AUTO_TEST_CASE(key_io_invalid)
{
    UniValue tests = read_json(std::string(json_tests::key_io_invalid, json_tests::key_io_invalid + sizeof(json_tests::key_io_invalid))); // Negative testcases
    CKey privkey;
    CTxDestination destination;

    for (unsigned int idx = 0; idx < tests.size(); idx++) {
        const UniValue& test = tests[idx];
        std::string strTest = test.write();
        if (test.size() < 1) // Allow for extra stuff (useful for comments)
        {
            BOOST_ERROR("Bad test: " << strTest);
            continue;
        }
        std::string exp_base58string = test[0].get_str();

        // must be invalid as public and as private key
        for (const auto& chain : { CBaseChainParams::MAIN, CBaseChainParams::TESTNET, CBaseChainParams::REGTEST }) {
            SelectParams(chain);
            destination = DecodeDestination(exp_base58string);
            BOOST_CHECK_MESSAGE(!IsValidDestination(destination), "IsValid pubkey in mainnet:" + strTest);
            privkey = DecodeSecret(exp_base58string);
            BOOST_CHECK_MESSAGE(!privkey.IsValid(), "IsValid privkey in mainnet:" + strTest);
        }
    }
}

BOOST_AUTO_TEST_CASE(asset_type_from_destination_is_script_not_variant)
{
    SelectParams(CBaseChainParams::MAIN);

    // Load-bearing: a v0/20 script is ORU, but WitnessUnknown is not a
    // constructible destination (GetScriptForDestination emits nothing).
    WitnessUnknown unk{};
    unk.version = 0;
    unk.length = WITNESS_V0_KEYHASH_SIZE;
    std::fill(unk.program, unk.program + unk.length, static_cast<unsigned char>(0x33));
    const CTxDestination unk_dest{unk};
    BOOST_CHECK(!IsValidDestination(unk_dest));
    BOOST_CHECK(GetScriptForDestination(unk_dest).empty());
    const CScript unk_as_wpkh = CScript() << RIGOR_PUSH_0 << std::vector<unsigned char>(unk.program, unk.program + unk.length);
    BOOST_CHECK_EQUAL(AssetTypeFromScriptPubKey(unk_as_wpkh), ASSET_ORU);
    BOOST_CHECK(!GetAssetTypeFromDestination(unk_dest));

    auto tagged_v1 = [](const std::array<unsigned char, 4>& tag) {
        std::vector<unsigned char> prog(WITNESS_V1_RES_SIZE, 0);
        std::copy(tag.begin(), tag.end(), prog.begin());
        return CTxDestination{WitnessV1Res{XOnlyPubKey{prog}}};
    };
    const CTxDestination cvg2 = tagged_v1(RigorTags::CVG2);
    const CTxDestination res2 = tagged_v1(RigorTags::Res2WirePrefix);
    BOOST_CHECK(!IsValidDestination(cvg2));
    BOOST_CHECK(!IsValidDestination(res2));
    BOOST_CHECK_EQUAL(AssetTypeFromScriptPubKey(GetScriptForDestination(cvg2)), ASSET_INVALID);
    BOOST_CHECK_EQUAL(AssetTypeFromScriptPubKey(GetScriptForDestination(res2)), ASSET_INVALID);
    BOOST_CHECK(!GetAssetTypeFromDestination(cvg2));
    BOOST_CHECK(!GetAssetTypeFromDestination(res2));

    const std::vector<unsigned char> oru_prog(WITNESS_V0_KEYHASH_SIZE, 0x11);
    const CTxDestination oru{WitnessV0KeyHash{uint160{Span{oru_prog}}}};
    BOOST_CHECK(IsValidDestination(oru));
    BOOST_CHECK(GetAssetTypeFromDestination(oru) == ASSET_ORU);

    const std::vector<unsigned char> res_prog = TestEd25519PubKey();
    const CTxDestination res{WitnessV1Res{XOnlyPubKey{Span{res_prog}}}};
    BOOST_CHECK(IsValidDestination(res));
    BOOST_CHECK(GetAssetTypeFromDestination(res) == ASSET_RES);

 // classification is byte-shape, but an address that is not a
    // public key would be unspendable. Encode/decode and IsValidDestination refuse it.
    const std::vector<unsigned char> zeros(WITNESS_V1_RES_SIZE, 0);
    const CTxDestination off_curve{WitnessV1Res{XOnlyPubKey{Span{zeros}}}};
    BOOST_CHECK(!IsValidDestination(off_curve));
    BOOST_CHECK(EncodeDestination(off_curve).empty());
    BOOST_CHECK_EQUAL(AssetTypeFromScriptPubKey(GetScriptForDestination(off_curve)), ASSET_RES);
}

BOOST_AUTO_TEST_SUITE_END()
