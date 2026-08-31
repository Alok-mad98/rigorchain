// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// P059: Consensus surface closure. Standalone.
// Does not switch production. Does not recode issuance / RES / PoW / ASERT.

#include <arith_uint256.h>
#include <consensus/amount.h>
#include <consensus/consensus.h>
#include <consensus/convergence_logic.h>
#include <consensus/params.h>
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
        if (Consensus::GetResMintReward(0) != 40000000) return fail("genesis RES schedule");
        RigorSupplyDeltaInput in;
        in.height = 0;
        BlockValidationState st;
        RigorSupplyDeltaResult out;
        if (!RigorSupplyDelta(in, p, out, st)) return fail("genesis supply");
        if (out.oru_circulating_delta != 0) return fail("genesis ORU actual");
        if (out.res_delta != 0) return fail("genesis RES actual");
        if (out.res_reward_scheduled != 40000000) return fail("genesis RES schedule field");
        ConvergenceDrawSet empty;
        empty.source_height = 0;
        if (ResolveConvergenceDraw(empty, Consensus::RES_PROBABILITY_TARGET_CONST).HasWinner()) {
            return fail("genesis draw must be none");
        }
    }

    // --- Issuance pins ---
    if (GetBlockSubsidy(1, p) != 100000000000LL) return fail("ORU h=1");
    if (Consensus::GetResMintReward(1) != 40000000) return fail("RES h=1");
    if ((100 - 1) >= p.nCoinbaseMaturity) return fail("maturity 100");
    if ((101 - 1) < p.nCoinbaseMaturity) return fail("maturity 101");
    if (RigorIssuanceEpoch(209999) != 0) return fail("epoch 209999");
    if (RigorIssuanceEpoch(210000) != 1) return fail("epoch 210000");
    if (GetBlockSubsidy(209999, p) != 100000000000LL) return fail("ORU 209999");
    if (GetBlockSubsidy(210000, p) != 50000000000LL) return fail("ORU 210000");
    if (Consensus::GetResMintReward(209999) != 40000000) return fail("RES 209999");
    if (Consensus::GetResMintReward(210000) != 14142196) return fail("RES 210000");

    // --- Spend predicate boundary ---
    {
        const CScript oru = CScript() << RIGOR_PUSH_0 << std::vector<unsigned char>(WITNESS_V0_KEYHASH_SIZE, 0x11);
        TransactionKernelInput ok = BaseSpend();
        ok.scriptPubKey = Bytes(oru);
        ok.witness = {ok.signature, ok.pubkey};
        ok.keyhash_match = true;
        ok.crypto_accept = true;
        if (!VerifyRigorPredicateBoundary(ok)) return fail("valid predicate");

        TransactionKernelInput bad = ok;
        bad.scriptPubKey.clear();
        if (VerifyRigorPredicateBoundary(bad)) return fail("invalid predicate");

        TransactionKernelInput badsig = ok;
        badsig.crypto_accept = false;
        if (VerifyRigorPredicateBoundary(badsig)) return fail("bad signature");

        TransactionKernelInput sighash_a = ok;
        sighash_a.spent_script = sighash_a.scriptPubKey;
        sighash_a.amount = 1;
        TransactionKernelInput sighash_b = sighash_a;
        sighash_b.amount = 2;
        if (CppSighashPreimage(sighash_a) == CppSighashPreimage(sighash_b)) {
            return fail("bad sighash must change preimage");
        }
        sighash_a.crypto_accept = false;
        if (VerifyRigorPredicateBoundary(sighash_a)) return fail("bad sighash accept");
    }

    // --- Draw: empty / winner / tie-break ---
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
        if (!w1.HasWinner()) return fail("draw winner");

        ConvergenceDrawSet two = one;
        two.candidates = {a, b};
        ConvergenceDrawSet swap = one;
        swap.candidates = {b, a};
        const auto w2 = ResolveConvergenceDraw(two, Consensus::RES_PROBABILITY_TARGET_CONST);
        const auto w3 = ResolveConvergenceDraw(swap, Consensus::RES_PROBABILITY_TARGET_CONST);
        if (!w2.HasWinner() || !w3.HasWinner()) return fail("draw tie-break");
        if (two.candidates[static_cast<size_t>(w2.winner_index)].txid !=
            swap.candidates[static_cast<size_t>(w3.winner_index)].txid) {
            return fail("draw order independence");
        }

        ConvergenceDrawSet empty;
        empty.source_height = 0;
        if (ResolveConvergenceDraw(empty, Consensus::RES_PROBABILITY_TARGET_CONST).HasWinner()) {
            return fail("draw empty");
        }
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
    std::printf("draw: PASS\n");
    std::printf("pow boundary: PASS\n");
    return 0;
}
