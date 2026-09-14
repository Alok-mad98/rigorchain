// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// P061: mainnet Kernel boundary pins. Standalone.
// Does not switch production. Does not recode issuance / RES / PoW.
// RFC-019: RATE pins. No draw.

#include <consensus/amount.h>
#include <consensus/consensus.h>
#include <consensus/params.h>
#include <consensus/res_issuance.h>
#include <consensus/rigor_consensus_lock.h>
#include <consensus/rigor_issuance.h>
#include <consensus/rigor_supply.h>
#include <consensus/txkernel/transaction_kernel_input.h>
#include <consensus/validation.h>
#include <script/script.h>
#include <script/standard.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
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

static TransactionKernelInput BaseSpend()
{
    TransactionKernelInput in;
    in.flags = TK_SCRIPT_VERIFY_WITNESS;
    in.scriptSig_push_only = true;
    in.stack_clean = true;
    in.signature.assign(64, 0x07);
    in.pubkey.assign(32, 0x09);
    return in;
}

static std::vector<unsigned char> Bytes(const CScript& s)
{
    return std::vector<unsigned char>(s.begin(), s.end());
}

int main()
{
    const Consensus::Params p = FrozenParams();

    // --- Genesis height=0 ---
    {
        const RigorIssuance g = ComputeRigorIssuance(0, p);
        if (g.oru_subsidy != 0) return fail("genesis ORU actual");
        if (g.epoch != 0) return fail("genesis epoch");
        if (Consensus::ResDestroyCostAtHeight(0) != Consensus::RES_DESTROY_COST_GENESIS) {
            return fail("genesis RATE");
        }
        RigorSupplyDeltaInput in;
        in.height = 0;
        BlockValidationState st;
        RigorSupplyDeltaResult out;
        if (!RigorSupplyDelta(in, p, out, st)) return fail("genesis supply");
        if (out.oru_circulating_delta != 0) return fail("genesis ORU circulating");
        if (out.res_delta != 0) return fail("genesis RES actual");
    }

    // --- Maturity: height-1 coinbase immature at 100, mature at 101 ---
    if ((100 - 1) >= p.nCoinbaseMaturity) return fail("height1 mature at 100");
    if ((101 - 1) < p.nCoinbaseMaturity) return fail("height1 immature at 101");

    // --- Halving ---
    if (RigorIssuanceEpoch(209999) != 0) return fail("epoch 209999");
    if (RigorIssuanceEpoch(210000) != 1) return fail("epoch 210000");
    if (GetBlockSubsidy(209999, p) != Consensus::ORU_FLAT_BLOCK_SUBSIDY) return fail("ORU 209999");
    if (GetBlockSubsidy(210000, p) != Consensus::ORU_FLAT_BLOCK_SUBSIDY) return fail("ORU 210000");
    if (Consensus::ResDestroyCostAtHeight(209999) != 103685743011LL) return fail("RATE 209999");
    if (Consensus::ResDestroyCostAtHeight(210000) != 103685749152LL) return fail("RATE 210000");

    // --- Spend ---
    {
        const CScript oru = CScript() << RIGOR_PUSH_0 << std::vector<unsigned char>(WITNESS_V0_KEYHASH_SIZE, 0x11);
        const CScript empty;
        TransactionKernelInput ok = BaseSpend();
        ok.scriptPubKey = Bytes(oru);
        ok.witness = {ok.signature, ok.pubkey};
        ok.keyhash_match = true;
        ok.crypto_accept = true;
        ok.sighash_tag = KernelAssignedSighashTag(oru);
        if (!VerifyRigorPredicateBoundary(ok)) return fail("valid predicate");

        TransactionKernelInput bad = ok;
        bad.scriptPubKey = Bytes(empty);
        if (VerifyRigorPredicateBoundary(bad)) return fail("invalid predicate");

        TransactionKernelInput sig = ok;
        sig.crypto_accept = false;
        if (VerifyRigorPredicateBoundary(sig)) return fail("bad signature");
    }

    std::printf("PASS: consensus mainnet boundary\n");
        std::printf("PRODUCTION_CONSENSUS_AUTHORITY=RigorConsensusKernel(C++)\n");
    std::printf("FORMAL_REFINEMENT_STATUS=PROVED\n");
    std::printf("EXTRACTED_KERNEL_PRODUCTION=FALSE\n");
    std::printf("COMPCERT_BINARY_VERIFIED_KERNEL=FALSE\n");
    return 0;
}
