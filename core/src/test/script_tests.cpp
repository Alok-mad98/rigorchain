// Copyright (c) 2011-2022 The Bitcoin Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <test/data/script_tests.json.h>

#include <core_io.h>
#include <hash.h>
#include <key.h>
#include <rpc/util.h>
#include <script/predicate_verify.h>
#include <script/rigor_tags.h>
#include <script/script.h>
#include <script/script_error.h>
#include <script/sigcache.h>
#include <script/sign.h>
#include <script/signingprovider.h>
#include <script/standard.h>
#include <streams.h>
#include <test/util/json.h>
#include <test/util/random.h>
#include <test/util/setup_common.h>
#include <test/util/transaction_utils.h>
#include <util/fs.h>
#include <util/strencodings.h>
#include <util/system.h>

#include <set>

#include <cassert>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <set>
#include <string>
#include <vector>

#include <boost/test/unit_test.hpp>

#include <univalue.h>

// Uncomment if you want to output updated JSON tests.
// #define UPDATE_JSON_TESTS

unsigned int ParseScriptFlags(std::string strFlags);
std::string FormatScriptFlags(unsigned int flags);

struct ScriptErrorDesc
{
    ScriptError_t err;
    const char *name;
};

static ScriptErrorDesc script_errors[]={
    {SCRIPT_ERR_OK, "OK"},
    {SCRIPT_ERR_UNKNOWN_ERROR, "UNKNOWN_ERROR"},
    {SCRIPT_ERR_EVAL_FALSE, "EVAL_FALSE"},
    {SCRIPT_ERR_OP_RETURN, "RIGOR_MARKER_RETURN"},
    {SCRIPT_ERR_SCRIPT_SIZE, "SCRIPT_SIZE"},
    {SCRIPT_ERR_PUSH_SIZE, "PUSH_SIZE"},
    {SCRIPT_ERR_OP_COUNT, "OP_COUNT"},
    {SCRIPT_ERR_STACK_SIZE, "STACK_SIZE"},
    {SCRIPT_ERR_SIG_COUNT, "SIG_COUNT"},
    {SCRIPT_ERR_PUBKEY_COUNT, "PUBKEY_COUNT"},
    {SCRIPT_ERR_VERIFY, "VERIFY"},
    {SCRIPT_ERR_EQUALVERIFY, "EQUALVERIFY"},
    {SCRIPT_ERR_CHECKMULTISIGVERIFY, "CHECKMULTISIGVERIFY"},
    {SCRIPT_ERR_CHECKSIGVERIFY, "CHECKSIGVERIFY"},
    {SCRIPT_ERR_NUMEQUALVERIFY, "NUMEQUALVERIFY"},
    {SCRIPT_ERR_BAD_OPCODE, "BAD_OPCODE"},
    {SCRIPT_ERR_DISABLED_OPCODE, "DISABLED_OPCODE"},
    {SCRIPT_ERR_INVALID_STACK_OPERATION, "INVALID_STACK_OPERATION"},
    {SCRIPT_ERR_INVALID_ALTSTACK_OPERATION, "INVALID_ALTSTACK_OPERATION"},
    {SCRIPT_ERR_UNBALANCED_CONDITIONAL, "UNBALANCED_CONDITIONAL"},
    {SCRIPT_ERR_NEGATIVE_LOCKTIME, "NEGATIVE_LOCKTIME"},
    {SCRIPT_ERR_UNSATISFIED_LOCKTIME, "UNSATISFIED_LOCKTIME"},
    {SCRIPT_ERR_SIG_HASHTYPE, "SIG_HASHTYPE"},
    {SCRIPT_ERR_SIG_DER, "SIG_DER"},
    {SCRIPT_ERR_MINIMALDATA, "MINIMALDATA"},
    {SCRIPT_ERR_SIG_PUSHONLY, "SIG_PUSHONLY"},
    {SCRIPT_ERR_SIG_HIGH_S, "SIG_HIGH_S"},
    {SCRIPT_ERR_SIG_NULLDUMMY, "SIG_NULLDUMMY"},
    {SCRIPT_ERR_PUBKEYTYPE, "PUBKEYTYPE"},
    {SCRIPT_ERR_CLEANSTACK, "CLEANSTACK"},
    {SCRIPT_ERR_MINIMALIF, "MINIMALIF"},
    {SCRIPT_ERR_SIG_NULLFAIL, "NULLFAIL"},
    {SCRIPT_ERR_DISCOURAGE_UPGRADABLE_NOPS, "DISCOURAGE_UPGRADABLE_NOPS"},
    {SCRIPT_ERR_DISCOURAGE_UPGRADABLE_WITNESS_PROGRAM, "DISCOURAGE_UPGRADABLE_WITNESS_PROGRAM"},
    {SCRIPT_ERR_WITNESS_PROGRAM_WRONG_LENGTH, "WITNESS_PROGRAM_WRONG_LENGTH"},
    {SCRIPT_ERR_WITNESS_PROGRAM_WITNESS_EMPTY, "WITNESS_PROGRAM_WITNESS_EMPTY"},
    {SCRIPT_ERR_WITNESS_PROGRAM_MISMATCH, "WITNESS_PROGRAM_MISMATCH"},
    {SCRIPT_ERR_WITNESS_MALLEATED, "WITNESS_MALLEATED"},
    {SCRIPT_ERR_WITNESS_MALLEATED_P2SH, "WITNESS_MALLEATED_P2SH"},
    {SCRIPT_ERR_WITNESS_UNEXPECTED, "WITNESS_UNEXPECTED"},
    {SCRIPT_ERR_WITNESS_PUBKEYTYPE, "WITNESS_PUBKEYTYPE"},
    {SCRIPT_ERR_OP_CODESEPARATOR, "RIGOR_RESERVED_LEGACY_CODESEPARATOR"},
    {SCRIPT_ERR_SIG_FINDANDDELETE, "SIG_FINDANDDELETE"},
    {SCRIPT_ERR_SECP256K1_DISABLED, "SECP256K1_DISABLED"},
    {SCRIPT_ERR_CONVERGE_UNSPENDABLE, "CONVERGE_UNSPENDABLE"},
    {SCRIPT_ERR_RIGOR_SCRIPT_TYPE, "RIGOR_SCRIPT_TYPE"},
};

static std::string FormatScriptError(ScriptError_t err)
{
    for (const auto& se : script_errors)
        if (se.err == err)
            return se.name;
    BOOST_ERROR("Unknown scripterror enumeration value, update script_errors in script_tests.cpp.");
    return "";
}

static ScriptError_t ParseScriptError(const std::string& name)
{
    for (const auto& se : script_errors)
        if (se.name == name)
            return se.err;
    BOOST_ERROR("Unknown scripterror \"" << name << "\" in test description");
    return SCRIPT_ERR_UNKNOWN_ERROR;
}

BOOST_FIXTURE_TEST_SUITE(script_tests, BasicTestingSetup)

void DoTest(const CScript& scriptPubKey, const CScript& scriptSig, const CScriptWitness& scriptWitness, uint32_t flags, const std::string& message, int scriptError, CAmount nValue = 0)
{
    bool expect = (scriptError == SCRIPT_ERR_OK);
    if (flags & SCRIPT_VERIFY_CLEANSTACK) {
        flags |= 0;
        flags |= SCRIPT_VERIFY_WITNESS;
    }
    ScriptError err;
    const CTransaction txCredit{BuildCreditingTransaction(scriptPubKey, nValue)};
    CMutableTransaction tx = BuildSpendingTransaction(scriptSig, scriptWitness, txCredit);
    // JSON / TestBuilder share chain_id = uint256::ZERO. Not a consensus global.
    PrecomputedTransactionData txdata;
    txdata.Init(tx, {txCredit.vout[0]});
    MutableTransactionSignatureChecker checker(&tx, 0, txCredit.vout[0].nValue, txdata, MissingDataBehavior::ASSERT_FAIL, uint256::ZERO);
    BOOST_CHECK_MESSAGE(VerifyRigorPredicate(scriptSig, scriptPubKey, &scriptWitness, flags, checker, &err) == expect, message);
    BOOST_CHECK_MESSAGE(err == scriptError, FormatScriptError(err) + " where " + FormatScriptError((ScriptError_t)scriptError) + " expected: " + message);

    int witversion;
    std::vector<unsigned char> witprogram;
    const bool is_witness_program = scriptPubKey.IsWitnessProgram(witversion, witprogram);

    // Verify that removing flags from a passing test or adding flags to a failing test does not change the result.
    for (int i = 0; i < 16; ++i) {
        uint32_t extra_flags(InsecureRandBits(16));
        uint32_t combined_flags{expect ? (flags & ~extra_flags) : (flags | extra_flags)};
        // Weed out some invalid flag combinations.
        if (combined_flags & SCRIPT_VERIFY_CLEANSTACK && ~combined_flags & SCRIPT_VERIFY_WITNESS) continue;
        // Witness programs require SCRIPT_VERIFY_WITNESS; dropping it fails, adding it may pass.
        if (is_witness_program) {
            if (expect && (flags & SCRIPT_VERIFY_WITNESS) && !(combined_flags & SCRIPT_VERIFY_WITNESS)) continue;
            if (!expect && !(flags & SCRIPT_VERIFY_WITNESS) && (combined_flags & SCRIPT_VERIFY_WITNESS)) continue;
        }
        BOOST_CHECK_MESSAGE(VerifyRigorPredicate(scriptSig, scriptPubKey, &scriptWitness, combined_flags, MutableTransactionSignatureChecker(&tx, 0, txCredit.vout[0].nValue, txdata, MissingDataBehavior::ASSERT_FAIL, uint256::ZERO), &err) == expect, message + strprintf(" (with flags %x)", combined_flags));
    }
}

namespace
{
const unsigned char vchKey0[32] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1};
const unsigned char vchKey1[32] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0};
const unsigned char vchKey2[32] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0};

struct KeyData
{
    CKey key0, key1, key2;
    CPubKey pubkey0, pubkey1, pubkey2;

    KeyData()
    {
        key0.Set(vchKey0, vchKey0 + 32, true);
        pubkey0 = key0.GetPubKey();
        key1.Set(vchKey1, vchKey1 + 32, true);
        pubkey1 = key1.GetPubKey();
        key2.Set(vchKey2, vchKey2 + 32, true);
        pubkey2 = key2.GetPubKey();
    }
};

/** JSON / DoTest share this chain_id. Not a consensus global. */
static const uint256& ScriptTestChainId()
{
    return uint256::ZERO;
}

class TestBuilder
{
private:
    CScriptWitness scriptWitness;
    CTransactionRef creditTx;
    CMutableTransaction spendTx;
    bool havePush{false};
    std::vector<unsigned char> push;
    std::string comment;
    uint32_t flags;
    int scriptError{SCRIPT_ERR_OK};
    CAmount nValue;
    SigVersion sigversion;

    void DoPush()
    {
        if (havePush) {
            spendTx.vin[0].scriptSig << push;
            havePush = false;
        }
    }

    void DoPush(const std::vector<unsigned char>& data)
    {
        DoPush();
        push = data;
        havePush = true;
    }

public:
    TestBuilder(const CScript& scriptPubKey, const std::string& comment_, uint32_t flags_, CAmount nValue_ = 0, SigVersion sigversion_ = SigVersion::WITNESS_V0)
        : comment(comment_), flags(flags_), nValue(nValue_), sigversion(sigversion_)
    {
        creditTx = MakeTransactionRef(BuildCreditingTransaction(scriptPubKey, nValue));
        spendTx = BuildSpendingTransaction(CScript(), CScriptWitness(), *creditTx);
    }

    TestBuilder& ScriptError(ScriptError_t err)
    {
        scriptError = err;
        return *this;
    }

    TestBuilder& Opcode(const opcodetype& _op)
    {
        DoPush();
        spendTx.vin[0].scriptSig << _op;
        return *this;
    }

    TestBuilder& Num(int num)
    {
        DoPush();
        spendTx.vin[0].scriptSig << num;
        return *this;
    }

    TestBuilder& Push(const std::string& hex)
    {
        DoPush(ParseHex(hex));
        return *this;
    }

    TestBuilder& Push(const std::vector<unsigned char>& data)
    {
        DoPush(data);
        return *this;
    }

    TestBuilder& Push(const CScript& _script)
    {
        DoPush(std::vector<unsigned char>(_script.begin(), _script.end()));
        return *this;
    }

    //! Sign the real scriptPubKey with chain_id = 0. Witness pubkeys are 32-byte Ed25519 payloads.
    TestBuilder& PushSig(const CKey& key)
    {
        uint256 hash = SignatureHash(creditTx->vout[0].scriptPubKey, spendTx, 0, nValue, sigversion, ScriptTestChainId());
        std::vector<unsigned char> vchSig;
        BOOST_CHECK(key.Sign(hash, vchSig));
        DoPush(vchSig);
        return *this;
    }

    TestBuilder& PushWitSig(const CKey& key)
    {
        return PushSig(key).AsWit();
    }

    TestBuilder& PushPub(const CPubKey& pubkey)
    {
        DoPush(pubkey.GetEd25519Payload());
        return *this;
    }

    TestBuilder& PushWitPub(const CPubKey& pubkey)
    {
        return PushPub(pubkey).AsWit();
    }

    TestBuilder& DamagePush(unsigned int pos)
    {
        assert(havePush);
        assert(pos < push.size());
        push[pos] ^= 1;
        return *this;
    }

    TestBuilder& TruncatePush(size_t new_size)
    {
        assert(havePush);
        assert(new_size <= push.size());
        push.resize(new_size);
        return *this;
    }

    TestBuilder& PadPush(size_t new_size, unsigned char fill = 0x01)
    {
        assert(havePush);
        push.resize(new_size, fill);
        return *this;
    }

    TestBuilder& Test()
    {
        TestBuilder copy = *this;
        DoPush();
        DoTest(creditTx->vout[0].scriptPubKey, spendTx.vin[0].scriptSig, scriptWitness, flags, comment, scriptError, nValue);
        *this = copy;
        return *this;
    }

    TestBuilder& AsWit()
    {
        assert(havePush);
        scriptWitness.stack.push_back(push);
        havePush = false;
        return *this;
    }

    UniValue GetJSON()
    {
        DoPush();
        UniValue array(UniValue::VARR);
        if (!scriptWitness.stack.empty() || nValue != 0) {
            UniValue wit(UniValue::VARR);
            for (unsigned i = 0; i < scriptWitness.stack.size(); i++) {
                wit.push_back(HexStr(scriptWitness.stack[i]));
            }
            wit.push_back(ValueFromAmount(nValue));
            array.push_back(wit);
        }
        array.push_back(FormatScript(spendTx.vin[0].scriptSig));
        array.push_back(FormatScript(creditTx->vout[0].scriptPubKey));
        array.push_back(FormatScriptFlags(flags));
        array.push_back(FormatScriptError((ScriptError_t)scriptError));
        array.push_back(comment);
        return array;
    }

    std::string GetComment() const
    {
        return comment;
    }
};

std::string JSONPrettyPrint(const UniValue& univalue)
{
    std::string ret = univalue.write(4);
    size_t pos = 0;
    while ((pos = ret.find(" \n", pos)) != std::string::npos) {
        ret.replace(pos, 2, "\n");
        pos++;
    }
    return ret;
}

CScript P2WPKH(const CPubKey& pubkey)
{
    return GetScriptForDestination(WitnessV0KeyHash(pubkey));
}

CScript NativeRes(const CPubKey& pubkey)
{
    return CScript() << RIGOR_PUSH_1 << ToByteVector(pubkey.GetResWitnessProgram());
}

CScript Mint(const CPubKey& pubkey, int64_t maturity)
{
    return GetResMintScriptForPubKey(maturity, pubkey.GetEd25519Payload());
}
} // namespace

BOOST_AUTO_TEST_CASE(script_build)
{
    const KeyData keys;
    const uint32_t wit_flags = SCRIPT_VERIFY_WITNESS;
    const uint32_t wit_nullfail = wit_flags | SCRIPT_VERIFY_NULLFAIL;
    const CAmount amt = 1 * COIN;

    std::vector<TestBuilder> tests;

    // --- Rejected locks (not a consensus spend path) ---
    tests.push_back(TestBuilder(CScript(), "empty scriptPubKey", wit_flags)
                        .ScriptError(SCRIPT_ERR_RIGOR_SCRIPT_TYPE));
    tests.push_back(TestBuilder(CScript() << RIGOR_PUSH_1, "anyone-can-spend RIGOR_PUSH_1", wit_flags)
                        .ScriptError(SCRIPT_ERR_RIGOR_SCRIPT_TYPE));
    tests.push_back(TestBuilder(CScript() << ToByteVector(keys.pubkey0) << RIGOR_RESERVED_LEGACY_CHECKSIG, "P2PK", wit_flags)
                        .ScriptError(SCRIPT_ERR_RIGOR_SCRIPT_TYPE));
    tests.push_back(TestBuilder(CScript() << RIGOR_RESERVED_LEGACY_DUP << RIGOR_RESERVED_LEGACY_HASH160 << ToByteVector(keys.pubkey0.GetID()) << RIGOR_RESERVED_LEGACY_EQUALVERIFY << RIGOR_RESERVED_LEGACY_CHECKSIG, "P2PKH", wit_flags)
                        .ScriptError(SCRIPT_ERR_RIGOR_SCRIPT_TYPE));
    tests.push_back(TestBuilder(CScript() << RIGOR_RESERVED_LEGACY_HASH160 << ToByteVector(CScriptID(CScript() << RIGOR_PUSH_1)) << RIGOR_RESERVED_LEGACY_EQUAL, "P2SH", wit_flags)
                        .ScriptError(SCRIPT_ERR_RIGOR_SCRIPT_TYPE));
    tests.push_back(TestBuilder(CScript() << RIGOR_PUSH_2 << ToByteVector(keys.pubkey0) << ToByteVector(keys.pubkey1) << RIGOR_PUSH_2 << RIGOR_RESERVED_LEGACY_CHECKMULTISIG,
                                "CHECKMULTISIG", wit_flags)
                        .ScriptError(SCRIPT_ERR_RIGOR_SCRIPT_TYPE));
    tests.push_back(TestBuilder(CScript() << RIGOR_MARKER_RETURN << std::vector<unsigned char>{'x'}, "RIGOR_MARKER_RETURN", wit_flags)
                        .ScriptError(SCRIPT_ERR_RIGOR_SCRIPT_TYPE));
    tests.push_back(TestBuilder(GetConvergeScript(keys.pubkey0.GetEd25519KeyHash()), "RIGOR_MARKER_CONVERGE is unspendable", wit_flags)
                        .ScriptError(SCRIPT_ERR_RIGOR_SCRIPT_TYPE));
    tests.push_back(TestBuilder(CScript() << RIGOR_PUSH_0 << ToByteVector(uint256()), "P2WSH v0/32", wit_flags)
                        .ScriptError(SCRIPT_ERR_WITNESS_PROGRAM_WRONG_LENGTH));
    tests.push_back(TestBuilder(CScript() << RIGOR_PUSH_2 << std::vector<unsigned char>(32, 0x11), "witness v2 unknown", wit_flags)
                        .ScriptError(SCRIPT_ERR_RIGOR_SCRIPT_TYPE));
    tests.push_back(TestBuilder(CScript() << RIGOR_PUSH_1 << std::vector<unsigned char>(20, 0x22), "witness v1/20", wit_flags)
                        .ScriptError(SCRIPT_ERR_RIGOR_SCRIPT_TYPE));
    {
        std::vector<unsigned char> cvg2(32, 0x00);
        std::memcpy(cvg2.data(), RigorTags::CVG2.data(), 4);
        tests.push_back(TestBuilder(CScript() << RIGOR_PUSH_1 << cvg2, "witness v1 CVG2 prefix", wit_flags)
                            .Push(std::vector<unsigned char>(64, 0x01)).AsWit()
                            .PushPub(keys.pubkey0).AsWit()
                            .ScriptError(SCRIPT_ERR_DISCOURAGE_UPGRADABLE_WITNESS_PROGRAM));
        std::vector<unsigned char> res2(32, 0x00);
        std::memcpy(res2.data(), RigorTags::Res2WirePrefix.data(), 4);
        tests.push_back(TestBuilder(CScript() << RIGOR_PUSH_1 << res2, "witness v1 RES2 prefix", wit_flags)
                            .Push(std::vector<unsigned char>(64, 0x01)).AsWit()
                            .PushPub(keys.pubkey0).AsWit()
                            .ScriptError(SCRIPT_ERR_DISCOURAGE_UPGRADABLE_WITNESS_PROGRAM));
    }

    // --- P2WPKH ---
    tests.push_back(TestBuilder(P2WPKH(keys.pubkey0), "P2WPKH", wit_flags, amt)
                        .PushWitSig(keys.key0)
                        .PushWitPub(keys.pubkey0));
    tests.push_back(TestBuilder(P2WPKH(keys.pubkey0), "P2WPKH without WITNESS flag", 0, amt)
                        .PushWitSig(keys.key0)
                        .PushWitPub(keys.pubkey0)
                        .ScriptError(SCRIPT_ERR_RIGOR_SCRIPT_TYPE));
    tests.push_back(TestBuilder(P2WPKH(keys.pubkey0), "P2WPKH nonempty scriptSig", wit_flags, amt)
                        .Num(1)
                        .PushWitSig(keys.key0)
                        .PushWitPub(keys.pubkey0)
                        .ScriptError(SCRIPT_ERR_WITNESS_MALLEATED));
    tests.push_back(TestBuilder(P2WPKH(keys.pubkey0), "P2WPKH empty witness", wit_flags, amt)
                        .ScriptError(SCRIPT_ERR_WITNESS_PROGRAM_MISMATCH));
    tests.push_back(TestBuilder(P2WPKH(keys.pubkey0), "P2WPKH one witness item", wit_flags, amt)
                        .PushWitSig(keys.key0)
                        .ScriptError(SCRIPT_ERR_WITNESS_PROGRAM_MISMATCH));
    tests.push_back(TestBuilder(P2WPKH(keys.pubkey0), "P2WPKH three witness items", wit_flags, amt)
                        .PushWitSig(keys.key0)
                        .PushWitPub(keys.pubkey0)
                        .Push(std::vector<unsigned char>{0x01}).AsWit()
                        .ScriptError(SCRIPT_ERR_WITNESS_PROGRAM_MISMATCH));
    tests.push_back(TestBuilder(P2WPKH(keys.pubkey0), "P2WPKH 63-byte sig", wit_flags, amt)
                        .PushSig(keys.key0).TruncatePush(63).AsWit()
                        .PushWitPub(keys.pubkey0)
                        .ScriptError(SCRIPT_ERR_SIG_DER));
    tests.push_back(TestBuilder(P2WPKH(keys.pubkey0), "P2WPKH 65-byte sig", wit_flags, amt)
                        .PushSig(keys.key0).PadPush(65).AsWit()
                        .PushWitPub(keys.pubkey0)
                        .ScriptError(SCRIPT_ERR_SIG_DER));
    tests.push_back(TestBuilder(P2WPKH(keys.pubkey0), "P2WPKH 66-byte sig", wit_flags, amt)
                        .PushSig(keys.key0).PadPush(66).AsWit()
                        .PushWitPub(keys.pubkey0)
                        .ScriptError(SCRIPT_ERR_SIG_DER));
    tests.push_back(TestBuilder(P2WPKH(keys.pubkey0), "P2WPKH damaged sig", wit_flags, amt)
                        .PushSig(keys.key0).DamagePush(0).AsWit()
                        .PushWitPub(keys.pubkey0)
                        .ScriptError(SCRIPT_ERR_EVAL_FALSE));
    tests.push_back(TestBuilder(P2WPKH(keys.pubkey0), "P2WPKH damaged sig NULLFAIL", wit_nullfail, amt)
                        .PushSig(keys.key0).DamagePush(0).AsWit()
                        .PushWitPub(keys.pubkey0)
                        .ScriptError(SCRIPT_ERR_SIG_NULLFAIL));
    tests.push_back(TestBuilder(P2WPKH(keys.pubkey0), "P2WPKH wrong pubkey", wit_flags, amt)
                        .PushWitSig(keys.key0)
                        .PushWitPub(keys.pubkey1)
                        .ScriptError(SCRIPT_ERR_EQUALVERIFY));
    tests.push_back(TestBuilder(P2WPKH(keys.pubkey0), "P2WPKH 33-byte wrapped pubkey", wit_flags, amt)
                        .PushWitSig(keys.key0)
                        .Push(std::vector<unsigned char>(keys.pubkey0.begin(), keys.pubkey0.end())).AsWit()
                        .ScriptError(SCRIPT_ERR_EQUALVERIFY));
    tests.push_back(TestBuilder(P2WPKH(keys.pubkey0), "P2WPKH wrong key signs", wit_flags, amt)
                        .PushWitSig(keys.key1)
                        .PushWitPub(keys.pubkey0)
                        .ScriptError(SCRIPT_ERR_EVAL_FALSE));

    // --- Native RES v1 ---
    tests.push_back(TestBuilder(NativeRes(keys.pubkey0), "RES v1", wit_flags, amt, SigVersion::WITNESS_V1_RES)
                        .PushWitSig(keys.key0)
                        .PushWitPub(keys.pubkey0));
    tests.push_back(TestBuilder(NativeRes(keys.pubkey0), "RES v1 empty witness", wit_flags, amt, SigVersion::WITNESS_V1_RES)
                        .ScriptError(SCRIPT_ERR_WITNESS_PROGRAM_WITNESS_EMPTY));
    tests.push_back(TestBuilder(NativeRes(keys.pubkey0), "RES v1 one witness item", wit_flags, amt, SigVersion::WITNESS_V1_RES)
                        .PushWitSig(keys.key0)
                        .ScriptError(SCRIPT_ERR_WITNESS_PROGRAM_MISMATCH));
    tests.push_back(TestBuilder(NativeRes(keys.pubkey0), "RES v1 damaged sig", wit_flags, amt, SigVersion::WITNESS_V1_RES)
                        .PushSig(keys.key0).DamagePush(0).AsWit()
                        .PushWitPub(keys.pubkey0)
                        .ScriptError(SCRIPT_ERR_EVAL_FALSE));
    tests.push_back(TestBuilder(NativeRes(keys.pubkey0), "RES v1 63-byte sig", wit_flags, amt, SigVersion::WITNESS_V1_RES)
                        .PushSig(keys.key0).TruncatePush(63).AsWit()
                        .PushWitPub(keys.pubkey0)
                        .ScriptError(SCRIPT_ERR_SIG_DER));
    tests.push_back(TestBuilder(NativeRes(keys.pubkey0), "RES v1 wrong pubkey", wit_flags, amt, SigVersion::WITNESS_V1_RES)
                        .PushWitSig(keys.key0)
                        .PushWitPub(keys.pubkey1)
                        .ScriptError(SCRIPT_ERR_WITNESS_PROGRAM_MISMATCH));
    tests.push_back(TestBuilder(NativeRes(keys.pubkey0), "RES v1 nonempty scriptSig", wit_flags, amt, SigVersion::WITNESS_V1_RES)
                        .Num(0)
                        .PushWitSig(keys.key0)
                        .PushWitPub(keys.pubkey0)
                        .ScriptError(SCRIPT_ERR_WITNESS_MALLEATED));

    // --- RIGOR_MARKER_RES_MINT (DoTest spend height is 0) ---
    tests.push_back(TestBuilder(Mint(keys.pubkey0, 0), "RES mint maturity 0", 0, amt, SigVersion::RES_MINT)
                        .PushSig(keys.key0)
                        .PushPub(keys.pubkey0));
    tests.push_back(TestBuilder(Mint(keys.pubkey0, 100), "RES mint immature", 0, amt, SigVersion::RES_MINT)
                        .PushSig(keys.key0)
                        .PushPub(keys.pubkey0)
                        .ScriptError(SCRIPT_ERR_UNSATISFIED_LOCKTIME));
    tests.push_back(TestBuilder(Mint(keys.pubkey0, 0), "RES mint wrong pubkey", 0, amt, SigVersion::RES_MINT)
                        .PushSig(keys.key0)
                        .PushPub(keys.pubkey1)
                        .ScriptError(SCRIPT_ERR_EQUALVERIFY));
    tests.push_back(TestBuilder(Mint(keys.pubkey0, 0), "RES mint 63-byte sig", 0, amt, SigVersion::RES_MINT)
                        .PushSig(keys.key0).TruncatePush(63)
                        .PushPub(keys.pubkey0)
                        .ScriptError(SCRIPT_ERR_SIG_DER));
    tests.push_back(TestBuilder(Mint(keys.pubkey0, 0), "RES mint damaged sig", 0, amt, SigVersion::RES_MINT)
                        .PushSig(keys.key0).DamagePush(0)
                        .PushPub(keys.pubkey0)
                        .ScriptError(SCRIPT_ERR_EVAL_FALSE));
    tests.push_back(TestBuilder(Mint(keys.pubkey0, 0), "RES mint damaged sig NULLFAIL", SCRIPT_VERIFY_NULLFAIL, amt, SigVersion::RES_MINT)
                        .PushSig(keys.key0).DamagePush(0)
                        .PushPub(keys.pubkey0)
                        .ScriptError(SCRIPT_ERR_SIG_NULLFAIL));
    tests.push_back(TestBuilder(Mint(keys.pubkey0, 0), "RES mint empty scriptSig", 0, amt, SigVersion::RES_MINT)
                        .ScriptError(SCRIPT_ERR_INVALID_STACK_OPERATION));
    tests.push_back(TestBuilder(Mint(keys.pubkey0, 0), "RES mint non-push scriptSig", 0, amt, SigVersion::RES_MINT)
                        .Opcode(RIGOR_RESERVED_LEGACY_CHECKSIG)
                        .ScriptError(SCRIPT_ERR_SIG_PUSHONLY));
    tests.push_back(TestBuilder(Mint(keys.pubkey0, 0), "RES mint unexpected witness", SCRIPT_VERIFY_WITNESS, amt, SigVersion::RES_MINT)
                        .PushSig(keys.key0)
                        .PushPub(keys.pubkey0)
                        .Push(std::vector<unsigned char>{0x01}).AsWit()
                        .ScriptError(SCRIPT_ERR_WITNESS_UNEXPECTED));

    std::set<std::string> tests_set;

    {
        UniValue json_tests = read_json(std::string(json_tests::script_tests, json_tests::script_tests + sizeof(json_tests::script_tests)));

        for (unsigned int idx = 0; idx < json_tests.size(); idx++) {
            const UniValue& tv = json_tests[idx];
            UniValue arr(UniValue::VARR);
            const size_t flags_idx = (tv.size() > 0 && tv[0].isArray()) ? 3 : 2;
            for (size_t i = 0; i < tv.size(); ++i) {
                if (i == flags_idx && tv[i].isStr()) {
                    arr.push_back(FormatScriptFlags(ParseScriptFlags(tv[i].get_str())));
                } else {
                    arr.push_back(tv[i]);
                }
            }
            tests_set.insert(JSONPrettyPrint(arr));
        }
    }

#ifdef UPDATE_JSON_TESTS
    std::string strGen;
#endif
    for (TestBuilder& test : tests) {
        test.Test();
        std::string str = JSONPrettyPrint(test.GetJSON());
#ifdef UPDATE_JSON_TESTS
        strGen += str + ",\n";
#else
        if (tests_set.count(str) == 0) {
            BOOST_CHECK_MESSAGE(false, "Missing auto script_valid test: " + test.GetComment());
        }
#endif
    }

#ifdef UPDATE_JSON_TESTS
    FILE* file = fsbridge::fopen("script_tests.json.gen", "w");
    fputs("[\n", file);
    fputs("[\"RIGOR script_tests.json: consensus spend paths (predicate model).\"],\n", file);
    fputs("[\"Format: [[wit..., amount]?, scriptSig, scriptPubKey, flags, expected_scripterror, comment]\"],\n", file);
    fputs("[\"chain_id is uint256::ZERO (matches DoTest). nLockTime=0, nSequence=max.\"],\n", file);
    fputs("[\"Spendable: witness v0/20 P2WPKH, witness v1/32 native RES (not CVG2/RES2), RIGOR_MARKER_RES_MINT.\"],\n", file);
    fputs("[\"Witness programs require SCRIPT_VERIFY_WITNESS. Other scriptPubKeys fail with RIGOR_SCRIPT_TYPE.\"],\n", file);
    fputs(strGen.c_str(), file);
    fputs("[\"end\"]\n]\n", file);
    fclose(file);
#endif
}

BOOST_AUTO_TEST_CASE(script_json_test)
{
    // Read tests from test/data/script_tests.json
    // Format is an array of arrays
    // Inner arrays are [ ["wit"..., nValue]?, "scriptSig", "scriptPubKey", "flags", "expected_scripterror" ]
    // ... where scriptSig and scriptPubKey are stringified
    // scripts.
    // If a witness is given, then the last value in the array should be the
    // amount (nValue) to use in the crediting tx
    UniValue tests = read_json(std::string(json_tests::script_tests, json_tests::script_tests + sizeof(json_tests::script_tests)));

    for (unsigned int idx = 0; idx < tests.size(); idx++) {
        const UniValue& test = tests[idx];
        std::string strTest = test.write();
        CScriptWitness witness;
        CAmount nValue = 0;
        unsigned int pos = 0;
        if (test.size() > 0 && test[pos].isArray()) {
            unsigned int i=0;
            for (i = 0; i < test[pos].size()-1; i++) {
                witness.stack.push_back(ParseHex(test[pos][i].get_str()));
            }
            nValue = AmountFromValue(test[pos][i]);
            pos++;
        }
        if (test.size() < 4 + pos) // Allow size > 3; extra stuff ignored (useful for comments)
        {
            if (test.size() != 1) {
                BOOST_ERROR("Bad test: " << strTest);
            }
            continue;
        }
        std::string scriptSigString = test[pos++].get_str();
        CScript scriptSig = ParseScript(scriptSigString);
        std::string scriptPubKeyString = test[pos++].get_str();
        CScript scriptPubKey = ParseScript(scriptPubKeyString);
        unsigned int scriptflags = ParseScriptFlags(test[pos++].get_str());
        int scriptError = ParseScriptError(test[pos++].get_str());

        DoTest(scriptPubKey, scriptSig, witness, scriptflags, strTest, scriptError, nValue);
    }
}

BOOST_AUTO_TEST_CASE(script_PushData)
{
    // Check that PUSHDATA1, PUSHDATA2, and PUSHDATA4 create the same value on
    // the stack as the 1-75 opcodes do.
    static const unsigned char direct[] = { 1, 0x5a };
    static const unsigned char pushdata1[] = { RIGOR_PUSHDATA1, 1, 0x5a };
    static const unsigned char pushdata2[] = { RIGOR_PUSHDATA2, 1, 0, 0x5a };
    static const unsigned char pushdata4[] = { RIGOR_PUSHDATA4, 1, 0, 0, 0, 0x5a };

    std::vector<std::vector<unsigned char> > directStack;
    BOOST_CHECK(EvalPushOnlyScript(CScript(direct, direct + sizeof(direct)), directStack));

    std::vector<std::vector<unsigned char> > pushdata1Stack;
    BOOST_CHECK(EvalPushOnlyScript(CScript(pushdata1, pushdata1 + sizeof(pushdata1)), pushdata1Stack));
    BOOST_CHECK(pushdata1Stack == directStack);

    std::vector<std::vector<unsigned char> > pushdata2Stack;
    BOOST_CHECK(EvalPushOnlyScript(CScript(pushdata2, pushdata2 + sizeof(pushdata2)), pushdata2Stack));
    BOOST_CHECK(pushdata2Stack == directStack);

    std::vector<std::vector<unsigned char> > pushdata4Stack;
    BOOST_CHECK(EvalPushOnlyScript(CScript(pushdata4, pushdata4 + sizeof(pushdata4)), pushdata4Stack));
    BOOST_CHECK(pushdata4Stack == directStack);

    const std::vector<unsigned char> pushdata1_trunc{RIGOR_PUSHDATA1, 1};
    const std::vector<unsigned char> pushdata2_trunc{RIGOR_PUSHDATA2, 1, 0};
    const std::vector<unsigned char> pushdata4_trunc{RIGOR_PUSHDATA4, 1, 0, 0, 0};

    std::vector<std::vector<unsigned char>> stack_ignore;
    BOOST_CHECK(!EvalPushOnlyScript(CScript(pushdata1_trunc.begin(), pushdata1_trunc.end()), stack_ignore));
    BOOST_CHECK(!EvalPushOnlyScript(CScript(pushdata2_trunc.begin(), pushdata2_trunc.end()), stack_ignore));
    BOOST_CHECK(!EvalPushOnlyScript(CScript(pushdata4_trunc.begin(), pushdata4_trunc.end()), stack_ignore));
}

BOOST_AUTO_TEST_CASE(script_cltv_truncated)
{
    const auto script_cltv_trunc = CScript() << RIGOR_RESERVED_LEGACY_CLTV;
    std::vector<std::vector<unsigned char>> stack_ignore;
    BOOST_CHECK(!EvalPushOnlyScript(script_cltv_trunc, stack_ignore));
}

BOOST_AUTO_TEST_CASE(script_legacy_locks_rejected)
{
    ScriptError err;
    CKey key;
    key.MakeNewKey(true);
    const uint32_t flags = SCRIPT_VERIFY_WITNESS;

    const CScript p2pk = CScript() << ToByteVector(key.GetPubKey()) << RIGOR_RESERVED_LEGACY_CHECKSIG;
    BOOST_CHECK(!VerifyRigorPredicate(CScript(), p2pk, nullptr, flags, BaseSignatureChecker(), &err));
    BOOST_CHECK_EQUAL(err, SCRIPT_ERR_RIGOR_SCRIPT_TYPE);

    const CScript p2pkh = CScript() << RIGOR_RESERVED_LEGACY_DUP << RIGOR_RESERVED_LEGACY_HASH160 << ToByteVector(key.GetPubKey().GetID()) << RIGOR_RESERVED_LEGACY_EQUALVERIFY << RIGOR_RESERVED_LEGACY_CHECKSIG;
    BOOST_CHECK(!VerifyRigorPredicate(CScript(), p2pkh, nullptr, flags, BaseSignatureChecker(), &err));
    BOOST_CHECK_EQUAL(err, SCRIPT_ERR_RIGOR_SCRIPT_TYPE);

    const CScript ms = CScript() << RIGOR_PUSH_1 << ToByteVector(key.GetPubKey()) << RIGOR_PUSH_1 << RIGOR_RESERVED_LEGACY_CHECKMULTISIG;
    BOOST_CHECK(!VerifyRigorPredicate(CScript(), ms, nullptr, flags, BaseSignatureChecker(), &err));
    BOOST_CHECK_EQUAL(err, SCRIPT_ERR_RIGOR_SCRIPT_TYPE);
}

BOOST_AUTO_TEST_CASE(script_p2wpkh_sign_and_verify)
{
    FillableSigningProvider keystore;
    CKey key;
    key.MakeNewKey(true);
    BOOST_CHECK(keystore.AddKey(key));
    const CScript spk = GetScriptForDestination(WitnessV0KeyHash(key.GetPubKey()));
    CMutableTransaction txFrom = BuildCreditingTransaction(spk, COIN);
    CMutableTransaction txTo = BuildSpendingTransaction(CScript(), CScriptWitness(), CTransaction(txFrom));
    SignatureData dummy;
    BOOST_CHECK(SignSignature(keystore, CTransaction(txFrom), txTo, 0, uint256::ZERO, dummy));
    ScriptError err;
    PrecomputedTransactionData txdata;
    txdata.Init(txTo, {txFrom.vout[0]});
    BOOST_CHECK(VerifyRigorPredicate(txTo.vin[0].scriptSig, spk, &txTo.vin[0].scriptWitness,
        SCRIPT_VERIFY_WITNESS,
        MutableTransactionSignatureChecker(&txTo, 0, txFrom.vout[0].nValue, txdata, MissingDataBehavior::ASSERT_FAIL, uint256::ZERO), &err));
    BOOST_CHECK_EQUAL(err, SCRIPT_ERR_OK);
}

BOOST_AUTO_TEST_CASE(script_standard_push)
{
    for (int i=0; i<67000; i++) {
        CScript script;
        script << i;
        BOOST_CHECK_MESSAGE(script.IsPushOnly(), "Number " << i << " is not pure push.");
        std::vector<std::vector<unsigned char>> stack;
        BOOST_CHECK_MESSAGE(EvalPushOnlyScript(script, stack), "Number " << i << " is not a push-only encoding.");
    }

    for (unsigned int i=0; i<=MAX_SCRIPT_ELEMENT_SIZE; i++) {
        std::vector<unsigned char> data(i, '\111');
        CScript script;
        script << data;
        BOOST_CHECK_MESSAGE(script.IsPushOnly(), "Length " << i << " is not pure push.");
        std::vector<std::vector<unsigned char>> stack;
        BOOST_CHECK_MESSAGE(EvalPushOnlyScript(script, stack), "Length " << i << " is not a push-only encoding.");
        BOOST_CHECK_EQUAL(stack.size(), 1U);
        BOOST_CHECK(stack[0] == data);
    }
}

BOOST_AUTO_TEST_CASE(script_IsPushOnly_on_invalid_scripts)
{
    // IsPushOnly returns false when given a script containing only pushes that
    // are invalid due to truncation. IsPushOnly() is consensus critical
    // because P2SH evaluation uses it, although this specific behavior should
    // not be consensus critical as the P2SH evaluation would fail first due to
    // the invalid push. Still, it doesn't hurt to test it explicitly.
    static const unsigned char direct[] = { 1 };
    BOOST_CHECK(!CScript(direct, direct+sizeof(direct)).IsPushOnly());
}

BOOST_AUTO_TEST_CASE(script_GetScriptAsm)
{
    BOOST_CHECK_EQUAL("RIGOR_RESERVED_LEGACY_CLTV", ScriptToAsmStr(CScript() << RIGOR_RESERVED_LEGACY_CLTV, true));
    BOOST_CHECK_EQUAL("RIGOR_RESERVED_LEGACY_CLTV", ScriptToAsmStr(CScript() << RIGOR_RESERVED_LEGACY_CLTV, true));
    BOOST_CHECK_EQUAL("RIGOR_RESERVED_LEGACY_CLTV", ScriptToAsmStr(CScript() << RIGOR_RESERVED_LEGACY_CLTV));
    BOOST_CHECK_EQUAL("RIGOR_RESERVED_LEGACY_CLTV", ScriptToAsmStr(CScript() << RIGOR_RESERVED_LEGACY_CLTV));

    // Consensus signatures are 64-byte Ed25519; Bitcoin sighash suffixes are not decoded.
    const std::string ed_sig(128, 'a'); // 64 bytes of 0xaa
    const std::string pk32(64, 'b');
    BOOST_CHECK_EQUAL(ed_sig + " " + pk32,
        ScriptToAsmStr(CScript() << ToByteVector(ParseHex(ed_sig)) << ToByteVector(ParseHex(pk32)), true));
    BOOST_CHECK_EQUAL(ed_sig + " " + pk32,
        ScriptToAsmStr(CScript() << ToByteVector(ParseHex(ed_sig)) << ToByteVector(ParseHex(pk32))));
}

static CScript ScriptFromHex(const std::string& str)
{
    std::vector<unsigned char> data = ParseHex(str);
    return CScript(data.begin(), data.end());
}

BOOST_AUTO_TEST_CASE(script_FindAndDelete)
{
    // P0-018: FindAndDelete was part of the deleted Script VM and has been removed.
    BOOST_TEST_MESSAGE("FindAndDelete removed with the Script VM");
}

BOOST_AUTO_TEST_CASE(script_HasValidOps)
{
    // Exercise the HasValidOps functionality
    CScript script;
    script = ScriptFromHex("76a9141234567890abcdefa1a2a3a4a5a6a7a8a9a0aaab88ac"); // Normal script
    BOOST_CHECK(script.HasValidOps());
    script = ScriptFromHex("76a914ff34567890abcdefa1a2a3a4a5a6a7a8a9a0aaab88ac");
    BOOST_CHECK(script.HasValidOps());
    script = ScriptFromHex("ff88ac"); // Script with RIGOR_BYTE_INVALID explicit
    BOOST_CHECK(!script.HasValidOps());
    script = ScriptFromHex("88acc0"); // Script with undefined opcode
    BOOST_CHECK(!script.HasValidOps());
}

static CMutableTransaction TxFromHex(const std::string& str)
{
    CMutableTransaction tx;
    SpanReader{SER_DISK, SERIALIZE_TRANSACTION_NO_WITNESS, ParseHex(str)} >> tx;
    return tx;
}

static std::vector<CTxOut> TxOutsFromJSON(const UniValue& univalue)
{
    assert(univalue.isArray());
    std::vector<CTxOut> prevouts;
    for (size_t i = 0; i < univalue.size(); ++i) {
        CTxOut txout;
        SpanReader{SER_DISK, 0, ParseHex(univalue[i].get_str())} >> txout;
        prevouts.push_back(std::move(txout));
    }
    return prevouts;
}

static CScriptWitness ScriptWitnessFromJSON(const UniValue& univalue)
{
    assert(univalue.isArray());
    CScriptWitness scriptwitness;
    for (size_t i = 0; i < univalue.size(); ++i) {
        auto bytes = ParseHex(univalue[i].get_str());
        scriptwitness.stack.push_back(std::move(bytes));
    }
    return scriptwitness;
}

static std::vector<unsigned int> AllConsensusFlags()
{
    std::vector<unsigned int> ret;
    const unsigned int bits[] = {
        SCRIPT_VERIFY_SIGPUSHONLY,
        SCRIPT_VERIFY_CLEANSTACK,
        SCRIPT_VERIFY_WITNESS,
        SCRIPT_VERIFY_NULLFAIL,
    };
    for (unsigned int i = 0; i < 16; ++i) {
        unsigned int flag = 0;
        for (int b = 0; b < 4; ++b) {
            if (i & (1u << b)) flag |= bits[b];
        }
        if ((flag & SCRIPT_VERIFY_CLEANSTACK) && !(flag & SCRIPT_VERIFY_WITNESS)) continue;
        ret.push_back(flag);
    }
    return ret;
}

/** Precomputed list of all valid combinations of consensus-relevant script validation flags. */
static const std::vector<unsigned int> ALL_CONSENSUS_FLAGS = AllConsensusFlags();

static void AssetTest(const UniValue& test)
{
    BOOST_CHECK(test.isObject());

    CMutableTransaction mtx = TxFromHex(test["tx"].get_str());
    const std::vector<CTxOut> prevouts = TxOutsFromJSON(test["prevouts"]);
    BOOST_CHECK(prevouts.size() == mtx.vin.size());
    size_t idx = test["index"].getInt<int64_t>();
    uint32_t test_flags{ParseScriptFlags(test["flags"].get_str())};
    bool fin = test.exists("final") && test["final"].get_bool();

    if (test.exists("success")) {
        mtx.vin[idx].scriptSig = ScriptFromHex(test["success"]["scriptSig"].get_str());
        mtx.vin[idx].scriptWitness = ScriptWitnessFromJSON(test["success"]["witness"]);
        CTransaction tx(mtx);
        PrecomputedTransactionData txdata;
        txdata.Init(tx, std::vector<CTxOut>(prevouts));
        CachingTransactionSignatureChecker txcheck(&tx, idx, prevouts[idx].nValue, true, txdata, uint256::ZERO);
        for (const auto flags : ALL_CONSENSUS_FLAGS) {
            // "final": true tests are valid for all flags. Others are only valid with flags that are
            // a subset of test_flags.
            if (fin || ((flags & test_flags) == flags)) {
                bool ret = VerifyRigorPredicate(tx.vin[idx].scriptSig, prevouts[idx].scriptPubKey, &tx.vin[idx].scriptWitness, flags, txcheck, nullptr);
                BOOST_CHECK(ret);
            }
        }
    }

    if (test.exists("failure")) {
        mtx.vin[idx].scriptSig = ScriptFromHex(test["failure"]["scriptSig"].get_str());
        mtx.vin[idx].scriptWitness = ScriptWitnessFromJSON(test["failure"]["witness"]);
        CTransaction tx(mtx);
        PrecomputedTransactionData txdata;
        txdata.Init(tx, std::vector<CTxOut>(prevouts));
        CachingTransactionSignatureChecker txcheck(&tx, idx, prevouts[idx].nValue, true, txdata, uint256::ZERO);
        for (const auto flags : ALL_CONSENSUS_FLAGS) {
            // If a test is supposed to fail with test_flags, it should also fail with any superset thereof.
            if ((flags & test_flags) == test_flags) {
                bool ret = VerifyRigorPredicate(tx.vin[idx].scriptSig, prevouts[idx].scriptPubKey, &tx.vin[idx].scriptWitness, flags, txcheck, nullptr);
                BOOST_CHECK(!ret);
            }
        }
    }
}

BOOST_AUTO_TEST_CASE(script_assets_test)
{
    // See src/test/fuzz/script_assets_test_minimizer.cpp for information on how to generate
    // the script_assets_test.json file used by this test.

    const char* dir = std::getenv("DIR_UNIT_TEST_DATA");
    BOOST_WARN_MESSAGE(dir != nullptr, "Variable DIR_UNIT_TEST_DATA unset, skipping script_assets_test");
    if (dir == nullptr) return;
    auto path = fs::path(dir) / "script_assets_test.json";
    bool exists = fs::exists(path);
    BOOST_WARN_MESSAGE(exists, "File $DIR_UNIT_TEST_DATA/script_assets_test.json not found, skipping script_assets_test");
    if (!exists) return;
    std::ifstream file{path};
    BOOST_CHECK(file.is_open());
    file.seekg(0, std::ios::end);
    size_t length = file.tellg();
    file.seekg(0, std::ios::beg);
    std::string data(length, '\0');
    file.read(data.data(), data.size());
    UniValue tests = read_json(data);
    BOOST_CHECK(tests.isArray());
    BOOST_CHECK(tests.size() > 0);

    for (size_t i = 0; i < tests.size(); i++) {
        AssetTest(tests[i]);
    }
    file.close();
}

BOOST_AUTO_TEST_SUITE_END()
