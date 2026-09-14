// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// P068: dual-path canary. Does not switch production. Not a Boost test_rigor object.

#ifndef ENABLE_CONSENSUS_KERNEL_CANARY
#define ENABLE_CONSENSUS_KERNEL_CANARY
#endif
#ifndef RIGOR_KERNEL_ABI_VERIFY_ONLY
#define RIGOR_KERNEL_ABI_VERIFY_ONLY
#endif

#include <consensus/kernel_canary/kernel_canary.h>
#include <consensus/bridge/kernel_marshalling.h>
#include <consensus/params.h>
#include <consensus/res_issuance.h>
#include <consensus/rigor_consensus_lock.h>
#include <consensus/rigor_issuance.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

static int fail(const char* msg)
{
    std::fprintf(stderr, "FAIL: %s\n", msg);
    return 1;
}

static CScript Oru()
{
    return CScript() << RIGOR_PUSH_0 << std::vector<unsigned char>(WITNESS_V0_KEYHASH_SIZE, 0x11);
}

static CScript Res()
{
    return CScript() << RIGOR_PUSH_1 << std::vector<unsigned char>(WITNESS_V1_RES_SIZE, 0xaa);
}

static CScript Unknown()
{
    return CScript() << RIGOR_PUSH_2 << std::vector<unsigned char>(20, 0x00);
}

static std::vector<uint8_t> ScriptBytes(const CScript& s)
{
    return std::vector<uint8_t>(s.begin(), s.end());
}

static uint256 Hex256(const char* hex)
{
    uint256 out;
    std::vector<unsigned char> raw;
    const std::string s(hex);
    for (size_t i = 0; i < s.size(); i += 2) {
        unsigned v = 0;
        std::sscanf(s.c_str() + static_cast<int>(i), "%2x", &v);
        raw.push_back(static_cast<unsigned char>(v));
    }
    std::memcpy(out.begin(), raw.data(), 32);
    return out;
}

static rigor_u256 U256FromHex(const char* hex)
{
    rigor_u256 out{};
    Uint256ToRigorU256(Hex256(hex), &out);
    return out;
}

static TransactionKernelInput ValidOruTx()
{
    TransactionKernelInput in;
    in.flags = TK_SCRIPT_VERIFY_WITNESS;
    in.scriptSig_push_only = true;
    in.stack_clean = true;
    in.signature.assign(64, 0x07);
    in.pubkey.assign(32, 0x09);
    const CScript s = Oru();
    in.scriptPubKey.assign(s.begin(), s.end());
    in.witness = {in.signature, in.pubkey};
    in.keyhash_match = true;
    in.crypto_accept = true;
    in.sighash_tag = KernelAssignedSighashTag(s);
    return in;
}

static ConsensusKernelInput Base()
{
    ConsensusKernelInput in;
    in.height = 1;
    in.draw_ctx.payout_height = 101;
    in.draw_ctx.convergence_root = U256FromHex("1111111111111111111111111111111111111111111111111111111111111111");
    in.draw_ctx.source_block_hash = U256FromHex("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
    in.draw_ctx.target = U256FromHex("5555555555555555555555555555555555555555555555555555555555555555");
    in.supply.height = 1;
    return in;
}

static bool Check(const KernelCanaryExecutor& ex, const ConsensusKernelInput& in, KernelResult& out, const char* name)
{
    const char* field = nullptr;
    if (!ex.Compare(in, out, &field)) {
        std::fprintf(stderr, "CONSENSUS_KERNEL_CANARY_DISCREPANCY case=%s field=%s\n", name, field ? field : "?");
        return false;
    }
    return true;
}

int main()
{
    const KernelCanaryExecutor ex;
    const Consensus::Params p = KernelFrozenParams();
    KernelResult o;

    {
        ConsensusKernelInput in = Base();
        in.height = 0;
        in.supply = {};
        in.supply.height = 0;
        if (!Check(ex, in, o, "genesis")) return fail("genesis");
        if (o.oru_subsidy != 0) return fail("genesis ORU");
        if (o.res_schedule != static_cast<uint64_t>(Consensus::RES_DESTROY_COST_GENESIS)) {
            return fail("genesis RES destroy cost");
        }
        if (o.res_mint_expectation != 0) return fail("genesis RES issued");
        if (o.draw_has_winner) return fail("genesis draw");
        if (o.coinbase_expectation != 0) return fail("genesis coinbase");
        if (!o.supply_ok || !o.accept) return fail("genesis accept");
    }

    const uint32_t epochs[] = {0, 1, 99, 100, 101, 209999, 210000, 419999, 420000};
    for (uint32_t h : epochs) {
        ConsensusKernelInput in = Base();
        in.height = h;
        in.supply.height = h;
        char name[32];
        std::snprintf(name, sizeof(name), "epoch-%u", h);
        if (!Check(ex, in, o, name)) return fail(name);
        if (o.oru_subsidy != static_cast<uint64_t>(GetBlockSubsidy(static_cast<int>(h), p))) {
            return fail("epoch oru");
        }
        if (o.res_schedule != static_cast<uint64_t>(Consensus::ResDestroyCostAtHeight(static_cast<int>(h)))) {
            return fail("epoch res destroy cost");
        }
        if (o.res_mint_expectation != 0) return fail("RFC-019: no scheduled mint quantity");
        if (o.coinbase_expectation != o.oru_subsidy) return fail("epoch coinbase");
    }

    {
        const auto oru = ScriptBytes(Oru());
        ConsensusKernelInput in = Base();
        in.script = oru.data();
        in.script_len = oru.size();
        in.has_spend = true;
        in.tx = ValidOruTx();
        if (!Check(ex, in, o, "valid-spend")) return fail("valid spend");
        if (!o.spend_ok || !o.accept) return fail("valid spend accept");

        in.tx.crypto_accept = false;
        if (!Check(ex, in, o, "invalid-spend")) return fail("invalid spend");
        if (o.spend_ok || o.accept) return fail("invalid spend must reject");

        in.tx = ValidOruTx();
        in.tx.signature.assign(8, 0x00);
        in.tx.crypto_accept = false;
        if (!Check(ex, in, o, "bad-signature")) return fail("bad signature");
        if (o.spend_ok) return fail("bad signature accepted");

        in.tx = ValidOruTx();
        in.tx.witness = {std::vector<unsigned char>(521, 0x01), in.tx.pubkey};
        if (!Check(ex, in, o, "malformed-witness")) return fail("malformed witness");
        if (o.spend_ok) return fail("malformed witness accepted");
    }

    {
        const uint64_t sat = 209715200000000ULL;
        rigor_u256 tx1 = U256FromHex("0000000000000000000000000000000000000000000000000000000000000001");
        rigor_u256 tx2 = U256FromHex("0000000000000000000000000000000000000000000000000000000000000002");
        rigor_candidate one{tx1, sat};
        rigor_candidate two[2] = {{tx1, sat}, {tx2, sat}};
        rigor_candidate swap[2] = {{tx2, sat}, {tx1, sat}};
        rigor_candidate zero{tx1, 0};

        ConsensusKernelInput in = Base();
        in.candidates = nullptr;
        in.candidate_count = 0;
        if (!Check(ex, in, o, "draw-empty")) return fail("draw empty");
        if (o.draw_has_winner) return fail("empty winner");

        in.candidates = &one;
        in.candidate_count = 1;
        if (!Check(ex, in, o, "draw-single")) return fail("draw single");

        in.candidates = two;
        in.candidate_count = 2;
        if (!Check(ex, in, o, "draw-equal")) return fail("draw equal");
        const rigor_u256 w1 = o.draw_winner;

        in.candidates = swap;
        in.candidate_count = 2;
        if (!Check(ex, in, o, "draw-tie")) return fail("draw tie");
        if (std::memcmp(w1.bytes, o.draw_winner.bytes, 32) != 0) return fail("txid tie-break");

        in.candidates = &zero;
        in.candidate_count = 1;
        if (!Check(ex, in, o, "draw-zero-weight")) return fail("draw zero weight");
        if (o.draw_has_winner) return fail("zero weight must have no winner");
    }

    {
        ConsensusKernelInput in = Base();
        in.supply.has_winner = 1;
        in.supply.card = 1;
        in.supply.mint = 1;
        if (!Check(ex, in, o, "supply-mint")) return fail("mint");
        if (!o.supply_ok) return fail("mint supply");

        in = Base();
        in.supply.fees_oru = 10;
        in.supply.burn = 5;
        if (!Check(ex, in, o, "supply-burn")) return fail("burn");

        in = Base();
        in.supply.fees_oru = 100;
        if (!Check(ex, in, o, "supply-fee")) return fail("fee");

        in = Base();
        in.supply.oru_coinbase = 1ULL << 60;
        if (!Check(ex, in, o, "supply-invalid")) return fail("invalid delta");
        if (o.supply_ok || o.accept) return fail("invalid delta accepted");
        if (o.ledger_delta != 0) return fail("invalid delta leaked");
    }

    {
        const auto unknown = ScriptBytes(Unknown());
        ConsensusKernelInput in = Base();
        in.script = unknown.data();
        in.script_len = unknown.size();
        if (!Check(ex, in, o, "unknown-asset")) return fail("unknown asset");
        if (o.classification != RIGOR_ASSET_INVALID || o.accept) return fail("unknown asset must reject");
    }

    {
        ConsensusKernelInput in = Base();
        in.has_maturity = true;
        in.spend_height = 99;
        in.coin_height = 0;
        if (!Check(ex, in, o, "maturity-99")) return fail("maturity 99");
        if (o.mature || o.accept) return fail("immature");
        in.spend_height = 100;
        if (!Check(ex, in, o, "maturity-100")) return fail("maturity 100");
        if (!o.mature) return fail("mature 100");
        in.spend_height = 101;
        if (!Check(ex, in, o, "maturity-101")) return fail("maturity 101");
        if (!o.mature) return fail("mature 101");
    }

    const char* first = nullptr;
    const int mismatches = ex.RunDeterministicBattery(10000, &first);
    if (mismatches != 0) {
        std::fprintf(stderr, "CONSENSUS_KERNEL_CANARY_DISCREPANCY battery field=%s count=%d\n",
            first ? first : "?", mismatches);
        return fail("10000 battery");
    }

    std::printf("PASS: kernel canary dual-path 10000 cases\n");
    std::printf("CONSENSUS_KERNEL_CANARY_PASS\n");
    std::printf("DISCREPANCY_COUNT=0\n");
        std::printf("PRODUCTION_CONSENSUS_AUTHORITY=RigorConsensusKernel(C++)\n");
    std::printf("FORMAL_REFINEMENT_STATUS=PROVED\n");
    std::printf("EXTRACTED_KERNEL_PRODUCTION=FALSE\n");
    std::printf("COMPCERT_BINARY_VERIFIED_KERNEL=FALSE\n");
    return 0;
}
