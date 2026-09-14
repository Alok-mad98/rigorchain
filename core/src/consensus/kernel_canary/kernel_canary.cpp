// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// P068 canary TU. Calls existing authorities / C ABI only. No formula copy.
//
// RFC-019: issuance surface is (oru_subsidy, RATE(h), epoch). The lottery
// and W(h) schedule are gone. Draw fields stay zero.

#if !defined(ENABLE_CONSENSUS_KERNEL_CANARY)
#error "consensus/kernel_canary is verify-only; compile with -DENABLE_CONSENSUS_KERNEL_CANARY"
#endif
#ifndef RIGOR_KERNEL_ABI_VERIFY_ONLY
#error "kernel canary needs the C ABI compile flag; not a production object"
#endif

#include <consensus/kernel_canary/kernel_canary.h>

#include <consensus/bridge/consensus_kernel_c_api.h>
#include <consensus/bridge/kernel_marshalling.h>
#include <consensus/params.h>
#include <consensus/res_issuance.h>
#include <consensus/rigor_issuance.h>
#include <consensus/rigor_supply.h>
#include <consensus/validation.h>
#include <primitives/transaction.h>
#include <script/standard.h>

#include <cstdio>
#include <cstring>
#include <vector>

namespace {

rigor_asset_type MapAsset(AssetType a)
{
    switch (a) {
    case ASSET_ORU: return RIGOR_ASSET_ORU;
    case ASSET_RES: return RIGOR_ASSET_RES;
    case ASSET_NONE: return RIGOR_ASSET_NONE;
    default: return RIGOR_ASSET_INVALID;
    }
}

bool ClassifyOk(const ConsensusKernelInput& in, rigor_asset_type a)
{
    if (in.script_len == 0) return true;
    return a == RIGOR_ASSET_ORU || a == RIGOR_ASSET_RES;
}

bool SpendOk(const ConsensusKernelInput& in)
{
    if (!in.has_spend) return true;
    return VerifyRigorPredicateBoundary(in.tx);
}

void FinishAccept(const ConsensusKernelInput& in, KernelResult& out)
{
    const bool genesis_draw_ok = !(in.height == 0 && out.draw_has_winner != 0);
    out.accept = out.supply_ok && out.mature && out.spend_ok &&
                 ClassifyOk(in, out.classification) && genesis_draw_ok;
}

bool SameU256(const rigor_u256& a, const rigor_u256& b)
{
    return std::memcmp(a.bytes, b.bytes, 32) == 0;
}

const char* ResultsEqual(const KernelResult& a, const KernelResult& b)
{
    if (a.accept != b.accept) return "accept";
    if (a.oru_subsidy != b.oru_subsidy) return "issuance.oru";
    if (a.res_schedule != b.res_schedule) return "issuance.res_destroy_cost";
    if (a.res_mint_expectation != b.res_mint_expectation) return "res_mint_expectation";
    if (a.epoch != b.epoch) return "issuance.epoch";
    if (a.draw_has_winner != b.draw_has_winner) return "draw.has_winner";
    if (a.draw_has_winner && !SameU256(a.draw_winner, b.draw_winner)) return "draw.winner";
    if (a.classification != b.classification) return "classification";
    if (a.ledger_delta != b.ledger_delta) return "ledger_delta";
    if (a.coinbase_expectation != b.coinbase_expectation) return "coinbase_expectation";
    if (a.spend_ok != b.spend_ok) return "spend";
    if (a.supply_ok != b.supply_ok) return "supply";
    if (a.mature != b.mature) return "maturity";
    return nullptr;
}

void FillIssuanceDerived(uint32_t height, const rigor_issuance_result& iss, KernelResult& out)
{
    out.oru_subsidy = iss.oru_subsidy;
    out.res_schedule = iss.res_destroy_cost;
    out.epoch = iss.epoch;
    out.res_mint_expectation = 0;
    out.coinbase_expectation = iss.oru_subsidy;
}

RigorSupplyDeltaInput SupplyFromAbi(const rigor_supply_input& s)
{
    RigorSupplyDeltaInput din;
    din.height = static_cast<int>(s.height);
    din.fees_oru = static_cast<CAmount>(s.fees_oru);
    din.fee_burn = static_cast<CAmount>(s.burn);
    din.converge_sum = static_cast<CAmount>(s.converge);
    din.res_mint = static_cast<CAmount>(s.mint);
    din.oru_coinbase = static_cast<CAmount>(s.oru_coinbase);
    din.res_bootstrap = 0;
    din.fees_res = 0;
    din.res_coinbase = RigorResCoinbaseExpected(din.fees_res, din.res_bootstrap);
    return din;
}

} // namespace

bool KernelCanaryExecutor::RunCpp(const ConsensusKernelInput& in, KernelResult& out) const
{
    out = KernelResult{};
    const Consensus::Params params = KernelFrozenParams();

    const CAmount oru = GetBlockSubsidy(static_cast<int>(in.height), params);
    const CAmount destroy_cost = Consensus::ResDestroyCostAtHeight(static_cast<int>(in.height));
    const RigorIssuance pack = ComputeRigorIssuance(static_cast<int>(in.height), params);
    if (pack.oru_subsidy != oru) return false;
    rigor_issuance_result iss{};
    iss.oru_subsidy = static_cast<uint64_t>(pack.oru_subsidy);
    iss.res_destroy_cost = static_cast<uint64_t>(destroy_cost);
    iss.epoch = static_cast<uint32_t>(pack.epoch);
    FillIssuanceDerived(in.height, iss, out);

    const CScript spk = RigorScriptFromBytes(in.script, in.script_len);
    std::vector<std::vector<unsigned char>> sols;
    (void)ClassifyRigorTxout(spk, sols);
    out.classification = MapAsset(ClassifyRigorAsset(spk));

    if (in.has_maturity) {
        out.mature = (static_cast<int>(in.spend_height) - static_cast<int>(in.coin_height)) >= params.nCoinbaseMaturity;
    } else {
        out.mature = true;
    }

    out.draw_has_winner = 0;

    out.spend_ok = SpendOk(in);

    BlockValidationState state;
    RigorSupplyDeltaResult sdelta;
    out.supply_ok = RigorSupplyDelta(SupplyFromAbi(in.supply), params, sdelta, state);
    out.ledger_delta = out.supply_ok ? static_cast<int64_t>(sdelta.oru_circulating_delta) : 0;

    FinishAccept(in, out);
    return true;
}

bool KernelCanaryExecutor::RunAbi(const ConsensusKernelInput& in, KernelResult& out) const
{
    out = KernelResult{};

    rigor_issuance_result iss{};
    if (!rigor_compute_issuance(in.height, &iss)) return false;
    FillIssuanceDerived(in.height, iss, out);

    if (!rigor_classify_script(in.script, in.script_len, &out.classification)) return false;

    if (in.has_maturity) {
        uint8_t eligible = 0;
        if (!rigor_coinbase_mature(in.spend_height, in.coin_height, &eligible)) return false;
        out.mature = eligible != 0;
    } else {
        out.mature = true;
    }

    rigor_draw_result draw{};
    if (!rigor_resolve_draw(in.candidates, in.candidate_count, &in.draw_ctx, &draw)) return false;
    out.draw_has_winner = draw.has_winner;
    out.draw_winner = draw.winner;

    out.spend_ok = SpendOk(in);

    rigor_supply_result supply{};
    out.supply_ok = rigor_supply_delta(&in.supply, &supply);
    out.ledger_delta = out.supply_ok ? supply.delta : 0;

    FinishAccept(in, out);
    return true;
}

bool KernelCanaryExecutor::Compare(const ConsensusKernelInput& in, KernelResult& out, const char** field) const
{
    KernelResult a, b;
    if (!RunCpp(in, a) || !RunAbi(in, b)) {
        if (field) *field = "path_exec";
        std::fprintf(stderr, "CONSENSUS_KERNEL_CANARY_DISCREPANCY field=path_exec\n");
        return false;
    }
    const char* mismatch = ResultsEqual(a, b);
    if (mismatch) {
        if (field) *field = mismatch;
        std::fprintf(stderr, "CONSENSUS_KERNEL_CANARY_DISCREPANCY field=%s\n", mismatch);
        return false;
    }
    out = a;
    return true;
}

int KernelCanaryExecutor::RunDeterministicBattery(int iterations, const char** first_field) const
{
    if (iterations <= 0) return 0;

    static const uint32_t kHeights[] = {0, 1, 99, 100, 101, 209999, 210000, 419999, 420000};
    static const uint8_t kOru[] = {
        0x00, 0x14, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11,
        0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x11};
    static const uint8_t kRes[] = {
        0x51, 0x20, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
        0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa,
        0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa, 0xaa};
    static const uint8_t kUnknown[] = {
        0x76, 0xa9, 0x14, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x88, 0xac};
    static const uint8_t kTrunc[] = {0x00, 0x14};

    rigor_u256 c{}, b{}, t0{}, tx1{}, tx2{};
    for (int i = 0; i < 32; ++i) {
        c.bytes[i] = 0x11;
        b.bytes[i] = 0xaa;
    }
    tx1.bytes[31] = 0x01;
    tx2.bytes[31] = 0x02;
    rigor_u256 tprod{};
    for (int i = 0; i < 32; ++i) tprod.bytes[i] = 0x55;
    const uint64_t sat = 209715200000000ULL;
    rigor_candidate one{tx1, sat};
    rigor_candidate two[2] = {{tx1, sat}, {tx2, sat}};
    rigor_candidate swap[2] = {{tx2, sat}, {tx1, sat}};
    rigor_candidate zero{tx1, 0};

    auto ValidTx = [&]() {
        TransactionKernelInput tx;
        tx.flags = TK_SCRIPT_VERIFY_WITNESS;
        tx.scriptSig_push_only = true;
        tx.stack_clean = true;
        tx.signature.assign(64, 0x07);
        tx.pubkey.assign(32, 0x09);
        tx.scriptPubKey.assign(kOru, kOru + sizeof(kOru));
        tx.witness = {tx.signature, tx.pubkey};
        tx.keyhash_match = true;
        tx.crypto_accept = true;
        return tx;
    };

    int mismatches = 0;
    for (int i = 0; i < iterations; ++i) {
        ConsensusKernelInput in{};
        in.height = kHeights[i % 9];
        in.draw_ctx.payout_height = 101;
        in.draw_ctx.convergence_root = c;
        in.draw_ctx.source_block_hash = b;
        in.draw_ctx.target = tprod;
        in.supply.height = in.height;

        switch ((i / 9) % 5) {
        case 0:
            in.script = kOru;
            in.script_len = sizeof(kOru);
            break;
        case 1:
            in.script = kRes;
            in.script_len = sizeof(kRes);
            break;
        case 2:
            in.script = kUnknown;
            in.script_len = sizeof(kUnknown);
            break;
        case 3:
            in.script = nullptr;
            in.script_len = 0;
            break;
        default:
            in.script = kTrunc;
            in.script_len = sizeof(kTrunc);
            break;
        }

        switch ((i / 45) % 5) {
        case 0:
            in.candidates = nullptr;
            in.candidate_count = 0;
            break;
        case 1:
            in.candidates = &one;
            in.candidate_count = 1;
            break;
        case 2:
            in.candidates = two;
            in.candidate_count = 2;
            break;
        case 3:
            in.candidates = swap;
            in.candidate_count = 2;
            break;
        default:
            in.candidates = &zero;
            in.candidate_count = 1;
            in.draw_ctx.target = t0;
            break;
        }

        switch ((i / 225) % 5) {
        case 0: // mint
            in.supply.has_winner = 1;
            in.supply.card = 1;
            in.supply.mint = (in.height == 0) ? 0 : 1;
            break;
        case 1: // burn
            in.supply.burn = 5;
            in.supply.fees_oru = 10;
            break;
        case 2: // fee
            in.supply.fees_oru = 100;
            break;
        case 3: // invalid coinbase
            in.supply.oru_coinbase = 1ULL << 60;
            break;
        default:
            break;
        }

        switch ((i / 1125) % 5) {
        case 1:
            in.has_spend = true;
            in.tx = ValidTx();
            break;
        case 2:
            in.has_spend = true;
            in.tx = ValidTx();
            in.tx.crypto_accept = false;
            break;
        case 3:
            in.has_spend = true;
            in.tx = ValidTx();
            in.tx.signature.assign(8, 0x00);
            in.tx.crypto_accept = false;
            break;
        case 4:
            in.has_spend = true;
            in.tx = ValidTx();
            in.tx.witness = {std::vector<unsigned char>(521, 0x01), in.tx.pubkey};
            break;
        default:
            break;
        }

        switch ((i / 5625) % 4) {
        case 1:
            in.has_maturity = true;
            in.spend_height = 99;
            in.coin_height = 0;
            break;
        case 2:
            in.has_maturity = true;
            in.spend_height = 100;
            in.coin_height = 0;
            break;
        case 3:
            in.has_maturity = true;
            in.spend_height = 101;
            in.coin_height = 0;
            break;
        default:
            break;
        }

        KernelResult got;
        const char* field = nullptr;
        if (!Compare(in, got, &field)) {
            ++mismatches;
            if (first_field && !*first_field) *first_field = field ? field : "compare";
            break;
        }
    }
    return mismatches;
}
