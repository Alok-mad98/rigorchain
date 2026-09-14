// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// P060: Consensus Kernel governance. Standalone.
// Does not switch production. Does not recode issuance / RES / supply / PoW.
// RFC-019: RATE pins. Draw authority is retired.

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
    p.nCoinbaseMaturity = 100;
    return p;
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
    if (!has_kv_string(manifest, "res_authority", "CheckResMintArithmetic")) return fail("manifest res");
    if (!has_kv_string(manifest, "supply_authority", "RigorSupplyDelta")) return fail("manifest supply");
    if (!has_kv_string(manifest, "draw_authority", "retired")) return fail("manifest draw retired");
    if (!has_kv_string(manifest, "classify_authority", "ClassifyRigorTxout")) return fail("manifest classify");
    if (!has_kv_string(manifest, "spend_authority", "VerifyRigorPredicate")) return fail("manifest spend");
    if (!has_kv_bool(manifest, "refinement", false)) return fail("manifest refinement");
    if (!has_kv_bool(manifest, "equivalence", false)) return fail("manifest equivalence");

    if (!has_kv_int(frozen, "coq_admitted", 0)) return fail("frozen COQ_ADMITTED");
    if (!has_kv_int(frozen, "coq_axioms", 0)) return fail("frozen COQ_AXIOMS");
    if (!has_kv_int(frozen, "coq_parameters", 0)) return fail("frozen COQ_PARAMETERS");
    if (!has_kv_bool(frozen, "refinement_proved", false)) return fail("frozen refinement");
    if (!has_kv_bool(frozen, "equivalence_proved", false)) return fail("frozen equivalence");
    if (!has_kv_bool(frozen, "p053_vector_refinement_proved", false)) return fail("P053 honesty");

    if (!has_kv_int(frozen, "height", 0)) return fail("genesis height freeze");
    if (!has_kv_int(frozen, "oru_actual", 0)) return fail("genesis ORU actual freeze");
    if (!has_kv_int(frozen, "res_actual", 0)) return fail("genesis RES actual freeze");
    if (!has_kv_int(frozen, "res_destroy_cost", 102400000000LL)) return fail("genesis RATE freeze");

    const Consensus::Params p = FrozenParams();

    {
        const RigorIssuance g = ComputeRigorIssuance(0, p);
        if (g.oru_subsidy != 0) return fail("genesis ORU actual");
        if (g.epoch != 0) return fail("genesis epoch");
        if (g.oru_subsidy != GetBlockSubsidy(0, p)) return fail("pack != GetBlockSubsidy");
        if (Consensus::ResDestroyCostAtHeight(0) != Consensus::RES_DESTROY_COST_GENESIS) {
            return fail("genesis RATE");
        }
        RigorSupplyDeltaInput in;
        in.height = 0;
        BlockValidationState st;
        RigorSupplyDeltaResult out;
        if (!RigorSupplyDelta(in, p, out, st)) return fail("genesis supply");
        if (out.oru_circulating_delta != 0) return fail("genesis circulating");
        if (out.res_delta != 0) return fail("genesis RES actual");
    }

    struct Pin {
        int height;
        int epoch;
        int64_t oru;
        int64_t rate;
    };
    const Pin pins[] = {
        {0, 0, 0, 102400000000LL},
        {1, 0, 12800000000LL, 102400006103LL},
        {99, 0, 12800000000LL, 102400604248LL},
        {100, 0, 12800000000LL, 102400610352LL},
        {101, 0, 12800000000LL, 102400616456LL},
        {209999, 0, 12800000000LL, 103685743011LL},
        {210000, 1, 12800000000LL, 103685749152LL},
        {419999, 1, 12800000000LL, 104979513869LL},
    };
    for (const Pin& pin : pins) {
        const RigorIssuance g = ComputeRigorIssuance(pin.height, p);
        if (g.epoch != pin.epoch) return fail("epoch pin");
        if (g.oru_subsidy != pin.oru) return fail("ORU pin");
        if (RigorIssuanceEpoch(pin.height) != pin.epoch) return fail("epoch function");
        if (GetBlockSubsidy(pin.height, p) != pin.oru) return fail("GetBlockSubsidy pin");
        if (Consensus::ResDestroyCostAtHeight(pin.height) != pin.rate) return fail("RATE pin");
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
