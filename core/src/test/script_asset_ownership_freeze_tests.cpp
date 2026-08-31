// Copyright (c) 2026 The RigorChain developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <consensus/res_owner.h>
#include <consensus/tx_check.h>
#include <crypto/ed25519_provider.h>
#include <crypto/sha256.h>
#include <hash.h>
#include <key.h>
#include <policy/policy.h>
#include <primitives/transaction.h>
#include <script/predicate_verify.h>
#include <script/rigor_tags.h>
#include <script/script.h>
#include <script/standard.h>
#include <span.h>
#include <test/data/rigor_script_asset_classifier.json.h>
#include <test/util/json.h>
#include <test/util/script.h>
#include <test/util/setup_common.h>
#include <util/strencodings.h>
#include <wallet/asset_domain.h>

#include <cstring>
#include <string>
#include <vector>

#include <boost/test/unit_test.hpp>

BOOST_FIXTURE_TEST_SUITE(script_asset_ownership_freeze_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(res_witness_program_is_the_pubkey)
{
    CKey key;
    key.MakeNewKey(true);
    const std::vector<unsigned char> pk = key.GetPubKey().GetEd25519Payload();
    BOOST_REQUIRE_EQUAL(pk.size(), ed25519::PUBKEY_SIZE);
    uint256 program;
    BOOST_REQUIRE(ed25519::ResWitnessProgram(pk, Span<unsigned char>{program.begin(), 32}));
    BOOST_CHECK(std::equal(pk.begin(), pk.end(), program.begin()));
    BOOST_CHECK(Hash160(pk) == Hash160(std::vector<unsigned char>(program.begin(), program.end())));
}

BOOST_AUTO_TEST_CASE(sha256_of_pubkey_is_not_the_res_program)
{
    CKey key;
    key.MakeNewKey(true);
    const std::vector<unsigned char> pk = key.GetPubKey().GetEd25519Payload();
    uint256 sha;
    CSHA256().Write(pk.data(), pk.size()).Finalize(sha.begin());
    uint256 program;
    BOOST_REQUIRE(ed25519::ResWitnessProgram(pk, Span<unsigned char>{program.begin(), 32}));
    BOOST_CHECK(program != sha);
}

BOOST_AUTO_TEST_CASE(sha256_program_cannot_be_spent_by_the_hashed_pubkey)
{
    CKey key;
    key.MakeNewKey(true);
    const std::vector<unsigned char> pk = key.GetPubKey().GetEd25519Payload();
    uint256 sha;
    CSHA256().Write(pk.data(), pk.size()).Finalize(sha.begin());
    const std::vector<unsigned char> sha_bytes(sha.begin(), sha.end());
    if (std::memcmp(sha_bytes.data(), RigorTags::CVG2.data(), 4) == 0 ||
        std::memcmp(sha_bytes.data(), RigorTags::Res2WirePrefix.data(), 4) == 0) {
        return;
    }
    const CScript spk = CScript() << RIGOR_PUSH_1 << sha_bytes;
    // Classification is byte-shape only, so SHA256(pk) is a perfectly well-formed
    // RES program. What it is not is *spendable by pk* — that is the point of this
    // test, and it is enforced by the witness/program comparison below rather than
    // by making the classifier inspect the curve (TASK-010 R-2).
    BOOST_CHECK_EQUAL(AssetTypeFromScriptPubKey(spk), ASSET_RES);
    BOOST_CHECK(Consensus::IsAllowedScript(spk));

    std::vector<unsigned char> dummy_sig(ed25519::SIGNATURE_SIZE, 0x11);
    CScriptWitness wit;
    wit.stack.push_back(dummy_sig);
    wit.stack.push_back(pk);
    ScriptError err{SCRIPT_ERR_OK};
    class OkSig : public BaseSignatureChecker
    {
    public:
        bool CheckEd25519Signature(const std::vector<unsigned char>&, const std::vector<unsigned char>&,
                                   const CScript&, SigVersion) const override
        {
            return true;
        }
    };
    BOOST_CHECK(!VerifyRigorPredicate(CScript(), spk, &wit, SCRIPT_VERIFY_WITNESS, OkSig(), &err));
    BOOST_CHECK_EQUAL(err, SCRIPT_ERR_WITNESS_PROGRAM_MISMATCH);
}

BOOST_AUTO_TEST_CASE(mint_paying_hash160_of_sha256_pk_cannot_be_spent_by_pk)
{
    const std::vector<unsigned char> pk(32, 0x11);
    uint256 sha;
    CSHA256().Write(pk.data(), pk.size()).Finalize(sha.begin());
    const uint160 wrong = Hash160(std::vector<unsigned char>(sha.begin(), sha.end()));
    const CScript mint = GetResMintScript(/*maturity_height=*/0, wrong);
    BOOST_CHECK(IsResMintScript(mint));
    BOOST_CHECK(Consensus::CheckResRewardOwner(wrong, mint));
    BOOST_CHECK(!Consensus::CheckResRewardOwner(Hash160(pk), mint));

    std::vector<unsigned char> sig(ed25519::SIGNATURE_SIZE, 0x22);
    const CScript scriptSig = CScript() << sig << pk;
    class MatureOk : public BaseSignatureChecker
    {
    public:
        int GetSpendHeight() const override { return 1; }
        bool CheckEd25519Signature(const std::vector<unsigned char>&, const std::vector<unsigned char>&,
                                   const CScript&, SigVersion) const override
        {
            return true;
        }
    };
    ScriptError err{SCRIPT_ERR_OK};
    BOOST_CHECK(!VerifyRigorPredicate(scriptSig, mint, nullptr, SCRIPT_VERIFY_NONE, MatureOk(), &err));
    BOOST_CHECK_EQUAL(err, SCRIPT_ERR_EQUALVERIFY);
}

BOOST_AUTO_TEST_CASE(unknown_scripts_are_invalid_not_oru_or_none)
{
    BOOST_CHECK_EQUAL(AssetTypeFromScriptPubKey(CScript()), ASSET_INVALID);
    BOOST_CHECK_EQUAL(AssetTypeFromScriptPubKey(CScript() << RIGOR_PUSH_1), ASSET_INVALID);
    BOOST_CHECK_EQUAL(AssetTypeFromScriptPubKey(CScript() << RIGOR_PUSH_0 << std::vector<unsigned char>(32, 0x01)), ASSET_INVALID);
    const CScript p2pkh = CScript() << RIGOR_RESERVED_LEGACY_DUP << RIGOR_RESERVED_LEGACY_HASH160 << std::vector<unsigned char>(20, 0x11)
                                    << RIGOR_RESERVED_LEGACY_EQUALVERIFY << RIGOR_RESERVED_LEGACY_CHECKSIG;
    BOOST_CHECK_EQUAL(AssetTypeFromScriptPubKey(p2pkh), ASSET_INVALID);
    BOOST_CHECK(!Consensus::IsAllowedScript(p2pkh));

    std::vector<unsigned char> cvg2_prog(32, 0);
    std::memcpy(cvg2_prog.data(), RigorTags::CVG2.data(), 4);
    const CScript cvg2_spk = CScript() << RIGOR_PUSH_1 << cvg2_prog;
    BOOST_CHECK_EQUAL(AssetTypeFromScriptPubKey(cvg2_spk), ASSET_INVALID);

    std::vector<unsigned char> res2_prog(32, 0);
    std::memcpy(res2_prog.data(), RigorTags::Res2WirePrefix.data(), 4);
    const CScript res2_spk = CScript() << RIGOR_PUSH_1 << res2_prog;
    BOOST_CHECK_EQUAL(AssetTypeFromScriptPubKey(res2_spk), ASSET_INVALID);

    const CScript opret = CScript() << RIGOR_MARKER_RETURN << std::vector<unsigned char>{'x'};
    BOOST_CHECK_EQUAL(AssetTypeFromScriptPubKey(opret), ASSET_NONE);
}

BOOST_AUTO_TEST_CASE(constructible_and_allowed_are_derived_from_asset_type)
{
    const CScript corpus[] = {
        CScript(),
        CScript() << RIGOR_PUSH_1,
        CScript() << RIGOR_RESERVED_LEGACY_DUP << RIGOR_RESERVED_LEGACY_HASH160 << std::vector<unsigned char>(20, 0x11) << RIGOR_RESERVED_LEGACY_EQUALVERIFY << RIGOR_RESERVED_LEGACY_CHECKSIG,
        CScript() << RIGOR_PUSH_0 << std::vector<unsigned char>(32, 0x01),
        CScript() << RIGOR_PUSH_0 << std::vector<unsigned char>(20, 0x22),
        TestNativeResScript(),
        CScript() << RIGOR_MARKER_RETURN,
        CScript() << RIGOR_MARKER_RETURN << std::vector<unsigned char>{'x'},
        GetConvergeScript(uint160()),
        GetResMintScript(/*maturity_height=*/0, uint160()),
    };
    for (const CScript& s : corpus) {
        const AssetType a = AssetTypeFromScriptPubKey(s);
        BOOST_CHECK_EQUAL(IsRigorConstructibleScript(s), a != ASSET_INVALID);
        BOOST_CHECK_EQUAL(Consensus::IsAllowedScript(s), a == ASSET_ORU || a == ASSET_RES);
    }
}

BOOST_AUTO_TEST_CASE(script_asset_classifier_golden_vectors)
{
    UniValue root = read_json(std::string(json_tests::rigor_script_asset_classifier,
                                          json_tests::rigor_script_asset_classifier + sizeof(json_tests::rigor_script_asset_classifier)));
    BOOST_REQUIRE(root.isArray() && root.size() == 1);
    const UniValue& vectors = root[0]["vectors"];
    BOOST_REQUIRE(vectors.isArray());
    BOOST_REQUIRE_EQUAL(vectors.size(), 11U);

    auto parse_asset = [](const std::string& s) {
        if (s == "ORU") return ASSET_ORU;
        if (s == "RES") return ASSET_RES;
        if (s == "NONE") return ASSET_NONE;
        BOOST_REQUIRE_EQUAL(s, "INVALID");
        return ASSET_INVALID;
    };

    for (size_t i = 0; i < vectors.size(); ++i) {
        const UniValue& v = vectors[i];
        const std::vector<unsigned char> raw = ParseHex(v["input"]["script_hex"].get_str());
        const CScript spk(raw.begin(), raw.end());
        const AssetType expect = parse_asset(v["expected"]["asset"].get_str());
        const bool expect_allowed = v["expected"]["is_allowed"].get_bool();
        const bool expect_res = v["expected"]["is_res"].get_bool();
        BOOST_CHECK_MESSAGE(AssetTypeFromScriptPubKey(spk) == expect,
                            v["id"].get_str() + " asset");
        BOOST_CHECK_MESSAGE(Consensus::IsAllowedScript(spk) == expect_allowed,
                            v["id"].get_str() + " allowed");
        BOOST_CHECK_MESSAGE(IsResScript(spk) == expect_res, v["id"].get_str() + " is_res");
        BOOST_CHECK_MESSAGE(IsRigorConstructibleScript(spk) == (expect != ASSET_INVALID),
                            v["id"].get_str() + " live");
        BOOST_CHECK_MESSAGE(Consensus::IsAllowedScript(spk) == (expect == ASSET_ORU || expect == ASSET_RES),
                            v["id"].get_str() + " allowed-from-asset");
    }

    // TASK-010 R-2: the C++ classifier is byte-shape only, exactly like the Coq
    // model (ScriptMintWitness.asset_from_script). An untagged 32-byte v1 program
    // is RES whether or not it decodes to a curve point; a program that is not a
    // public key yields an unspendable output, which relay policy declines to
    // carry (`IsStandard`) and addresses refuse to express (`key_io`), but which
    // consensus classifies on its bytes like everything else.
    const CScript zero_v1 = CScript() << RIGOR_PUSH_1 << std::vector<unsigned char>(32, 0);
    BOOST_CHECK_EQUAL(AssetTypeFromScriptPubKey(zero_v1), ASSET_RES);
    BOOST_CHECK(Consensus::IsAllowedScript(zero_v1));
    BOOST_CHECK(!ed25519::IsCanonicalOnCurvePubkey(std::vector<unsigned char>(32, 0)));
    TxoutType type{};
    BOOST_CHECK(!::IsStandard(zero_v1, MAX_OP_RETURN_RELAY, type));
    BOOST_CHECK_EQUAL(type, TxoutType::WITNESS_V1_RES);
    BOOST_CHECK(::IsStandard(TestNativeResScript(), MAX_OP_RETURN_RELAY, type));
}

BOOST_AUTO_TEST_CASE(wallet_send_uses_script_derive_not_witness_version)
{
    const CScript v0 = CScript() << RIGOR_PUSH_0 << std::vector<unsigned char>(20, 0x22);
    const CScript v1 = TestNativeResScript();
    const CScript converge = GetConvergeScript(uint160());
    BOOST_CHECK(wallet::WalletStrictSendDestinationOk(v0, ASSET_ORU));
    BOOST_CHECK(!wallet::WalletStrictSendDestinationOk(v1, ASSET_ORU));
    BOOST_CHECK(wallet::WalletStrictSendDestinationOk(v1, ASSET_RES));
    BOOST_CHECK(!wallet::WalletStrictSendDestinationOk(v0, ASSET_RES));
    BOOST_CHECK(wallet::WalletStrictSendDestinationOk(converge, ASSET_ORU));
    BOOST_CHECK(!wallet::WalletStrictSendDestinationOk(converge, ASSET_RES));
    const CScript garbage = CScript() << RIGOR_PUSH_1;
    BOOST_CHECK(!wallet::WalletStrictSendDestinationOk(garbage, ASSET_ORU));
    BOOST_CHECK(!wallet::WalletStrictSendDestinationOk(garbage, ASSET_RES));
}

BOOST_AUTO_TEST_SUITE_END()
