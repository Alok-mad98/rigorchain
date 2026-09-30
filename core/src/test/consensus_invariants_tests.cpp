// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// Pins the consensus invariants in docs/protocol/RIGOR_CONSENSUS_INVARIANTS.md.
// Literals are intentional: updating a constant and its caller together must
// still fail this file. This suite does not define a new consensus rule.

#include <arith_uint256.h>
#include <chainparams.h>
#include <chainparamsbase.h>
#include <coins.h>
#include <consensus/amount.h>
#include <consensus/merkle.h>
#include <consensus/res_issuance.h>
#include <consensus/rigor_consensus_lock.h>
#include <consensus/rigor_issuance.h>
#include <consensus/rigor_supply.h>
#include <consensus/tx_check.h>
#include <consensus/tx_verify.h>
#include <consensus/validation.h>
#include <crypto/ed25519_provider.h>
#include <hash.h>
#include <pow.h>
#include <primitives/block.h>
#include <primitives/transaction.h>
#include <rigor_kernel/pow_digest.h>
#include <script/predicate_verify.h>
#include <script/rigor_bytes.h>
#include <script/rigor_opcode_namespace.h>
#include <script/rigor_tags.h>
#include <script/script.h>
#include <script/script_error.h>
#include <script/standard.h>
#include <test/util/setup_common.h>
#include <uint256.h>
#include <util/strencodings.h>

#include <boost/test/unit_test.hpp>

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

BOOST_FIXTURE_TEST_SUITE(consensus_invariants_tests, BasicTestingSetup)

namespace {

const uint256 MAINNET_GENESIS =
    uint256S("0xe2ce92cfef740f92e8cc84a8541d8e760c776c668afd9b61740b3a4315241468");
const uint256 MAINNET_MERKLE =
    uint256S("0x9d3fbd207af927fc8e2c57e975d8d5845913bf059d956592e8a0bcf38b3515dd");
const uint256 ZERO_HEADER_POW =
    uint256S("0xfc04ded6f5d9f677b57536b4751c512d9e817b9dcc34e866945b8fdfc17f05dd");

CScript OruScript()
{
    return CScript() << RIGOR_PUSH_0 << std::vector<unsigned char>(WITNESS_V0_KEYHASH_SIZE, 0x11);
}

CScript ResScript()
{
    return CScript() << RIGOR_PUSH_1 << std::vector<unsigned char>(WITNESS_V1_RES_SIZE, 0xaa);
}

CScript DataScript()
{
    return CScript() << RIGOR_MARKER_RETURN << std::vector<unsigned char>{'d', 'a', 't', 'a'};
}

CMutableTransaction SpendableSkeleton()
{
    CMutableTransaction tx;
    tx.vin.resize(1);
    tx.vin[0].prevout.hash = uint256S("0x0101010101010101010101010101010101010101010101010101010101010101");
    tx.vin[0].prevout.n = 0;
    return tx;
}

} // namespace

BOOST_AUTO_TEST_CASE(oru_subsidy_is_flat_and_has_no_halving)
{
    const auto main = CreateChainParams(*m_node.args, CBaseChainParams::MAIN);
    const auto regtest = CreateChainParams(*m_node.args, CBaseChainParams::REGTEST);
    const auto& main_params = main->GetConsensus();
    const auto& reg_params = regtest->GetConsensus();

    BOOST_CHECK_EQUAL(COIN, 100000000);
    BOOST_CHECK_EQUAL(Consensus::ORU_GENESIS_BLOCK_SUBSIDY, 0);
    BOOST_CHECK_EQUAL(Consensus::ORU_FLAT_BLOCK_SUBSIDY, 128 * COIN);
    BOOST_CHECK_EQUAL(Consensus::ORU_FLAT_BLOCK_SUBSIDY, 12800000000LL);
    BOOST_CHECK_EQUAL(GetBlockSubsidy(-1, main_params), 0);
    BOOST_CHECK_EQUAL(GetBlockSubsidy(0, main_params), 0);
    BOOST_CHECK_EQUAL(GetBlockSubsidy(0, main_params), Consensus::ORU_GENESIS_BLOCK_SUBSIDY);

    const int heights[] = {1, 2, 100, 209999, 210000, 419999, 420000, 10000000};
    for (int h : heights) {
        BOOST_CHECK_EQUAL(GetBlockSubsidy(h, main_params), 12800000000LL);
        BOOST_CHECK_EQUAL(GetBlockSubsidy(h, main_params), GetBlockSubsidy(h, reg_params));
        BOOST_CHECK_EQUAL(GetBlockSubsidy(h, main_params), GetBlockSubsidy(1, main_params));
    }
}

BOOST_AUTO_TEST_CASE(coinbase_cannot_mint_oru_or_res)
{
    const auto main = CreateChainParams(*m_node.args, CBaseChainParams::MAIN);
    const auto& params = main->GetConsensus();
    BOOST_CHECK_EQUAL(params.res_wallet_bootstrap_coinbase, 0);

    RigorSupplyDeltaResult out;
    {
        RigorSupplyDeltaInput in;
        in.height = 1;
        in.oru_coinbase = 128 * COIN;
        BlockValidationState state;
        BOOST_CHECK(RigorSupplyDelta(in, params, out, state));
        BOOST_CHECK_EQUAL(out.oru_subsidy, 12800000000LL);
        BOOST_CHECK_EQUAL(out.oru_claimable, 12800000000LL);
    }
    {
        RigorSupplyDeltaInput in;
        in.height = 1;
        in.oru_coinbase = 128 * COIN + 1;
        BlockValidationState state;
        BOOST_CHECK(!RigorSupplyDelta(in, params, out, state));
        BOOST_CHECK_EQUAL(state.GetRejectReason(), "bad-cb-amount");
    }
    {
        RigorSupplyDeltaInput in;
        in.height = 0;
        in.oru_coinbase = 0;
        BlockValidationState state;
        BOOST_CHECK(RigorSupplyDelta(in, params, out, state));
        BOOST_CHECK_EQUAL(out.oru_subsidy, 0);
    }
    {
        RigorSupplyDeltaInput in;
        in.height = 0;
        in.oru_coinbase = 1;
        BlockValidationState state;
        BOOST_CHECK(!RigorSupplyDelta(in, params, out, state));
        BOOST_CHECK_EQUAL(state.GetRejectReason(), "bad-cb-amount");
    }
    {
        RigorSupplyDeltaInput in;
        in.height = 1;
        in.oru_coinbase = 128 * COIN;
        in.res_coinbase = 1;
        BlockValidationState state;
        BOOST_CHECK(!RigorSupplyDelta(in, params, out, state));
        BOOST_CHECK_EQUAL(state.GetRejectReason(), "bad-cb-res-amount");
    }
    BOOST_CHECK_EQUAL(RigorResCoinbaseExpected(0, 0), 0);
    BOOST_CHECK_EQUAL(Consensus::ORU_RES_TRANSFER_FEE_BURN_NUM, 1);
    BOOST_CHECK_EQUAL(Consensus::ORU_RES_TRANSFER_FEE_BURN_DEN, 2);
    BOOST_CHECK_EQUAL(Consensus::OruResTransferFeeBurnShare(100), 50);
}

BOOST_AUTO_TEST_CASE(res_max_supply_and_destroy_cost_are_closed_form)
{
    BOOST_CHECK_EQUAL(Consensus::RES_COIN, 100000000);
    BOOST_CHECK_EQUAL(Consensus::RES_COIN, COIN);
    BOOST_CHECK_EQUAL(Consensus::RES_DESTROY_COST_GENESIS, 1024 * COIN);
    BOOST_CHECK_EQUAL(Consensus::RES_DESTROY_COST_GENESIS, 102400000000LL);
    BOOST_CHECK_EQUAL(Consensus::RES_DESTROY_POLE, 33554432ULL);
    BOOST_CHECK_EQUAL(Consensus::RES_DESTROY_POLE, 1ULL << 25);
    BOOST_CHECK_EQUAL(Consensus::RES_DESTROY_SHIFT, 40);
    BOOST_CHECK_EQUAL(Consensus::RES_MAX_SUPPLY, 419430400000000LL);
    BOOST_CHECK_EQUAL(Consensus::RES_MAX_SUPPLY, (CAmount{1} << 22) * Consensus::RES_COIN);
    BOOST_CHECK_EQUAL(Consensus::ResDerivedMaxSupply(), Consensus::RES_MAX_SUPPLY);
    BOOST_CHECK_EQUAL(Consensus::ResDestroyCostAtHeight(0), 102400000000LL);
    BOOST_CHECK_EQUAL(Consensus::ResDestroyCostAtHeight(1), 102400006103LL);
    BOOST_CHECK_EQUAL(Consensus::ResMinMintBurnAtoms(0), 1024);
    BOOST_CHECK_EQUAL(Consensus::ResMinMintBurnAtoms(1), 1025);
    BOOST_CHECK_EQUAL(Consensus::MIN_CONVERGENCE_AMOUNT, 1);
    BOOST_CHECK_EQUAL(Consensus::ResCumulativeMaxAtHeight(0), 0);
    BOOST_CHECK_LT(Consensus::ResCumulativeMaxAtHeight(1), Consensus::RES_MAX_SUPPLY);
}

BOOST_AUTO_TEST_CASE(res_mint_predicate_rejects_underpay_and_free_mint)
{
    BOOST_CHECK(Consensus::CheckResMintArithmetic(Consensus::RES_COIN, 1024 * COIN, 0));
    BOOST_CHECK(!Consensus::CheckResMintArithmetic(Consensus::RES_COIN, 1024 * COIN - 1, 0));
    BOOST_CHECK(Consensus::CheckResMintArithmetic(1, 1024, 0));
    BOOST_CHECK(!Consensus::CheckResMintArithmetic(1, 1023, 0));
    BOOST_CHECK(Consensus::CheckResMintArithmetic(0, 1, 0));
    BOOST_CHECK(!Consensus::CheckResMintArithmetic(1, 0, 0));
    BOOST_CHECK(!Consensus::CheckResMintArithmetic(1, MAX_MONEY, -1));
    BOOST_CHECK(!Consensus::CheckResMintArithmetic(Consensus::RES_MAX_SUPPLY + 1, MAX_MONEY, 0));
    BOOST_CHECK(Consensus::CheckResMintArithmetic(Consensus::RES_MAX_SUPPLY, MAX_MONEY, 0));
}

BOOST_AUTO_TEST_CASE(body_mint_requires_converged_oru)
{
    const auto main = CreateChainParams(*m_node.args, CBaseChainParams::MAIN);
    const auto& params = main->GetConsensus();
    const CScript oru = OruScript();
    const CScript res = ResScript();
    const CAmount burn = 1024 * COIN;
    const uint256 hin = uint256S("0x1111111111111111111111111111111111111111111111111111111111111111");

    CCoinsView paid_base;
    CCoinsViewCache paid(&paid_base);
    paid.AddCoin(COutPoint(hin, 0), Coin(CTxOut(burn, oru), 0, false), false);

    CMutableTransaction mint;
    mint.vin.emplace_back(COutPoint(hin, 0));
    mint.vout.emplace_back(burn, GetConvergeScript(uint160()));
    mint.vout.emplace_back(Consensus::RES_COIN, res);
    TxValidationState ok_state;
    CAmount fee = -1;
    BOOST_CHECK(Consensus::CheckTxInputs(CTransaction(mint), ok_state, paid, 0, params, fee));
    BOOST_CHECK_EQUAL(fee, 0);

    CCoinsView short_base;
    CCoinsViewCache short_coins(&short_base);
    const uint256 hshort = uint256S("0x2222222222222222222222222222222222222222222222222222222222222222");
    short_coins.AddCoin(COutPoint(hshort, 0), Coin(CTxOut(burn - 1, oru), 0, false), false);
    CMutableTransaction under;
    under.vin.emplace_back(COutPoint(hshort, 0));
    under.vout.emplace_back(burn - 1, GetConvergeScript(uint160()));
    under.vout.emplace_back(Consensus::RES_COIN, res);
    TxValidationState under_state;
    BOOST_CHECK(!Consensus::CheckTxInputs(CTransaction(under), under_state, short_coins, 0, params, fee));
    BOOST_CHECK_EQUAL(under_state.GetRejectReason(), "bad-txns-res-mint-underpaid");

    CCoinsView bare_base;
    CCoinsViewCache bare(&bare_base);
    const uint256 hbare = uint256S("0x3333333333333333333333333333333333333333333333333333333333333333");
    bare.AddCoin(COutPoint(hbare, 0), Coin(CTxOut(burn, oru), 0, false), false);
    CMutableTransaction no_burn;
    no_burn.vin.emplace_back(COutPoint(hbare, 0));
    no_burn.vout.emplace_back(Consensus::RES_COIN, res);
    TxValidationState noburn_state;
    BOOST_CHECK(!Consensus::CheckTxInputs(CTransaction(no_burn), noburn_state, bare, 0, params, fee));
    BOOST_CHECK_EQUAL(noburn_state.GetRejectReason(), "bad-txns-res-mint-no-burn");
}

BOOST_AUTO_TEST_CASE(res_cannot_convert_to_oru)
{
    const auto main = CreateChainParams(*m_node.args, CBaseChainParams::MAIN);
    CCoinsView base;
    CCoinsViewCache coins(&base);
    const uint256 hres = uint256S("0x4444444444444444444444444444444444444444444444444444444444444444");
    coins.AddCoin(COutPoint(hres, 0), Coin(CTxOut(Consensus::RES_COIN, ResScript()), 0, false), false);

    CMutableTransaction tx;
    tx.vin.emplace_back(COutPoint(hres, 0));
    tx.vout.emplace_back(COIN, OruScript());
    TxValidationState state;
    CAmount fee = 0;
    BOOST_CHECK(!Consensus::CheckTxInputs(CTransaction(tx), state, coins, 1, main->GetConsensus(), fee));
    BOOST_CHECK_EQUAL(state.GetRejectReason(), "illegal-res-to-oru-conversion");
}

BOOST_AUTO_TEST_CASE(script_templates_are_closed)
{
    std::vector<std::vector<unsigned char>> sols;
    const CScript oru = OruScript();
    const CScript res = ResScript();
    const CScript burn = GetConvergeScript(uint160());
    const CScript data = DataScript();

    BOOST_CHECK(ClassifyRigorTxout(oru, sols) == TxoutType::WITNESS_V0_KEYHASH);
    BOOST_CHECK(AssetTypeFromScriptPubKey(oru) == ASSET_ORU);
    BOOST_CHECK_EQUAL(sols.at(0).size(), 20U);

    BOOST_CHECK(ClassifyRigorTxout(res, sols) == TxoutType::WITNESS_V1_RES);
    BOOST_CHECK(AssetTypeFromScriptPubKey(res) == ASSET_RES);
    BOOST_CHECK_EQUAL(sols.at(0).size(), 32U);

    BOOST_CHECK(ClassifyRigorTxout(burn, sols) == TxoutType::CONVERGE);
    BOOST_CHECK(AssetTypeFromScriptPubKey(burn) == ASSET_ORU);
    BOOST_CHECK(IsConvergenceScript(burn));
    BOOST_CHECK_EQUAL(burn.size(), 22U);
    BOOST_CHECK_EQUAL(static_cast<unsigned>(burn[21]), 0xbdu);

    BOOST_CHECK(ClassifyRigorTxout(data, sols) == TxoutType::NULL_DATA);
    BOOST_CHECK(AssetTypeFromScriptPubKey(data) == ASSET_NONE);

    CScript p2wsh = CScript() << RIGOR_PUSH_0 << std::vector<unsigned char>(32, 0x22);
    BOOST_CHECK(ClassifyRigorTxout(p2wsh, sols) == TxoutType::NONSTANDARD);
    BOOST_CHECK(AssetTypeFromScriptPubKey(p2wsh) == ASSET_INVALID);

    CScript witness_v2 = CScript() << RIGOR_PUSH_2 << std::vector<unsigned char>(32, 0x22);
    BOOST_CHECK(ClassifyRigorTxout(witness_v2, sols) == TxoutType::NONSTANDARD);

    std::vector<unsigned char> cvg2(32, 0x11);
    std::memcpy(cvg2.data(), "CVG2", 4);
    CScript cvg2_spk = CScript() << RIGOR_PUSH_1 << cvg2;
    BOOST_CHECK(ClassifyRigorTxout(cvg2_spk, sols) == TxoutType::NONSTANDARD);

    std::vector<unsigned char> res2(32, 0x11);
    std::memcpy(res2.data(), "RES2", 4);
    CScript res2_spk = CScript() << RIGOR_PUSH_1 << res2;
    BOOST_CHECK(ClassifyRigorTxout(res2_spk, sols) == TxoutType::NONSTANDARD);

    CScript p2pkh = CScript()
        << RIGOR_RESERVED_LEGACY_DUP
        << RIGOR_RESERVED_LEGACY_HASH160
        << std::vector<unsigned char>(20, 0x33)
        << RIGOR_RESERVED_LEGACY_EQUALVERIFY
        << RIGOR_RESERVED_LEGACY_CHECKSIG;
    BOOST_CHECK(ClassifyRigorTxout(p2pkh, sols) == TxoutType::NONSTANDARD);
    BOOST_CHECK(AssetTypeFromScriptPubKey(p2pkh) == ASSET_INVALID);
}

BOOST_AUTO_TEST_CASE(only_listed_templates_pass_checktransaction)
{
    {
        CMutableTransaction tx = SpendableSkeleton();
        tx.vout.emplace_back(0, DataScript());
        TxValidationState state;
        BOOST_CHECK(CheckTransaction(CTransaction(tx), state));
    }
    {
        CMutableTransaction tx = SpendableSkeleton();
        tx.vout.emplace_back(1, DataScript());
        TxValidationState state;
        BOOST_CHECK(!CheckTransaction(CTransaction(tx), state));
        BOOST_CHECK_EQUAL(state.GetRejectReason(), "bad-txns-opreturn-value");
    }
    {
        CMutableTransaction tx = SpendableSkeleton();
        CScript p2pkh = CScript()
            << RIGOR_RESERVED_LEGACY_DUP
            << RIGOR_RESERVED_LEGACY_HASH160
            << std::vector<unsigned char>(20, 0x33)
            << RIGOR_RESERVED_LEGACY_EQUALVERIFY
            << RIGOR_RESERVED_LEGACY_CHECKSIG;
        tx.vout.emplace_back(COIN, p2pkh);
        TxValidationState state;
        BOOST_CHECK(!CheckTransaction(CTransaction(tx), state));
        BOOST_CHECK_EQUAL(state.GetRejectReason(), "bad-rigor-script-type");
    }
    {
        CMutableTransaction tx = SpendableSkeleton();
        tx.vout.emplace_back(COIN, OruScript());
        tx.vout.emplace_back(Consensus::RES_COIN, ResScript());
        tx.vout.emplace_back(COIN, GetConvergeScript(uint160()));
        TxValidationState state;
        BOOST_CHECK(CheckTransaction(CTransaction(tx), state));
    }
}

BOOST_AUTO_TEST_CASE(opcode_bytes_keep_consensus_meaning)
{
    BOOST_CHECK_EQUAL(static_cast<unsigned>(RIGOR_MARKER_RETURN), 0x6au);
    BOOST_CHECK_EQUAL(static_cast<unsigned>(RIGOR_MARKER_CONVERGE), 0xbdu);
    BOOST_CHECK_EQUAL(static_cast<unsigned>(RIGOR_MARKER_RES_MINT), 0xbbu);
    BOOST_CHECK_EQUAL(static_cast<unsigned>(RIGOR_BURNED_CVG_CLAIM), 0xbcu);
    BOOST_CHECK(ClassifyRigorOpcode(0x6a) == RigorOpcodeClass::ASSET_MARKER);
    BOOST_CHECK(ClassifyRigorOpcode(0xbd) == RigorOpcodeClass::ASSET_MARKER);
    BOOST_CHECK(ClassifyRigorOpcode(0xbb) == RigorOpcodeClass::ASSET_MARKER);
    BOOST_CHECK(ClassifyRigorOpcode(0xbc) == RigorOpcodeClass::BURNED);
    BOOST_CHECK(ClassifyRigorOpcode(0xac) == RigorOpcodeClass::RESERVED_INERT);
    BOOST_CHECK(ClassifyRigorOpcode(0xff) == RigorOpcodeClass::INVALID);
}

BOOST_AUTO_TEST_CASE(authorization_is_ed25519_only)
{
    BOOST_REQUIRE(ed25519::EnsureRuntime());
    BOOST_CHECK_EQUAL(ed25519::PUBKEY_SIZE, 32U);
    BOOST_CHECK_EQUAL(ed25519::SIGNATURE_SIZE, 64U);

    const auto oru_tag = GetSighashDomainTag(SigVersion::WITNESS_V0);
    const auto res_tag = GetSighashDomainTag(SigVersion::WITNESS_V1_RES);
    BOOST_REQUIRE_EQUAL(oru_tag.size(), 8U);
    BOOST_REQUIRE_EQUAL(res_tag.size(), 8U);
    BOOST_CHECK(std::memcmp(oru_tag.data(), "RGRORU01", 8) == 0);
    BOOST_CHECK(std::memcmp(res_tag.data(), "RGRRES01", 8) == 0);
    const auto mint_tag = GetSighashDomainTag(SigVersion::RES_MINT);
    BOOST_REQUIRE_EQUAL(mint_tag.size(), 8U);
    BOOST_CHECK(std::memcmp(mint_tag.data(), "RGRMNT01", 8) == 0);

    const auto pk = ParseHex("d75a980182b10ab7d54bfed3c964073a0ee172f3daa62325af021a68f707511a");
    const auto sig = ParseHex(
        "e5564300c360ac729086e2cc806e828a84877f1eb8e5d974d873e06522490155"
        "5fb8821590a33bacc61e39701cf9b46bd25bf5f0595bbe24655141438e7a100b");
    const std::vector<unsigned char> empty;
    BOOST_CHECK_EQUAL(pk.size(), 32U);
    BOOST_CHECK_EQUAL(sig.size(), 64U);
    BOOST_CHECK(ed25519::VerifyDetached(sig, empty, pk));
    auto flipped = sig;
    flipped[0] ^= 0x01;
    BOOST_CHECK(!ed25519::VerifyDetached(flipped, empty, pk));

    BaseSignatureChecker checker;
    ScriptError err = SCRIPT_ERR_OK;
    CScriptWitness empty_wit;
    BOOST_CHECK(!VerifyRigorPredicate(CScript(), OruScript(), &empty_wit, SCRIPT_VERIFY_WITNESS, checker, &err));

    const std::vector<unsigned char> pk32(32, 0x11);
    const uint160 program = Hash160(pk32);
    CScript spk = CScript() << RIGOR_PUSH_0 << std::vector<unsigned char>(program.begin(), program.end());
    CScriptWitness bad_sig;
    bad_sig.stack.push_back(std::vector<unsigned char>(ed25519::SIGNATURE_SIZE, 0x22));
    bad_sig.stack.push_back(pk32);
    BOOST_CHECK(!VerifyRigorPredicate(CScript(), spk, &bad_sig, SCRIPT_VERIFY_WITNESS | SCRIPT_VERIFY_NULLFAIL, checker, &err));

    const std::vector<unsigned char> pk33(33, 0x02);
    const uint160 program33 = Hash160(pk33);
    CScript secp_spk = CScript() << RIGOR_PUSH_0 << std::vector<unsigned char>(program33.begin(), program33.end());
    CScriptWitness secp_wit;
    secp_wit.stack.push_back(std::vector<unsigned char>(ed25519::SIGNATURE_SIZE, 0x22));
    secp_wit.stack.push_back(pk33);
    err = SCRIPT_ERR_OK;
    BOOST_CHECK(!VerifyRigorPredicate(CScript(), secp_spk, &secp_wit, SCRIPT_VERIFY_WITNESS, checker, &err));
    BOOST_CHECK_EQUAL(err, SCRIPT_ERR_PUBKEYTYPE);
}

BOOST_AUTO_TEST_CASE(ledger_is_utxo_admission)
{
    BOOST_CHECK(EntersUtxoSet(OruScript()));
    BOOST_CHECK(EntersUtxoSet(ResScript()));
    BOOST_CHECK(!EntersUtxoSet(GetConvergeScript(uint160())));
    BOOST_CHECK(!EntersUtxoSet(DataScript()));

    CMutableTransaction mtx = SpendableSkeleton();
    mtx.vout.emplace_back(COIN, OruScript());
    mtx.vout.emplace_back(Consensus::RES_COIN, ResScript());
    mtx.vout.emplace_back(COIN, GetConvergeScript(uint160()));
    mtx.vout.emplace_back(0, DataScript());
    const CTransaction tx{mtx};

    CCoinsView base;
    CCoinsViewCache coins(&base);
    AddCoins(coins, tx, 1, false);
    BOOST_CHECK(coins.HaveCoin(COutPoint(tx.GetHash(), 0)));
    BOOST_CHECK(coins.HaveCoin(COutPoint(tx.GetHash(), 1)));
    BOOST_CHECK(!coins.HaveCoin(COutPoint(tx.GetHash(), 2)));
    BOOST_CHECK(!coins.HaveCoin(COutPoint(tx.GetHash(), 3)));
}

BOOST_AUTO_TEST_CASE(pow_is_sha3_rgrpow01)
{
    BOOST_CHECK_EQUAL(std::string(rigor_kernel::POW_DOMAIN_TAG, rigor_kernel::POW_DOMAIN_TAG_LEN), "RGRPOW01");
    BOOST_CHECK_EQUAL(rigor_kernel::POW_DOMAIN_TAG_LEN, 8U);
    BOOST_CHECK_EQUAL(rigor_kernel::POW_HEADER_LEN, 80U);
    BOOST_CHECK_EQUAL(rigor_kernel::POW_PREIMAGE_LEN, 88U);

    const CBlockHeader zero;
    BOOST_CHECK_EQUAL(rigor_kernel::GetRigorPowHash(zero), ZERO_HEADER_POW);

    const auto main = CreateChainParams(*m_node.args, CBaseChainParams::MAIN);
    const auto& consensus = main->GetConsensus();
    const CBlock& genesis = main->GenesisBlock();
    BOOST_CHECK(CheckProofOfWork(genesis, genesis.nBits, consensus));

    CBlockHeader mutated = genesis;
    mutated.nNonce = genesis.nNonce + 1;
    BOOST_CHECK(mutated.GetHash() != consensus.hashGenesisBlock);
    arith_uint256 target;
    bool negative = false;
    bool overflow = false;
    target.SetCompact(mutated.nBits, &negative, &overflow);
    BOOST_CHECK(!negative);
    BOOST_CHECK(!overflow);
    BOOST_CHECK(UintToArith256(rigor_kernel::GetRigorPowHash(mutated)) > target);
    BOOST_CHECK(!CheckProofOfWork(mutated, mutated.nBits, consensus));

    CBlockHeader untagged_probe;
    BOOST_CHECK(untagged_probe.GetHash() != consensus.hashGenesisBlock);
    const arith_uint256 limit = UintToArith256(consensus.powLimit);
    const unsigned bits = limit.GetCompact();
    const bool under = !(UintToArith256(rigor_kernel::GetRigorPowHash(untagged_probe)) > limit);
    BOOST_CHECK(!under);
    BOOST_CHECK_EQUAL(CheckProofOfWork(untagged_probe, bits, consensus), under);
}

BOOST_AUTO_TEST_CASE(mainnet_genesis_identity)
{
    const auto main = CreateChainParams(*m_node.args, CBaseChainParams::MAIN);
    const auto& consensus = main->GetConsensus();
    const CBlock& genesis = main->GenesisBlock();

    BOOST_CHECK_EQUAL(genesis.GetHash(), MAINNET_GENESIS);
    BOOST_CHECK_EQUAL(consensus.hashGenesisBlock, MAINNET_GENESIS);
    BOOST_CHECK_EQUAL(genesis.hashMerkleRoot, MAINNET_MERKLE);
    BOOST_CHECK_EQUAL(genesis.hashMerkleRoot, BlockMerkleRoot(genesis));
    BOOST_CHECK(genesis.hashPrevBlock.IsNull());
    BOOST_CHECK_EQUAL(genesis.nVersion, 1);
    BOOST_CHECK_EQUAL(genesis.nTime, 1792908000U);
    BOOST_CHECK_EQUAL(genesis.nBits, 0x1d008000U);
    BOOST_CHECK_EQUAL(genesis.nNonce, 0U);
    BOOST_REQUIRE_EQUAL(genesis.vtx.size(), 1U);
    BOOST_CHECK_EQUAL(genesis.vtx[0]->vout[0].nValue, 0);
    const std::string sig(genesis.vtx[0]->vin[0].scriptSig.begin(), genesis.vtx[0]->vin[0].scriptSig.end());
    BOOST_CHECK(sig.find("Selection not computation. Convergence defines state. Naming is consensus.") != std::string::npos);
    BOOST_CHECK(consensus.hashGenesisBlock != uint256S("0x1c2d643638c3db29c241f4daca5bbe90f8439f7b03d22a686247214ed52ad758"));
}

BOOST_AUTO_TEST_CASE(no_versionbits_activation)
{
    const auto main = CreateChainParams(*m_node.args, CBaseChainParams::MAIN);
    const auto& consensus = main->GetConsensus();
    BOOST_CHECK_EQUAL(static_cast<int>(Consensus::MAX_VERSION_BITS_DEPLOYMENTS), 1);
    BOOST_CHECK_EQUAL(consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime, Consensus::BIP9Deployment::NEVER_ACTIVE);
    BOOST_CHECK(consensus.vDeployments[Consensus::DEPLOYMENT_TESTDUMMY].nStartTime != Consensus::BIP9Deployment::ALWAYS_ACTIVE);
}

BOOST_AUTO_TEST_SUITE_END()
