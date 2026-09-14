// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <chainparams.h>
#include <coins.h>
#include <consensus/amount.h>
#include <consensus/convergenceres.h>
#include <consensus/res_amount.h>
#include <consensus/res_proof.h>
#include <consensus/rigor_consensus_lock.h>
#include <consensus/tx_check.h>
#include <consensus/tx_verify.h>
#include <consensus/validation.h>
#include <primitives/transaction.h>
#include <script/predicate_verify.h>
#include <script/script.h>
#include <script/standard.h>
#include <test/fuzz/FuzzedDataProvider.h>
#include <test/fuzz/fuzz.h>
#include <test/fuzz/util.h>
#include <test/util/setup_common.h>
#include <uint256.h>

#include <algorithm>
#include <cassert>
#include <cstdint>
#include <cstring>
#include <numeric>
#include <vector>

namespace {

const TestingSetup* g_setup;

CScript OruSpk()
{
    return CScript() << RIGOR_PUSH_0 << std::vector<unsigned char>(20, 0x11);
}

CScript ResSpk(uint8_t fill)
{
    return CScript() << RIGOR_PUSH_1 << std::vector<unsigned char>(WITNESS_V1_RES_SIZE, fill);
}

CScript Cvg2Spk()
{
    return Consensus::ConvergenceOutputScript();
}

CAmount ConsumeResAmount(FuzzedDataProvider& p)
{
    CAmount n = ConsumeMoney(p);
    n -= n % RES_ATOMIC_UNIT;
    return n;
}

CScript ConsumeOutputSpk(FuzzedDataProvider& p, int spend_height)
{
    CScript out;
    CallOneOf(
        p,
        [&] { out = OruSpk(); },
        [&] { out = ResSpk(0xab); },
        [&] { out = ResSpk(0xcd); },
        [&] { out = Cvg2Spk(); },
        [&] {
            out = GetResMintScriptForPubKey(
                static_cast<int64_t>(spend_height) + Params().GetConsensus().nCoinbaseMaturity,
                std::vector<unsigned char>(32, 0x11));
        },
        [&] { out = CScript() << RIGOR_MARKER_RETURN << std::vector<unsigned char>{0xaa}; });
    return out;
}

CAmount ConsumeOutputValue(FuzzedDataProvider& p, const CScript& spk, int spend_height)
{
    if (spk.IsUnspendable()) return 0;
    if (IsResScript(spk)) {
        // RFC-020: RES is minted at destroy_cost(h), not a scheduled reward. For fuzzing use RES_COIN.
        if (p.ConsumeBool()) {
            return RES_COIN;
        }
        const CAmount n = ConsumeResAmount(p);
        return n == 0 ? RES_ATOMIC_UNIT : n;
    }
    if (IsConvergenceScript(spk) && p.ConsumeBool()) {
        return Consensus::MIN_CONVERGENCE_AMOUNT;
    }
    return ConsumeMoney(p, 20 * COIN);
}

} // namespace

void initialize_res_checktxinputs()
{
    static const auto testing_setup = MakeNoLogFileContext<const TestingSetup>();
    g_setup = testing_setup.get();
}

FUZZ_TARGET_INIT(res_checktxinputs, initialize_res_checktxinputs)
{
    (void)g_setup;
    FuzzedDataProvider fuzzed_data_provider{buffer.data(), buffer.size()};

    const Consensus::Params& params = Params().GetConsensus();
    const int spend_height = fuzzed_data_provider.ConsumeIntegralInRange<int>(0, 10'000);

    CCoinsView backend;
    CCoinsViewCache view{&backend};

    struct CatalogCoin {
        COutPoint prevout;
    };
    std::vector<CatalogCoin> catalog;

    auto add_coin = [&](uint8_t id, CTxOut out, int height, uint32_t flags) {
        CatalogCoin entry;
        entry.prevout = COutPoint(uint256{id}, 0);
        Coin coin{std::move(out), height, /*fCoinBaseIn=*/false};
        (void)flags;
        view.AddCoin(entry.prevout, std::move(coin), /*possible_overwrite=*/false);
        catalog.push_back(entry);
    };

    add_coin(1, CTxOut{20 * COIN, OruSpk()}, /*height=*/0, /*flags=*/0);
    add_coin(2, CTxOut{ConsumeMoney(fuzzed_data_provider, 20 * COIN), OruSpk()}, 0, 0);
    add_coin(3, CTxOut{10 * RES_COIN, ResSpk(0xab)}, 0, 0);
    add_coin(4, CTxOut{ConsumeResAmount(fuzzed_data_provider), ResSpk(0xcd)}, 0, 0);
    add_coin(5, CTxOut{10 * COIN, Cvg2Spk()}, 0, 0);
    add_coin(6, CTxOut{10 * RES_COIN, ResSpk(0xab)}, spend_height, 0);
    add_coin(7, CTxOut{10 * RES_COIN, ResSpk(0xab)}, 0, 0);

    std::vector<size_t> order(catalog.size());
    std::iota(order.begin(), order.end(), 0);
    for (size_t i = 0; i < order.size(); ++i) {
        const size_t j = fuzzed_data_provider.ConsumeIntegralInRange<size_t>(0, order.size() - 1);
        std::swap(order[i], order[j]);
    }
    const size_t n_in = fuzzed_data_provider.ConsumeIntegralInRange<size_t>(1, catalog.size());
    const size_t n_out = fuzzed_data_provider.ConsumeIntegralInRange<size_t>(1, 4);

    CMutableTransaction mtx;
    mtx.nVersion = 2;
    mtx.vin.resize(n_in);
    for (size_t i = 0; i < n_in; ++i) {
        mtx.vin[i].prevout = catalog[order[i]].prevout;
        mtx.vin[i].nSequence = CTxIn::SEQUENCE_FINAL;
    }
    mtx.vout.resize(n_out);
    for (size_t i = 0; i < n_out; ++i) {
        mtx.vout[i].scriptPubKey = ConsumeOutputSpk(fuzzed_data_provider, spend_height);
        mtx.vout[i].nValue = ConsumeOutputValue(fuzzed_data_provider, mtx.vout[i].scriptPubKey, spend_height);
    }

    if (fuzzed_data_provider.ConsumeBool()) {
        mtx.vin[0].scriptWitness = ConsumeScriptWitness(fuzzed_data_provider, /*max_stack_elem_size=*/80);
    }

    const CTransaction tx{mtx};
    TxValidationState dummy;
    if (!CheckTransaction(tx, dummy)) {
        return;
    }
    if (!view.HaveInputs(tx)) {
        return;
    }

    bool spent_cvg2 = false;
    CAmount res_in = 0;
    CAmount oru_in = 0;
    for (const CTxIn& txin : tx.vin) {
        const Coin& coin = view.AccessCoin(txin.prevout);
        if (coin.IsSpent()) return;
        if (IsConvergenceScript(coin.out.scriptPubKey)) spent_cvg2 = true;
        const AssetType a = AssetTypeFromScriptPubKey(coin.out.scriptPubKey);
        if (a == ASSET_RES) res_in += coin.out.nValue;
        else if (a == ASSET_ORU) oru_in += coin.out.nValue;
    }

    const CAmount res_out = Consensus::GetOutputValueByAsset(tx, ASSET_RES);
    const CAmount oru_out = Consensus::GetOutputValueByAsset(tx, ASSET_ORU);

    TxValidationState state;
    CAmount txfee = 0;
    const bool ok = Consensus::CheckTxInputs(tx, state, view, spend_height, params, txfee);
    if (!ok) {
        return;
    }

    assert(MoneyRange(txfee));
    assert(!spent_cvg2);
    if (oru_out > 0) {
        assert(oru_in > 0);
    }
    if (res_in > 0) {
        assert(res_out <= res_in);
    } else if (res_out > 0) {
        // Body transactions cannot issue RES. Passing CheckTxInputs here is a consensus leak.
        assert(false);
    }
}
