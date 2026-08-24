// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <chainparams.h>
#include <coins.h>
#include <consensus/amount.h>
#include <consensus/convergence_logic.h>
#include <consensus/convergence_root_eligibility.h>
#include <consensus/convergenceres.h>
#include <consensus/res_amount.h>
#include <consensus/res_owner.h>
#include <consensus/rigor_res.h>
#include <consensus/tx_verify.h>
#include <consensus/validation.h>
#include <crypto/common.h>
#include <hash.h>
#include <primitives/transaction.h>
#include <script/script.h>
#include <script/standard.h>
#include <test/fuzz/FuzzedDataProvider.h>
#include <test/fuzz/fuzz.h>
#include <test/fuzz/util.h>
#include <test/util/setup_common.h>
#include <uint256.h>

#include <cassert>
#include <cstdint>
#include <cstring>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace {

const TestingSetup* g_setup;

constexpr int kPayoutHeight = 200;
constexpr uint32_t kSourceNbits = 0x207fffff;
constexpr CAmount kSaturatingWeight = 4'000'000 * COIN;
static_assert(kSaturatingWeight <= MAX_MONEY, "saturating draw weight must fit CAmount");

ConvergenceDrawSet g_parent_set;
ConvergenceDrawOutcome g_parent_outcome;
std::vector<unsigned char> g_owner_program;
uint256 g_bind;
COutPoint g_oru_prevout;
COutPoint g_res_prevout;

CScript ResSpkFromProgram(const std::vector<unsigned char>& program32)
{
    return CScript() << OP_1 << program32;
}

CScript HonestMintSpk()
{
    return GetResMintScriptForProgram(
        static_cast<int64_t>(kPayoutHeight) + Params().GetConsensus().nCoinbaseMaturity,
        g_owner_program);
}

CScript OruSpk()
{
    return CScript() << OP_0 << std::vector<unsigned char>(20, 0x11);
}

std::vector<unsigned char> MakeResProgram(uint8_t fill)
{
    return std::vector<unsigned char>(WITNESS_V1_RES_SIZE, fill);
}

bool IsNamedMintReject(const std::string& reason)
{
    static constexpr std::string_view kNamed[] = {
        "bad-txns-spend-non-asset-input",
        "bad-rigor-res-mint-domain",
        "bad-rigor-res-draw-set-height",
        "bad-rigor-res-without-winner",
        "bad-rigor-res-owner-mismatch",
        "bad-rigor-res-amount",
        "bad-rigor-res-mint-script",
        "bad-cb-missing",
    };
    for (const std::string_view named : kNamed) {
        if (reason == named) return true;
    }
    return false;
}

CScript ConsumePayoutSpk(FuzzedDataProvider& p)
{
    CScript out;
    CallOneOf(
        p,
        [&] { out = HonestMintSpk(); },
        [&] { out = ResSpkFromProgram(g_owner_program); },
        [&] { out = ResSpkFromProgram(MakeResProgram(0xcd)); },
        [&] { out = OruSpk(); },
        [&] { out = Consensus::ConvergenceOutputScript(); },
        [&] { out = ConsumeScript(p); });
    return out;
}

CAmount ConsumeMintAmount(FuzzedDataProvider& p, int height)
{
    const CAmount reward = Consensus::GetResMintReward(height);
    CAmount out = reward;
    CallOneOf(
        p,
        [&] { out = reward; },
        [&] { out = reward > 0 ? reward + RES_ATOMIC_UNIT : RES_COIN; },
        [&] { out = 0; },
        [&] {
            out = ConsumeMoney(p);
            out -= out % RES_ATOMIC_UNIT;
        });
    return out;
}

} // namespace

void initialize_res_mint_eval()
{
    static const auto testing_setup = MakeNoLogFileContext<const TestingSetup>();
    g_setup = testing_setup.get();

    g_owner_program = MakeResProgram(0xab);
    g_bind = uint256S("0x5555555555555555555555555555555555555555555555555555555555555555");
    const uint256 convergence_txid = uint256::ONE;

    g_parent_set = {};
    g_parent_set.source_block_hash = g_bind;
    g_parent_set.source_height = kPayoutHeight - 1;
    g_parent_set.source_nbits = kSourceNbits;
    g_parent_set.convergence_root = Consensus::ComputeConvergenceRootFromEligibilityTxids({convergence_txid});
    g_parent_set.candidates.push_back(ConvergenceDrawCandidate{
        convergence_txid, static_cast<uint64_t>(kSaturatingWeight), Hash160(g_owner_program)});

    g_parent_outcome = ResolveConvergenceDraw(g_parent_set, Params().GetConsensus().RES_PROBABILITY_TARGET);
    assert(g_parent_set.IsSet());
    assert(g_parent_outcome.HasWinner());

    g_oru_prevout = COutPoint(uint256S("0xaaaabbbb11111111111111111111111111111111111111111111111111111111"), 0);
    g_res_prevout = COutPoint(uint256S("0xbbbbcccc22222222222222222222222222222222222222222222222222222222"), 0);
}

FUZZ_TARGET_INIT(res_mint_eval, initialize_res_mint_eval)
{
    (void)g_setup;
    FuzzedDataProvider fuzzed_data_provider{buffer.data(), buffer.size()};

    const int eval_height = fuzzed_data_provider.ConsumeBool() ?
        kPayoutHeight :
        fuzzed_data_provider.ConsumeIntegralInRange<int>(-1, 10'000);

    CMutableTransaction mtx;
    mtx.nVersion = 2;
    mtx.vin.resize(1);
    if (fuzzed_data_provider.ConsumeBool()) {
        mtx.vin[0].prevout.SetNull();
        mtx.vin[0].scriptSig = CScript() << eval_height;
    } else {
        mtx.vin[0].prevout = g_oru_prevout;
    }
    mtx.vin[0].nSequence = CTxIn::SEQUENCE_FINAL;
    mtx.vout.emplace_back(50 * COIN, OruSpk());

    const CScript payout = ConsumePayoutSpk(fuzzed_data_provider);
    CAmount mint_amount = ConsumeMintAmount(fuzzed_data_provider, eval_height);
    if (payout.IsUnspendable()) mint_amount = 0;
    mtx.vout.emplace_back(mint_amount, payout);
    if (fuzzed_data_provider.ConsumeBool()) {
        mtx.vout.emplace_back(mint_amount, payout);
    }

    const CTransaction tx{mtx};
    BlockValidationState state;
    CAmount minted = 0;
    const bool ok = rigor::ConnectEvaluateCoinbaseResMint(tx, eval_height,
        Params().GetConsensus(), g_parent_set, g_parent_outcome, state, minted);

    if (!ok) {
        const std::string reason = state.GetRejectReason();
        assert(!reason.empty());
        assert(IsNamedMintReject(reason));
        assert(minted == 0);
        return;
    }

    assert(tx.IsCoinBase());
    assert(minted == Consensus::GetResMintReward(eval_height));
    assert(minted > 0);
    assert(Consensus::CheckResRewardOwner(Hash160(g_owner_program), payout));
}
