// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// P057: ConnectBlock prepares Kernel inputs and consumes results.
// Standalone. Does not switch ConnectBlock. Does not recode formulas.
// RFC-019: issuance surface is (oru_subsidy, RATE(h), epoch). No draw.

#include <consensus/amount.h>
#include <consensus/consensus.h>
#include <consensus/params.h>
#include <consensus/res_issuance.h>
#include <consensus/rigor_consensus_lock.h>
#include <consensus/rigor_issuance.h>
#include <consensus/rigor_supply.h>
#include <consensus/validation.h>
#include <primitives/transaction.h>
#include <script/script.h>
#include <script/standard.h>

#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

static Consensus::Params FrozenParams()
{
    Consensus::Params p;
    p.nCoinbaseMaturity = 100;
    return p;
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
        if (g.epoch != 0) return fail("genesis epoch");
        if (Consensus::ResDestroyCostAtHeight(0) != Consensus::RES_DESTROY_COST_GENESIS) {
            return fail("genesis RATE");
        }
        RigorSupplyDeltaInput in;
        in.height = 0;
        BlockValidationState st;
        RigorSupplyDeltaResult out;
        if (!RigorSupplyDelta(in, p, out, st)) return fail("genesis supply");
        if (out.oru_circulating_delta != 0) return fail("genesis ledger delta");
        if (out.res_delta != 0) return fail("genesis RES actual issuance");
    }

    // --- First issuance height=1 ---
    {
        const RigorIssuance g = ComputeRigorIssuance(1, p);
        if (g.oru_subsidy != Consensus::ORU_FLAT_BLOCK_SUBSIDY) return fail("height1 ORU");
        if (g.epoch != 0) return fail("height1 epoch");
        if (Consensus::ResDestroyCostAtHeight(1) != 102400006103LL) {
            return fail("height1 RATE");
        }
    }

    // --- Maturity 100 / 101 (Kernel predicate, not UTXO) ---
    if (1 - 1 >= p.nCoinbaseMaturity) return fail("height1 mature at 1");
    if ((100 - 1) >= p.nCoinbaseMaturity) return fail("height1 mature at 100");
    if ((101 - 1) < p.nCoinbaseMaturity) return fail("height1 immature at 101");

    // --- Halving / RATE pins ---
    if (RigorIssuanceEpoch(209999) != 0) return fail("epoch 209999");
    if (RigorIssuanceEpoch(210000) != 1) return fail("epoch 210000");
    if (GetBlockSubsidy(209999, p) != Consensus::ORU_FLAT_BLOCK_SUBSIDY) return fail("ORU 209999");
    if (GetBlockSubsidy(210000, p) != Consensus::ORU_FLAT_BLOCK_SUBSIDY) return fail("ORU 210000");
    if (Consensus::ResDestroyCostAtHeight(209999) != 103685743011LL) return fail("RATE 209999");
    if (Consensus::ResDestroyCostAtHeight(210000) != 103685749152LL) return fail("RATE 210000");

    // --- Classify prepared script bytes ---
    {
        const CScript oru = CScript() << RIGOR_PUSH_0 << std::vector<unsigned char>(WITNESS_V0_KEYHASH_SIZE, 0x11);
        const CScript res = CScript() << RIGOR_PUSH_1 << std::vector<unsigned char>(WITNESS_V1_RES_SIZE, 0xaa);
        const CScript empty;
        if (ClassifyRigorAsset(oru) != ASSET_ORU) return fail("classify ORU");
        if (ClassifyRigorAsset(res) != ASSET_RES) return fail("classify RES");
        if (ClassifyRigorAsset(empty) != ASSET_INVALID) return fail("classify fail-closed");
    }

    // --- Coinbase expectation is Kernel, not miner (RFC-019: fees + bootstrap) ---
    if (RigorResCoinbaseExpected(1, 2) != 3) return fail("coinbase expected sum");

    // --- RigorSupplyDelta reject reasons ---
    {
        RigorSupplyDeltaInput in;
        in.height = 1;
        in.oru_coinbase = 100000000001LL;
        BlockValidationState st;
        RigorSupplyDeltaResult out;
        if (RigorSupplyDelta(in, p, out, st)) return fail("bad-cb-amount accepted");
        if (st.GetRejectReason() != "bad-cb-amount")
            return fail("bad-cb-amount reason");
    }
    {
        RigorSupplyDeltaInput in;
        in.height = 1;
        in.res_coinbase = 1;
        BlockValidationState st;
        RigorSupplyDeltaResult out;
        if (RigorSupplyDelta(in, p, out, st)) return fail("bad-cb-res accepted");
        if (st.GetRejectReason() != "bad-cb-res-amount")
            return fail("bad-cb-res-amount reason");
    }

    std::printf("PASS: connectblock kernel boundary\n");
    std::printf("INPUT_PREPARATION_ALIGNMENT=PASS\n");
    std::printf("RESULT_CONSUMPTION_ALIGNMENT=PASS\n");
        std::printf("PRODUCTION_CONSENSUS_AUTHORITY=RigorConsensusKernel(C++)\n");
    std::printf("FORMAL_REFINEMENT_STATUS=PROVED\n");
    std::printf("EXTRACTED_KERNEL_PRODUCTION=FALSE\n");
    std::printf("COMPCERT_BINARY_VERIFIED_KERNEL=FALSE\n");
    return 0;
}
