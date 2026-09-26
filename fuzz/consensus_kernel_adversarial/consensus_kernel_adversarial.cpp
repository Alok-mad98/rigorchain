// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// P0-063 adversarial harness. Production Kernel only. Crash => CONSENSUS_KERNEL_CRASH_FOUND.

#include "consensus_kernel_input.h"

#include <consensus/amount.h>
#include <consensus/consensus.h>
#include <consensus/params.h>
#include <consensus/rigor_consensus_lock.h>
#include <consensus/res_issuance.h>
#include <consensus/rigor_issuance.h>
#include <consensus/rigor_supply.h>
#include <consensus/validation.h>
#include <primitives/transaction.h>
#include <script/script.h>
#include <script/standard.h>

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
    p.nCoinbaseMaturity = 100;
    return p;
}

static CAmount OracleOru(int height)
{
    if (height < 0) return 0;
    if (height == 0) return 0;
    return Consensus::ORU_FLAT_BLOCK_SUBSIDY;
}

void ConsensusKernelAdversarialOne(const uint8_t* data, size_t size)
{
    const Consensus::Params p = FrozenParams();
    const ConsensusKernelInput in = ParseConsensusKernelInput(data, size);

    const int height = in.height;
    (void)GetBlockSubsidy(height, p);
    (void)Consensus::ResDestroyCostAtHeight(height);
    (void)ComputeRigorIssuance(height, p);

    if (height == 0) {
        if (GetBlockSubsidy(0, p) != 0) std::abort();
        if (Consensus::ResDestroyCostAtHeight(0) != Consensus::RES_DESTROY_COST_GENESIS) std::abort();
    }
    if (height >= 0) {
        if (GetBlockSubsidy(height, p) != OracleOru(height)) std::abort();
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

    RigorSupplyDeltaInput sin;
    sin.height = height;
    sin.fees_oru = in.fees_oru;
    sin.fee_burn = in.fee_burn;
    sin.converge_sum = in.converge_sum;
    sin.res_mint = in.res_mint < 0 ? 0 : in.res_mint;
    sin.oru_coinbase = in.oru_coinbase;
    sin.res_coinbase = in.res_coinbase;
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
                static_cast<long long>(Consensus::ResDestroyCostAtHeight(h)));
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
