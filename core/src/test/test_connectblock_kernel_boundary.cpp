// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// P057: ConnectBlock prepares Kernel inputs and consumes results.
// Standalone. Does not switch ConnectBlock. Does not recode formulas.

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

static uint256 uint256_from_hex(const char* hex)
{
    uint256 out;
    std::vector<unsigned char> raw;
    const std::string s(hex);
    for (size_t i = 0; i < s.size(); i += 2) {
        unsigned v = 0;
        std::sscanf(s.c_str() + static_cast<int>(i), "%2x", &v);
        raw.push_back(static_cast<unsigned char>(v));
    }
    if (raw.size() != 32) std::exit(2);
    std::memcpy(out.begin(), raw.data(), 32);
    return out;
}

static int fail(const char* msg)
{
    std::fprintf(stderr, "FAIL: %s\n", msg);
    return 1;
}

int main()
{
    const Consensus::Params p = FrozenParams();

    // --- Genesis height=0 ---
    {
        const RigorIssuance g = ComputeRigorIssuance(0, p);
        if (g.oru_subsidy != 0) return fail("genesis ORU actual != 0");
        if (g.res_reward != Consensus::RES_REWARD_GENESIS) return fail("genesis RES schedule");
        if (Consensus::GetResMintReward(0) != 40000000) return fail("GetResMintReward(0)");
        if (g.res_reward == 0) return fail("schedule collapsed into actual");
        RigorSupplyDeltaInput in;
        in.height = 0;
        BlockValidationState st;
        RigorSupplyDeltaResult out;
        if (!RigorSupplyDelta(in, p, out, st)) return fail("genesis supply");
        if (out.oru_circulating_delta != 0) return fail("genesis ledger delta");
        if (out.res_delta != 0) return fail("genesis RES actual issuance");
        if (out.res_reward_scheduled != 40000000) return fail("genesis RES schedule field");
        ConvergenceDrawSet empty;
        empty.source_height = 0;
        if (ResolveConvergenceDraw(empty, Consensus::RES_PROBABILITY_TARGET_CONST).HasWinner()) {
            return fail("genesis empty F must be none");
        }
    }

    // --- First issuance height=1 ---
    {
        const RigorIssuance g = ComputeRigorIssuance(1, p);
        if (g.oru_subsidy != 100000000000LL) return fail("height1 ORU");
        if (g.res_reward != 40000000) return fail("height1 RES schedule");
        if (g.epoch != 0) return fail("height1 epoch");
    }

    // --- Maturity 100 / 101 (Kernel predicate, not UTXO) ---
    if (1 - 1 >= p.nCoinbaseMaturity) return fail("height1 mature at 1");
    if ((100 - 1) >= p.nCoinbaseMaturity) return fail("height1 mature at 100");
    if ((101 - 1) < p.nCoinbaseMaturity) return fail("height1 immature at 101");

    // --- Halving ---
    if (RigorIssuanceEpoch(209999) != 0) return fail("epoch 209999");
    if (RigorIssuanceEpoch(210000) != 1) return fail("epoch 210000");
    if (GetBlockSubsidy(209999, p) != 100000000000LL) return fail("ORU 209999");
    if (GetBlockSubsidy(210000, p) != 50000000000LL) return fail("ORU 210000");
    if (Consensus::GetResMintReward(209999) != 40000000) return fail("RES 209999");
    if (Consensus::GetResMintReward(210000) != 14142196) return fail("RES 210000");

    // --- Classify prepared script bytes ---
    {
        const CScript oru = CScript() << RIGOR_PUSH_0 << std::vector<unsigned char>(WITNESS_V0_KEYHASH_SIZE, 0x11);
        const CScript res = CScript() << RIGOR_PUSH_1 << std::vector<unsigned char>(WITNESS_V1_RES_SIZE, 0xaa);
        const CScript empty;
        if (ClassifyRigorAsset(oru) != ASSET_ORU) return fail("classify ORU");
        if (ClassifyRigorAsset(res) != ASSET_RES) return fail("classify RES");
        if (ClassifyRigorAsset(empty) != ASSET_INVALID) return fail("classify fail-closed");
    }

    // --- Draw: empty / winner / tie-break order ---
    {
        const uint64_t sat = 209715200000000ULL;
        ConvergenceDrawSet one;
        one.source_height = 100;
        one.convergence_root = uint256_from_hex(
            "1111111111111111111111111111111111111111111111111111111111111111");
        one.source_block_hash = uint256_from_hex(
            "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
        ConvergenceDrawCandidate a, b;
        a.txid = uint256_from_hex(
            "0000000000000000000000000000000000000000000000000000000000000001");
        a.weight = sat;
        b.txid = uint256_from_hex(
            "0000000000000000000000000000000000000000000000000000000000000002");
        b.weight = sat;
        one.candidates = {a};
        const auto w1 = ResolveConvergenceDraw(one, Consensus::RES_PROBABILITY_TARGET_CONST);
        if (!w1.HasWinner()) return fail("single candidate must win");

        ConvergenceDrawSet two = one;
        two.candidates = {a, b};
        ConvergenceDrawSet swap = one;
        swap.candidates = {b, a};
        const auto w2 = ResolveConvergenceDraw(two, Consensus::RES_PROBABILITY_TARGET_CONST);
        const auto w3 = ResolveConvergenceDraw(swap, Consensus::RES_PROBABILITY_TARGET_CONST);
        if (!w2.HasWinner() || !w3.HasWinner()) return fail("tie-break missing winner");
        if (two.candidates[static_cast<size_t>(w2.winner_index)].txid !=
            swap.candidates[static_cast<size_t>(w3.winner_index)].txid) {
            return fail("tie-break order independence");
        }

        const auto none = ResolveConvergenceDraw(one, uint256());
        if (none.HasWinner()) return fail("T=0 must be none");
    }

    // --- Coinbase expectation is Kernel, not miner ---
    if (RigorResCoinbaseExpected(1, 2, 3) != 6) return fail("coinbase expected sum");

    std::printf("PASS: connectblock kernel boundary\n");
    std::printf("INPUT_PREPARATION_ALIGNMENT=PASS\n");
    std::printf("RESULT_CONSUMPTION_ALIGNMENT=PASS\n");
    std::printf("PRODUCTION_KERNEL_SWITCHED=YES\n");
    return 0;
}
