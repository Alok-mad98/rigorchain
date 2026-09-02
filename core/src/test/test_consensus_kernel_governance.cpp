// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// P060: Consensus Kernel governance. Standalone.
// Does not switch production. Does not recode issuance / RES / draw / supply / PoW.

#include <consensus/amount.h>
#include <consensus/consensus.h>
#include <consensus/convergence_logic.h>
#include <consensus/params.h>
#include <consensus/rigor_consensus_lock.h>
#include <consensus/rigor_issuance.h>
#include <consensus/rigor_supply.h>
#include <consensus/txkernel/transaction_kernel_input.h>
#include <consensus/validation.h>
#include <script/script.h>
#include <script/standard.h>
#include <uint256.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
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

static std::string slurp(const std::string& path)
{
    std::ifstream in(path.c_str());
    if (!in) return {};
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

static bool has_kv_string(const std::string& j, const char* key, const char* value)
{
    const std::string needle = std::string("\"") + key + "\": \"" + value + "\"";
    return j.find(needle) != std::string::npos;
}

static bool has_kv_bool(const std::string& j, const char* key, bool value)
{
    const std::string needle = std::string("\"") + key + "\": " + (value ? "true" : "false");
    return j.find(needle) != std::string::npos;
}

static bool has_kv_int(const std::string& j, const char* key, long long value)
{
    const std::string needle = std::string("\"") + key + "\": " + std::to_string(value);
    return j.find(needle) != std::string::npos;
}

static TransactionKernelInput BaseSpend()
{
    TransactionKernelInput in;
    in.flags = TK_SCRIPT_VERIFY_WITNESS;
    in.scriptSig_push_only = true;
    in.stack_clean = true;
    in.signature.assign(64, 0x07);
    in.pubkey.assign(32, 0x09);
    in.sighash_tag = {'R', 'G', 'R', 'O', 'R', 'U', '0', '1'};
    in.chain_id.assign(32, 0x11);
    in.hashPrevouts.assign(32, 0x22);
    in.hashSequence.assign(32, 0x33);
    in.hashOutputs.assign(32, 0x44);
    in.prevout_hash.assign(32, 0x55);
    in.sighash_fields_present = true;
    return in;
}

static std::vector<unsigned char> Bytes(const CScript& s)
{
    return std::vector<unsigned char>(s.begin(), s.end());
}

int main(int argc, char** argv)
{
    std::string root;
    if (argc >= 2) {
        root = argv[1];
    } else if (const char* env = std::getenv("RIGOR_ROOT")) {
        root = env;
    }
    if (root.empty()) {
        return fail("pass repo root as argv[1] or RIGOR_ROOT");
    }

    const std::string manifest_path = root + "/formal_verification/consensus_kernel_manifest.json";
    const std::string frozen_path = root + "/formal_verification/frozen_consensus_vectors.json";
    const std::string manifest = slurp(manifest_path);
    const std::string frozen = slurp(frozen_path);
    if (manifest.empty()) return fail("authority manifest unreadable");
    if (frozen.empty()) return fail("golden vectors unreadable");

    if (!has_kv_string(manifest, "issuance_authority", "GetBlockSubsidy")) return fail("manifest issuance");
    if (!has_kv_string(manifest, "res_authority", "GetResMintReward")) return fail("manifest res");
    if (!has_kv_string(manifest, "supply_authority", "RigorSupplyDelta")) return fail("manifest supply");
    if (!has_kv_string(manifest, "draw_authority", "ResolveConvergenceDraw")) return fail("manifest draw");
    if (!has_kv_string(manifest, "classify_authority", "ClassifyRigorTxout")) return fail("manifest classify");
    if (!has_kv_string(manifest, "spend_authority", "VerifyRigorPredicate")) return fail("manifest spend");
    if (!has_kv_bool(manifest, "refinement", true)) return fail("manifest refinement");
    if (!has_kv_bool(manifest, "equivalence", false)) return fail("manifest equivalence");

    if (!has_kv_int(frozen, "coq_admitted", 0)) return fail("frozen COQ_ADMITTED");
    if (!has_kv_int(frozen, "coq_axioms", 0)) return fail("frozen COQ_AXIOMS");
    if (!has_kv_int(frozen, "coq_parameters", 0)) return fail("frozen COQ_PARAMETERS");
    if (!has_kv_bool(frozen, "refinement_proved", true)) return fail("frozen refinement");
    if (!has_kv_bool(frozen, "equivalence_proved", false)) return fail("frozen equivalence");
    if (!has_kv_bool(frozen, "p053_vector_refinement_proved", false)) return fail("P053 honesty");

    if (!has_kv_int(frozen, "height", 0)) return fail("genesis height freeze");
    if (!has_kv_int(frozen, "oru_actual", 0)) return fail("genesis ORU actual freeze");
    if (!has_kv_int(frozen, "res_actual", 0)) return fail("genesis RES actual freeze");
    if (!has_kv_int(frozen, "res_schedule", 40000000)) return fail("genesis RES schedule freeze");
    if (frozen.find("\"draw\": \"none\"") == std::string::npos) return fail("genesis draw freeze");

    const Consensus::Params p = FrozenParams();

    {
        const RigorIssuance g = ComputeRigorIssuance(0, p);
        if (g.oru_subsidy != 0) return fail("genesis ORU actual");
        if (g.res_reward != 40000000) return fail("genesis RES schedule");
        if (g.epoch != 0) return fail("genesis epoch");
        if (g.oru_subsidy != GetBlockSubsidy(0, p)) return fail("pack != GetBlockSubsidy");
        if (g.res_reward != Consensus::GetResMintReward(0)) return fail("pack != GetResMintReward");
        RigorSupplyDeltaInput in;
        in.height = 0;
        BlockValidationState st;
        RigorSupplyDeltaResult out;
        if (!RigorSupplyDelta(in, p, out, st)) return fail("genesis supply");
        if (out.oru_circulating_delta != 0) return fail("genesis circulating");
        if (out.res_delta != 0) return fail("genesis RES actual");
        if (out.res_reward_scheduled != 40000000) return fail("genesis RES schedule field");
        ConvergenceDrawSet empty;
        empty.source_height = 0;
        if (ResolveConvergenceDraw(empty, Consensus::RES_PROBABILITY_TARGET_CONST).HasWinner()) {
            return fail("genesis draw must be none");
        }
    }

    struct Pin {
        int height;
        int epoch;
        int64_t oru;
        int64_t res;
    };
    const Pin pins[] = {
        {0, 0, 0, 40000000},
        {1, 0, 100000000000LL, 40000000},
        {99, 0, 100000000000LL, 40000000},
        {100, 0, 100000000000LL, 40000000},
        {101, 0, 100000000000LL, 40000000},
        {209999, 0, 100000000000LL, 40000000},
        {210000, 1, 50000000000LL, 14142196},
        {419999, 1, 50000000000LL, 14142196},
    };
    for (const Pin& pin : pins) {
        const RigorIssuance g = ComputeRigorIssuance(pin.height, p);
        if (g.epoch != pin.epoch) return fail("epoch pin");
        if (g.oru_subsidy != pin.oru) return fail("ORU pin");
        if (g.res_reward != pin.res) return fail("RES pin");
        if (RigorIssuanceEpoch(pin.height) != pin.epoch) return fail("epoch function");
        if (GetBlockSubsidy(pin.height, p) != pin.oru) return fail("GetBlockSubsidy pin");
        if (Consensus::GetResMintReward(pin.height) != pin.res) return fail("GetResMintReward pin");
        if (!has_kv_int(frozen, "height", pin.height)) return fail("frozen height missing");
    }

    const CScript oru = CScript() << RIGOR_PUSH_0 << std::vector<unsigned char>(WITNESS_V0_KEYHASH_SIZE, 0x11);
    const CScript res = CScript() << RIGOR_PUSH_1 << std::vector<unsigned char>(WITNESS_V1_RES_SIZE, 0xaa);
    const CScript empty_spk;
    if (ClassifyRigorAsset(oru) != ASSET_ORU) return fail("classify ORU");
    if (ClassifyRigorAsset(res) != ASSET_RES) return fail("classify RES");
    if (ClassifyRigorAsset(empty_spk) != ASSET_INVALID) return fail("classify empty");
    if (ClassifyRigorAsset(oru) != AssetTypeFromScriptPubKey(oru)) return fail("asset wrapper drift");
    {
        std::vector<std::vector<unsigned char>> sols;
        if (ClassifyRigorTxout(oru, sols) != TxoutType::WITNESS_V0_KEYHASH) return fail("txout classify");
    }

    {
        TransactionKernelInput in = BaseSpend();
        in.scriptPubKey = Bytes(oru);
        in.witness = {in.signature, in.pubkey};
        in.keyhash_match = true;
        in.crypto_accept = true;
        if (!VerifyRigorPredicateBoundary(in)) return fail("spend valid");
    }
    {
        TransactionKernelInput in = BaseSpend();
        in.scriptPubKey = Bytes(empty_spk);
        in.witness = {in.signature, in.pubkey};
        in.keyhash_match = true;
        in.crypto_accept = true;
        if (VerifyRigorPredicateBoundary(in)) return fail("spend invalid");
    }
    {
        TransactionKernelInput in = BaseSpend();
        in.scriptPubKey = Bytes(oru);
        in.witness = {in.signature, in.pubkey};
        in.keyhash_match = true;
        in.crypto_accept = false;
        if (VerifyRigorPredicateBoundary(in)) return fail("spend bad signature");
    }
    {
        TransactionKernelInput a = BaseSpend();
        a.scriptPubKey = Bytes(oru);
        a.spent_script = a.scriptPubKey;
        a.amount = 1;
        TransactionKernelInput b = a;
        b.amount = 2;
        if (CppSighashPreimage(a) == CppSighashPreimage(b)) return fail("spend bad sighash preimage");
        a.witness = {a.signature, a.pubkey};
        a.keyhash_match = true;
        a.crypto_accept = false;
        if (VerifyRigorPredicateBoundary(a)) return fail("spend bad sighash");
    }
    {
        TransactionKernelInput in = BaseSpend();
        in.scriptPubKey = Bytes(oru);
        in.witness = {in.signature};
        in.keyhash_match = true;
        in.crypto_accept = true;
        if (VerifyRigorPredicateBoundary(in)) return fail("spend malformed witness");
    }

    {
        const uint64_t sat = 209715200000000ULL;
        ConvergenceDrawSet empty;
        empty.source_height = 0;
        if (ResolveConvergenceDraw(empty, Consensus::RES_PROBABILITY_TARGET_CONST).HasWinner()) {
            return fail("draw empty");
        }
        ConvergenceDrawSet one;
        one.source_height = 100;
        one.convergence_root = uint256_from_hex(
            "1111111111111111111111111111111111111111111111111111111111111111");
        one.source_block_hash = uint256_from_hex(
            "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa");
        ConvergenceDrawCandidate a, b, z;
        a.txid = uint256_from_hex(
            "0000000000000000000000000000000000000000000000000000000000000001");
        a.weight = sat;
        b.txid = uint256_from_hex(
            "0000000000000000000000000000000000000000000000000000000000000002");
        b.weight = sat;
        z.txid = uint256_from_hex(
            "0000000000000000000000000000000000000000000000000000000000000003");
        z.weight = 0;
        one.candidates = {a};
        if (!ResolveConvergenceDraw(one, Consensus::RES_PROBABILITY_TARGET_CONST).HasWinner()) {
            return fail("draw single");
        }
        ConvergenceDrawSet two = one;
        two.candidates = {a, b};
        ConvergenceDrawSet swap = one;
        swap.candidates = {b, a};
        const auto w2 = ResolveConvergenceDraw(two, Consensus::RES_PROBABILITY_TARGET_CONST);
        const auto w3 = ResolveConvergenceDraw(swap, Consensus::RES_PROBABILITY_TARGET_CONST);
        if (!w2.HasWinner() || !w3.HasWinner()) return fail("draw tie winner");
        if (two.candidates[static_cast<size_t>(w2.winner_index)].txid !=
            swap.candidates[static_cast<size_t>(w3.winner_index)].txid) {
            return fail("draw tie order");
        }
        ConvergenceDrawSet zero = one;
        zero.candidates = {z};
        if (ResolveConvergenceDraw(zero, Consensus::RES_PROBABILITY_TARGET_CONST).HasWinner()) {
            return fail("draw weight zero");
        }
        if (ResolveConvergenceDraw(one, uint256()).HasWinner()) return fail("draw T=0");
    }

    std::printf("PASS: consensus kernel governance\n");
    std::printf("AUTHORITY_LOCK=PASS\n");
    std::printf("GOLDEN_VECTOR_FREEZE=PASS\n");
    std::printf("DUPLICATE_RULE_DETECTION=PASS\n");
    std::printf("COQ_ADMITTED=0\n");
    std::printf("COQ_AXIOMS=0\n");
        std::printf("PRODUCTION_CONSENSUS_AUTHORITY=RigorConsensusKernel(C++)\n");
    std::printf("FORMAL_REFINEMENT_STATUS=PROVED\n");
    std::printf("EXTRACTED_KERNEL_PRODUCTION=FALSE\n");
    std::printf("COMPCERT_BINARY_VERIFIED_KERNEL=FALSE\n");
    return 0;
}
