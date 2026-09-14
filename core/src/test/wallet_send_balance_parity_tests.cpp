// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <consensus/amount.h>
#include <policy/feerate.h>
#include <policy/policy.h>
#include <script/standard.h>
#include <test/util/setup_common.h>
#include <validation.h>
#include <wallet/coincontrol.h>
#include <wallet/receive.h>
#include <wallet/spend.h>
#include <wallet/test/util.h>
#include <wallet/transaction.h>
#include <wallet/wallet.h>

#include <boost/test/unit_test.hpp>

namespace wallet {

namespace {

CAmount AvailableOru(const CWallet& w)
{
    LOCK(w.cs_wallet);
    CCoinControl cc;
    cc.m_asset_type = ASSET_ORU;
    CoinFilterParams params;
    params.m_asset_type = ASSET_ORU;
    return AvailableCoins(w, &cc, std::nullopt, params).GetTotalAmount();
}

void SyncWalletTip(CWallet& w, const TestChain100Setup& setup)
{
    LOCK(w.cs_wallet);
    LOCK(Assert(setup.m_node.chainman)->GetMutex());
    w.SetLastBlockProcessed(setup.m_node.chainman->ActiveChain().Height(),
                            setup.m_node.chainman->ActiveChain().Tip()->GetBlockHash());
}

void CommitAndMine(CWallet& w, TestChain100Setup& setup, const CTransactionRef& tx)
{
    w.CommitTransaction(tx, {}, {});
    CMutableTransaction blocktx;
    {
        LOCK(w.cs_wallet);
        blocktx = CMutableTransaction(*w.mapWallet.at(tx->GetHash()).tx);
    }
    setup.CreateAndProcessBlock({CMutableTransaction(blocktx)}, GetTestMiningCoinbaseScript(setup.coinbaseKey));
    SyncWalletTip(w, setup);
    LOCK(w.cs_wallet);
    auto it = w.mapWallet.find(tx->GetHash());
    BOOST_REQUIRE(it != w.mapWallet.end());
    it->second.m_state = TxStateConfirmed{setup.m_node.chainman->ActiveChain().Tip()->GetBlockHash(),
                                          setup.m_node.chainman->ActiveChain().Height(), /*index=*/1};
}

/**
 * Fund a confirmed ORU UTXO of `balance_atomic` and lock every other ORU coin
 * so AvailableCoins / CreateTransaction see exactly that balance.
 */
void FundExactOruBalance(CWallet& w, TestChain100Setup& setup, CAmount balance_atomic)
{
    const auto dest = w.GetNewDestination(OutputType::BECH32, "parity_fund");
    BOOST_REQUIRE(dest);
    const CScript fund_script = GetScriptForDestination(*dest);
    CRecipient fund{fund_script, balance_atomic, /*subtract_fee=*/false, ASSET_ORU};
    CCoinControl cc;
    cc.m_feerate = CFeeRate(DEFAULT_MIN_RELAY_TX_FEE);
    cc.fOverrideFeeRate = true;
    auto created = CreateTransaction(w, {fund}, /*change_pos=*/-1, cc);
    BOOST_REQUIRE_MESSAGE(created, util::ErrorString(created).original);
    CommitAndMine(w, setup, created->tx);

    LOCK(w.cs_wallet);
    CoinFilterParams params;
    params.m_asset_type = ASSET_ORU;
    COutPoint funded;
    for (const COutput& c : AvailableCoins(w, nullptr, std::nullopt, params).All()) {
        if (c.txout.nValue == balance_atomic && c.txout.scriptPubKey == fund_script) {
            funded = c.outpoint;
            break;
        }
    }
    BOOST_REQUIRE(!funded.IsNull());
    for (const COutput& c : AvailableCoins(w, nullptr, std::nullopt, params).All()) {
        if (c.outpoint != funded) {
            w.LockCoin(c.outpoint);
        }
    }
}

CCoinControl DefaultOruFeeControl()
{
    CCoinControl cc;
    cc.m_asset_type = ASSET_ORU;
    cc.m_feerate = CFeeRate(DEFAULT_MIN_RELAY_TX_FEE);
    cc.fOverrideFeeRate = true;
    return cc;
}

CAmount RecipientAmount(const CreatedTransactionResult& r)
{
    CAmount a = 0;
    for (size_t i = 0; i < r.tx->vout.size(); ++i) {
        if (static_cast<int>(i) == r.change_pos) continue;
        a += r.tx->vout[i].nValue;
    }
    return a;
}

CAmount ChangeAmount(const CreatedTransactionResult& r)
{
    if (r.change_pos < 0) return 0;
    return r.tx->vout[r.change_pos].nValue;
}

} // namespace

BOOST_FIXTURE_TEST_SUITE(wallet_send_balance_parity_tests, TestChain100Setup)

BOOST_AUTO_TEST_CASE(case1_send_half_balance)
{
    CreateAndProcessBlock({}, GetTestMiningCoinbaseScript(coinbaseKey));
    auto wallet = CreateSyncedWallet(*m_node.chain, WITH_LOCK(Assert(m_node.chainman)->GetMutex(), return m_node.chainman->ActiveChain()), coinbaseKey);
    constexpr CAmount BALANCE = 100000;
    FundExactOruBalance(*wallet, *this, BALANCE);
    BOOST_CHECK_EQUAL(AvailableOru(*wallet), BALANCE);

    const auto dest = wallet->GetNewDestination(OutputType::BECH32, "recv");
    BOOST_REQUIRE(dest);
    CRecipient recipient{GetScriptForDestination(*dest), 50000, /*subtract_fee=*/false, ASSET_ORU};
    auto res = CreateTransaction(*wallet, {recipient}, /*change_pos=*/-1, DefaultOruFeeControl());
    BOOST_REQUIRE_MESSAGE(res, util::ErrorString(res).original);
    BOOST_CHECK_GT(res->fee, 0);
    BOOST_CHECK_EQUAL(RecipientAmount(*res), 50000);
    BOOST_CHECK_EQUAL(RecipientAmount(*res) + ChangeAmount(*res) + res->fee, BALANCE);
}

BOOST_AUTO_TEST_CASE(case2_near_full_sffo_off)
{
    CreateAndProcessBlock({}, GetTestMiningCoinbaseScript(coinbaseKey));
    auto wallet = CreateSyncedWallet(*m_node.chain, WITH_LOCK(Assert(m_node.chainman)->GetMutex(), return m_node.chainman->ActiveChain()), coinbaseKey);
    constexpr CAmount BALANCE = 100000;
    FundExactOruBalance(*wallet, *this, BALANCE);

    const auto dest = wallet->GetNewDestination(OutputType::BECH32, "recv");
    BOOST_REQUIRE(dest);
    CRecipient recipient{GetScriptForDestination(*dest), 99000, /*subtract_fee=*/false, ASSET_ORU};
    auto res = CreateTransaction(*wallet, {recipient}, /*change_pos=*/-1, DefaultOruFeeControl());
    BOOST_REQUIRE_MESSAGE(res, util::ErrorString(res).original);
    BOOST_CHECK_EQUAL(RecipientAmount(*res), 99000);
    BOOST_CHECK_EQUAL(RecipientAmount(*res) + ChangeAmount(*res) + res->fee, BALANCE);
}

BOOST_AUTO_TEST_CASE(case3_near_full_sffo_on)
{
    CreateAndProcessBlock({}, GetTestMiningCoinbaseScript(coinbaseKey));
    auto wallet = CreateSyncedWallet(*m_node.chain, WITH_LOCK(Assert(m_node.chainman)->GetMutex(), return m_node.chainman->ActiveChain()), coinbaseKey);
    constexpr CAmount BALANCE = 100000;
    FundExactOruBalance(*wallet, *this, BALANCE);

    const auto dest = wallet->GetNewDestination(OutputType::BECH32, "recv");
    BOOST_REQUIRE(dest);
    CRecipient recipient{GetScriptForDestination(*dest), 99000, /*subtract_fee=*/true, ASSET_ORU};
    auto res = CreateTransaction(*wallet, {recipient}, /*change_pos=*/-1, DefaultOruFeeControl());
    BOOST_REQUIRE_MESSAGE(res, util::ErrorString(res).original);
    BOOST_CHECK_GT(res->fee, 0);
    BOOST_CHECK_LT(RecipientAmount(*res), 99000);
    BOOST_CHECK_EQUAL(RecipientAmount(*res) + ChangeAmount(*res) + res->fee, BALANCE);
}

BOOST_AUTO_TEST_CASE(case3b_sffo_on_vs_off_recipient_differs)
{
    CreateAndProcessBlock({}, GetTestMiningCoinbaseScript(coinbaseKey));
    auto wallet = CreateSyncedWallet(*m_node.chain, WITH_LOCK(Assert(m_node.chainman)->GetMutex(), return m_node.chainman->ActiveChain()), coinbaseKey);
    constexpr CAmount BALANCE = 100000;
    FundExactOruBalance(*wallet, *this, BALANCE);

    const auto dest = wallet->GetNewDestination(OutputType::BECH32, "recv");
    BOOST_REQUIRE(dest);
    const CScript script = GetScriptForDestination(*dest);

    CCoinControl cc = DefaultOruFeeControl();
    cc.m_feerate = CFeeRate(10000);

    CRecipient off{script, 50000, /*subtract_fee=*/false, ASSET_ORU};
    CRecipient on{script, 50000, /*subtract_fee=*/true, ASSET_ORU};

    auto res_off = CreateTransaction(*wallet, {off}, /*change_pos=*/-1, cc);
    auto res_on = CreateTransaction(*wallet, {on}, /*change_pos=*/-1, cc);
    BOOST_REQUIRE_MESSAGE(res_off, util::ErrorString(res_off).original);
    BOOST_REQUIRE_MESSAGE(res_on, util::ErrorString(res_on).original);

    BOOST_CHECK_EQUAL(RecipientAmount(*res_off), 50000);
    BOOST_CHECK_LT(RecipientAmount(*res_on), 50000);
    BOOST_CHECK_EQUAL(RecipientAmount(*res_on) + ChangeAmount(*res_on) + res_on->fee, BALANCE);
}

BOOST_AUTO_TEST_CASE(case4_insufficient_confirmed_fails)
{
    CreateAndProcessBlock({}, GetTestMiningCoinbaseScript(coinbaseKey));
    auto wallet = CreateSyncedWallet(*m_node.chain, WITH_LOCK(Assert(m_node.chainman)->GetMutex(), return m_node.chainman->ActiveChain()), coinbaseKey);
    constexpr CAmount BALANCE = 100000;
    FundExactOruBalance(*wallet, *this, BALANCE);

    const auto dest = wallet->GetNewDestination(OutputType::BECH32, "recv");
    BOOST_REQUIRE(dest);
    CRecipient recipient{GetScriptForDestination(*dest), BALANCE + 1, /*subtract_fee=*/false, ASSET_ORU};
    auto res = CreateTransaction(*wallet, {recipient}, /*change_pos=*/-1, DefaultOruFeeControl());
    BOOST_CHECK(!res);
}

BOOST_AUTO_TEST_CASE(case5_immature_coinbase_not_spendable)
{
    CreateAndProcessBlock({}, GetTestMiningCoinbaseScript(coinbaseKey));
    auto wallet = CreateSyncedWallet(*m_node.chain, WITH_LOCK(Assert(m_node.chainman)->GetMutex(), return m_node.chainman->ActiveChain()), coinbaseKey);

    BOOST_CHECK_GT(GetAssetImmatureBalance(*wallet, ASSET_ORU, ISMINE_SPENDABLE), 0);

    {
        LOCK(wallet->cs_wallet);
        CoinFilterParams params;
        params.m_asset_type = ASSET_ORU;
        for (const COutput& c : AvailableCoins(*wallet, nullptr, std::nullopt, params).All()) {
            wallet->LockCoin(c.outpoint);
        }
    }
    BOOST_CHECK_EQUAL(AvailableOru(*wallet), 0);

    const auto dest = wallet->GetNewDestination(OutputType::BECH32, "recv");
    BOOST_REQUIRE(dest);
    CRecipient recipient{GetScriptForDestination(*dest), 1 * COIN, /*subtract_fee=*/false, ASSET_ORU};
    auto res = CreateTransaction(*wallet, {recipient}, /*change_pos=*/-1, DefaultOruFeeControl());
    BOOST_CHECK(!res);
}

BOOST_AUTO_TEST_CASE(case6_change_output_generated)
{
    CreateAndProcessBlock({}, GetTestMiningCoinbaseScript(coinbaseKey));
    auto wallet = CreateSyncedWallet(*m_node.chain, WITH_LOCK(Assert(m_node.chainman)->GetMutex(), return m_node.chainman->ActiveChain()), coinbaseKey);
    constexpr CAmount BALANCE = 100000;
    FundExactOruBalance(*wallet, *this, BALANCE);

    const auto dest = wallet->GetNewDestination(OutputType::BECH32, "recv");
    BOOST_REQUIRE(dest);
    CRecipient recipient{GetScriptForDestination(*dest), 40000, /*subtract_fee=*/false, ASSET_ORU};
    auto res = CreateTransaction(*wallet, {recipient}, /*change_pos=*/-1, DefaultOruFeeControl());
    BOOST_REQUIRE_MESSAGE(res, util::ErrorString(res).original);
    BOOST_CHECK_GE(res->change_pos, 0);
    BOOST_CHECK_EQUAL(res->tx->vout.size(), 2U);
    BOOST_CHECK_EQUAL(AssetTypeFromScriptPubKey(res->tx->vout[res->change_pos].scriptPubKey), ASSET_ORU);
    BOOST_CHECK_EQUAL(res->tx->GetValueOut(ASSET_RES), 0);
}

BOOST_AUTO_TEST_CASE(sffo_full_balance_vs_insufficient_without)
{
    // Classic UI symptom: "use available balance" with SFFO ON must succeed;
    // the same amount with SFFO OFF must fail (fee has nowhere to come from).
    CreateAndProcessBlock({}, GetTestMiningCoinbaseScript(coinbaseKey));
    auto wallet = CreateSyncedWallet(*m_node.chain, WITH_LOCK(Assert(m_node.chainman)->GetMutex(), return m_node.chainman->ActiveChain()), coinbaseKey);
    constexpr CAmount BALANCE = 100000;
    FundExactOruBalance(*wallet, *this, BALANCE);

    const auto dest = wallet->GetNewDestination(OutputType::BECH32, "sffo");
    BOOST_REQUIRE(dest);
    const CScript script = GetScriptForDestination(*dest);
    CCoinControl cc = DefaultOruFeeControl();
    cc.m_feerate = CFeeRate(10000);

    CRecipient with_sffo{script, BALANCE, /*subtract_fee=*/true, ASSET_ORU};
    auto res_on = CreateTransaction(*wallet, {with_sffo}, /*change_pos=*/-1, cc);
    BOOST_REQUIRE_MESSAGE(res_on, util::ErrorString(res_on).original);
    BOOST_CHECK_GT(res_on->fee, 0);
    BOOST_CHECK_LT(RecipientAmount(*res_on), BALANCE);
    BOOST_CHECK_EQUAL(RecipientAmount(*res_on) + ChangeAmount(*res_on) + res_on->fee, BALANCE);

    CRecipient without_sffo{script, BALANCE, /*subtract_fee=*/false, ASSET_ORU};
    auto res_off = CreateTransaction(*wallet, {without_sffo}, /*change_pos=*/-1, cc);
    BOOST_CHECK_MESSAGE(!res_off, "SFFO OFF must fail when amount equals full spendable balance");
}

BOOST_AUTO_TEST_CASE(oru_send_ignores_res_balance)
{
    CreateAndProcessBlock({}, GetTestMiningCoinbaseScript(coinbaseKey));
    auto wallet = CreateSyncedWallet(*m_node.chain, WITH_LOCK(Assert(m_node.chainman)->GetMutex(), return m_node.chainman->ActiveChain()), coinbaseKey);
    constexpr CAmount BALANCE = 100000;
    FundExactOruBalance(*wallet, *this, BALANCE);
    const CAmount res_trusted = GetAssetBalance(*wallet, ASSET_RES, 1).trusted;

    const auto dest = wallet->GetNewDestination(OutputType::BECH32, "recv");
    BOOST_REQUIRE(dest);
    CRecipient recipient{GetScriptForDestination(*dest), BALANCE + res_trusted + 1, false, ASSET_ORU};
    auto res = CreateTransaction(*wallet, {recipient}, -1, DefaultOruFeeControl());
    BOOST_CHECK(!res);
}

BOOST_AUTO_TEST_CASE(available_oru_p2wpkh_input_bytes_known)
{
    // ALPHA1-RELEASE-BLOCKER-FIX-001: dummy Ed25519 fee signatures must yield
    // input_bytes > 0 so sendall / coin-control available-balance never abort.
    CreateAndProcessBlock({}, GetTestMiningCoinbaseScript(coinbaseKey));
    auto wallet = CreateSyncedWallet(*m_node.chain, WITH_LOCK(Assert(m_node.chainman)->GetMutex(), return m_node.chainman->ActiveChain()), coinbaseKey);
    constexpr CAmount BALANCE = 100000;
    FundExactOruBalance(*wallet, *this, BALANCE);

    LOCK(wallet->cs_wallet);
    CoinFilterParams params;
    params.m_asset_type = ASSET_ORU;
    int n = 0;
    for (const COutput& c : AvailableCoins(*wallet, nullptr, std::nullopt, params).All()) {
        BOOST_CHECK_EQUAL(AssetTypeFromScriptPubKey(c.txout.scriptPubKey), ASSET_ORU);
        BOOST_CHECK_GT(c.input_bytes, 0);
        ++n;
    }
    BOOST_CHECK_GT(n, 0);
}

// A transaction the node refuses must not leave its inputs marked spent. Keeping the
// local spend made IsSpent() hide the whole input from AvailableCoins and from the
// per-asset balances, with no error to the caller and no way back except a manual
// abandontransaction (docs/audit/ORU_DESTROY_DUST_SPENDMAP_AUDIT.md).
BOOST_AUTO_TEST_CASE(rejected_broadcast_keeps_inputs_spendable)
{
    CreateAndProcessBlock({}, GetTestMiningCoinbaseScript(coinbaseKey));
    auto wallet = CreateSyncedWallet(*m_node.chain, WITH_LOCK(Assert(m_node.chainman)->GetMutex(), return m_node.chainman->ActiveChain()), coinbaseKey);
    constexpr CAmount BALANCE = 100000;
    FundExactOruBalance(*wallet, *this, BALANCE);
    BOOST_CHECK_EQUAL(AvailableOru(*wallet), BALANCE);
    wallet->SetBroadcastTransactions(true);

    const auto dest = wallet->GetNewDestination(OutputType::BECH32, "recv");
    BOOST_REQUIRE(dest);
    CRecipient recipient{GetScriptForDestination(*dest), 40000, /*subtract_fee=*/false, ASSET_ORU};
    CCoinControl cc = DefaultOruFeeControl();
    auto res = CreateTransaction(*wallet, {recipient}, /*change_pos=*/-1, cc);
    BOOST_REQUIRE_MESSAGE(res, util::ErrorString(res).original);
    const CTransactionRef tx = res->tx;
    BOOST_REQUIRE(!tx->vin.empty());

    // Cap the fee below what this transaction already pays, but only now that it is
    // built, so the refusal happens at broadcast instead of in CreateTransaction.
    wallet->m_default_max_tx_fee = 1;

    std::string broadcast_error;
    BOOST_CHECK(!wallet->CommitTransaction(tx, {}, {}, &broadcast_error));
    BOOST_CHECK(!broadcast_error.empty());

    {
        LOCK(wallet->cs_wallet);
        BOOST_CHECK(wallet->mapWallet.at(tx->GetHash()).isAbandoned());
        for (const CTxIn& txin : tx->vin) {
            BOOST_CHECK(!wallet->IsSpent(txin.prevout));
        }
    }
    // The whole selected input is back, not just the amount that would have been change.
    BOOST_CHECK_EQUAL(AvailableOru(*wallet), BALANCE);
}

BOOST_AUTO_TEST_SUITE_END()

} // namespace wallet
