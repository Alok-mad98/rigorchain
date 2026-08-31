// Copyright (c) 2013-2022 The Bitcoin Core developers
// Copyright (c) 2026 The RigorChain developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <test/data/rigor_sighash_p0_018.json.h>

#include <consensus/amount.h>
#include <consensus/tx_check.h>
#include <crypto/ed25519_provider.h>
#include <hash.h>
#include <key.h>
#include <script/predicate_verify.h>
#include <script/rigor_tags.h>
#include <script/script.h>
#include <script/standard.h>
#include <span.h>
#include <streams.h>
#include <test/util/sighash_preimage.h>
#include <test/util/json.h>
#include <test/util/random.h>
#include <test/util/script.h>
#include <test/util/setup_common.h>
#include <uint256.h>
#include <util/strencodings.h>

#include <boost/test/unit_test.hpp>

#include <array>
#include <cstring>
#include <limits>
#include <string>
#include <vector>

void static RandomScript(CScript &script) {
    static const opcodetype oplist[] = {RIGOR_PUSH_0, RIGOR_PUSH_1, RIGOR_PUSH_2, RIGOR_PUSH_3, RIGOR_RESERVED_LEGACY_CHECKSIG, RIGOR_RESERVED_LEGACY_IF, RIGOR_RESERVED_LEGACY_VERIF, RIGOR_MARKER_RETURN, RIGOR_RESERVED_LEGACY_CODESEPARATOR};
    script = CScript();
    int ops = (InsecureRandRange(10));
    for (int i=0; i<ops; i++)
        script << oplist[InsecureRandRange(std::size(oplist))];
}

void static RandomTransaction(CMutableTransaction& tx)
{
    tx.nVersion = int(InsecureRand32());
    tx.vin.clear();
    tx.vout.clear();
    tx.nLockTime = (InsecureRandBool()) ? InsecureRand32() : 0;
    int ins = (InsecureRandBits(2)) + 1;
    int outs = (InsecureRandBits(2)) + 1;
    for (int in = 0; in < ins; in++) {
        tx.vin.push_back(CTxIn());
        CTxIn &txin = tx.vin.back();
        txin.prevout.hash = InsecureRand256();
        txin.prevout.n = InsecureRandBits(2);
        RandomScript(txin.scriptSig);
        txin.nSequence = (InsecureRandBool()) ? InsecureRand32() : std::numeric_limits<uint32_t>::max();
    }
    for (int out = 0; out < outs; out++) {
        tx.vout.push_back(CTxOut());
        CTxOut &txout = tx.vout.back();
        txout.nValue = InsecureRandMoneyAmount();
        RandomScript(txout.scriptPubKey);
    }
}

static uint256 BytesToUint256(const std::vector<unsigned char>& b)
{
    BOOST_REQUIRE_EQUAL(b.size(), 32U);
    uint256 out;
    std::memcpy(out.begin(), b.data(), 32);
    return out;
}

static uint256 FillUint256(unsigned char v)
{
    uint256 out;
    std::memset(out.begin(), v, 32);
    return out;
}

static uint256 HashPreimage(const std::vector<unsigned char>& preimage)
{
    uint256 out;
    CHash256().Write(preimage).Finalize(out);
    return out;
}

enum class GoldenPath { ORU, RES, MINT };

struct GoldenFixture {
    CMutableTransaction tx;
    CScript spent;
    CAmount amount{5'000'000'000};
    uint256 chain_id{FillUint256(0x11)};
    SigVersion sigversion{SigVersion::WITNESS_V0};
};

static CScript GoldenOruScript()
{
    return CScript() << RIGOR_PUSH_0 << std::vector<unsigned char>(20, 0x33);
}

static CScript GoldenResScript()
{
    return CScript() << RIGOR_PUSH_1 << std::vector<unsigned char>(32, 0x44);
}

static CScript GoldenMintScript()
{
    uint160 pkh;
    std::memset(pkh.begin(), 0x55, 20);
    return GetResMintScript(100, pkh);
}

static GoldenFixture MakeGolden(GoldenPath path)
{
    GoldenFixture f;
    f.tx.nVersion = 2;
    f.tx.nLockTime = 500000000;
    f.tx.vin.resize(2);
    f.tx.vin[0].prevout.hash = FillUint256(0x22);
    f.tx.vin[0].prevout.n = 1;
    f.tx.vin[0].nSequence = 0xfffffffe;
    f.tx.vin[1].prevout.hash = FillUint256(0xaa);
    f.tx.vin[1].prevout.n = 2;
    f.tx.vin[1].nSequence = 0x00000001;
    const CScript oru = GoldenOruScript();
    const CScript res = GoldenResScript();
    const CScript mint = GoldenMintScript();
    f.tx.vout.resize(2);
    f.tx.vout[0].nValue = 4'999'999'999;
    f.tx.vout[1].nValue = 1;
    switch (path) {
    case GoldenPath::ORU:
        f.spent = oru;
        f.sigversion = SigVersion::WITNESS_V0;
        f.tx.vout[0].scriptPubKey = oru;
        f.tx.vout[1].scriptPubKey = oru;
        break;
    case GoldenPath::RES:
        f.spent = res;
        f.sigversion = SigVersion::WITNESS_V1_RES;
        f.tx.vout[0].scriptPubKey = res;
        f.tx.vout[1].scriptPubKey = res;
        break;
    case GoldenPath::MINT:
        f.spent = mint;
        f.sigversion = SigVersion::RES_MINT;
        f.tx.vout[0].scriptPubKey = mint;
        f.tx.vout[1].scriptPubKey = oru;
        break;
    }
    return f;
}

BOOST_FIXTURE_TEST_SUITE(sighash_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(sighash_is_unique)
{
    CMutableTransaction txTo;
    RandomTransaction(txTo);
    CScript scriptCode;
    RandomScript(scriptCode);
    const unsigned int nIn = 0;
    BOOST_REQUIRE(nIn < txTo.vin.size());

    const uint256 h1 = SignatureHash(scriptCode, txTo, nIn, 1 * COIN, SigVersion::WITNESS_V0, TestSighashChainId());
    const uint256 h2 = SignatureHash(scriptCode, txTo, nIn, 1 * COIN, SigVersion::WITNESS_V0, TestSighashChainId());
    BOOST_CHECK(h1 == h2);

    BOOST_REQUIRE(!txTo.vout.empty());
    txTo.vout[0].nValue += 1;
    const uint256 h3 = SignatureHash(scriptCode, txTo, nIn, 1 * COIN, SigVersion::WITNESS_V0, TestSighashChainId());
    BOOST_CHECK(h1 != h3);

    const uint256 h_res = SignatureHash(scriptCode, txTo, nIn, 1 * COIN, SigVersion::WITNESS_V1_RES, TestSighashChainId());
    BOOST_CHECK(h3 != h_res);
    const uint256 h_mint = SignatureHash(scriptCode, txTo, nIn, 1 * COIN, SigVersion::RES_MINT, TestSighashChainId());
    BOOST_CHECK(h1 != h_mint && h_res != h_mint);
    BOOST_CHECK(h3 != SignatureHash(scriptCode, txTo, nIn, 1 * COIN, SigVersion::WITNESS_V0, uint256::ONE));
}

BOOST_AUTO_TEST_CASE(sighash_pairwise_domain_tags)
{
    const GoldenFixture f = MakeGolden(GoldenPath::ORU);
    const uint256 h_oru = SignatureHash(f.spent, f.tx, 0, f.amount, SigVersion::WITNESS_V0, f.chain_id);
    const uint256 h_res = SignatureHash(f.spent, f.tx, 0, f.amount, SigVersion::WITNESS_V1_RES, f.chain_id);
    const uint256 h_mint = SignatureHash(f.spent, f.tx, 0, f.amount, SigVersion::RES_MINT, f.chain_id);
    BOOST_CHECK(h_oru != h_res);
    BOOST_CHECK(h_oru != h_mint);
    BOOST_CHECK(h_res != h_mint);
}

BOOST_AUTO_TEST_CASE(sighash_cross_network_replay)
{
    const GoldenFixture f = MakeGolden(GoldenPath::ORU);
    const uint256 mainnet = SignatureHash(f.spent, f.tx, 0, f.amount, SigVersion::WITNESS_V0, f.chain_id);
    const uint256 other = SignatureHash(f.spent, f.tx, 0, f.amount, SigVersion::WITNESS_V0, uint256::ONE);
    BOOST_CHECK(mainnet != other);

    std::vector<unsigned char> seed(ed25519::SEED_SIZE, 0x07);
    std::vector<unsigned char> pk(ed25519::PUBKEY_SIZE);
    std::vector<unsigned char> sk(ed25519::SECRETKEY_SIZE);
    BOOST_REQUIRE(ed25519::SeedToKeypair(seed, pk, sk));
    std::vector<unsigned char> sig(ed25519::SIGNATURE_SIZE);
    BOOST_REQUIRE(ed25519::SignDetached(Span{mainnet.begin(), 32}, sk, Span{sig.data(), sig.size()}));

    PrecomputedTransactionData txdata(f.tx);
    MutableTransactionSignatureChecker ok_checker(&f.tx, 0, f.amount, txdata, MissingDataBehavior::ASSERT_FAIL, f.chain_id);
    BOOST_CHECK(ok_checker.CheckEd25519Signature(sig, pk, f.spent, SigVersion::WITNESS_V0));

    PrecomputedTransactionData other_data(f.tx);
    MutableTransactionSignatureChecker bad_checker(&f.tx, 0, f.amount, other_data, MissingDataBehavior::ASSERT_FAIL, uint256::ONE);
    BOOST_CHECK(!bad_checker.CheckEd25519Signature(sig, pk, f.spent, SigVersion::WITNESS_V0));

    MutableTransactionSignatureChecker zero_checker(&f.tx, 0, f.amount, txdata, MissingDataBehavior::ASSERT_FAIL, uint256::ZERO);
    BOOST_CHECK(!zero_checker.CheckEd25519Signature(sig, pk, f.spent, SigVersion::WITNESS_V0));
}

BOOST_AUTO_TEST_CASE(sighash_field_sensitivity)
{
    const GoldenFixture base = MakeGolden(GoldenPath::ORU);
    const uint256 h0 = SignatureHash(base.spent, base.tx, 0, base.amount, base.sigversion, base.chain_id);

    auto differs = [&](const GoldenFixture& m) {
        return SignatureHash(m.spent, m.tx, 0, m.amount, m.sigversion, m.chain_id) != h0;
    };

    {
        GoldenFixture m = base;
        m.tx.nVersion += 1;
        BOOST_CHECK(differs(m));
    }
    {
        GoldenFixture m = base;
        m.tx.vin[0].prevout.hash = FillUint256(0x23);
        BOOST_CHECK(differs(m));
    }
    {
        GoldenFixture m = base;
        m.tx.vin[1].prevout.n += 1;
        BOOST_CHECK(differs(m));
    }
    {
        GoldenFixture m = base;
        m.tx.vin[0].nSequence -= 1;
        BOOST_CHECK(differs(m));
    }
    {
        GoldenFixture m = base;
        m.tx.vin[1].nSequence += 1;
        BOOST_CHECK(differs(m));
    }
    {
        GoldenFixture m = base;
        m.tx.vout[0].nValue -= 1;
        BOOST_CHECK(differs(m));
    }
    {
        GoldenFixture m = base;
        m.tx.vout[1].scriptPubKey = GoldenResScript();
        BOOST_CHECK(differs(m));
    }
    {
        GoldenFixture m = base;
        m.amount -= 1;
        BOOST_CHECK(differs(m));
    }
    {
        GoldenFixture m = base;
        m.spent = GoldenResScript();
        BOOST_CHECK(differs(m));
    }
    {
        GoldenFixture m = base;
        m.tx.nLockTime += 1;
        BOOST_CHECK(differs(m));
    }
    {
        GoldenFixture m = base;
        m.chain_id = FillUint256(0x12);
        BOOST_CHECK(differs(m));
    }
}

BOOST_AUTO_TEST_CASE(sighash_cross_asset_binding)
{
    std::vector<unsigned char> seed(ed25519::SEED_SIZE, 0x09);
    std::vector<unsigned char> pk(ed25519::PUBKEY_SIZE);
    std::vector<unsigned char> sk(ed25519::SECRETKEY_SIZE);
    BOOST_REQUIRE(ed25519::SeedToKeypair(seed, pk, sk));

    const CScript oru = CScript() << RIGOR_PUSH_0 << ToByteVector(Hash160(pk));
    uint256 res_prog;
    BOOST_REQUIRE(ed25519::ResWitnessProgram(pk, Span<unsigned char>{res_prog.begin(), 32}));
    const CScript res = CScript() << RIGOR_PUSH_1 << std::vector<unsigned char>(res_prog.begin(), res_prog.end());

    CMutableTransaction tx;
    tx.nVersion = 2;
    tx.vin.resize(2);
    tx.vin[0].prevout.hash = FillUint256(0x01);
    tx.vin[0].prevout.n = 0;
    tx.vin[0].nSequence = CTxIn::SEQUENCE_FINAL;
    tx.vin[1].prevout.hash = FillUint256(0x02);
    tx.vin[1].prevout.n = 0;
    tx.vin[1].nSequence = CTxIn::SEQUENCE_FINAL;
    tx.vout.resize(1);
    tx.vout[0].nValue = COIN;
    tx.vout[0].scriptPubKey = oru;
    const CAmount amount = COIN;
    const uint256 chain_id = FillUint256(0x11);

    const uint256 h_oru = SignatureHash(oru, tx, 0, amount, SigVersion::WITNESS_V0, chain_id);
    const uint256 h_res = SignatureHash(res, tx, 1, amount, SigVersion::WITNESS_V1_RES, chain_id);
    BOOST_CHECK(h_oru != h_res);

    std::vector<unsigned char> sig_oru(ed25519::SIGNATURE_SIZE);
    std::vector<unsigned char> sig_res(ed25519::SIGNATURE_SIZE);
    BOOST_REQUIRE(ed25519::SignDetached(Span{h_oru.begin(), 32}, sk, Span{sig_oru.data(), sig_oru.size()}));
    BOOST_REQUIRE(ed25519::SignDetached(Span{h_res.begin(), 32}, sk, Span{sig_res.data(), sig_res.size()}));

    PrecomputedTransactionData txdata(tx);
    MutableTransactionSignatureChecker checker0(&tx, 0, amount, txdata, MissingDataBehavior::ASSERT_FAIL, chain_id);
    MutableTransactionSignatureChecker checker1(&tx, 1, amount, txdata, MissingDataBehavior::ASSERT_FAIL, chain_id);

    BOOST_CHECK(checker0.CheckEd25519Signature(sig_oru, pk, oru, SigVersion::WITNESS_V0));
    BOOST_CHECK(!checker0.CheckEd25519Signature(sig_oru, pk, res, SigVersion::WITNESS_V1_RES));
    BOOST_CHECK(!checker1.CheckEd25519Signature(sig_oru, pk, res, SigVersion::WITNESS_V1_RES));
    BOOST_CHECK(checker1.CheckEd25519Signature(sig_res, pk, res, SigVersion::WITNESS_V1_RES));
    BOOST_CHECK(!checker1.CheckEd25519Signature(sig_res, pk, oru, SigVersion::WITNESS_V0));
}

BOOST_AUTO_TEST_CASE(sighash_golden_vectors_p0_018)
{
    UniValue root = read_json(std::string(json_tests::rigor_sighash_p0_018,
                                          json_tests::rigor_sighash_p0_018 + sizeof(json_tests::rigor_sighash_p0_018)));
    BOOST_REQUIRE(root.isArray() && root.size() == 1);
    const UniValue& vectors = root[0]["vectors"];
    BOOST_REQUIRE(vectors.isArray());
    BOOST_REQUIRE_EQUAL(vectors.size(), 3U);

    const std::array<GoldenPath, 3> paths{GoldenPath::ORU, GoldenPath::RES, GoldenPath::MINT};
    for (size_t i = 0; i < paths.size(); ++i) {
        const GoldenFixture f = MakeGolden(paths[i]);
        const std::vector<unsigned char> preimage = ReconstructSighashPreimage(f.spent, f.tx, 0, f.amount, f.sigversion, f.chain_id);
        const uint256 reconstructed = HashPreimage(preimage);
        const uint256 live = SignatureHash(f.spent, f.tx, 0, f.amount, f.sigversion, f.chain_id);
        BOOST_CHECK_MESSAGE(reconstructed == live, "independent preimage hash must match SignatureHash");

        const UniValue& v = vectors[i];
        const auto expected_pre = ParseHex(v["preimage"].get_str());
        const uint256 expected_digest = BytesToUint256(ParseHex(v["digest"].get_str()));
        BOOST_CHECK_EQUAL(HexStr(preimage), HexStr(expected_pre));
        BOOST_CHECK(live == expected_digest);
    }
}

BOOST_AUTO_TEST_CASE(classify_rigor_txout_corpus_p0_018)
{
    const CScript oru = GoldenOruScript();
    const CScript res = TestNativeResScript();
    const CScript mint = GoldenMintScript();
    uint160 cvg_hash;
    std::memset(cvg_hash.begin(), 0x66, 20);
    const CScript converge = GetConvergeScript(cvg_hash);
    const CScript opreturn = CScript() << RIGOR_MARKER_RETURN << std::vector<unsigned char>{'x'};
    const CScript p2pkh = CScript() << RIGOR_RESERVED_LEGACY_DUP << RIGOR_RESERVED_LEGACY_HASH160 << std::vector<unsigned char>(20, 0x11) << RIGOR_RESERVED_LEGACY_EQUALVERIFY << RIGOR_RESERVED_LEGACY_CHECKSIG;
    const CScript p2wsh = CScript() << RIGOR_PUSH_0 << std::vector<unsigned char>(32, 0x77);
    CScript cvg2 = CScript() << RIGOR_PUSH_1;
    std::vector<unsigned char> cvg2_prog(32, 0x00);
    cvg2_prog[0] = 'C'; cvg2_prog[1] = 'V'; cvg2_prog[2] = 'G'; cvg2_prog[3] = '2';
    cvg2 << cvg2_prog;
    CScript res2 = CScript() << RIGOR_PUSH_1;
    std::vector<unsigned char> res2_prog(32, 0x00);
    res2_prog[0] = 'R'; res2_prog[1] = 'E'; res2_prog[2] = 'S'; res2_prog[3] = '2';
    res2 << res2_prog;
    const CScript v1_20 = CScript() << RIGOR_PUSH_1 << std::vector<unsigned char>(20, 0x88);

    auto check = [](const CScript& spk, TxoutType expect_t, AssetType expect_a, bool expect_allowed) {
        std::vector<std::vector<unsigned char>> sols;
        BOOST_CHECK_EQUAL(ClassifyRigorTxout(spk, sols), expect_t);
        BOOST_CHECK_EQUAL(AssetTypeFromScriptPubKey(spk), expect_a);
        BOOST_CHECK_EQUAL(Consensus::IsAllowedScript(spk), expect_allowed);
        BOOST_CHECK_EQUAL(IsResScript(spk), expect_t == TxoutType::WITNESS_V1_RES || expect_t == TxoutType::RES_MINT);
    };

    check(oru, TxoutType::WITNESS_V0_KEYHASH, ASSET_ORU, true);
    check(res, TxoutType::WITNESS_V1_RES, ASSET_RES, true);
    check(mint, TxoutType::RES_MINT, ASSET_RES, true);
    check(converge, TxoutType::CONVERGE, ASSET_ORU, true);
    check(opreturn, TxoutType::NULL_DATA, ASSET_NONE, false);
    check(p2pkh, TxoutType::NONSTANDARD, ASSET_INVALID, false);
    check(p2wsh, TxoutType::NONSTANDARD, ASSET_INVALID, false);
    check(cvg2, TxoutType::NONSTANDARD, ASSET_INVALID, false);
    check(res2, TxoutType::NONSTANDARD, ASSET_INVALID, false);
    check(v1_20, TxoutType::NONSTANDARD, ASSET_INVALID, false);
}

BOOST_AUTO_TEST_SUITE_END()
