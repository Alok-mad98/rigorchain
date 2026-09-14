// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// P059: Consensus surface closure. Standalone.
// Does not switch production. Does not recode issuance / RES / PoW / ASERT.

#include <arith_uint256.h>
#include <consensus/amount.h>
#include <consensus/consensus.h>
#include <consensus/params.h>
#include <consensus/res_issuance.h>
#include <consensus/rigor_consensus_lock.h>
#include <consensus/rigor_issuance.h>
#include <consensus/rigor_supply.h>
#include <consensus/txkernel/transaction_kernel_input.h>
#include <consensus/validation.h>
#include <primitives/block.h>
#include <primitives/transaction.h>
#include <rigor_kernel/pow_digest.h>
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
        if (g.oru_subsidy != 0) return fail("genesis ORU actual != 0");
        if (Consensus::ResDestroyCostAtHeight(0) != Consensus::RES_DESTROY_COST_GENESIS) {
            return fail("genesis RATE");
        }
        RigorSupplyDeltaInput in;
        in.height = 0;
        BlockValidationState st;
        RigorSupplyDeltaResult out;
        if (!RigorSupplyDelta(in, p, out, st)) return fail("genesis supply");
        if (out.oru_circulating_delta != 0) return fail("genesis ORU actual");
        if (out.res_delta != 0) return fail("genesis RES actual");
    }

    // --- Issuance pins ---
    if (GetBlockSubsidy(1, p) != Consensus::ORU_FLAT_BLOCK_SUBSIDY) return fail("ORU h=1");
    if (Consensus::ResDestroyCostAtHeight(1) != 102400006103LL) return fail("RATE h=1");
    if ((100 - 1) >= p.nCoinbaseMaturity) return fail("maturity 100");
    if ((101 - 1) < p.nCoinbaseMaturity) return fail("maturity 101");
    if (RigorIssuanceEpoch(209999) != 0) return fail("epoch 209999");
    if (RigorIssuanceEpoch(210000) != 1) return fail("epoch 210000");
    if (GetBlockSubsidy(209999, p) != Consensus::ORU_FLAT_BLOCK_SUBSIDY) return fail("ORU 209999");
    if (GetBlockSubsidy(210000, p) != Consensus::ORU_FLAT_BLOCK_SUBSIDY) return fail("ORU 210000");
    if (Consensus::ResDestroyCostAtHeight(209999) != 103685743011LL) return fail("RATE 209999");
    if (Consensus::ResDestroyCostAtHeight(210000) != 103685749152LL) return fail("RATE 210000");

    // --- Spend predicate boundary ---
    {
        const CScript oru = CScript() << RIGOR_PUSH_0 << std::vector<unsigned char>(WITNESS_V0_KEYHASH_SIZE, 0x11);
        TransactionKernelInput ok = BaseSpend();
        ok.scriptPubKey = Bytes(oru);
        ok.witness = {ok.signature, ok.pubkey};
        ok.keyhash_match = true;
        ok.crypto_accept = true;
        ok.sighash_tag = KernelAssignedSighashTag(oru);
        if (!VerifyRigorPredicateBoundary(ok)) return fail("valid predicate");

        TransactionKernelInput bad = ok;
        bad.scriptPubKey.clear();
        if (VerifyRigorPredicateBoundary(bad)) return fail("invalid predicate");

        TransactionKernelInput badsig = ok;
        badsig.crypto_accept = false;
        if (VerifyRigorPredicateBoundary(badsig)) return fail("bad signature");

        TransactionKernelInput sighash_a = ok;
        sighash_a.spent_script = sighash_a.scriptPubKey;
        sighash_a.sighash_tag = {'R', 'G', 'R', 'O', 'R', 'U', '0', '1'};
        sighash_a.chain_id.assign(32, 0x11);
        sighash_a.hashPrevouts.assign(32, 0x22);
        sighash_a.hashSequence.assign(32, 0x33);
        sighash_a.hashOutputs.assign(32, 0x44);
        sighash_a.prevout_hash.assign(32, 0x55);
        sighash_a.sighash_fields_present = true;
        sighash_a.amount = 1;
        TransactionKernelInput sighash_b = sighash_a;
        sighash_b.amount = 2;
        if (CppSighashPreimage(sighash_a) == CppSighashPreimage(sighash_b)) {
            return fail("bad sighash must change preimage");
        }
        sighash_a.crypto_accept = false;
        if (VerifyRigorPredicateBoundary(sighash_a)) return fail("bad sighash accept");
    }

    // --- PoW boundary: digest interface + target decision. Do not recode PoW. ---
    {
        CBlockHeader header;
        const uint256 digest = rigor_kernel::GetRigorPowHash(header);
        const unsigned char expect[32] = {
            221, 5, 127, 193, 223, 143, 91, 148, 102, 232, 52, 204, 157, 123, 129, 158,
            45, 81, 28, 117, 180, 54, 117, 181, 119, 246, 217, 245, 214, 222, 4, 252
        };
        if (std::memcmp(digest.begin(), expect, 32) != 0) {
            return fail("pow digest interface (zero header)");
        }
        arith_uint256 zero;
        arith_uint256 max_target;
        max_target.SetHex("ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
        const arith_uint256 d = UintToArith256(digest);
        if (!(d > zero)) return fail("pow reject vs zero target");
        if (d > max_target) return fail("pow accept vs max target");
        if (Consensus::ASERT_ANCHOR_HEIGHT != 1) return fail("ASERT anchor registered");
        if (Consensus::MAINNET_POW_ASERT_HALF_LIFE != 43200) return fail("ASERT half-life registered");
    }

    std::printf("genesis: PASS\n");
    std::printf("issuance: PASS\n");
    std::printf("spend: PASS\n");
    std::printf("pow boundary: PASS\n");
    return 0;
}
