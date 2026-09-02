// Copyright (c) 2026 The Rigor Core developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// A1: production VerifyRigorPredicate is MakeKernelInput + the P058 boundary.
// Standalone lock that the public entry and the boundary still agree.
//
// P058 proves CXX_VerifyRigorPredicate = Coq_spend_allowed over an explicit
// TransactionKernelInput. Production now marshals through MakeKernelInput
// and calls VerifyRigorPredicateBoundary for the boolean. This test pins
// that the public entry and a direct boundary call still match; the fuzz
// harness in fuzz/predicate_boundary_differential extends it to random bytes.

#include <script/rigor_tags.h>
#include <script/fixed_ed25519_checker.h>
#include <test/predicate_boundary_bridge.h>

#include <cstdio>
#include <string>
#include <vector>

namespace {

int g_checked = 0;
int g_accepted = 0;

const std::vector<unsigned char> PK(32, 0xaa);
const std::vector<unsigned char> PK_OTHER(32, 0xbb);
const std::vector<unsigned char> SIG(64, 0x11);

std::vector<unsigned char> Bytes(size_t n, unsigned char v) { return std::vector<unsigned char>(n, v); }

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

int g_crypto_calls = 0;
int g_crypto_calls_max = 0;
int g_shrank = 0;
int g_diag_calls = 0;

/** Production (boundary) vs diagnostic (pre-A1). Diagnostic is not an
 *  authority. Safety property is acceptance monotonicity:
 *  NEW_ACCEPT=true && LEGACY_ACCEPT=false is ACCEPTANCE_SET_GREW.
 *  Ed25519 invocation counts are recorded as a security/performance
 *  metric, not as a consensus accept condition. */
bool Check(const std::string& name, const PredicateConcreteInput& c, bool expect)
{
    ++g_checked;
    ScriptError prod_err = SCRIPT_ERR_UNKNOWN_ERROR;
    ScriptError diag_err = SCRIPT_ERR_UNKNOWN_ERROR;
    int prod_calls = 0;
    int diag_calls = 0;
    const bool prod = RunProductionObserved(c, prod_err, prod_calls);
    const bool bound = RunBoundary(c);
    const bool diag = RunDiagnosticObserved(c, diag_err, diag_calls);
    g_crypto_calls += prod_calls;
    g_diag_calls += diag_calls;
    if (prod_calls > g_crypto_calls_max) g_crypto_calls_max = prod_calls;
    bool ok = true;
    if (prod != expect) {
        std::fprintf(stderr, "FAIL: %s: production=%d expected=%d\n", name.c_str(), (int)prod, (int)expect);
        ok = false;
    }
    if (prod != bound) {
        std::fprintf(stderr,
                     "FAIL: %s: production=%d boundary=%d (P058 boundary diverges from authority)\n",
                     name.c_str(), (int)prod, (int)bound);
        ok = false;
    }
    if (AcceptanceGrew(prod, diag)) {
        std::fprintf(stderr,
                     "FAIL: %s: ACCEPTANCE_SET_GREW NEW_ACCEPT=TRUE LEGACY_ACCEPT=FALSE err=%d\n",
                     name.c_str(), (int)diag_err);
        ok = false;
    }
    if (LastFixedEd25519TupleMismatch() > 0) {
        std::fprintf(stderr, "FAIL: %s: FIXED_CHECKER_TUPLE_MISMATCH=%d\n",
                     name.c_str(), LastFixedEd25519TupleMismatch());
        ok = false;
    }
    if (!prod && diag) {
        ++g_shrank;
        if (prod_err != SCRIPT_ERR_RIGOR_CLASSIFIER_DIVERGENCE) {
            std::fprintf(stderr,
                         "FAIL: %s: shrink without SCRIPT_ERR_RIGOR_CLASSIFIER_DIVERGENCE err=%d\n",
                         name.c_str(), (int)prod_err);
            ok = false;
        }
    }
    if (prod_calls > 1) {
        std::fprintf(stderr, "FAIL: %s: REPEAT_CRYPTO production Ed25519 trips=%d\n", name.c_str(), prod_calls);
        ok = false;
    }
    if (ok && prod) ++g_accepted;
    return ok;
}

const unsigned int WIT = SCRIPT_VERIFY_WITNESS;
const unsigned int WIT_CLEAN = SCRIPT_VERIFY_WITNESS | SCRIPT_VERIFY_CLEANSTACK;
const unsigned int WIT_PUSH = SCRIPT_VERIFY_WITNESS | SCRIPT_VERIFY_SIGPUSHONLY;

/** Pin how many times a spend reaches the Ed25519 primitive.
 *
 *  These counts are the pre-A1 counts. Two ways to break them, neither
 *  visible to a verdict comparison:
 *
 *  - gating crypto_accept on less than the pre-A1 predicate did, so a spend
 *    rejected by script shape alone now enters the checker. Under
 *    MissingDataBehavior::ASSERT_FAIL a negative amount there is
 *    assert(!"Missing data"), i.e. a ScriptError becomes a node abort.
 *  - letting the reject-path ScriptError classifier repeat the verification
 *    MakeKernelInput already performed, doubling ~63us of Ed25519 on the
 *    attacker-reachable path. */
bool CheckCryptoCalls(const std::string& name, const PredicateConcreteInput& c,
                      bool expect_verdict, int expect_calls)
{
    ++g_checked;
    ScriptError err = SCRIPT_ERR_UNKNOWN_ERROR;
    ScriptError diag_err = SCRIPT_ERR_UNKNOWN_ERROR;
    int calls = 0;
    int diag_calls = 0;
    const bool prod = RunProductionObserved(c, err, calls);
    const bool bound = RunBoundary(c);
    const bool diag = RunDiagnosticObserved(c, diag_err, diag_calls);
    g_crypto_calls += calls;
    g_diag_calls += diag_calls;
    if (calls > g_crypto_calls_max) g_crypto_calls_max = calls;
    bool ok = true;
    if (prod != expect_verdict) {
        std::fprintf(stderr, "FAIL: %s: production=%d expected=%d\n", name.c_str(), (int)prod, (int)expect_verdict);
        ok = false;
    }
    if (prod != bound) {
        std::fprintf(stderr, "FAIL: %s: production=%d boundary=%d\n", name.c_str(), (int)prod, (int)bound);
        ok = false;
    }
    if (AcceptanceGrew(prod, diag)) {
        std::fprintf(stderr,
                     "FAIL: %s: ACCEPTANCE_SET_GREW NEW_ACCEPT=TRUE LEGACY_ACCEPT=FALSE\n",
                     name.c_str());
        ok = false;
    }
    if (!prod && diag) ++g_shrank;
    if (calls != expect_calls) {
        std::fprintf(stderr,
                     "FAIL: %s: Ed25519 trips=%d want %d (security/performance metric, not consensus)\n",
                     name.c_str(), calls, expect_calls);
        ok = false;
    }
    if (calls > 0 && expect_calls == 0) {
        std::fprintf(stderr, "FAIL: %s: EARLY_CRYPTO bypassed decode/flag gate\n", name.c_str());
        ok = false;
    }
    if (calls > 1) {
        std::fprintf(stderr, "FAIL: %s: REPEAT_CRYPTO trips=%d\n", name.c_str(), calls);
        ok = false;
    }
    if (LastFixedEd25519TupleMismatch() > 0) {
        std::fprintf(stderr, "FAIL: %s: FIXED_CHECKER_TUPLE_MISMATCH=%d\n",
                     name.c_str(), LastFixedEd25519TupleMismatch());
        ok = false;
    }
    if (ok && prod) ++g_accepted;
    return ok;
}

} // namespace

int main()
{
    bool ok = true;
    if (!AcceptanceGrewSelfTest()) {
        std::fprintf(stderr, "FAIL: AcceptanceGrew self-test (detector is inert)\n");
        return 1;
    }
    {
        BridgeSignatureChecker inner(true, 0);
        FixedEd25519Checker checker(inner, true);
        const CScript a;
        CScript b;
        b << 1;
        const std::vector<unsigned char> sig(64, 0x11);
        const std::vector<unsigned char> pk(32, 0xaa);
        if (!checker.CheckEd25519Signature(sig, pk, a, SigVersion::WITNESS_V0) ||
            !checker.CheckEd25519Signature(sig, pk, a, SigVersion::WITNESS_V0) ||
            checker.TupleMismatch() != 0) {
            std::fprintf(stderr, "FAIL: FixedEd25519Checker same-tuple mismatch\n");
            return 1;
        }
        if (!checker.CheckEd25519Signature(sig, pk, b, SigVersion::WITNESS_V0) ||
            checker.TupleMismatch() != 1) {
            std::fprintf(stderr, "FAIL: FixedEd25519Checker detector inert\n");
            return 1;
        }
    }

    // --- Witness v0 P2WPKH ---
    {
        PredicateConcreteInput c;
        c.scriptPubKey = WitnessV0(PK);
        c.witness = Stack({SIG, PK});
        c.flags = WIT;
        c.ed25519_ok = true;
        ok &= Check("v0 valid", c, true);

        PredicateConcreteInput bad_sig_verdict = c;
        bad_sig_verdict.ed25519_ok = false;
        ok &= Check("v0 ed25519 reject", bad_sig_verdict, false);

        PredicateConcreteInput wrong_key = c;
        wrong_key.witness = Stack({SIG, PK_OTHER});
        ok &= Check("v0 keyhash mismatch", wrong_key, false);

        PredicateConcreteInput no_flag = c;
        no_flag.flags = 0;
        ok &= Check("v0 missing WITNESS flag", no_flag, false);

        PredicateConcreteInput with_sigscript = c;
        with_sigscript.scriptSig = CScript() << Bytes(1, 0x51);
        ok &= Check("v0 non-empty scriptSig", with_sigscript, false);

        PredicateConcreteInput one_item = c;
        one_item.witness = Stack({SIG});
        ok &= Check("v0 witness one item", one_item, false);

        PredicateConcreteInput three_items = c;
        three_items.witness = Stack({SIG, PK, Bytes(1, 0x00)});
        ok &= Check("v0 witness three items", three_items, false);

        PredicateConcreteInput empty_witness = c;
        empty_witness.witness = Stack({});
        ok &= Check("v0 witness empty", empty_witness, false);

        PredicateConcreteInput short_sig = c;
        short_sig.witness = Stack({Bytes(63, 0x11), PK});
        ok &= Check("v0 signature 63 bytes", short_sig, false);

        PredicateConcreteInput long_sig = c;
        long_sig.witness = Stack({Bytes(65, 0x11), PK});
        ok &= Check("v0 signature 65 bytes", long_sig, false);

        PredicateConcreteInput short_pk = c;
        short_pk.witness = Stack({SIG, Bytes(31, 0xaa)});
        ok &= Check("v0 pubkey 31 bytes", short_pk, false);

        PredicateConcreteInput huge_item = c;
        huge_item.witness = Stack({Bytes(521, 0x11), PK});
        ok &= Check("v0 witness item 521 bytes", huge_item, false);

        PredicateConcreteInput clean = c;
        clean.flags = WIT_CLEAN;
        ok &= Check("v0 with CLEANSTACK", clean, true);

        PredicateConcreteInput unknown_bits = c;
        unknown_bits.flags = WIT | (1u << 20);
        ok &= Check("v0 unknown high flag bits inert", unknown_bits, true);
    }

    // --- v0 with a 32-byte program (P2WSH is not a consensus spend path) ---
    {
        PredicateConcreteInput c;
        c.scriptPubKey = CScript() << RIGOR_PUSH_0 << Bytes(32, 0x33);
        c.witness = Stack({SIG, PK});
        c.flags = WIT;
        c.ed25519_ok = true;
        ok &= Check("v0 32-byte program", c, false);
    }

    // --- Witness v1 RES key path ---
    {
        PredicateConcreteInput c;
        c.scriptPubKey = WitnessV1(PK);
        c.witness = Stack({SIG, PK});
        c.flags = WIT;
        c.ed25519_ok = true;
        ok &= Check("v1 valid", c, true);

        PredicateConcreteInput mismatch = c;
        mismatch.witness = Stack({SIG, PK_OTHER});
        ok &= Check("v1 pubkey not program", mismatch, false);

        PredicateConcreteInput reject = c;
        reject.ed25519_ok = false;
        ok &= Check("v1 ed25519 reject", reject, false);

        PredicateConcreteInput empty_witness = c;
        empty_witness.witness = Stack({});
        ok &= Check("v1 witness empty", empty_witness, false);

        PredicateConcreteInput three = c;
        three.witness = Stack({SIG, PK, Bytes(1, 0x00)});
        ok &= Check("v1 witness three items", three, false);

        PredicateConcreteInput huge = c;
        huge.witness = Stack({Bytes(521, 0x11), PK});
        ok &= Check("v1 witness item 521 bytes", huge, false);

        PredicateConcreteInput no_flag = c;
        no_flag.flags = 0;
        ok &= Check("v1 missing WITNESS flag", no_flag, false);
    }

    // --- v1 program that is not RES-native, and an unknown version ---
    {
        std::vector<unsigned char> cvg2(32, 0x00);
        std::copy(RigorTags::CVG2.begin(), RigorTags::CVG2.end(), cvg2.begin());
        PredicateConcreteInput c;
        c.scriptPubKey = WitnessV1(cvg2);
        c.witness = Stack({SIG, cvg2});
        c.flags = WIT;
        c.ed25519_ok = true;
        ok &= Check("v1 CVG2-prefixed program", c, false);

        PredicateConcreteInput short_program;
        short_program.scriptPubKey = WitnessV1(Bytes(20, 0x44));
        short_program.witness = Stack({SIG, Bytes(20, 0x44)});
        short_program.flags = WIT;
        short_program.ed25519_ok = true;
        ok &= Check("v1 20-byte program", short_program, false);

        PredicateConcreteInput v2;
        v2.scriptPubKey = CScript() << RIGOR_PUSH_2 << Bytes(32, 0x55);
        v2.witness = Stack({SIG, PK});
        v2.flags = WIT;
        v2.ed25519_ok = true;
        ok &= Check("witness version 2", v2, false);
    }

    // --- RES mint spend ---
    {
        PredicateConcreteInput c;
        c.scriptPubKey = MintScript(0, PK);
        c.scriptSig = CScript() << SIG << PK;
        c.flags = WIT;
        c.spend_height = 10;
        c.ed25519_ok = true;
        ok &= Check("mint valid", c, true);

        PredicateConcreteInput mature = c;
        mature.scriptPubKey = MintScript(100, PK);
        mature.spend_height = 99;
        ok &= Check("mint immature", mature, false);

        PredicateConcreteInput at_maturity = c;
        at_maturity.scriptPubKey = MintScript(100, PK);
        at_maturity.spend_height = 100;
        ok &= Check("mint at maturity", at_maturity, true);

        PredicateConcreteInput wrong_key = c;
        wrong_key.scriptSig = CScript() << SIG << PK_OTHER;
        ok &= Check("mint keyhash mismatch", wrong_key, false);

        PredicateConcreteInput reject = c;
        reject.ed25519_ok = false;
        ok &= Check("mint ed25519 reject", reject, false);

        PredicateConcreteInput one_item = c;
        one_item.scriptSig = CScript() << SIG;
        ok &= Check("mint scriptSig one item", one_item, false);

        PredicateConcreteInput empty_sigscript = c;
        empty_sigscript.scriptSig = CScript();
        ok &= Check("mint scriptSig empty", empty_sigscript, false);

        PredicateConcreteInput short_sig = c;
        short_sig.scriptSig = CScript() << Bytes(63, 0x11) << PK;
        ok &= Check("mint signature 63 bytes", short_sig, false);

        PredicateConcreteInput short_pk = c;
        short_pk.scriptSig = CScript() << SIG << Bytes(31, 0xaa);
        ok &= Check("mint pubkey 31 bytes", short_pk, false);

        PredicateConcreteInput not_pushonly = c;
        not_pushonly.scriptSig = CScript() << SIG << PK << RIGOR_MARKER_RES_MINT;
        ok &= Check("mint scriptSig not push-only", not_pushonly, false);

        PredicateConcreteInput prefix_item = c;
        prefix_item.scriptSig = CScript() << Bytes(1, 0x07) << SIG << PK;
        ok &= Check("mint scriptSig prefix item", prefix_item, true);

        PredicateConcreteInput prefix_clean = prefix_item;
        prefix_clean.flags = WIT_CLEAN;
        ok &= Check("mint prefix item with CLEANSTACK", prefix_clean, false);

        PredicateConcreteInput with_witness = c;
        with_witness.witness = Stack({Bytes(1, 0x00)});
        ok &= Check("mint with non-empty witness", with_witness, false);

        PredicateConcreteInput sigpushonly = c;
        sigpushonly.flags = WIT_PUSH;
        ok &= Check("mint with SIGPUSHONLY", sigpushonly, true);
    }

    // --- Not a spend path at all ---
    {
        PredicateConcreteInput empty;
        empty.flags = WIT;
        empty.ed25519_ok = true;
        ok &= Check("empty scriptPubKey", empty, false);

        PredicateConcreteInput junk;
        junk.scriptPubKey = CScript() << Bytes(10, 0x99);
        junk.witness = Stack({SIG, PK});
        junk.flags = WIT;
        junk.ed25519_ok = true;
        ok &= Check("non-witness non-mint script", junk, false);

        PredicateConcreteInput pushonly_violation;
        pushonly_violation.scriptPubKey = WitnessV0(PK);
        pushonly_violation.scriptSig = CScript() << RIGOR_MARKER_RES_MINT;
        pushonly_violation.witness = Stack({SIG, PK});
        pushonly_violation.flags = WIT_PUSH;
        pushonly_violation.ed25519_ok = true;
        ok &= Check("SIGPUSHONLY violation", pushonly_violation, false);
    }

    // --- Ed25519 invocation counts (A1: same function must also mean same
    //     number of trips to the primitive) ---
    {
        PredicateConcreteInput v0;
        v0.scriptPubKey = WitnessV0(PK);
        v0.witness = Stack({SIG, PK});
        v0.flags = WIT;
        v0.ed25519_ok = true;
        ok &= CheckCryptoCalls("crypto-count v0 accept", v0, true, 1);

        PredicateConcreteInput v0_bad = v0;
        v0_bad.ed25519_ok = false;
        ok &= CheckCryptoCalls("crypto-count v0 bad signature", v0_bad, false, 1);

        PredicateConcreteInput v0_no_flag = v0;
        v0_no_flag.flags = 0;
        ok &= CheckCryptoCalls("crypto-count v0 missing WITNESS flag", v0_no_flag, false, 0);

        PredicateConcreteInput v0_sigscript = v0;
        v0_sigscript.scriptSig = CScript() << Bytes(1, 0x51);
        ok &= CheckCryptoCalls("crypto-count v0 non-empty scriptSig", v0_sigscript, false, 0);

        PredicateConcreteInput v0_wrong_key = v0;
        v0_wrong_key.witness = Stack({SIG, PK_OTHER});
        ok &= CheckCryptoCalls("crypto-count v0 keyhash mismatch", v0_wrong_key, false, 0);

        PredicateConcreteInput v0_short_sig = v0;
        v0_short_sig.witness = Stack({Bytes(63, 0x11), PK});
        ok &= CheckCryptoCalls("crypto-count v0 short signature", v0_short_sig, false, 0);

        PredicateConcreteInput v1;
        v1.scriptPubKey = WitnessV1(PK);
        v1.witness = Stack({SIG, PK});
        v1.flags = WIT;
        v1.ed25519_ok = true;
        ok &= CheckCryptoCalls("crypto-count v1 accept", v1, true, 1);

        PredicateConcreteInput v1_bad = v1;
        v1_bad.ed25519_ok = false;
        ok &= CheckCryptoCalls("crypto-count v1 bad signature", v1_bad, false, 1);

        PredicateConcreteInput v1_no_flag = v1;
        v1_no_flag.flags = 0;
        ok &= CheckCryptoCalls("crypto-count v1 missing WITNESS flag", v1_no_flag, false, 0);

        PredicateConcreteInput mint;
        mint.scriptPubKey = MintScript(0, PK);
        mint.scriptSig = CScript() << SIG << PK;
        mint.flags = WIT_CLEAN;
        mint.spend_height = 100;
        mint.ed25519_ok = true;
        ok &= CheckCryptoCalls("crypto-count mint accept", mint, true, 1);

        PredicateConcreteInput mint_bad = mint;
        mint_bad.ed25519_ok = false;
        ok &= CheckCryptoCalls("crypto-count mint bad signature", mint_bad, false, 1);

        PredicateConcreteInput mint_immature = mint;
        mint_immature.scriptPubKey = MintScript(500, PK);
        ok &= CheckCryptoCalls("crypto-count mint immature", mint_immature, false, 0);

        PredicateConcreteInput mint_pushonly = mint;
        mint_pushonly.scriptSig = CScript() << SIG << PK << RIGOR_MARKER_RES_MINT;
        mint_pushonly.flags = WIT_CLEAN | SCRIPT_VERIFY_SIGPUSHONLY;
        ok &= CheckCryptoCalls("crypto-count mint non-push scriptSig", mint_pushonly, false, 0);

        PredicateConcreteInput unknown;
        unknown.scriptPubKey = CScript() << Bytes(20, 0x02);
        unknown.witness = Stack({SIG, PK});
        unknown.flags = WIT;
        unknown.ed25519_ok = true;
        ok &= CheckCryptoCalls("crypto-count unknown script", unknown, false, 0);
    }

    if (!ok) return 1;
    if (g_accepted < 5) {
        std::fprintf(stderr, "FAIL: only %d accepting cases; the comparison is vacuous\n", g_accepted);
        return 1;
    }

    std::printf("PASS: predicate boundary differential\n");
    std::printf("PREDICATE_BOUNDARY_VS_PRODUCTION=SAME-FUNCTION\n");
    std::printf("ACCEPTANCE_MONOTONICITY=PASS\n");
    std::printf("cases=%d accepting=%d\n", g_checked, g_accepted);
    std::printf("ED25519_INVOCATIONS_RECORDED=%d max=%d\n", g_crypto_calls, g_crypto_calls_max);
    std::printf("ED25519_COUNT_NOT_CONSENSUS_CONDITION=TRUE\n");
    std::printf("ACCEPTANCE_SET_SHRANK=%d (metric, legacy is not an authority)\n", g_shrank);
    std::printf("ED25519_DIAGNOSTIC_TRIPS=%d\n", g_diag_calls);
    std::printf("FIXED_CHECKER_TUPLE_MISMATCH=0\n");
    return 0;
}
