// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// F-1 / F-2: C++ MakeKernelInput vs MakeKernelInput.v.
// Field map: extracted raw record -> CoqMakeCryptoAccept == TKI.crypto_accept.
// Bounded domain: same nest order as all_raw in MakeKernelInput.v (9216 states).

#include <script/rigor_tags.h>
#include <test/makekernelinput_coq_oracle.h>
#include <test/predicate_boundary_bridge.h>

#include <consensus/amount.h>
#include <hash.h>

#include <array>
#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

namespace {

const std::vector<unsigned char> PK(32, 0xaa);
const std::vector<unsigned char> SIG(64, 0x11);

std::vector<unsigned char> KeyHash(const std::vector<unsigned char>& pubkey)
{
    const uint160 h = Hash160(pubkey);
    return std::vector<unsigned char>(h.begin(), h.end());
}

CScript WitnessV0(const std::vector<unsigned char>& pubkey)
{
    return CScript() << RIGOR_PUSH_0 << KeyHash(pubkey);
}

CScript WitnessV1(const std::vector<unsigned char>& program)
{
    return CScript() << RIGOR_PUSH_1 << program;
}

CScript MintScript(int64_t maturity, const std::vector<unsigned char>& pubkey)
{
    return GetResMintScript(maturity, Hash160(pubkey));
}

CScriptWitness Stack(const std::vector<std::vector<unsigned char>>& items)
{
    CScriptWitness w;
    w.stack = items;
    return w;
}

int g_checked = 0;
int g_failed = 0;

std::vector<unsigned char> Tag8(const std::array<unsigned char, 8>& t)
{
    return std::vector<unsigned char>(t.begin(), t.end());
}

void ExpectPathSighash(const char* name, const PredicateConcreteInput& c, const TransactionKernelInput& in)
{
    std::vector<unsigned char> want;
    bool expect_tag = false;
    if (IsResMintScript(c.scriptPubKey)) {
        want = Tag8(RigorTags::SighashMintV1);
        expect_tag = true;
    } else {
        int version = 0;
        std::vector<unsigned char> program;
        if (c.scriptPubKey.IsWitnessProgram(version, program)) {
            if (version == 0) {
                want = Tag8(RigorTags::SighashOruV1);
                expect_tag = true;
            } else if (version == 1 && program.size() == WITNESS_V1_RES_SIZE &&
                       IsRigorResNativeWitnessProgram(program)) {
                want = Tag8(RigorTags::SighashResV1);
                expect_tag = true;
            }
        }
    }
    if (expect_tag) {
        if (in.sighash_tag != want) {
            std::fprintf(stderr, "FAIL: %s: sighash_tag empty or wrong path\n", name);
            ++g_failed;
        }
        const std::vector<unsigned char> script(c.scriptPubKey.begin(), c.scriptPubKey.end());
        if (in.spent_script != script) {
            std::fprintf(stderr, "FAIL: %s: spent_script is not scriptPubKey\n", name);
            ++g_failed;
        }
    } else {
        if (!in.sighash_tag.empty()) {
            std::fprintf(stderr, "FAIL: %s: non-path script carries a sighash tag\n", name);
            ++g_failed;
        }
        if (!CppSighashPreimage(in).empty()) {
            std::fprintf(stderr, "FAIL: %s: empty tag still produced a preimage\n", name);
            ++g_failed;
        }
    }
    if (in.sighash_fields_present) {
        if (in.chain_id.size() != 32 || in.hashPrevouts.size() != 32 ||
            in.hashSequence.size() != 32 || in.hashOutputs.size() != 32 ||
            in.prevout_hash.size() != 32) {
            std::fprintf(stderr, "FAIL: %s: present sighash 32-byte fields not well-formed\n", name);
            ++g_failed;
        }
    } else if (!in.chain_id.empty() || !in.hashPrevouts.empty() ||
               !in.hashSequence.empty() || !in.hashOutputs.empty() ||
               !in.prevout_hash.empty()) {
        std::fprintf(stderr, "FAIL: %s: missing tx context forged zero sighash fields\n", name);
        ++g_failed;
    }
    if (!in.sighash_fields_present && !CppSighashPreimage(in).empty()) {
        std::fprintf(stderr, "FAIL: %s: missing tx context still produced a tagged preimage\n", name);
        ++g_failed;
    }
}

void CheckCase(const char* name, const PredicateConcreteInput& c)
{
    ++g_checked;
    const TransactionKernelInput in = MakeKernelInput(c);
    ExpectPathSighash(name, c, in);
    const CoqRawPredicateInput raw = ExtractRaw(c, in);
    const bool coq = CoqMakeCryptoAccept(raw);
    if (coq != in.crypto_accept) {
        std::fprintf(stderr,
                     "FAIL: %s: CoqMakeCryptoAccept=%d cpp.crypto_accept=%d\n",
                     name, (int)coq, (int)in.crypto_accept);
        ++g_failed;
    }
    const bool bound = VerifyRigorPredicateBoundary(in);
    if (in.crypto_accept == false && bound) {
        std::fprintf(stderr, "FAIL: %s: crypto_accept=false but boundary accepted\n", name);
        ++g_failed;
    }
}

int DomainWalk()
{
    int64_t n = 0;
    int64_t trues = 0;
    const int vers[3] = {0, 1, 2};
    const int lens[3] = {0, 20, 32};
    for (int flags_push = 0; flags_push < 2; ++flags_push)
    for (int flags_wit = 0; flags_wit < 2; ++flags_wit)
    for (int empty = 0; empty < 2; ++empty)
    for (int push = 0; push < 2; ++push)
    for (int is_mint = 0; is_mint < 2; ++is_mint)
    for (int native = 0; native < 2; ++native)
    for (int enc = 0; enc < 2; ++enc)
    for (int kh = 0; kh < 2; ++kh)
    for (int two = 0; two < 2; ++two)
    for (int crypto = 0; crypto < 2; ++crypto)
    for (int vi = 0; vi < 3; ++vi)
    for (int li = 0; li < 3; ++li) {
        CoqRawPredicateInput r;
        r.flags = (flags_push ? (int64_t{1} << 5) : 0) + (flags_wit ? (int64_t{1} << 11) : 0);
        r.scriptSig_empty = empty != 0;
        r.scriptSig_push_only = push != 0;
        r.is_mint = is_mint != 0;
        r.witness_version = vers[vi];
        r.witness_program_len = lens[li];
        r.is_res_native_v1 = native != 0;
        r.encoding_ok = enc != 0;
        r.keyhash_match = kh != 0;
        r.witness_two = two != 0;
        r.witness_items_ok = true;
        r.mint_parsed = true;
        r.mint_mature = true;
        r.crypto_ok = crypto != 0;
        r.sighash_tag = CoqPathSighashTag(r.is_mint, r.witness_version, r.is_res_native_v1);
        ++n;
        if (CoqMakeCryptoAccept(r)) ++trues;
    }
    if (n != 9216) {
        std::fprintf(stderr, "FAIL: domain size %lld want 9216\n", (long long)n);
        return 1;
    }
    if (trues != 300) {
        std::fprintf(stderr, "FAIL: DOMAIN_CRYPTO_TRUE %lld want 300 (must match EXTRACTED_ORACLE_CRYPTO_TRUE)\n",
                     (long long)trues);
        return 1;
    }
    std::printf("BOUNDED_GATE_DOMAIN=9216\n");
    std::printf("DOMAIN_CRYPTO_TRUE=%lld\n", (long long)trues);
    return 0;
}

int RealTxSighashDump()
{
    class CannedTxViewChecker final : public BaseSignatureChecker
    {
    public:
        bool GetKernelSighashTxView(KernelSighashTxView& out) const override
        {
            out.chain_id = uint256S("22");
            out.nVersion = 2;
            out.hashPrevouts = uint256S("33");
            out.hashSequence = uint256S("44");
            out.hashOutputs = uint256S("55");
            out.prevout_hash = uint256S("66");
            out.prevout_n = 3;
            out.amount = 40 * COIN;
            out.nSequence = 9;
            out.nLockTime = 7;
            return true;
        }
    };
    const CScript spent = WitnessV0(PK);
    CannedTxViewChecker checker;
    CScriptWitness wit;
    wit.stack = {SIG, PK};
    const TransactionKernelInput in =
        MakeKernelInput(CScript(), spent, wit, SCRIPT_VERIFY_WITNESS, checker);
    if (in.sighash_tag != Tag8(RigorTags::SighashOruV1)) {
        std::fprintf(stderr, "FAIL: real-tx ORU tag\n");
        return 1;
    }
    if (in.nVersion != 2 || in.prevout_n != 3 || in.nSequence != 9 || in.nLockTime != 7) {
        std::fprintf(stderr, "FAIL: real-tx header fields\n");
        return 1;
    }
    if (in.amount != 40 * COIN) {
        std::fprintf(stderr, "FAIL: real-tx amount\n");
        return 1;
    }
    const uint256 chain = uint256S("22");
    const std::vector<unsigned char> chain_b(chain.begin(), chain.end());
    if (in.chain_id != chain_b) {
        std::fprintf(stderr, "FAIL: real-tx chain_id\n");
        return 1;
    }
    const uint256 hp256 = uint256S("33");
    const std::vector<unsigned char> hp(hp256.begin(), hp256.end());
    if (in.hashPrevouts != hp) {
        std::fprintf(stderr, "FAIL: real-tx hashPrevouts\n");
        return 1;
    }
    if (!in.sighash_fields_present) {
        std::fprintf(stderr, "FAIL: real-tx sighash_fields_present\n");
        return 1;
    }
    const auto pre = CppSighashPreimage(in);
    if (pre.size() < 8 || !std::equal(in.sighash_tag.begin(), in.sighash_tag.end(), pre.begin())) {
        std::fprintf(stderr, "FAIL: real-tx preimage missing path tag\n");
        return 1;
    }
    std::printf("MAKEKERNELINPUT_SIGHASH=VECTOR-TESTED\n");
    return 0;
}

int SignatureHashEquivalence()
{
    CMutableTransaction tx;
    tx.nVersion = 2;
    tx.nLockTime = 7;
    tx.vin.resize(1);
    tx.vin[0].prevout = COutPoint(uint256S("66"), 3);
    tx.vin[0].nSequence = 9;
    tx.vout.resize(1);
    tx.vout[0].nValue = 50 * COIN;
    tx.vout[0].scriptPubKey = WitnessV0(PK);

    const uint256 chain = uint256S("22");
    const CAmount amount = 40 * COIN;
    class TxViewNoCrypto final : public MutableTransactionSignatureChecker
    {
    public:
        using MutableTransactionSignatureChecker::MutableTransactionSignatureChecker;
        bool CheckEd25519Signature(const std::vector<unsigned char>&,
                                   const std::vector<unsigned char>&,
                                   const CScript&,
                                   SigVersion) const override
        {
            return false;
        }
    };
    TxViewNoCrypto checker(&tx, 0, amount, MissingDataBehavior::FAIL, chain, /*nSpendHeightIn=*/100);

    auto check_path = [&](const char* name, const CScript& spent, const CScript& scriptSig,
                          const CScriptWitness& wit, unsigned int flags, SigVersion v) -> int {
        const TransactionKernelInput in =
            MakeKernelInput(scriptSig, spent, wit, flags, checker);
        if (!in.sighash_fields_present) {
            std::fprintf(stderr, "FAIL: %s: SignatureHash lock missing tx view\n", name);
            return 1;
        }
        const auto pre = CppSighashPreimage(in);
        if (pre.empty()) {
            std::fprintf(stderr, "FAIL: %s: SignatureHash lock empty preimage\n", name);
            return 1;
        }
        const uint256 from_kernel = Hash(pre);
        const uint256 live = SignatureHash(spent, tx, 0, amount, v, chain);
        if (from_kernel != live) {
            std::fprintf(stderr, "FAIL: %s: CppSighashPreimage digest != SignatureHash\n", name);
            return 1;
        }
        return 0;
    };

    CScriptWitness wit;
    wit.stack = {SIG, PK};
    if (check_path("oru", WitnessV0(PK), CScript(), wit, SCRIPT_VERIFY_WITNESS,
                   SigVersion::WITNESS_V0) != 0) {
        return 1;
    }
    if (check_path("res", WitnessV1(PK), CScript(), wit, SCRIPT_VERIFY_WITNESS,
                   SigVersion::WITNESS_V1_RES) != 0) {
        return 1;
    }
    const CScript mint_sig = CScript() << SIG << PK;
    if (check_path("mint", MintScript(0, PK), mint_sig, CScriptWitness(),
                   SCRIPT_VERIFY_WITNESS | SCRIPT_VERIFY_CLEANSTACK,
                   SigVersion::RES_MINT) != 0) {
        return 1;
    }
    std::printf("MAKEKERNELINPUT_SIGHASH_EQ_SIGNATUREHASH=PASS\n");
    return 0;
}

int PathTagGates()
{
    if (!CoqPathSighashTag(false, 2, false).empty()) {
        std::fprintf(stderr, "FAIL: unknown witness version must assign empty tag\n");
        return 1;
    }
    if (!CoqPathSighashTag(false, 1, false).empty()) {
        std::fprintf(stderr, "FAIL: non-native v1 must assign empty tag\n");
        return 1;
    }
    if (CoqPathSighashTag(false, 0, false) != Tag8(RigorTags::SighashOruV1)) {
        std::fprintf(stderr, "FAIL: v0 must assign ORU tag\n");
        return 1;
    }

    CoqRawPredicateInput mint;
    mint.is_mint = true;
    mint.scriptSig_push_only = true;
    mint.mint_parsed = true;
    mint.mint_mature = true;
    mint.encoding_ok = true;
    mint.keyhash_match = true;
    mint.crypto_ok = true;
    mint.sighash_tag = CoqPathSighashTag(false, 0, false);
    if (CoqMakeCryptoAccept(mint)) {
        std::fprintf(stderr, "FAIL: mint+ORU tag accepted\n");
        return 1;
    }

    const unsigned int WIT = SCRIPT_VERIFY_WITNESS;
    PredicateConcreteInput c;
    c.scriptPubKey = WitnessV0(PK);
    c.witness = Stack({SIG, PK});
    c.flags = WIT;
    c.ed25519_ok = true;
    TransactionKernelInput in = MakeKernelInput(c);
    in.crypto_accept = true;
    in.keyhash_match = true;
    in.sighash_tag = Tag8(RigorTags::SighashResV1);
    if (VerifyRigorPredicateBoundary(in)) {
        std::fprintf(stderr, "FAIL: hand-built ORU with RES tag accepted\n");
        return 1;
    }
    std::printf("MAKEKERNELINPUT_PATH_TAG_GATES=PASS\n");
    return 0;
}

} // namespace

int main()
{
    if (DomainWalk() != 0) return 1;

    const unsigned int WIT = SCRIPT_VERIFY_WITNESS;
    const unsigned int WIT_PUSH = SCRIPT_VERIFY_WITNESS | SCRIPT_VERIFY_SIGPUSHONLY;
    const unsigned int WIT_CLEAN = SCRIPT_VERIFY_WITNESS | SCRIPT_VERIFY_CLEANSTACK;

    {
        PredicateConcreteInput c;
        c.scriptPubKey = WitnessV0(PK);
        c.witness = Stack({SIG, PK});
        c.flags = WIT;
        c.ed25519_ok = true;
        CheckCase("v0 accept", c);
        c.ed25519_ok = false;
        CheckCase("v0 crypto reject", c);
        c.ed25519_ok = true;
        c.flags = 0;
        CheckCase("v0 no witness flag", c);
        c.flags = WIT;
        c.scriptSig = CScript() << SIG;
        CheckCase("v0 nonempty scriptSig", c);
        c.scriptSig.clear();
        c.witness = Stack({SIG, std::vector<unsigned char>(32, 0xbb)});
        CheckCase("v0 keyhash mismatch", c);
        c.witness = Stack({std::vector<unsigned char>(63, 0x11), PK});
        CheckCase("v0 short sig", c);
        c.witness = Stack({SIG, PK, PK});
        CheckCase("v0 three stack items", c);
        c.witness = Stack({std::vector<unsigned char>(521, 0x11), PK});
        CheckCase("v0 oversize item", c);
        c.scriptPubKey = CScript() << RIGOR_PUSH_0 << std::vector<unsigned char>(19, 0x11);
        c.witness = Stack({SIG, PK});
        CheckCase("v0 short program", c);
    }
    {
        PredicateConcreteInput c;
        c.scriptPubKey = WitnessV1(PK);
        c.witness = Stack({SIG, PK});
        c.flags = WIT;
        c.ed25519_ok = true;
        CheckCase("v1 accept", c);
        c.ed25519_ok = false;
        CheckCase("v1 crypto reject", c);
        c.ed25519_ok = true;
        c.scriptPubKey = WitnessV1(std::vector<unsigned char>(32, 0));
        CheckCase("v1 zero program", c);
        std::vector<unsigned char> tagged(32, 0xaa);
        tagged[0] = 'C'; tagged[1] = 'V'; tagged[2] = 'G'; tagged[3] = '2';
        c.scriptPubKey = WitnessV1(tagged);
        CheckCase("v1 cvg2 prefix", c);
        c.scriptPubKey = CScript() << RIGOR_PUSH_1 << std::vector<unsigned char>(31, 0xaa);
        c.witness = Stack({SIG, PK});
        CheckCase("v1 short program", c);
        c.scriptPubKey = CScript() << RIGOR_PUSH_2 << KeyHash(PK);
        c.witness = Stack({SIG, PK});
        CheckCase("v2 program", c);
    }
    {
        PredicateConcreteInput c;
        c.scriptPubKey = MintScript(0, PK);
        c.scriptSig = CScript() << SIG << PK;
        c.flags = WIT_CLEAN;
        c.spend_height = 100;
        c.ed25519_ok = true;
        CheckCase("mint accept", c);
        c.ed25519_ok = false;
        CheckCase("mint crypto reject", c);
        c.ed25519_ok = true;
        c.scriptPubKey = MintScript(500, PK);
        CheckCase("mint immature", c);
        c.scriptPubKey = MintScript(0, PK);
        c.scriptSig = CScript() << SIG << PK << RIGOR_MARKER_RES_MINT;
        c.flags = WIT_CLEAN | SCRIPT_VERIFY_SIGPUSHONLY;
        CheckCase("mint non-push", c);
        c.flags = WIT_CLEAN;
        c.scriptSig = CScript() << SIG;
        CheckCase("mint short scriptSig", c);
        c.scriptSig.clear();
        CheckCase("mint empty scriptSig", c);
    }
    {
        PredicateConcreteInput c;
        c.flags = WIT;
        c.ed25519_ok = true;
        CheckCase("empty scriptPubKey", c);
        c.scriptPubKey = CScript() << std::vector<unsigned char>(10, 0x99);
        c.witness = Stack({SIG, PK});
        CheckCase("non-witness junk", c);
        PredicateConcreteInput push;
        push.scriptPubKey = WitnessV0(PK);
        push.scriptSig = CScript() << RIGOR_MARKER_RES_MINT;
        push.witness = Stack({SIG, PK});
        push.flags = WIT_PUSH;
        push.ed25519_ok = true;
        CheckCase("sigpushonly violation", push);
    }

    if (g_failed) {
        std::fprintf(stderr, "FAIL: makekernelinput field map failures=%d / %d\n", g_failed, g_checked);
        return 1;
    }
    if (g_checked < 20) {
        std::fprintf(stderr, "FAIL: field-map grid too small (%d)\n", g_checked);
        return 1;
    }
    std::printf("MAKEKERNELINPUT_FIELD_MAP=VECTOR-TESTED\n");
    std::printf("MAKEKERNELINPUT_GATES=PROVED\n");
    std::printf("BOUNDED_GATE_DOMAIN=9216\n");
    std::printf("field_map_cases=%d\n", g_checked);
    if (RealTxSighashDump() != 0) return 1;
    if (SignatureHashEquivalence() != 0) return 1;
    if (PathTagGates() != 0) return 1;
    return 0;
}
