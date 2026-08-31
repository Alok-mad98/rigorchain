// Copyright (c) 2026 The RigorChain developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <test/data/rigor_sighash_vectors.json.h>

#include <consensus/amount.h>
#include <hash.h>
#include <primitives/transaction.h>
#include <script/predicate_verify.h>
#include <script/script.h>
#include <test/util/json.h>
#include <test/util/setup_common.h>
#include <test/util/sighash_preimage.h>
#include <uint256.h>
#include <util/strencodings.h>

#include <boost/test/unit_test.hpp>

#include <cstring>
#include <string>
#include <vector>

BOOST_FIXTURE_TEST_SUITE(rigor_sighash_vector_tests, BasicTestingSetup)

static uint256 BytesToUint256(const std::vector<unsigned char>& b)
{
    BOOST_REQUIRE_EQUAL(b.size(), 32U);
    uint256 out;
    std::memcpy(out.begin(), b.data(), 32);
    return out;
}

static SigVersion ParseSigVersion(const std::string& s)
{
    if (s == "WITNESS_V0") return SigVersion::WITNESS_V0;
    if (s == "WITNESS_V1_RES") return SigVersion::WITNESS_V1_RES;
    BOOST_REQUIRE_EQUAL(s, "RES_MINT");
    return SigVersion::RES_MINT;
}

static CMutableTransaction TxFromVector(const UniValue& v)
{
    CMutableTransaction tx;
    tx.nVersion = v["nVersion"].getInt<int32_t>();
    tx.nLockTime = v["nLockTime"].getInt<uint32_t>();
    const UniValue& vin = v["vin"];
    BOOST_REQUIRE(vin.isArray());
    for (size_t i = 0; i < vin.size(); ++i) {
        CTxIn in;
        in.prevout.hash = BytesToUint256(ParseHex(vin[i]["hash"].get_str()));
        in.prevout.n = vin[i]["n"].getInt<uint32_t>();
        in.nSequence = vin[i]["nSequence"].getInt<uint32_t>();
        tx.vin.push_back(std::move(in));
    }
    const UniValue& vout = v["vout"];
    BOOST_REQUIRE(vout.isArray());
    for (size_t i = 0; i < vout.size(); ++i) {
        CTxOut out;
        out.nValue = vout[i]["value"].getInt<CAmount>();
        const std::vector<unsigned char> spk = ParseHex(vout[i]["scriptPubKey"].get_str());
        out.scriptPubKey = CScript(spk.begin(), spk.end());
        tx.vout.push_back(std::move(out));
    }
    return tx;
}

BOOST_AUTO_TEST_CASE(sighash_golden_vectors_p0_024)
{
    UniValue root = read_json(std::string(
        json_tests::rigor_sighash_vectors,
        json_tests::rigor_sighash_vectors + sizeof(json_tests::rigor_sighash_vectors)));
    BOOST_REQUIRE(root.isArray());
    BOOST_REQUIRE_EQUAL(root.size(), 3U);

    for (size_t i = 0; i < root.size(); ++i) {
        const UniValue& v = root[i];
        const std::string asset = v["asset"].get_str();
        const CMutableTransaction tx = TxFromVector(v);
        const std::vector<unsigned char> script_raw = ParseHex(v["script"].get_str());
        const CScript spent(script_raw.begin(), script_raw.end());
        const CAmount amount = std::stoll(v["amount"].get_str());
        const unsigned int nIn = v["nIn"].getInt<uint32_t>();
        const uint256 chain_id = BytesToUint256(ParseHex(v["chain_id"].get_str()));
        const SigVersion sigversion = ParseSigVersion(v["sigversion"].get_str());
        const uint256 live = SignatureHash(spent, tx, nIn, amount, sigversion, chain_id);
        const uint256 expected = BytesToUint256(ParseHex(v["expected_hash"].get_str()));
        const std::vector<unsigned char> preimage = ParseHex(v["preimage"].get_str());
        const uint256 hashed = Hash(preimage);
        BOOST_CHECK_MESSAGE(live == expected, asset + " SignatureHash != expected_hash");
        BOOST_CHECK_MESSAGE(hashed == expected, asset + " SHA256d(preimage) != expected_hash");
        BOOST_CHECK_MESSAGE(hashed == live, asset + " SHA256d(preimage) != SignatureHash");
        BOOST_REQUIRE_GE(preimage.size(), 8U);
        BOOST_CHECK_EQUAL(std::string(preimage.begin(), preimage.begin() + 8), v["tag"].get_str());
        const std::vector<unsigned char> rebuilt =
            ReconstructSighashPreimage(spent, tx, nIn, amount, sigversion, chain_id);
        BOOST_CHECK_MESSAGE(rebuilt == preimage, asset + " ReconstructSighashPreimage != golden layout");
    }
}

BOOST_AUTO_TEST_CASE(sighash_layout_extra_vin_matches_signaturehash)
{
    CMutableTransaction tx;
    tx.nVersion = 2;
    tx.nLockTime = 7;
    tx.vin.resize(3);
    for (unsigned i = 0; i < tx.vin.size(); ++i) {
        tx.vin[i].prevout.hash.SetHex(std::string(64, "ab"[i % 2]));
        tx.vin[i].prevout.n = i;
        tx.vin[i].nSequence = 1 + i;
    }
    const CScript oru = CScript() << RIGOR_PUSH_0 << std::vector<unsigned char>(20, 0x33);
    tx.vout.resize(2);
    tx.vout[0].nValue = 50 * COIN;
    tx.vout[0].scriptPubKey = oru;
    tx.vout[1].nValue = 1;
    tx.vout[1].scriptPubKey = oru;
    uint256 chain_id;
    chain_id.SetHex(std::string(64, '1'));
    const std::vector<unsigned char> pre =
        ReconstructSighashPreimage(oru, tx, 2, 9 * COIN, SigVersion::WITNESS_V0, chain_id);
    BOOST_CHECK(SignatureHash(oru, tx, 2, 9 * COIN, SigVersion::WITNESS_V0, chain_id) == Hash(pre));
}

BOOST_AUTO_TEST_SUITE_END()
