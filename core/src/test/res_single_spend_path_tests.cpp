// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

/*
 * SECURITY-001-A / A-7: a witness v1 RES output has exactly ONE spending path.
 *
 * The surviving path is the 2-item Ed25519 key path, whose signature commits to the spending
 * transaction through the sighash.
 *
 * The removed path was an 11-item "PCORB script path" whose only authorization was
 * VerifyConvergeProof over the σ digest. σ is a function of the convergence being proved and the
 * block context, never of the spending transaction, so a claim published on that path was
 * unauthenticated with respect to its own outputs: anyone could lift the eleven witness items into
 * a transaction paying themselves, satisfy every check identically, and outbid the owner.
 *
 * These tests exist so that path cannot come back unnoticed, at either the relay or the script
 * layer.
 */

#include <coins.h>
#include <consensus/amount.h>
#include <consensus/res_amount.h>
#include <consensus/res_proof.h>
#include <consensus/tx_check.h>
#include <consensus/validation.h>
#include <crypto/common.h>
#include <crypto/ed25519_provider.h>
#include <key.h>
#include <policy/policy.h>
#include <primitives/transaction.h>
#include <pubkey.h>
#include <script/rigor_tags.h>
#include <script/interpreter.h>
#include <script/standard.h>
#include <test/util/setup_common.h>
#include <test/util/transaction_utils.h>
#include <uint256.h>

#include <cstring>

#include <boost/test/unit_test.hpp>

namespace {

void MakeNativeResPrevout(CKey& sk_out, CScript& spk_out)
{
    for (int i = 0; i < 100; ++i) {
        sk_out.MakeNewKey(true);
        std::vector<unsigned char> prog;
        if (!Consensus::DeriveResWitnessProgramFromPubKey(sk_out.GetPubKey(), prog)) continue;
        if (prog.size() != 32) continue;
        if (std::memcmp(prog.data(), RigorTags::CVG2.data(), RigorTags::CVG2.size()) == 0) continue;
        if (std::memcmp(prog.data(), RigorTags::Res2WirePrefix.data(), RigorTags::Res2WirePrefix.size()) == 0) continue;
        spk_out = GetScriptForDestination(WitnessV1Res{XOnlyPubKey{prog}});
        return;
    }
    BOOST_FAIL("could not derive native RES witness program after retries");
}

/** The layout that used to be accepted: 6 proof pushes, sig, pubkey, leaf, two reserved pushes. */
CScriptWitness MakeRemovedPcorbScriptPathWitness(const std::vector<unsigned char>& ed_pk32)
{
    CScriptWitness wit;
    wit.stack.emplace_back(32, 0x11);          // convergence_txid
    {
        unsigned char le8[8];
        WriteLE64(le8, static_cast<uint64_t>(COIN));
        wit.stack.emplace_back(le8, le8 + sizeof(le8)); // convergence_amount
    }
    wit.stack.emplace_back(32, 0x22);          // bind_hash
    wit.stack.emplace_back(8, 0x00);           // height
    wit.stack.emplace_back(4, 0x00);           // target_prev
    wit.stack.emplace_back(32, 0x33);          // convergence_root
    wit.stack.emplace_back(ed25519::SIGNATURE_SIZE, 0x44); // σ signature
    wit.stack.push_back(ed_pk32);              // pubkey, formerly required to equal the program
    const CScript leaf = CScript() << OP_1;    // the leaf the removed branch demanded
    wit.stack.emplace_back(leaf.begin(), leaf.end());
    wit.stack.emplace_back(32, 0x00);          // reserved
    wit.stack.emplace_back(64, 0x00);          // reserved
    return wit;
}

} // namespace

BOOST_FIXTURE_TEST_SUITE(res_single_spend_path_tests, BasicTestingSetup)

// Relay: an 11-item stack on a native RES input is no longer standard, so the hijackable shape
// does not propagate even if some node were to accept it.
BOOST_AUTO_TEST_CASE(relay_rejects_removed_pcorb_script_path)
{
    CKey spend_sk;
    CScript prev_spk;
    MakeNativeResPrevout(spend_sk, prev_spk);

    const CTransaction txCredit = CTransaction(BuildCreditingTransaction(prev_spk, 50 * RES_COIN));
    CCoinsView base;
    CCoinsViewCache coins(&base);
    coins.AddCoin(COutPoint(txCredit.GetHash(), 0), Coin(txCredit.vout[0], 1, false), false);

    std::vector<unsigned char> program;
    BOOST_REQUIRE(Consensus::DeriveResWitnessProgramFromPubKey(spend_sk.GetPubKey(), program));

    CMutableTransaction spend = BuildSpendingTransaction(CScript(), CScriptWitness(), txCredit);
    spend.vin[0].scriptWitness = MakeRemovedPcorbScriptPathWitness(program);
    BOOST_REQUIRE_EQUAL(spend.vin[0].scriptWitness.stack.size(), 11u);

    BOOST_CHECK(!IsWitnessStandard(CTransaction(spend), coins));
}

// Script verification: even with the pubkey matching the program exactly — the condition the old
// branch checked — the 11-item stack fails. Nothing about this witness authorizes these outputs.
BOOST_AUTO_TEST_CASE(script_rejects_removed_pcorb_script_path_even_with_matching_program)
{
    CKey spend_sk;
    CScript prev_spk;
    MakeNativeResPrevout(spend_sk, prev_spk);

    std::vector<unsigned char> program;
    BOOST_REQUIRE(Consensus::DeriveResWitnessProgramFromPubKey(spend_sk.GetPubKey(), program));

    const CTransaction txCredit = CTransaction(BuildCreditingTransaction(prev_spk, 50 * RES_COIN));
    CMutableTransaction spend = BuildSpendingTransaction(CScript(), CScriptWitness(), txCredit);
    spend.vin[0].scriptWitness = MakeRemovedPcorbScriptPathWitness(program);

    ScriptError err = SCRIPT_ERR_OK;
    const bool ok = VerifyScript(spend.vin[0].scriptSig,
        prev_spk,
        &spend.vin[0].scriptWitness,
        SCRIPT_VERIFY_P2SH | SCRIPT_VERIFY_WITNESS,
        MutableTransactionSignatureChecker(&spend, 0, txCredit.vout[0].nValue, MissingDataBehavior::ASSERT_FAIL),
        &err);
    BOOST_CHECK(!ok);
    BOOST_CHECK_EQUAL(err, SCRIPT_ERR_WITNESS_PROGRAM_MISMATCH);
}

// The surviving path is untouched: a 2-item key-path witness is the standard shape, so ordinary RES
// transfers keep working and nothing is stranded by the removal.
BOOST_AUTO_TEST_CASE(relay_accepts_key_path_two_item_witness)
{
    CKey spend_sk;
    CScript prev_spk;
    MakeNativeResPrevout(spend_sk, prev_spk);

    const CTransaction txCredit = CTransaction(BuildCreditingTransaction(prev_spk, 50 * RES_COIN));
    CCoinsView base;
    CCoinsViewCache coins(&base);
    coins.AddCoin(COutPoint(txCredit.GetHash(), 0), Coin(txCredit.vout[0], 1, false), false);

    CMutableTransaction spend = BuildSpendingTransaction(CScript(), CScriptWitness(), txCredit);
    CScriptWitness wit;
    wit.stack.emplace_back(ed25519::SIGNATURE_SIZE + 1, 0x42); // detached sig + hashtype
    wit.stack.push_back(spend_sk.GetPubKey().GetEd25519Payload());
    spend.vin[0].scriptWitness = std::move(wit);

    BOOST_CHECK(IsWitnessStandard(CTransaction(spend), coins));
}

BOOST_AUTO_TEST_CASE(v1_program_is_sha256_of_pubkey)
{
    CKey spend_sk;
    CScript prev_spk;
    MakeNativeResPrevout(spend_sk, prev_spk);
    const std::vector<unsigned char> pk32 = spend_sk.GetPubKey().GetEd25519Payload();
    std::vector<unsigned char> program;
    BOOST_REQUIRE(Consensus::DeriveResWitnessProgramFromPubKey(spend_sk.GetPubKey(), program));
    BOOST_CHECK(program != pk32);
    uint256 hashed;
    BOOST_REQUIRE(ed25519::ResWitnessProgram(pk32, Span<unsigned char>{hashed.begin(), 32}));
    BOOST_CHECK(std::equal(program.begin(), program.end(), hashed.begin()));

    int wver = 0;
    std::vector<unsigned char> wprog;
    BOOST_REQUIRE(prev_spk.IsWitnessProgram(wver, wprog));
    BOOST_CHECK_EQUAL(wver, 1);
    BOOST_CHECK(wprog == program);
}

BOOST_AUTO_TEST_CASE(non_coinbase_res_mint_rejected)
{
    CMutableTransaction mtx;
    mtx.vin.resize(1);
    mtx.vin[0].prevout.hash = uint256S("0x01");
    mtx.vin[0].prevout.n = 0;
    mtx.vout.resize(1);
    mtx.vout[0].nValue = RES_COIN;
    mtx.vout[0].scriptPubKey = GetResMintScript(/*maturity_height=*/0, uint160());
    TxValidationState state;
    BOOST_CHECK(!CheckTransaction(CTransaction(mtx), state));
    BOOST_CHECK_EQUAL(state.GetRejectReason(), "bad-txns-res-mint-noncoinbase");
}

BOOST_AUTO_TEST_SUITE_END()
