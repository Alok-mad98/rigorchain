// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <coins.h>
#include <consensus/convergenceres.h>
#include <consensus/res_amount.h>
#include <key.h>
#include <key_io.h>
#include <primitives/transaction.h>
#include <script/predicate_verify.h>
#include <script/standard.h>
#include <wallet/tx_semantic.h>

#include <array>

#include <boost/test/unit_test.hpp>
#include <test/util/setup_common.h>

namespace wallet {
BOOST_FIXTURE_TEST_SUITE(wallet_tx_semantic_tests, TestingSetup)

static void AddPrevout(CCoinsViewCache& view, const CTransactionRef& prev, uint32_t n)
{
    view.AddCoin(COutPoint(prev->GetHash(), n), Coin(prev->vout[n], 1, false), false);
}

BOOST_AUTO_TEST_CASE(semantic_oru_transfer)
{
    CCoinsView empty;
    CCoinsViewCache view(&empty);
    CKey key;
    key.MakeNewKey(true);
    const CScript oru_spk = GetScriptForDestination(WitnessV0KeyHash(key.GetPubKey().GetID()));

    CMutableTransaction prev;
    prev.vout.emplace_back(COIN, oru_spk);
    const CTransactionRef prev_tx = MakeTransactionRef(prev);
    AddPrevout(view, prev_tx, 0);

    CMutableTransaction tx;
    tx.vin.emplace_back(COutPoint(prev_tx->GetHash(), 0), CScript{});
    tx.vout.emplace_back(COIN - 1000, oru_spk);

    BOOST_CHECK(ClassifyTxSemantic(*MakeTransactionRef(tx), view) == TxSemanticKind::OruTransfer);
    BOOST_CHECK(TxSemanticCanonicalLabel(TxSemanticKind::OruTransfer) == "ORU Transfer");
}

BOOST_AUTO_TEST_CASE(semantic_res_transfer)
{
    CCoinsView empty;
    CCoinsViewCache view(&empty);
    std::array<unsigned char, 32> res_prog{};
    res_prog.fill(0x22);
    const CScript res_spk = GetScriptForDestination(WitnessV1Res{XOnlyPubKey{res_prog}});

    CMutableTransaction prev;
    prev.vout.emplace_back(RES_COIN, res_spk);
    const CTransactionRef prev_tx = MakeTransactionRef(prev);
    AddPrevout(view, prev_tx, 0);

    CMutableTransaction tx;
    tx.vin.emplace_back(COutPoint(prev_tx->GetHash(), 0), CScript{});
    tx.vout.emplace_back(RES_COIN - 1, res_spk);

    BOOST_CHECK(ClassifyTxSemantic(*MakeTransactionRef(tx), view) == TxSemanticKind::ResTransfer);
}

BOOST_AUTO_TEST_CASE(semantic_res_mint)
{
    CCoinsView empty;
    CCoinsViewCache view(&empty);
    CKey key;
    key.MakeNewKey(true);
    const CScript oru_spk = GetScriptForDestination(WitnessV0KeyHash(key.GetPubKey().GetID()));
    std::array<unsigned char, 32> res_prog{};
    res_prog.fill(0x33);
    const CScript res_spk = GetScriptForDestination(WitnessV1Res{XOnlyPubKey{res_prog}});

    CMutableTransaction prev;
    prev.vout.emplace_back(COIN, oru_spk);
    const CTransactionRef prev_tx = MakeTransactionRef(prev);
    AddPrevout(view, prev_tx, 0);

    CMutableTransaction tx;
    tx.vin.emplace_back(COutPoint(prev_tx->GetHash(), 0), CScript{});
    tx.vout.emplace_back(RES_COIN, res_spk);

    BOOST_CHECK(ClassifyTxSemantic(*MakeTransactionRef(tx), view) == TxSemanticKind::ResMint);
}

BOOST_AUTO_TEST_CASE(semantic_oru_convergence)
{
    CCoinsView empty;
    CCoinsViewCache view(&empty);
    CKey key;
    key.MakeNewKey(true);
    const CScript oru_spk = GetScriptForDestination(WitnessV0KeyHash(key.GetPubKey().GetID()));

    CMutableTransaction prev;
    prev.vout.emplace_back(COIN, oru_spk);
    const CTransactionRef prev_tx = MakeTransactionRef(prev);
    AddPrevout(view, prev_tx, 0);

    CMutableTransaction tx;
    tx.vin.emplace_back(COutPoint(prev_tx->GetHash(), 0), CScript{});
    tx.vout.emplace_back(COIN / 2, Consensus::ConvergenceOutputScript());

    BOOST_CHECK(ClassifyTxSemantic(*MakeTransactionRef(tx), view) == TxSemanticKind::OruConvergence);
}

BOOST_AUTO_TEST_CASE(semantic_labels_forbid_bitcoin_type_strings)
{
    BOOST_CHECK(TxSemanticCanonicalLabel(TxSemanticKind::OruCoinbase).find("Mined") == std::string::npos);
    BOOST_CHECK(TxSemanticCanonicalLabel(TxSemanticKind::OruTransfer).find("Sent to") == std::string::npos);
    BOOST_CHECK(TxSemanticCanonicalLabel(TxSemanticKind::ResTransfer).find("Received with") == std::string::npos);
}

BOOST_AUTO_TEST_SUITE_END()
} // namespace wallet
