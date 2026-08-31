// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// P0-063 adversarial harness. Production Kernel only. Crash => CONSENSUS_KERNEL_CRASH_FOUND.

#include "consensus_kernel_input.h"

#include <consensus/amount.h>
#include <consensus/consensus.h>
#include <consensus/convergence_logic.h>
#include <consensus/params.h>
#include <consensus/rigor_consensus_lock.h>
#include <consensus/rigor_issuance.h>
#include <consensus/rigor_supply.h>
#include <consensus/validation.h>
#include <primitives/transaction.h>
#include <script/script.h>
#include <script/standard.h>
#include <uint256.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <limits>
#include <string>
#include <vector>

static Consensus::Params FrozenParams()
{
    Consensus::Params p;
    p.nBlockSubsidyBase = Consensus::ORU_INITIAL_BLOCK_SUBSIDY;
    p.nSubsidyHalvingInterval = Consensus::ORU_SUBSIDY_HALVING_INTERVAL;
    p.nBlockSubsidyMaxHalvingShifts = Consensus::ORU_SUBSIDY_MAX_SHIFTS;
    p.nCoinbaseMaturity = 100;
    return p;
}

static uint256 FromBytes(const uint8_t b[32])
{
    uint256 id;
    std::memcpy(id.begin(), b, 32);
    return id;
}

static int OracleOru(int height)
{
    if (height == 0) return 0;
    if (height < 0) return -1;
    const int epoch = height / 210000;
    const int shifts = epoch < 3 ? epoch : 3;
    return shifts;
}

void ConsensusKernelAdversarialOne(const uint8_t* data, size_t size)
{
    const Consensus::Params p = FrozenParams();
    const ConsensusKernelInput in = ParseConsensusKernelInput(data, size);

    const int height = in.height;
    (void)GetBlockSubsidy(height, p);
    (void)Consensus::GetResMintReward(height);
    (void)ComputeRigorIssuance(height, p);

    if (height == 0) {
        if (GetBlockSubsidy(0, p) != 0) std::abort();
        if (Consensus::GetResMintReward(0) != Consensus::RES_REWARD_GENESIS) std::abort();
    }
    if (height >= 0) {
        const int shifts = OracleOru(height);
        if (shifts >= 0) {
            const CAmount expect = (height == 0) ? 0 : (Consensus::ORU_INITIAL_BLOCK_SUBSIDY >> shifts);
            if (GetBlockSubsidy(height, p) != expect) std::abort();
        }
    }

    const CScript script(in.script.begin(), in.script.end());
    std::vector<std::vector<unsigned char>> sols;
    const TxoutType txout = ClassifyRigorTxout(script, sols);
    const AssetType asset = ClassifyRigorAsset(script);
    if (asset != AssetTypeFromScriptPubKey(script)) std::abort();
    if (txout == TxoutType::WITNESS_V0_KEYHASH && asset != ASSET_ORU) std::abort();
    if (txout == TxoutType::WITNESS_V1_RES && asset != ASSET_RES) std::abort();
    if (txout == TxoutType::RES_MINT && asset != ASSET_RES) std::abort();
    if (txout == TxoutType::CONVERGE && asset != ASSET_ORU) std::abort();
    if (txout == TxoutType::NULL_DATA && asset != ASSET_NONE) std::abort();
    if (txout == TxoutType::NONSTANDARD && asset != ASSET_INVALID) std::abort();

    ConvergenceDrawSet set;
    set.source_height = height < 0 ? 0 : height;
    set.source_block_hash = uint256S("0xaa");
    const int n = in.n_candidates < 0 ? 0 : (in.n_candidates > 4 ? 4 : in.n_candidates);
    if (n >= 1) {
        ConvergenceDrawCandidate c;
        c.txid = FromBytes(in.txid0);
        c.weight = in.weight0;
        set.candidates.push_back(c);
    }
    if (n >= 2) {
        ConvergenceDrawCandidate c;
        c.txid = FromBytes(in.txid1);
        c.weight = in.weight1;
        set.candidates.push_back(c);
    }
    set.convergence_root = uint256S("0xcc");
    uint256 T = Consensus::RES_PROBABILITY_TARGET_CONST;
    if (in.t_mode == 0) T = uint256();
    if (in.t_mode == 0xff) std::fill(T.begin(), T.end(), static_cast<unsigned char>(0xff));
    const ConvergenceDrawOutcome a = ResolveConvergenceDraw(set, T);
    if (n >= 2) {
        std::swap(set.candidates[0], set.candidates[1]);
        const ConvergenceDrawOutcome b = ResolveConvergenceDraw(set, T);
        if (a.HasWinner() != b.HasWinner()) std::abort();
        if (a.HasWinner()) {
            const uint256 wa = (a.winner_index == 0) ? FromBytes(in.txid0) : FromBytes(in.txid1);
            const uint256 wb = set.candidates[static_cast<size_t>(b.winner_index)].txid;
            if (wa != wb) std::abort();
        }
    }
    if (set.candidates.empty() && a.HasWinner()) std::abort();
    if (in.t_mode == 0 && a.HasWinner()) std::abort();

    RigorSupplyDeltaInput sin;
    sin.height = height;
    sin.fees_oru = in.fees_oru;
    sin.fee_burn = in.fee_burn;
    sin.converge_sum = in.converge_sum;
    sin.res_mint = in.res_mint < 0 ? 0 : in.res_mint;
    sin.oru_coinbase = in.oru_coinbase;
    sin.res_coinbase = in.res_coinbase;
    sin.has_winner = in.has_winner != 0;
    sin.draw_cardinality = in.draw_cardinality;
    BlockValidationState state;
    RigorSupplyDeltaResult out;
    (void)RigorSupplyDelta(sin, p, out, state);
    if (height == 0 && sin.res_mint == 0 && sin.oru_coinbase == 0 && sin.converge_sum == 0) {
        if (out.oru_subsidy != 0 || out.res_delta != 0) std::abort();
    }
}

static void SmokeBoundaries()
{
    const int heights[] = {0, 1, 99, 100, 101, 209999, 210000, 210001, 419999, 420000};
    for (int h : heights) {
        uint8_t buf[8]{};
        std::memcpy(buf, &h, sizeof(h));
        ConsensusKernelAdversarialOne(buf, sizeof(buf));
    }
    const int32_t maxh = std::numeric_limits<int32_t>::max();
    uint8_t maxbuf[8]{};
    std::memcpy(maxbuf, &maxh, sizeof(maxh));
    ConsensusKernelAdversarialOne(maxbuf, sizeof(maxbuf));
    const int32_t neg = -1;
    uint8_t negbuf[8]{};
    std::memcpy(negbuf, &neg, sizeof(neg));
    ConsensusKernelAdversarialOne(negbuf, sizeof(negbuf));

    uint8_t malformed[64];
    for (int i = 0; i < 64; ++i) malformed[i] = static_cast<uint8_t>(i * 17 + 3);
    ConsensusKernelAdversarialOne(malformed, sizeof(malformed));
    ConsensusKernelAdversarialOne(nullptr, 0);
}

#if defined(CONSENSUS_KERNEL_ADVERSARIAL_MAIN)
int main(int argc, char** argv)
{
    try {
        if (argc >= 2 && std::string(argv[1]) == "smoke") {
            SmokeBoundaries();
            std::printf("FUZZ_SMOKE=PASS\n");
            std::printf("CRASH_FOUND=NO\n");
            return 0;
        }
        if (argc >= 2 && std::string(argv[1]) == "issuance") {
            const Consensus::Params p = FrozenParams();
            const int h = std::atoi(argv[2]);
            const RigorIssuance g = ComputeRigorIssuance(h, p);
            std::printf(
                "{\"height\":%d,\"epoch\":%d,\"oru_subsidy\":%lld,\"res_schedule\":%lld}\n",
                h, g.epoch, static_cast<long long>(g.oru_subsidy),
                static_cast<long long>(g.res_reward));
            return 0;
        }
        std::vector<uint8_t> buf;
        int c;
        while ((c = std::fgetc(stdin)) != EOF) buf.push_back(static_cast<uint8_t>(c));
        if (buf.empty()) SmokeBoundaries();
        else ConsensusKernelAdversarialOne(buf.data(), buf.size());
        return 0;
    } catch (const std::exception& e) {
        std::fprintf(stderr, "CONSENSUS_KERNEL_CRASH_FOUND %s\n", e.what());
        return 1;
    } catch (...) {
        std::fprintf(stderr, "CONSENSUS_KERNEL_CRASH_FOUND\n");
        return 1;
    }
}
#else
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, size_t size)
{
    try {
        ConsensusKernelAdversarialOne(data, size);
    } catch (...) {
        std::abort();
    }
    return 0;
}
#endif
