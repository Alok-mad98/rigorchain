#!/usr/bin/env python3
# Copyright (c) 2026 The Rigor Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""A1: production VerifyRigorPredicate is MakeKernelInput + the P058 boundary.

P058 proves CXX_VerifyRigorPredicate = Coq_spend_allowed over an explicit
TransactionKernelInput. After A1 the production entry marshals through
MakeKernelInput and decides accept/reject with VerifyRigorPredicateBoundary.
This lint keeps that wiring, the four oracle-field derivations, and the
H1 encoding pin from rotting. The differential test remains as a lock
that the public entry and the boundary still agree.
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / "core/src"

BRIDGE = SRC / "test/predicate_boundary_bridge.h"
DIFF_TEST = SRC / "test/test_predicate_boundary_differential.cpp"
STUB = SRC / "test/predicate_boundary_ed25519_stub.cpp"
FUZZ = ROOT / "fuzz/predicate_boundary_differential/predicate_boundary_fuzz.cpp"
FUZZ_SMOKE = ROOT / "fuzz/predicate_boundary_differential/run_smoke.sh"
GATE = ROOT / "formal_verification/run_transaction_kernel_refinement.sh"
BOUNDARY_H = SRC / "consensus/txkernel/transaction_kernel_input.h"
PROD = SRC / "script/predicate_verify.cpp"
VALIDATION = SRC / "validation.cpp"
COQ = ROOT / "formal_verification/coq/TransactionKernelRefinement.v"

# The four booleans the boundary takes as given. Each must be derived from a
# production primitive in MakeKernelInput, or the comparison proves nothing.
ORACLES = {
    "scriptSig_push_only": "IsPushOnly",
    "stack_clean": "stack.size()",
    "keyhash_match": "Hash160",
    "crypto_accept": "CheckEd25519Signature",
}

# H1: production's signature-encoding rule is file-static. Pin the body so a
# richer encoding check cannot land in predicate_verify.cpp while MakeKernelInput
# still treats encoding as size == SIGNATURE_SIZE.
ENCODING_FN = re.compile(
    r"bool\s+static\s+IsValidEd25519SignatureEncoding\s*\([^)]*\)\s*\{(.*?)\n\}",
    re.DOTALL,
)
ENCODING_BODY = "return sig.size() == ed25519::SIGNATURE_SIZE;"

STATUS = "PREDICATE_BOUNDARY_VS_PRODUCTION=SAME-FUNCTION"


def main() -> int:
    errors: list[str] = []

    for path in (BRIDGE, DIFF_TEST, STUB, FUZZ, FUZZ_SMOKE, BOUNDARY_H, PROD):
        if not path.is_file():
            errors.append(f"missing {path.relative_to(ROOT)}")
    if errors:
        print("FAIL: predicate boundary differential")
        for e in errors:
            print(f"  - {e}")
        return 1

    marshal = BOUNDARY_H.read_text(encoding="utf-8")
    for field, primitive in ORACLES.items():
        if field not in marshal:
            errors.append(f"MakeKernelInput does not set {field}")
        elif primitive not in marshal:
            errors.append(f"MakeKernelInput sets {field} without using {primitive}")

    if "TransactionKernelInput MakeKernelInput(const CScript&" not in marshal:
        errors.append("production MakeKernelInput missing")
    if "inline bool VerifyRigorPredicateBoundary(" not in marshal:
        errors.append("VerifyRigorPredicateBoundary missing")

    # Both sides must actually be called from the test adaptor.
    bridge = BRIDGE.read_text(encoding="utf-8")
    if "RunProductionObserved(" not in bridge:
        errors.append("bridge never calls production VerifyRigorPredicate")
    if "VerifyRigorPredicateBoundary(" not in bridge:
        errors.append("bridge never calls VerifyRigorPredicateBoundary")
    if "AcceptanceGrew(" not in bridge:
        errors.append("bridge missing AcceptanceGrew")
    if "RunProduction(" in bridge.split("RunProductionObserved")[0] or re.search(
        r"\binline bool RunProduction\s*\(", bridge
    ):
        errors.append("dead RunProduction adaptor must be removed")
    if re.search(r"\binline bool RunDiagnostic\s*\(", bridge):
        errors.append("dead RunDiagnostic adaptor must be removed")
    if bridge.count("MakeKernelInput") < 2:
        errors.append("bridge must expose a test overload of production MakeKernelInput")

    # Flag bits and the push limit are duplicated in the boundary header.
    for guard in (
        "TK_SCRIPT_VERIFY_SIGPUSHONLY == SCRIPT_VERIFY_SIGPUSHONLY",
        "TK_SCRIPT_VERIFY_CLEANSTACK == SCRIPT_VERIFY_CLEANSTACK",
        "TK_SCRIPT_VERIFY_WITNESS == SCRIPT_VERIFY_WITNESS",
        "TK_MAX_SCRIPT_ELEMENT_SIZE == MAX_SCRIPT_ELEMENT_SIZE",
    ):
        if guard not in marshal:
            errors.append(f"production marshaller missing static_assert: {guard}")

    test = DIFF_TEST.read_text(encoding="utf-8")
    if "RunProductionObserved" not in test or "RunBoundary" not in test:
        errors.append("differential test does not run both implementations")
    if "vacuous" not in test:
        errors.append("differential test has no accepting-case floor")
    checks = test.count("Check(")
    if checks < 40:
        errors.append(f"differential test has only {checks} cases, want >= 40")

    # A1: a verdict-only differential cannot see crypto_accept being gated on
    # less than the pre-A1 predicate (ScriptError becomes assert under
    # ASSERT_FAIL) or the reject-path classifier repeating the verification.
    # Count the trips to the primitive instead.
    if "Calls()" not in bridge or "++m_calls" not in bridge:
        errors.append("BridgeSignatureChecker does not count Ed25519 invocations")
    if "RunProductionObserved" not in bridge:
        errors.append("bridge does not expose RunProductionObserved")
    crypto_cases = test.count("CheckCryptoCalls(")
    if crypto_cases < 14:
        errors.append(f"only {crypto_cases} Ed25519-invocation-count cases, want >= 14")
    for expected in (
        "missing WITNESS flag",
        "non-empty scriptSig",
        "bad signature",
    ):
        if f'crypto-count v0 {expected}"' not in test:
            errors.append(f"missing Ed25519-count case: v0 {expected}")
    if "MemoizingSignatureChecker" in PROD.read_text(encoding="utf-8"):
        errors.append("reject-path still uses MemoizingSignatureChecker; pass FixedEd25519Checker")
    if "FixedEd25519Checker" not in PROD.read_text(encoding="utf-8"):
        errors.append("reject-path classifier may repeat the Ed25519 verification")
    for gate in ("pushonly_gate_ok", "witness_gate_ok"):
        if gate not in marshal:
            errors.append(f"MakeKernelInput lacks the {gate} crypto gate")

    fuzz = FUZZ.read_text(encoding="utf-8")
    if "BOUNDARY_DIVERGENCE" not in fuzz:
        errors.append("fuzz harness does not report BOUNDARY_DIVERGENCE")
    if "vacuous" not in fuzz:
        errors.append("fuzz harness has no accepting-case floor")

    gate = GATE.read_text(encoding="utf-8")
    if "test_predicate_boundary_differential" not in gate:
        errors.append("gate does not build the differential test")
    if "fuzz/predicate_boundary_differential/run_smoke.sh" not in gate:
        errors.append("gate does not run the differential fuzz smoke")
    if STATUS not in gate:
        errors.append(f"gate does not print {STATUS}")
    if "ACCEPTANCE_MONOTONICITY=PASS" not in gate:
        errors.append("gate does not print ACCEPTANCE_MONOTONICITY")
    if "MakeKernelInput.v" not in gate:
        errors.append("gate does not compile MakeKernelInput.v")
    if "MAKEKERNELINPUT_GATES=PROVED" not in gate:
        errors.append("gate does not print MAKEKERNELINPUT_GATES=PROVED")
    if "MAKEKERNELINPUT_FIELD_MAP=VECTOR-TESTED" not in gate:
        errors.append("gate does not print MAKEKERNELINPUT_FIELD_MAP=VECTOR-TESTED")
    if "BOUNDED_EXHAUSTIVE_EQUIVALENCE" in gate:
        errors.append("gate still prints BOUNDED_EXHAUSTIVE_EQUIVALENCE (overclaim; DomainWalk does not call MakeKernelInput)")
    if "BOUNDED_GATE_DOMAIN=9216" not in gate:
        errors.append("gate does not print BOUNDED_GATE_DOMAIN=9216")
    if "DOMAIN_CRYPTO_TRUE=300" not in gate:
        errors.append("gate does not pin DOMAIN_CRYPTO_TRUE=300 to the extracted oracle")
    if "test_makekernelinput_field_map" not in gate:
        errors.append("gate does not build test_makekernelinput_field_map")
    if "extract_makekernelinput_oracle.sh" not in gate:
        errors.append("gate does not run extracted MakeKernelInput test oracle")
    if "EXTRACTED_KERNEL_TEST_ORACLE=PASS" not in gate:
        errors.append("gate does not print EXTRACTED_KERNEL_TEST_ORACLE")
    if "COMPCERT_VST_CANDIDATE=MakeKernelInput" not in gate:
        errors.append("gate does not pin CompCert/VST candidate to MakeKernelInput")
    if "COMPCERT_VST_DEFERRED=TRUE" not in gate:
        errors.append("gate does not keep CompCert/VST deferred")

    prod = PROD.read_text(encoding="utf-8")
    if "MakeKernelInput(" not in prod:
        errors.append("production VerifyRigorPredicate does not call MakeKernelInput")
    if "VerifyRigorPredicateBoundary(" not in prod:
        errors.append("production VerifyRigorPredicate does not call VerifyRigorPredicateBoundary")
    if "#include <consensus/txkernel/transaction_kernel_input.h>" not in prod:
        errors.append("predicate_verify.cpp does not include the kernel input header")

    if VALIDATION.is_file():
        val = VALIDATION.read_text(encoding="utf-8", errors="replace")
        if "VerifyRigorPredicateBoundary(" in val:
            errors.append("validation.cpp must not call VerifyRigorPredicateBoundary directly")
        if "ClassifyRigorPredicateError(" in val:
            errors.append("validation.cpp must not call the diagnostic predicate (second authority)")

    # A1-F2: production accept is the boundary only. Diagnostic classifies
    # ScriptError after a reject; it must not AND into the verdict.
    def extract_function_body(text: str, signature: str) -> str:
        start = text.find(signature)
        if start < 0:
            return ""
        brace = text.find("{", start)
        if brace < 0:
            return ""
        depth = 0
        for i, ch in enumerate(text[brace:], brace):
            if ch == "{":
                depth += 1
            elif ch == "}":
                depth -= 1
                if depth == 0:
                    return text[brace : i + 1]
        return text[brace:]

    vrp = extract_function_body(prod, "bool VerifyRigorPredicate(")
    if "VerifyRigorPredicateBoundary(" not in vrp:
        errors.append("production VerifyRigorPredicate does not call the boundary")
    if re.search(r"if\s*\(\s*ClassifyRigorPredicateError\s*\(", vrp):
        errors.append("production dual-verifies with the diagnostic predicate")
    if "&& ClassifyRigorPredicateError" in vrp:
        errors.append("production VerifyRigorPredicate uses diagnostic as authority")

    if "ACCEPTANCE_SET_GREW" not in test or "NEW_ACCEPT=TRUE" not in test:
        errors.append("differential test does not flag acceptance-set growth")
    if "LEGACY_ACCEPT=FALSE" not in test:
        errors.append("differential test does not name LEGACY_ACCEPT")
    if "ACCEPTANCE_MONOTONICITY=PASS" not in test:
        errors.append("differential test does not report ACCEPTANCE_MONOTONICITY")
    if "ED25519_COUNT_NOT_CONSENSUS_CONDITION" not in test:
        errors.append("Ed25519 counts must be recorded as non-consensus metrics")
    if "AcceptanceGrewSelfTest" not in test:
        errors.append("differential test missing AcceptanceGrew self-test")
    if re.search(r"prod\s*!=\s*diag", test):
        errors.append("differential must not require prod==diagnostic equality (that restores dual authority)")
    if "ACCEPTANCE_SET_SHRANK" not in test:
        errors.append("differential must record acceptance-set shrink as a metric")
    if "SCRIPT_ERR_RIGOR_CLASSIFIER_DIVERGENCE" not in test:
        errors.append("differential must pin classifier divergence ScriptError")

    if "BOUNDARY_DIVERGENCE" not in fuzz:
        errors.append("fuzz harness does not report BOUNDARY_DIVERGENCE")
    if "ACCEPTANCE_SET_GREW" not in fuzz or "NEW_ACCEPT=TRUE" not in fuzz:
        errors.append("fuzz harness does not report ACCEPTANCE_SET_GREW")
    if "EARLY_CRYPTO" not in fuzz:
        errors.append("fuzz harness does not detect early Ed25519 (decode/flag bypass)")
    if "REPEAT_CRYPTO" not in fuzz:
        errors.append("fuzz harness does not detect repeated Ed25519")
    if "PickFlags" not in fuzz:
        errors.append("fuzz harness does not vary flags")
    if "KnownFlags" not in fuzz or "UNKNOWN_FLAG_NOT_INERT" not in fuzz:
        errors.append("fuzz harness does not prove unknown flag bits are inert")
    if "521" not in fuzz:
        errors.append("fuzz harness does not cover oversize witness items")
    if not re.search(r"size\s*==\s*0", fuzz):
        errors.append("fuzz harness does not cover empty input")
    if "truncated" not in fuzz and "malformed mint" not in fuzz:
        errors.append("fuzz harness does not cover malformed mint decode")
    if "vacuous" not in fuzz:
        errors.append("fuzz harness has no accepting-case floor")
    if "ED25519_COUNT_NOT_CONSENSUS_CONDITION" not in fuzz:
        errors.append("fuzz must record Ed25519 counts as non-consensus metrics")
    if "--selftest" not in fuzz:
        errors.append("fuzz harness missing --selftest for ACCEPTANCE_SET_GREW detector")
    if "--selftest" not in FUZZ_SMOKE.read_text(encoding="utf-8"):
        errors.append("fuzz smoke does not run --selftest")
    if "PREDICATE_FUZZ_FORCE_BOUNDARY_ACCEPT" not in FUZZ_SMOKE.read_text(encoding="utf-8"):
        errors.append("fuzz smoke does not build force-accept detector proof")

    # Every reject reason the production predicate can reach on the mint path
    # must have a boundary counterpart.
    boundary = marshal
    prod_gates = {
        "IsResMintScript": "IsResMintScript",
        "IsWitnessProgram": "IsWitnessProgram",
        "IsRigorResNativeWitnessProgram": "IsRigorResNativeWitnessProgram",
        "WITNESS_V0_KEYHASH_SIZE": "WITNESS_V0_KEYHASH_SIZE",
        "WITNESS_V1_RES_SIZE": "WITNESS_V1_RES_SIZE",
        "ParseResMintScript": "ParseResMintScript",
    }
    for name, want in prod_gates.items():
        if name in prod and want not in boundary:
            errors.append(f"production uses {name} but the boundary does not")

    # H1: pin the restated signature-encoding rule.
    m = ENCODING_FN.search(prod)
    if not m:
        errors.append("cannot locate IsValidEd25519SignatureEncoding in predicate_verify.cpp")
    else:
        stmts = [
            ln.strip()
            for ln in m.group(1).splitlines()
            if ln.strip() and not ln.strip().startswith("//")
        ]
        if stmts != [ENCODING_BODY]:
            errors.append(
                "IsValidEd25519SignatureEncoding body changed; MakeKernelInput restates it. "
                f"want [{ENCODING_BODY}] got {stmts}"
            )
    if "ed25519::SIGNATURE_SIZE" not in marshal:
        errors.append("MakeKernelInput must derive crypto_accept from ed25519::SIGNATURE_SIZE, not a literal")
    if re.search(r"in\.signature\.size\(\)\s*==\s*64", marshal):
        errors.append("MakeKernelInput hardcodes signature length 64; use ed25519::SIGNATURE_SIZE")
    if "ed25519::PUBKEY_SIZE" not in marshal:
        errors.append("MakeKernelInput must derive the pubkey length from ed25519::PUBKEY_SIZE")

    # The Coq header must not read as if the production function were proved.
    coq = COQ.read_text(encoding="utf-8")
    header_end = coq.find("*)")
    header = coq[: header_end if header_end > 0 else 0]
    if "oracle" not in header.lower() and "external" not in header.lower():
        errors.append("TransactionKernelRefinement.v header does not name the oracle inputs")
    for field in ORACLES:
        if f"tki_{field}" not in coq:
            errors.append(f"Coq input lacks tki_{field}")

    marshal_coq = ROOT / "formal_verification/coq/MakeKernelInput.v"
    if not marshal_coq.is_file():
        errors.append("missing MakeKernelInput.v marshalling spec")
    else:
        mv = marshal_coq.read_text(encoding="utf-8")
        if "Definition make_crypto_accept" not in mv:
            errors.append("MakeKernelInput.v missing make_crypto_accept")
        if "witness_no_crypto_without_flag" not in mv:
            errors.append("MakeKernelInput.v missing witness-flag gate theorem")
        if "acceptance_grew" not in mv:
            errors.append("MakeKernelInput.v missing acceptance_grew")
        if "Definition to_tki" not in mv:
            errors.append("MakeKernelInput.v missing to_tki")
        if "make_crypto_reject_blocks_spend" not in mv:
            errors.append("MakeKernelInput.v missing make_crypto_reject_blocks_spend")
        if "Require Import TransactionKernelRefinement" not in mv:
            errors.append("MakeKernelInput.v is not connected to P058")
        if "Definition all_raw" not in mv:
            errors.append("MakeKernelInput.v missing bounded exhaustive domain")
        if "to_tki_projects_flags" not in mv:
            errors.append("MakeKernelInput.v missing field-projection theorems")
        if "empty_sighash" in mv:
            errors.append("MakeKernelInput.v still uses empty_sighash; connect SighashPreimage")
        if "Definition canonical_sighash" not in mv:
            errors.append("MakeKernelInput.v missing canonical_sighash")
        if "different_tags_different_preimages" not in mv:
            errors.append("MakeKernelInput.v missing different_tags_different_preimages")
        if "kernel_signature_message" not in mv:
            errors.append("MakeKernelInput.v missing kernel_signature_message")
        if "mint_oru_preimages_differ" not in mv:
            errors.append("MakeKernelInput.v missing mint/ORU preimage separation")
        if "Definition sighash_path_consistent" not in mv:
            errors.append("MakeKernelInput.v missing sighash_path_consistent")
        if "crypto_accept_implies_path_tag" not in mv:
            errors.append("MakeKernelInput.v missing crypto_accept_implies_path_tag")
        if "mint_oru_tag_rejected" not in mv:
            errors.append("MakeKernelInput.v missing mint_oru_tag_rejected (L-8 must be observable)")
        if "unknown_version_empty_tag" not in mv:
            errors.append("MakeKernelInput.v missing unknown_version_empty_tag")
        if "Definition tag_mismatch_pins" not in mv:
            errors.append("MakeKernelInput.v missing tag_mismatch_pins")
        if "path_sighash_tag (is_mint : bool) (ver : Z) (native : bool)" not in mv:
            errors.append("path_sighash_tag must take native (unknown/non-native → empty tag)")

    marshal_cpp = ROOT / "core/src/consensus/txkernel/transaction_kernel_input.h"
    mh = marshal_cpp.read_text(encoding="utf-8")
    if "FillMakeKernelSighash" not in mh and "AssignKernelSighashTag" not in mh:
        errors.append("MakeKernelInput does not fill sighash tag/script fields")
    if "GetKernelSighashTxView" not in mh:
        errors.append("MakeKernelInput does not read tx sighash fields from the checker")
    if "sighash_fields_present" not in mh:
        errors.append("MakeKernelInput missing sighash_fields_present")
    if "if (in.sighash_tag.empty() || !in.sighash_fields_present) return {};" not in mh:
        errors.append("CppSighashPreimage must fail-closed without sighash_fields_present")
    if "AssignKernelSighashZeros" in mh:
        errors.append("MakeKernelInput still forges zero sighash fields when no tx is present")
    if "IsRigorResNativeWitnessProgram" not in mh:
        errors.append("MakeKernelInput does not gate the RES tag on native RES")
    if "KernelPathSighashTagOk" not in mh:
        errors.append("VerifyRigorPredicateBoundary must check path-tag consistency")
    if "KernelAssignedSighashTag" not in mh:
        errors.append("MakeKernelInput missing KernelAssignedSighashTag")

    fmap = (SRC / "test/test_makekernelinput_field_map.cpp").read_text(encoding="utf-8")
    if "MAKEKERNELINPUT_SIGHASH=VECTOR-TESTED" not in fmap:
        errors.append("field-map test does not dump real sighash fields")
    if "ExpectPathSighash" not in fmap:
        errors.append("field-map test does not check path sighash tags")
    if "SignatureHashEquivalence" not in fmap or "SignatureHash(" not in fmap:
        errors.append("field-map test does not lock CppSighashPreimage to SignatureHash")
    if "MAKEKERNELINPUT_SIGHASH_EQ_SIGNATUREHASH=PASS" not in fmap:
        errors.append("field-map test does not print MAKEKERNELINPUT_SIGHASH_EQ_SIGNATUREHASH")
    if "sighash_fields_present" not in fmap:
        errors.append("field-map test does not check sighash_fields_present")
    if "BOUNDED_EXHAUSTIVE_EQUIVALENCE" in fmap:
        errors.append("field-map still prints BOUNDED_EXHAUSTIVE_EQUIVALENCE")
    if "BOUNDED_GATE_DOMAIN=9216" not in fmap:
        errors.append("field-map does not print BOUNDED_GATE_DOMAIN")
    if "DOMAIN_CRYPTO_TRUE" not in fmap or "trues != 300" not in fmap:
        errors.append("field-map DomainWalk must pin DOMAIN_CRYPTO_TRUE to 300")
    if "MAKEKERNELINPUT_PATH_TAG_GATES=PASS" not in fmap:
        errors.append("field-map does not run PathTagGates")
    if "MAKEKERNELINPUT_PATH_TAG_GATES=PASS" not in gate:
        errors.append("gate does not print MAKEKERNELINPUT_PATH_TAG_GATES")
    if "mint_oru_tag_rejected" not in gate:
        errors.append("gate does not Print Assumptions on mint_oru_tag_rejected")
    if "fuzz/makekernelinput_differential/run.sh" not in gate:
        errors.append("gate does not run makekernelinput out-of-domain fuzz smoke")

    if "SIGHASH_DOMAIN_CONNECTED=PASS" not in gate:
        errors.append("gate does not print SIGHASH_DOMAIN_CONNECTED")
    if "MakeKernelInput.different_tags_different_preimages" not in gate:
        errors.append("gate does not Print Assumptions on sighash domain separation")
    if "MakeKernelInput.crypto_accept_implies_path_tag" not in gate:
        errors.append("gate does not Print Assumptions on sighash_path_consistent")
    if "MAKEKERNELINPUT_SIGHASH_EQ_SIGNATUREHASH=PASS" not in gate:
        errors.append("gate does not print MAKEKERNELINPUT_SIGHASH_EQ_SIGNATUREHASH")
    if "primitives/transaction.cpp" not in gate:
        errors.append("gate does not link primitives/transaction.cpp for SignatureHash lock")

    workflow = ROOT / ".github/workflows/consensus-kernel-integrity.yml"
    if "run_transaction_kernel_refinement.sh" not in workflow.read_text(encoding="utf-8"):
        errors.append("CI workflow does not run run_transaction_kernel_refinement.sh")

    if errors:
        print("FAIL: predicate boundary differential")
        for e in errors:
            print(f"  - {e}")
        return 1

    print("SIGHASH_DOMAIN_CONNECTED=PASS")
    print("PREDICATE_BOUNDARY_DIFFERENTIAL=PASS")
    print(f"differential_cases={checks}")
    print(STATUS)
    print("ACCEPTANCE_MONOTONICITY=PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
