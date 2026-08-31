// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// P068: production-authority freeze battery. Boost only.
// Does not recode formulas. Does not switch ConnectBlock.

#include <chainparams.h>
#include <consensus/amount.h>
#include <consensus/consensus.h>
#include <consensus/convergence_logic.h>
#include <consensus/convergence_root_eligibility.h>
#include <consensus/params.h>
#include <consensus/rigor_consensus_lock.h>
#include <consensus/rigor_issuance.h>
#include <consensus/rigor_supply.h>
#include <consensus/validation.h>
#include <crypto/ed25519_provider.h>
#include <hash.h>
#include <policy/policy.h>
#include <primitives/transaction.h>
#include <script/predicate_verify.h>
#include <script/script.h>
#include <script/script_error.h>
#include <script/standard.h>
#include <test/util/setup_common.h>
#include <test/util/transaction_utils.h>
#include <uint256.h>

#include <boost/test/unit_test.hpp>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <numeric>
#include <string>
#include <utility>
#include <vector>

namespace {

constexpr CAmount kSaturatingWeight =
    65536 * static_cast<CAmount>(Consensus::CONVERGENCE_DRAW_WINDOW) * Consensus::MIN_CONVERGENCE_AMOUNT;

const int kFreezeHeights[] = {0, 1, 99, 100, 101, 209999, 210000, 419999, 420000};

CScript OruP2wpkh(unsigned char fill = 0x11)
{
    return CScript() << RIGOR_PUSH_0 << std::vector<unsigned char>(WITNESS_V0_KEYHASH_SIZE, fill);
}

CScript ResNative(unsigned char fill = 0xaa)
{
    return CScript() << RIGOR_PUSH_1 << std::vector<unsigned char>(WITNESS_V1_RES_SIZE, fill);
}

uint256 TxidByte(unsigned char b)
{
    uint256 id;
    std::fill(id.begin(), id.end(), static_cast<unsigned char>(0));
    *(id.begin() + 31) = b;
    return id;
}

ConvergenceDrawSet MakeDrawSet(int source_height, const std::vector<std::pair<uint256, uint64_t>>& members)
{
    ConvergenceDrawSet set;
    set.source_height = source_height;
    set.source_nbits = 0x207fffff;
    set.source_block_hash = uint256S("0xaa");
    std::vector<uint256> txids;
    for (const auto& m : members) {
        ConvergenceDrawCandidate c;
        c.txid = m.first;
        c.weight = m.second;
        set.candidates.push_back(c);
        txids.push_back(m.first);
    }
    set.convergence_root = Consensus::ComputeConvergenceRootFromEligibilityTxids(txids);
    return set;
}

} // namespace

BOOST_FIXTURE_TEST_SUITE(test_consensus_kernel_freeze, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(freeze_issuance_heights)
{
    const auto& c = Params().GetConsensus();
    BOOST_REQUIRE_EQUAL(Consensus::GetResMintReward(0), 40000000);
    BOOST_REQUIRE_EQUAL(GetBlockSubsidy(0, c), 0);
    BOOST_REQUIRE_EQUAL(ComputeRigorIssuance(0, c).res_reward, 40000000);
    BOOST_REQUIRE_MESSAGE(ComputeRigorIssuance(0, c).oru_subsidy == 0, "CONSENSUS_KERNEL_FREEZE_DRIFT");

    for (int h : kFreezeHeights) {
        const RigorIssuance pack = ComputeRigorIssuance(h, c);
        BOOST_REQUIRE_MESSAGE(pack.oru_subsidy == GetBlockSubsidy(h, c), "CONSENSUS_KERNEL_FREEZE_DRIFT");
        BOOST_REQUIRE_MESSAGE(pack.res_reward == Consensus::GetResMintReward(h), "CONSENSUS_KERNEL_FREEZE_DRIFT");
        BOOST_REQUIRE_MESSAGE(pack.epoch == RigorIssuanceEpoch(h), "CONSENSUS_KERNEL_FREEZE_DRIFT");
        const CAmount issued = (h == 0) ? 0 : pack.res_reward;
        if (h == 0) {
            BOOST_REQUIRE_EQUAL(issued, 0);
        }
    }

    BOOST_REQUIRE_EQUAL(GetBlockSubsidy(1, c), Consensus::ORU_INITIAL_BLOCK_SUBSIDY);
    BOOST_REQUIRE_EQUAL(GetBlockSubsidy(209999, c), Consensus::ORU_INITIAL_BLOCK_SUBSIDY);
    BOOST_REQUIRE_EQUAL(GetBlockSubsidy(210000, c), Consensus::ORU_INITIAL_BLOCK_SUBSIDY >> 1);
    BOOST_REQUIRE_EQUAL(GetBlockSubsidy(419999, c), Consensus::ORU_INITIAL_BLOCK_SUBSIDY >> 1);
    BOOST_REQUIRE_EQUAL(GetBlockSubsidy(420000, c), Consensus::ORU_INITIAL_BLOCK_SUBSIDY >> 2);
}

BOOST_AUTO_TEST_CASE(freeze_draw_cases)
{
    const uint256 T = Consensus::RES_PROBABILITY_TARGET_CONST;
    const uint64_t w = static_cast<uint64_t>(kSaturatingWeight);
    const uint256 a = TxidByte(0x11);
    const uint256 b = TxidByte(0x22);

    ConvergenceDrawSet empty;
    empty.source_height = 100;
    BOOST_REQUIRE_MESSAGE(!ResolveConvergenceDraw(empty, T).HasWinner(), "CONSENSUS_KERNEL_FREEZE_DRIFT");

    ConvergenceDrawSet single = MakeDrawSet(100, {{a, w}});
    const ConvergenceDrawOutcome one = ResolveConvergenceDraw(single, T);
    BOOST_REQUIRE_MESSAGE(one.HasWinner() && single.candidates[static_cast<size_t>(one.winner_index)].txid == a,
        "CONSENSUS_KERNEL_FREEZE_DRIFT");

    ConvergenceDrawSet eq = MakeDrawSet(100, {{a, w}, {b, w}});
    const uint256 w1 = eq.candidates[static_cast<size_t>(ResolveConvergenceDraw(eq, T).winner_index)].txid;
    std::swap(eq.candidates[0], eq.candidates[1]);
    const uint256 w2 = eq.candidates[static_cast<size_t>(ResolveConvergenceDraw(eq, T).winner_index)].txid;
    BOOST_REQUIRE_MESSAGE(w1 == w2, "CONSENSUS_KERNEL_FREEZE_DRIFT");

    BOOST_REQUIRE_MESSAGE(!ResolveConvergenceDraw(MakeDrawSet(100, {{a, w}}), uint256()).HasWinner(),
        "CONSENSUS_KERNEL_FREEZE_DRIFT");

    ConvergenceDrawSet ov = MakeDrawSet(100, {{a, std::numeric_limits<uint64_t>::max()}});
    BOOST_REQUIRE_MESSAGE(!ResolveConvergenceDraw(ov, T).HasWinner(), "CONSENSUS_KERNEL_FREEZE_DRIFT");

    ConvergenceDrawSet dup = MakeDrawSet(100, {{a, w}, {a, w}});
    const ConvergenceDrawOutcome d = ResolveConvergenceDraw(dup, T);
    BOOST_REQUIRE_MESSAGE(d.HasWinner(), "CONSENSUS_KERNEL_FREEZE_DRIFT");
    BOOST_REQUIRE_MESSAGE(dup.candidates[static_cast<size_t>(d.winner_index)].txid == a,
        "CONSENSUS_KERNEL_FREEZE_DRIFT");
}

BOOST_AUTO_TEST_CASE(freeze_classify_cases)
{
    auto expect = [](const CScript& s, AssetType a) {
        std::vector<std::vector<unsigned char>> sols;
        BOOST_REQUIRE_MESSAGE(ClassifyRigorAsset(s) == a, "CONSENSUS_KERNEL_FREEZE_DRIFT");
        (void)ClassifyRigorTxout(s, sols);
    };
    expect(OruP2wpkh(), ASSET_ORU);
    expect(ResNative(), ASSET_RES);
    expect(CScript() << RIGOR_PUSH_2 << std::vector<unsigned char>(20, 0x11), ASSET_INVALID);
    expect(CScript() << CScriptNum(0) << std::vector<unsigned char>(19, 0x11), ASSET_INVALID);
    const std::vector<unsigned char> huge(10 * 1024, 0xff);
    expect(CScript(huge.begin(), huge.end()), ASSET_INVALID);
    expect(CScript(), ASSET_INVALID);
}

BOOST_AUTO_TEST_CASE(freeze_transaction_fail_closed)
{
    const std::vector<unsigned char> seed = ParseHex("00112233445566778899aabbccddeeff00112233445566778899aabbccddeeff");
    std::vector<unsigned char> pubkey(ed25519::PUBKEY_SIZE);
    std::vector<unsigned char> seckey(ed25519::SECRETKEY_SIZE);
    BOOST_REQUIRE(ed25519::SeedToKeypair(seed, pubkey, seckey));

    const uint160 keyhash = Hash160(pubkey);
    const CScript scriptPubKey = GetScriptForDestination(WitnessV0KeyHash(keyhash));
    const CTransaction txCredit = CTransaction(BuildCreditingTransaction(scriptPubKey, COIN));
    CMutableTransaction txSpend = BuildSpendingTransaction(CScript(), CScriptWitness(), txCredit);
    PrecomputedTransactionData txdata(txSpend);
    const uint256 sighash = SignatureHash(scriptPubKey, txSpend, 0, txCredit.vout[0].nValue, SigVersion::WITNESS_V0,
        TestSighashChainId(), &txdata);
    std::vector<unsigned char> witSig(ed25519::SIGNATURE_SIZE);
    BOOST_REQUIRE(ed25519::SignDetached(Span{sighash.begin(), 32}, seckey, Span{witSig.data(), ed25519::SIGNATURE_SIZE}));

    auto verify = [&](const CScriptWitness* wit, const CMutableTransaction& tx, const uint256& chain) {
        ScriptError err{SCRIPT_ERR_UNKNOWN_ERROR};
        PrecomputedTransactionData data(tx);
        return VerifyRigorPredicate(CScript(), scriptPubKey, wit, STANDARD_SCRIPT_VERIFY_FLAGS,
            MutableTransactionSignatureChecker(&tx, 0, COIN, data, MissingDataBehavior::ASSERT_FAIL, chain), &err);
    };

    CScriptWitness good;
    good.stack.push_back(witSig);
    good.stack.push_back(pubkey);
    txSpend.vin[0].scriptWitness = good;
    BOOST_REQUIRE_MESSAGE(verify(&txSpend.vin[0].scriptWitness, txSpend, TestSighashChainId()),
        "CONSENSUS_KERNEL_FREEZE_DRIFT");

    CScriptWitness bad_sig = good;
    bad_sig.stack[0][0] ^= 0x01;
    txSpend.vin[0].scriptWitness = bad_sig;
    BOOST_REQUIRE_MESSAGE(!verify(&txSpend.vin[0].scriptWitness, txSpend, TestSighashChainId()),
        "CONSENSUS_KERNEL_FREEZE_DRIFT");

    std::vector<unsigned char> wrong_pk = pubkey;
    wrong_pk[0] ^= 0x01;
    CScriptWitness wrong;
    wrong.stack.push_back(witSig);
    wrong.stack.push_back(wrong_pk);
    txSpend.vin[0].scriptWitness = wrong;
    BOOST_REQUIRE_MESSAGE(!verify(&txSpend.vin[0].scriptWitness, txSpend, TestSighashChainId()),
        "CONSENSUS_KERNEL_FREEZE_DRIFT");

    CScriptWitness mutated = good;
    mutated.stack[0].push_back(0xff);
    txSpend.vin[0].scriptWitness = mutated;
    BOOST_REQUIRE_MESSAGE(!verify(&txSpend.vin[0].scriptWitness, txSpend, TestSighashChainId()),
        "CONSENSUS_KERNEL_FREEZE_DRIFT");

    CMutableTransaction digest = txSpend;
    digest.vout[0].nValue = COIN - 1;
    digest.vin[0].scriptWitness = good;
    BOOST_REQUIRE_MESSAGE(!verify(&digest.vin[0].scriptWitness, digest, TestSighashChainId()),
        "CONSENSUS_KERNEL_FREEZE_DRIFT");

    txSpend.vin[0].scriptWitness = good;
    BOOST_REQUIRE_MESSAGE(!verify(&txSpend.vin[0].scriptWitness, txSpend, uint256S("0xdeadbeef")),
        "CONSENSUS_KERNEL_FREEZE_DRIFT");
}

BOOST_AUTO_TEST_CASE(freeze_ledger_reject_via_authorities)
{
    const auto& c = Params().GetConsensus();
    RigorSupplyDeltaInput overflow;
    overflow.height = 1;
    overflow.oru_coinbase = std::numeric_limits<CAmount>::max();
    BlockValidationState state;
    RigorSupplyDeltaResult out;
    BOOST_REQUIRE_MESSAGE(!RigorSupplyDelta(overflow, c, out, state), "CONSENSUS_KERNEL_FREEZE_DRIFT");
    BOOST_REQUIRE_EQUAL(state.GetRejectReason(), "bad-cb-amount");

    BOOST_REQUIRE_EQUAL(ClassifyRigorAsset(CScript() << RIGOR_PUSH_2 << std::vector<unsigned char>(20, 0x00)),
        ASSET_INVALID);
}

BOOST_AUTO_TEST_SUITE_END()
