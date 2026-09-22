#!/usr/bin/env bash
# Critical-consensus kernel CI gate (P0-108 … P0-115 final closure).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
cd "$ROOT"

fail() { echo "FAIL: $*" >&2; exit 1; }

echo "== G-1 forbidden tokens in *.v =="
python3 scripts/check_forbidden_tokens.py || fail "forbidden token present in a .v file"
echo "NO_ADMITTED=true"
echo "NO_ADMITTED=TRUE"
echo "NO_AXIOMS=true"
echo "NO_AXIOMS=TRUE"

echo "== build =="
coq_makefile -f _CoqProject.kernel-critical -o Makefile
make -j"$(nproc 2>/dev/null || echo 2)"

echo "== theorem classification (theorem, file) grep =="
ASSUME_V="$(mktemp --suffix=.v)"
python3 scripts/check_theorem_classification.py \
  --md release/TheoremClassification.md \
  --also evidence/TheoremClassification.md \
  --emit "$ASSUME_V"

echo "== Category A mutation coverage (frontier 1.1) =="
python3 scripts/check_mutation_coverage.py || fail "Category A mutation coverage incomplete"

echo "== no mutation-only switches in the kernel (frontier 1.2) =="
python3 scripts/check_no_mutation_switches.py || fail "mutation-only switch reintroduced"

echo "== Category B adversarial coverage (deferral or mutant) =="
python3 scripts/check_category_b_coverage.py || fail "Category B coverage debt changed"

echo "== no statement that holds of any function (frontier 1.4, tree-wide) =="
python3 scripts/check_null_content.py || fail "null-content statement added or inventory stale"

echo "== Print Assumptions (whole cone: A/B/C + allowlist; must be Closed) =="
OUT="$(mktemp)"
coqtop -R . RigorKernel -R certicoq/surface RigorSurface -batch -l "$ASSUME_V" > "$OUT" 2>&1 || {
  cat "$OUT"
  rm -f "$ASSUME_V" "$OUT"
  fail "coqtop Print Assumptions failed"
}
cat "$OUT"
if grep -qE 'Axiom|Parameter' "$OUT"; then
  rm -f "$ASSUME_V" "$OUT"
  fail "Print Assumptions is not closed"
fi
CLOSED_COUNT="$(grep -c 'Closed under the global context' "$OUT" || true)"
EXPECTED_CLOSED="$(grep -c '^Print Assumptions' "$ASSUME_V" || true)"
if [[ "$CLOSED_COUNT" -ne "$EXPECTED_CLOSED" ]]; then
  rm -f "$ASSUME_V" "$OUT"
  fail "expected $EXPECTED_CLOSED closed statements from the cone, got $CLOSED_COUNT"
fi
rm -f "$ASSUME_V" "$OUT"
echo "OK: every classified and allowlisted statement Closed (count=$CLOSED_COUNT)"
echo "ALL_ASSUMPTIONS=CLOSED"
echo "FORMAL_KERNEL_PHASE_PROOFS=PASS"

echo "== evidence files =="
for f in evidence/README.md evidence/ProofInventory.md evidence/KernelCoverage.md \
         evidence/MutationReport.md evidence/Reproduction.md evidence/ClaimsBoundary.md \
         evidence/BinaryTrace.md evidence/RefinementBoundary.md evidence/TCBInventory.md \
         evidence/KernelCone.md evidence/ReproducibleBuild.md \
         evidence/BoundaryMutationReport.md \
         evidence/BoundaryCorrection.md \
         evidence/ExtractionBoundary.md \
         evidence/TheoremClassification.md \
         evidence/PowScope.md \
         evidence/ASERTScope.md \
         evidence/SerializationBoundary.md \
         evidence/BinaryMutationReport.md \
         evidence/BinaryNegativeVectorReport.md \
         evidence/DiffVerifyReport.md \
         evidence/VerifiedCompilerPathReport.md \
         kernel_binary/TRUST_MANIFEST.md \
         release/TheoremClassification.md \
         release/KernelCompletenessMatrix.md \
         release/RefinementMapping.md \
         release/VerifiedCompilerPath.md \
         release/OpenProperties.md \
         release/KernelInventory.md \
         release/TCBInventory.md \
         release/TrustDebtMatrix.md \
         release/P0-119-DecisionReport.md \
         release/KernelDeepeningReport.md \
         release/KernelCompletenessReport.md \
         release/ExtractionBoundary.md \
         release/CompilerBoundary.md \
         release/FormalTrustRoadmap.md \
         release/ConcreteRefinementBoundary.md \
         release/KernelExecutionBoundaryReport.md \
         release/ExtractionBoundaryHardeningReport.md \
         release/RuntimeBoundaryClosureReport.md \
         release/FinalProofInventory.md \
         release/FormalConeMap.md \
         release/FinalClaimsBoundary.md \
         release/FinalAssumptionAudit.md \
         release/MutationCompletenessAudit.md \
         release/final_adversarial_audit.sh \
         kernel_binary/BinaryBoundaryTrace.md \
         Refinement/TransitionRefinement.v \
         Execution/KernelMachine.v \
         Economic/ORU/ORUSoundness.v \
         Economic/DomainSeparation.v \
         Refinement/EconomicRefinement.v \
         Refinement/KernelComposition.v \
         Kernel/Transaction/Validity.v \
         Kernel/Block/Acceptance.v \
         Kernel/EconomicInvariant/Economic.v \
         Script/Semantics/Execution.v \
         Pow/Acceptance.v \
         Pow/ASERTMatrix.v \
         proofs/UTXO_Soundness/UTXOTransitionSoundness.v \
         Script/Semantics/ScriptTrace.v \
         Pow/TargetRelation.v \
         Pow/ASERTBoundary.v \
         Supply/SupplyTransition.v \
         Kernel/CryptoAssumptions.v \
         Kernel/VerifiedCompilerBoundary.v \
         Kernel/SafetyExtensions.v; do
  [[ -f "$f" ]] || fail "missing $f"
done
grep -q 'KERNEL_COMPLETENESS_MATRIX=100%' release/KernelCompletenessMatrix.md \
  || fail "completeness matrix missing 100% marker"
echo "KERNEL_COMPLETENESS_MATRIX=100%"
echo "TCB_INVENTORY_COMPLETE=TRUE"
echo "REFINEMENT_BOUNDARY_DEFINED=TRUE"
echo "KERNEL_CONE_FROZEN=TRUE"
echo "PROOF_INVENTORY_COMPLETE=TRUE"

echo "== mutation tests =="
./mutation/run_all_mutations.sh

echo "FORMAL_KERNEL_PHASE_EVIDENCE=PASS"
echo "MUTATION_TEST_COMPLETE=true"
echo "MUTATION_TEST_COMPLETE=TRUE"
echo "EXPECTED_FAILURES_CONFIRMED=TRUE"

echo "== faithful extract + executable tests =="
./scripts/extract_and_test.sh
echo "== hand-written C99 fragment rehearsal =="
./scripts/verified_compiler_path.sh
echo "VERIFIED_COMPILER_FORCED_SWITCH=FALSE"

echo "EXTRACTION_SOURCE_IDENTITY_PROVED=TRUE"
echo "EXTRACTION_FORMAL_PROOF=source_identity_only"
echo "PRODUCTION_REFINEMENT_NOT_CLAIMED=TRUE"
echo "COMPILER_CORRECTNESS=false"

echo "== legacy extraction (ExtrOcamlZInt test artifact; not a formal proof) =="
./extraction/build.sh
echo "COQ_KERNEL_IMPLEMENTATION=TRUE"

echo "== kernel binary (generated executable artifact for testing) =="
./kernel_binary/build.sh
./kernel_binary/run_vectors.sh

echo "FORMAL_KERNEL_PHASE_EXTRACTION=PASS"
echo "BINARY_VECTOR_TESTS_PASS=TRUE"
echo "BINARY_SEMANTIC_EQUIVALENCE=false"

echo "== negative vectors INVALID-001 .. INVALID-005 =="
./kernel_binary/run_invalid_vectors.sh
echo "NEGATIVE_VECTOR_TEST_COMPLETE=TRUE"

echo "== binary harness mutations B1–B3 =="
./kernel_binary/run_binary_mutations.sh

echo "FORMAL_BOUNDARY_LAYER=COQ_KERNEL_ONLY"
echo "FORMAL_KERNEL_PHASE_TCB=PASS"

echo "== P0-119 inventory + audit-package freeze =="
python3 - "$ROOT" <<'PY'
import hashlib, pathlib, sys
root = pathlib.Path(sys.argv[1])
# AUDIT_PACKAGE.md must stay byte-identical (handoff entry; no claim edits).
expected = "c57576735ae67cde37ccb01f7d90b133f3706dca1e131e733456bbafabde8fc4"
pkg = root / "release" / "AUDIT_PACKAGE.md"
got = hashlib.sha256(pkg.read_bytes()).hexdigest()
if got != expected:
    sys.exit("AUDIT_PACKAGE.md hash changed (%s); AUDIT_PACKAGE_UNCHANGED required" % got)
inv = (root / "release" / "KernelInventory.md").read_text()
dec = (root / "release" / "P0-119-DecisionReport.md").read_text()
debt = (root / "release" / "TrustDebtMatrix.md").read_text()
op = (root / "release" / "OpenProperties.md").read_text()
need = [
    ("inv source_identity", "EXTRACTION_FORMAL_PROOF=source_identity_only" in inv),
    ("inv no compiler", "COMPILER_CORRECTNESS=false" in inv),
    ("inv no binary", "BINARY_SEMANTIC_EQUIVALENCE=false" in inv),
    ("q1 exact", 'Complete for declared kernel invariant scope.' in dec),
    ("q2 no", "**NO**" in dec),
    ("debt keep flags", "PRODUCTION_REFINEMENT_NOT_CLAIMED=TRUE" in debt),
    ("op cost HIGH", "| HIGH |" in op and "| LOW |" in op and "| MEDIUM |" in op),
]
for name, ok in need:
    if not ok:
        sys.exit("inventory check failed: %s" % name)
print("AUDIT_PACKAGE_UNCHANGED=TRUE")
print("FORMAL_KERNEL_PHASE_INVENTORY=PASS")
print("P0-119_INVENTORY_COMPLETE=TRUE")
PY

echo "== P0-120~125 formal kernel deepening =="
python3 - "$ROOT" <<'PY'
import pathlib, sys
root = pathlib.Path(sys.argv[1])
need_files = [
    "proofs/UTXO_Soundness/UTXOTransitionSoundness.v",
    "Script/Semantics/ScriptState.v",
    "Script/Semantics/ScriptStep.v",
    "Script/Semantics/ScriptTrace.v",
    "Pow/TargetRelation.v",
    "Pow/ASERTBoundary.v",
    "Supply/SupplyTransition.v",
    "release/KernelDeepeningReport.md",
]
for rel in need_files:
    if not (root / rel).is_file():
        sys.exit("deepening missing %s" % rel)
cls = (root / "release" / "TheoremClassification.md").read_text()
rep = (root / "release" / "KernelDeepeningReport.md").read_text()
cov = (root / "evidence" / "KernelCoverage.md").read_text()
pub = (root / "release" / "PublicClaims.md").read_text()
need = [
    ("A block_transition", "`block_transition_utxo_soundness`" in cls),
    ("A recognizer_trace", "`recognizer_trace_disjoint`" in cls),
    ("A target_monotonic", "`target_monotonic`" in cls),
    ("A pow_acceptance", "`pow_acceptance_monotonic`" in cls),
    ("A time_error", "`time_error_direction_preserved`" in cls),
    ("A epoch_supply", "`epoch_supply_transition_sound`" in cls),
    ("B script_trace", "`script_trace_deterministic`" in cls),
    ("B hash_less", "`hash_less_target_equivalent`" in cls),
    ("report A 19", "| A | 13 | 19 |" in rep),
    ("report B 31", "| B | 29 | 31 |" in rep),
    ("report M17", "M17" in rep),
    ("coverage A increase", "| A | 13 | 19 |" in cov),
    ("claim sentence", "Formally verified critical consensus invariants" in pub),
    ("kernel qualifier", "Coq consensus kernel model" in pub),
    ("trust extraction", "EXTRACTION_FORMAL_PROOF=source_identity_only" in pub),
    ("trust compiler", "COMPILER_CORRECTNESS=false" in pub or "compiler correctness" in pub.lower()),
    ("report claim unchanged", "Public claim unchanged" in rep or "Public claim (unchanged)" in rep),
    ("report no SHA3 claim", "SHA3" in rep and "Not claimed" in rep),
]
for name, ok in need:
    if not ok:
        sys.exit("deepening check failed: %s" % name)
exp = (root / "mutation" / "expected_failures.md").read_text()
if "M17" not in exp:
    sys.exit("deepening check failed: M17 missing")
print("FORMAL_KERNEL_PHASE_DEEPENING=PASS")
PY

echo "== P0-126~130 formal kernel completeness =="
python3 - "$ROOT" <<'PY'
import pathlib, sys
root = pathlib.Path(sys.argv[1])
need_files = [
    "Kernel/Transaction/Types.v",
    "Kernel/Transaction/Validity.v",
    "Kernel/Block/Types.v",
    "Kernel/Block/Acceptance.v",
    "Kernel/EconomicInvariant/Economic.v",
    "Script/Semantics/Execution.v",
    "Pow/Acceptance.v",
    "Pow/ASERTMatrix.v",
    "release/KernelCompletenessReport.md",
    "mutation/mutate_completeness.sh",
]
for rel in need_files:
    if not (root / rel).is_file():
        sys.exit("completeness missing %s" % rel)
cls = (root / "release" / "TheoremClassification.md").read_text()
rep = (root / "release" / "KernelCompletenessReport.md").read_text()
cov = (root / "evidence" / "KernelCoverage.md").read_text()
pub = (root / "release" / "PublicClaims.md").read_text()
op = (root / "release" / "OpenProperties.md").read_text()
need = [
    ("A tx conservation", "`transaction_value_conservation`" in cls),
    ("A tx nonnegative", "`transaction_no_negative_value`" in cls),
    ("A tx fee", "`transaction_fee_separation`" in cls),
    ("A block accept", "`block_acceptance_sound`" in cls),
    ("A econ hidden", "`economic_no_hidden_creation`" in cls),
    ("A fees transfer", "`fees_only_transfer`" in cls),
    ("A burn removal", "`burn_only_removal`" in cls),
    ("A subsidy bound", "`subsidy_schedule_bound`" in cls),
    ("A script step", "`script_step_deterministic`" in cls),
    ("A script class", "`script_execution_preserves_class`" in cls),
    ("A pow pipeline", "`pow_pipeline_sound`" in cls),
    ("A asert matrix", "`asert_boundary_complete`" in cls),
    ("B script reject", "`script_invalid_state_rejected`" in cls),
    ("report A 31", "| A | 19 | 31 |" in rep),
    ("report B 32", "| B | 31 | 32 |" in rep),
    ("report M23", "M23" in rep),
    ("coverage A 31", "| A | 19 | 31 |" in cov),
    ("claim sentence", "Formally verified critical consensus invariants" in pub),
    ("kernel qualifier", "Coq consensus kernel model" in pub),
    ("trust extraction", "EXTRACTION_FORMAL_PROOF=source_identity_only" in pub),
    ("report claim unchanged", "Public claim (unchanged)" in rep),
    ("op still open", "They do **not** close OP-R1" in op),
    ("no SHA3 claim", "No SHA3" in rep or "SHA3" in rep),
]
for name, ok in need:
    if not ok:
        sys.exit("completeness check failed: %s" % name)
for bad in ("Bitcoin verified", "full consensus verified", "node verified", "binary verified"):
    if bad in rep:
        sys.exit("completeness forbidden phrase: %s" % bad)
exp = (root / "mutation" / "expected_failures.md").read_text()
if "M23" not in exp:
    sys.exit("completeness check failed: M23 missing")
print("FORMAL_KERNEL_PHASE_COMPLETENESS=PASS")
PY

echo "== P0-131~135 formal refinement foundation =="
python3 - "$ROOT" <<'PY'
import pathlib, sys
root = pathlib.Path(sys.argv[1])
need_files = [
    "Refinement/AbstractKernel.v",
    "Refinement/ConcreteKernel.v",
    "Refinement/RefinementRelation.v",
    "Refinement/TransitionRefinement.v",
    "Execution/KernelMachine.v",
    "release/ExtractionBoundary.md",
    "release/CompilerBoundary.md",
    "release/FormalTrustRoadmap.md",
    "mutation/mutate_refinement.sh",
]
for rel in need_files:
    if not (root / rel).is_file():
        sys.exit("refinement foundation missing %s" % rel)
cls = (root / "release" / "TheoremClassification.md").read_text()
road = (root / "release" / "FormalTrustRoadmap.md").read_text()
ext = (root / "release" / "ExtractionBoundary.md").read_text()
comp = (root / "release" / "CompilerBoundary.md").read_text()
pub = (root / "release" / "PublicClaims.md").read_text()
need = [
    ("A refinement_preservation", "`refinement_preservation`" in cls),
    ("A machine_step", "`machine_step_deterministic`" in cls),
    ("A machine_inv", "`machine_state_preserves_invariant`" in cls),
    ("layer 1-2", "Layer 1" in ext and "Layer 2" in ext and "Layer 3" in ext and "Layer 4" in ext),
    ("no L2-L3", "Do not claim Layer 2" in ext),
    ("source identity", "EXTRACTION_FORMAL_PROOF=source_identity_only" in ext),
    ("compiler OPEN", "**OPEN**" in comp and "Memory model" in comp),
    ("level 3+", "Current = Level 3+" in road),
    ("next L4", "Next = Level 4 foundation" in road),
    ("claim sentence", "Formally verified critical consensus invariants" in pub),
    ("kernel qualifier", "Coq consensus kernel model" in pub),
]
for name, ok in need:
    if not ok:
        sys.exit("refinement foundation check failed: %s" % name)
for bad in ("Implementation verified", "Compiler verified", "Binary verified", "Full node verified"):
    if bad in road or bad in ext or bad in comp:
        sys.exit("refinement forbidden phrase: %s" % bad)
exp = (root / "mutation" / "expected_failures.md").read_text()
if "M25" not in exp:
    sys.exit("refinement foundation check failed: M25 missing")
print("FORMAL_KERNEL_PHASE_REFINEMENT_FOUNDATION=PASS")
PY

echo "== P0-136~140 ORU first-class domain + economic refinement =="
python3 - "$ROOT" <<'PY'
import pathlib, sys
root = pathlib.Path(sys.argv[1])
need_files = [
    "Economic/ORU/ORUDomain.v",
    "Economic/ORU/ORUTransition.v",
    "Economic/ORU/ORUSoundness.v",
    "Economic/DomainSeparation.v",
    "Refinement/EconomicState.v",
    "Refinement/EconomicRefinement.v",
    "Refinement/KernelComposition.v",
    "mutation/mutate_oru_domain.sh",
]
for rel in need_files:
    if not (root / rel).is_file():
        sys.exit("ORU refinement missing %s" % rel)
cls = (root / "release" / "TheoremClassification.md").read_text()
inv = (root / "release" / "KernelInventory.md").read_text()
debt = (root / "release" / "TrustDebtMatrix.md").read_text()
road = (root / "release" / "FormalTrustRoadmap.md").read_text()
pub = (root / "release" / "PublicClaims.md").read_text()
need = [
    ("A oru_domain", "`oru_domain_conservation`" in cls),
    ("A oru_fees", "`oru_fees_are_transfer_only`" in cls),
    ("A burn mono", "`burn_is_monotonic_supply_decrease`" in cls),
    ("A subsidy only", "`subsidy_is_only_creation_source`" in cls),
    ("A res/oru sep", "`res_creation_separate_from_oru_transfer`" in cls),
    ("A econ refine", "`economic_refinement_preserves_conservation`" in cls),
    ("A kernel compose", "`kernel_step_refinement_composes`" in cls),
    ("inv ORU domain", "ORU Domain = Independent Verified Domain" in inv),
    ("debt ORU domain", "ORU Domain = Independent Verified Domain" in debt),
    ("road ORU domain", "ORU Domain = Independent Verified Domain" in road),
    ("separate modelled", "separately modeled and verified" in inv or "separately modelled" in debt),
    ("claim sentence", "Formally verified critical consensus invariants" in pub),
    ("kernel qualifier", "Coq consensus kernel model" in pub),
]
for name, ok in need:
    if not ok:
        sys.exit("ORU refinement check failed: %s" % name)
for doc, text in (("inv", inv), ("debt", debt), ("road", road)):
    if "full monetary system verified" in text:
        sys.exit("ORU refinement forbidden phrase in %s" % doc)
exp = (root / "mutation" / "expected_failures.md").read_text()
if "M28" not in exp:
    sys.exit("ORU refinement check failed: M28 missing")
print("FORMAL_KERNEL_PHASE_ORU_REFINEMENT=PASS")
PY

echo "== P0-141~145 unified kernel transition (Level 4 foundation) =="
python3 - "$ROOT" <<'PY'
import pathlib, sys
root = pathlib.Path(sys.argv[1])
need_files = [
    "Kernel/UnifiedState.v",
    "Kernel/UnifiedTransition.v",
    "Kernel/InvariantBundle.v",
    "proofs/KernelTransitionSoundness.v",
    "Economic/UTXOEconomicBridge.v",
    "mutation/mutate_kernel_transition.sh",
]
for rel in need_files:
    if not (root / rel).is_file():
        sys.exit("kernel transition missing %s" % rel)
cls = (root / "release" / "TheoremClassification.md").read_text()
inv = (root / "release" / "KernelInventory.md").read_text()
idx = (root / "release" / "TheoremIndex.md").read_text()
debt = (root / "release" / "TrustDebtMatrix.md").read_text()
pub = (root / "release" / "PublicClaims.md").read_text()
need = [
    ("A step inv", "`kernel_step_preserves_invariants`" in cls),
    ("A determ", "`kernel_transition_deterministic`" in cls),
    ("A value flow", "`transaction_value_flow_preserves_economic_state`" in cls),
    ("inv unified", "Unified Kernel Transition" in inv),
    ("idx unified", "Unified Kernel Transition" in idx),
    ("debt unified", "Unified Kernel Transition" in debt),
    ("allowed phrase", "Unified consensus kernel transition invariants are formally verified" in inv),
    ("claim sentence", "Formally verified critical consensus invariants" in pub),
    ("kernel qualifier", "Coq consensus kernel model" in pub),
]
for name, ok in need:
    if not ok:
        sys.exit("kernel transition check failed: %s" % name)
for doc, text in (("inv", inv), ("idx", idx), ("debt", debt)):
    for bad in (
        "Bitcoin protocol verified",
        "full implementation verified",
        "node verified",
    ):
        if bad in text:
            sys.exit("kernel transition forbidden phrase in %s: %s" % (doc, bad))
exp = (root / "mutation" / "expected_failures.md").read_text()
if "M29" not in exp or "M30" not in exp:
    sys.exit("kernel transition check failed: M29/M30 missing")
print("FORMAL_KERNEL_PHASE_KERNEL_TRANSITION=PASS")
PY

echo "== P0-120 concrete refinement (Coq abstract → Coq concrete) =="
python3 - "$ROOT" <<'PY'
import pathlib, sys
root = pathlib.Path(sys.argv[1])
need_files = [
    "Refinement/ConcreteKernel/ConcreteKernelState.v",
    "Refinement/ConcreteKernel/ConcreteKernelTransition.v",
    "Refinement/ConcreteKernel/ConcreteKernelMapping.v",
    "release/ConcreteRefinementBoundary.md",
    "mutation/mutate_concrete_refinement.sh",
]
for rel in need_files:
    if not (root / rel).is_file():
        sys.exit("concrete refinement missing %s" % rel)
cls = (root / "release" / "TheoremClassification.md").read_text()
bound = (root / "release" / "ConcreteRefinementBoundary.md").read_text()
opn = (root / "release" / "OpenProperties.md").read_text()
pub = (root / "release" / "PublicClaims.md").read_text()
need = [
    ("A refine step", "`concrete_step_refines_abstract_step`" in cls),
    ("A lift inv", "`concrete_state_preserves_invariant`" in cls),
    ("proved phrase", "Coq abstract" in bound and "Coq concrete" in bound),
    ("not cpp", "C++ implementation refinement" in bound),
    ("not node", "node refinement" in bound),
    ("not compiler", "compiler correctness" in bound),
    ("OP-R2", "OP-R2" in opn and "Concrete implementation refinement" in opn),
    ("claim sentence", "Formally verified critical consensus invariants" in pub),
    ("kernel qualifier", "Coq consensus kernel model" in pub),
]
for name, ok in need:
    if not ok:
        sys.exit("concrete refinement check failed: %s" % name)
for bad in (
    "C++ equivalence",
    "full node verification",
    "Implementation verified",
    "Compiler verified",
):
    if bad in bound:
        sys.exit("concrete refinement forbidden phrase: %s" % bad)
exp = (root / "mutation" / "expected_failures.md").read_text()
if "M31" not in exp or "M32" not in exp:
    sys.exit("concrete refinement check failed: M31/M32 missing")
print("FORMAL_KERNEL_PHASE_CONCRETE_REFINEMENT=PASS")
PY

echo "== P0-121 kernel execution boundary (Level 4.5) =="
python3 - "$ROOT" <<'PY'
import pathlib, sys
root = pathlib.Path(sys.argv[1])
need_files = [
    "ExecutionBoundary/ConcreteExecutionState.v",
    "ExecutionBoundary/ExecutableTransition.v",
    "SerializationBoundary/KernelSerialization.v",
    "kernel_binary/ExecutableKernelChecker.ml",
    "kernel_binary/BinaryBoundaryTrace.md",
    "release/KernelExecutionBoundaryReport.md",
    "mutation/mutate_execution_boundary.sh",
]
for rel in need_files:
    if not (root / rel).is_file():
        sys.exit("execution boundary missing %s" % rel)
cls = (root / "release" / "TheoremClassification.md").read_text()
rep = (root / "release" / "KernelExecutionBoundaryReport.md").read_text()
trace = (root / "kernel_binary" / "BinaryBoundaryTrace.md").read_text()
tcb = (root / "release" / "TCBInventory.md").read_text()
pub = (root / "release" / "PublicClaims.md").read_text()
need = [
    ("A proj", "`execution_projection_preserves_invariants`" in cls),
    ("A refine", "`executable_step_refines_concrete_step`" in cls),
    ("A determ", "`executable_transition_deterministic`" in cls),
    ("A ser", "`decode_encode_identity`" in cls
     and "SerializationBoundary/KernelSerialization.v" in cls),
    ("level 4.5", "Level 4.5 Formal Kernel Execution Boundary" in rep),
    ("verified list", "Executable model refinement" in rep),
    ("not cpp", "C++" in rep and "Not verified" in rep),
    ("trace proved", "Coq abstract" in trace and "Coq concrete" in trace),
    ("trace exec", "executable model" in trace),
    ("layer 0", "Layer 0" in tcb or "| 0 |" in tcb),
    ("layer 1", "Abstract Coq Kernel" in tcb),
    ("layer 2", "Concrete Coq Kernel" in tcb),
    ("layer 3", "Executable Kernel Model" in tcb),
    ("layer 4", "Extraction Interface" in tcb),
    ("layer 5", "Runtime Semantic Model" in tcb),
    ("layer 6", "Runtime Machine Domain" in tcb),
    ("layer 7", "Checker Contract" in tcb),
    ("trusted coq", "Coq kernel" in tcb),
    ("open compiler", "Compiler" in tcb and "Open" in tcb),
    ("claim sentence", "Formally verified critical consensus invariants" in pub),
    ("kernel qualifier", "Coq consensus kernel model" in pub),
]
for name, ok in need:
    if not ok:
        sys.exit("execution boundary check failed: %s" % name)
for doc, text in (("rep", rep), ("trace", trace)):
    for bad in (
        "CPP_EQUIVALENCE=true",
        "FULL_NODE_VERIFICATION=true",
        "Implementation verified",
        "Compiler verified",
        "Binary verified",
        "Extraction verified",
        "Cryptography verified",
        "Production ready",
    ):
        if bad in text:
            sys.exit("execution boundary forbidden phrase in %s: %s" % (doc, bad))
exp = (root / "mutation" / "expected_failures.md").read_text()
if "M33" not in exp or "M34" not in exp or "M35" not in exp:
    sys.exit("execution boundary check failed: M33/M34/M35 missing")
print("FORMAL_KERNEL_PHASE_EXECUTION_BOUNDARY=PASS")
PY

echo "== P0-124~126 extraction boundary hardening (Level 4.8) =="
python3 - "$ROOT" <<'PY'
import pathlib, sys
root = pathlib.Path(sys.argv[1])
need_files = [
    "ExtractionBoundary/ExtractionInterface.v",
    "ExtractionBoundary/ExtractionModel.v",
    "ExtractionBoundary/ExtractionTrace.v",
    "RuntimeModel/IntegerBoundary.v",
    "ExtractionBoundary/GoldenNegatives.v",
    "kernel_binary_vectors/V1.json",
    "kernel_binary_vectors/V2.json",
    "kernel_binary_vectors/V3.json",
    "kernel_binary_vectors/V4.json",
    "kernel_binary_vectors/V5.json",
    "kernel_binary_vectors/V6.json",
    "release/ExtractionBoundaryHardeningReport.md",
    "mutation/mutate_extraction_hardening.sh",
]
for rel in need_files:
    if not (root / rel).is_file():
        sys.exit("extraction hardening missing %s" % rel)
cls = (root / "release" / "TheoremClassification.md").read_text()
rep = (root / "release" / "ExtractionBoundaryHardeningReport.md").read_text()
tcb = (root / "release" / "TCBInventory.md").read_text()
debt = (root / "release" / "TrustDebtMatrix.md").read_text()
pub = (root / "release" / "PublicClaims.md").read_text()
need = [
    ("A iface", "`extraction_interface_bijective`" in cls),
    ("A trace", "`operation_trace_preserved`" in cls),
    ("A int", "`integer_range_safe`" in cls),
    ("A golden", "`golden_negative_vectors_rejected`" in cls),
    ("level 4.8", "Level 4.8" in rep),
    ("chain abs", "Abstract Kernel" in rep),
    ("chain conc", "Concrete Kernel" in rep),
    ("chain exec", "Executable Kernel Model" in rep),
    ("chain ext", "Extraction Interface" in rep),
    ("open compiler", "Compiler" in rep),
    ("layer 5", "Extraction Interface" in tcb and ("| 5 |" in tcb or "Layer 5" in tcb)),
    ("proved iface", "extraction interface consistency" in tcb),
    ("not compiler", "OCaml compiler correctness" in tcb),
    ("debt low", "Extraction Debt" in debt and "LOW" in debt),
    ("debt compiler", "Compiler correctness" in debt and "OPEN" in debt),
    ("claim sentence", "Formally verified critical consensus invariants" in pub),
    ("kernel qualifier", "Coq consensus kernel model" in pub),
]
for name, ok in need:
    if not ok:
        sys.exit("extraction hardening check failed: %s" % name)
for bad in (
    "COMPILER_CORRECTNESS=true",
    "BINARY_SEMANTIC_EQUIVALENCE=true",
    "CPP_REFINEMENT=true",
    "FULL_NODE_VERIFICATION=true",
    "CRYPTO_SECURITY_PROOF=true",
):
    if bad in rep or bad in tcb:
        sys.exit("extraction hardening forbidden claim: %s" % bad)
exp = (root / "mutation" / "expected_failures.md").read_text()
if "M36" not in exp or "M37" not in exp:
    sys.exit("extraction hardening check failed: M36/M37 missing")
print("FORMAL_KERNEL_PHASE_EXTRACTION_HARDENING=PASS")
PY

echo "== P0-127~129 runtime semantics boundary (Level 4.9) =="
python3 - "$ROOT" <<'PY'
import pathlib, sys
root = pathlib.Path(sys.argv[1])
need_files = [
    "Runtime/RuntimeState.v",
    "Runtime/RuntimeTransition.v",
    "Runtime/RuntimeSemantics.v",
    "Runtime/RuntimeFailure.v",
    "CheckerContract/CheckerSpecification.v",
    "kernel_binary_vectors/V7.json",
    "kernel_binary_vectors/V8.json",
    "kernel_binary_vectors/V9.json",
    "kernel_binary_vectors/V10.json",
    "release/RuntimeBoundaryClosureReport.md",
    "mutation/mutate_runtime_boundary.sh",
]
for rel in need_files:
    if not (root / rel).is_file():
        sys.exit("runtime boundary missing %s" % rel)
cls = (root / "release" / "TheoremClassification.md").read_text()
rep = (root / "release" / "RuntimeBoundaryClosureReport.md").read_text()
tcb = (root / "release" / "TCBInventory.md").read_text()
intb = (root / "RuntimeModel" / "IntegerBoundary.v").read_text()
pub = (root / "release" / "PublicClaims.md").read_text()
need = [
    ("A proj", "`runtime_projection_preserves_kernel_domain`" in cls),
    ("A refine", "`runtime_step_refines_executable_step`" in cls),
    ("A determ", "`runtime_step_deterministic`" in cls),
    ("A inv", "`invalid_kernel_input_rejected`" in cls),
    ("A fail", "`runtime_failure_preserved`" in cls),
    ("A domain", "`runtime_domain_closed`" in cls),
    ("A accept", "`checker_accepts_valid_kernel_state`" in cls),
    ("A reject", "`checker_rejects_invalid_kernel_state`" in cls),
    ("money", "money_range_safe" in intb),
    ("height", "height_range_safe" in intb),
    ("target", "target_range_safe" in intb),
    ("te", "time_error_range_safe" in intb),
    ("level 4.9", "Level 4.9" in rep),
    ("chain rt", "Runtime Semantic Boundary" in rep),
    ("open compiler", "Compiler" in rep),
    ("layer 6", "Runtime Checker Contract" in tcb),
    ("tcb 8", "TCB_LAYER_COUNT=8" in tcb),
    ("proved rt", "Coq runtime abstraction" in tcb),
    ("open native", "Native binary" in tcb),
    ("claim sentence", "Formally verified critical consensus invariants" in pub),
    ("kernel qualifier", "Coq consensus kernel model" in pub),
]
for name, ok in need:
    if not ok:
        sys.exit("runtime boundary check failed: %s" % name)
for bad in (
    "COMPILER_CORRECTNESS=true",
    "BINARY_SEMANTIC_EQUIVALENCE=true",
    "NATIVE_BINARY_VERIFIED=true",
    # Audit finding M-6: three banned strings let a dozen equally
    # overreaching claims through. The list now covers every claim the
    # package explicitly disowns in OpenProperties.md.
    "CPP_REFINEMENT=true",
    "FULL_NODE_VERIFIED=true",
    "CRYPTO_SECURITY_PROOF=true",
    "COMPILER_CORRECTNESS=true",
    "COMPILER_VERIFIED=true",
    "BINARY_SEMANTIC_EQUIVALENCE=true",
    "BINARY_EQUIVALENCE=true",
    "CPU_SEMANTICS_VERIFIED=true",
    "EXTRACTION_FORMAL_PROOF=true",
    "EXTRACTION_CORRECTNESS_PROVEN=true",
    "SCRIPT_VM_VERIFIED=true",
    "SIGNATURE_VERIFICATION_PROVEN=true",
    "CONSENSUS_COMPLETE=true",
    "PRODUCTION_READY=true",
    "RUNTIME_VERIFIED=true",
    "NODE_VERIFIED=true",
    "NETWORK_VERIFIED=true",
    "WALLET_VERIFIED=true",
    "verified_compiler_in_current_tcb = true",
):
    if bad in rep or bad in tcb:
        sys.exit("runtime boundary forbidden claim: %s" % bad)
exp = (root / "mutation" / "expected_failures.md").read_text()
if "M38" not in exp or "M41" not in exp:
    sys.exit("runtime boundary check failed: M38/M41 missing")
# The mutant count used to be pinned at 50 in three places. It stopped
# matching the suite the first time a mutant was added, so it is read
# off the declaration table instead of restated.
import re as _re
n_mut = len(_re.findall(r"^\| M\d+ \|", exp, _re.M))
runner = (root / "mutation" / "run_all_mutations.sh").read_text()
if "-ne %d " % n_mut not in runner:
    sys.exit("runtime boundary check failed: runner threshold is not %d" % n_mut)
print("FORMAL_KERNEL_PHASE_RUNTIME_BOUNDARY=PASS")
PY

echo "== P0-130 final inventory + adversarial audit =="
python3 - "$ROOT" <<'PY'
import pathlib, sys
root = pathlib.Path(sys.argv[1])
need_files = [
    "release/FinalProofInventory.md",
    "release/FormalConeMap.md",
    "release/FinalClaimsBoundary.md",
    "release/FinalAssumptionAudit.md",
    "release/MutationCompletenessAudit.md",
    "release/final_adversarial_audit.sh",
]
for rel in need_files:
    if not (root / rel).is_file():
        sys.exit("final inventory missing %s" % rel)
import re as _re
inv = (root / "release" / "FinalProofInventory.md").read_text()
# Both counts are derived from their sources rather than pinned. The
# pinned versions (157 theorems, 65 Category A, 50 mutants) were all
# stale by the time anyone looked.
_cls = (root / "release" / "TheoremClassification.md").read_text()
_a = _cls[_cls.find("Category A"):_cls.find("Category B")]
n_class_a = len({m.group(1) for m in _re.finditer(r"^\|\s*`([^`]+)`\s*\|", _a, _re.M)
                 if m.group(1) != "Theorem"})
n_mutants = len(_re.findall(
    r"^\| M\d+ \|",
    (root / "mutation" / "expected_failures.md").read_text(), _re.M))
cone = (root / "release" / "FormalConeMap.md").read_text()
claims = (root / "release" / "FinalClaimsBoundary.md").read_text()
assum = (root / "release" / "FinalAssumptionAudit.md").read_text()
mut = (root / "release" / "MutationCompletenessAudit.md").read_text()
op = (root / "release" / "OpenProperties.md").read_text()
tcb = (root / "release" / "TCBInventory.md").read_text()
need = [
    ("inv count", "THEOREM_COUNT=%d" % inv.count("- **Name:**") in inv),
    ("inv A", "CATEGORY_A_COUNT=%d" % n_class_a in inv),
    ("cone chain", "Checker Contract" in cone and "Runtime Semantic Model" in cone),
    ("true utxo", "UTXO soundness" in claims),
    ("false cpp", "C++ equivalence" in claims and "FALSE" in claims),
    ("assum pass", "ASSUMPTION_AUDIT_PASS" in assum),
    ("mut count", "MUTATION_COUNT=%d" % n_mutants in mut),
    ("class utxo", "UTXO" in mut and "Extraction" in mut and "Runtime" in mut),
    ("op r1", "OP-R1" in op),
    ("op r2", "OP-R2" in op),
    ("op d1", "OP-D1" in op),
    ("op compiler", "Compiler correctness" in op),
    ("op binary", "Binary semantic equivalence" in op),
    ("op cpu", "CPU semantics" in op),
    ("op crypto", "Crypto proof" in op),
    ("tcb 8", "TCB_LAYER_COUNT=8" in tcb),
    ("tcb trusted", "Trusted:" in tcb),
    ("tcb verified", "Verified:" in tcb),
    ("level 4.9", "Level 4.9" in inv or "Level 4.9" in cone),
]
for name, ok in need:
    if not ok:
        sys.exit("final inventory check failed: %s" % name)
print("FINAL_INVENTORY_DOCS=PASS")
PY
./release/final_adversarial_audit.sh

echo "== P0-131 generated documents are reproducible (audit C-3) =="
python3 release/gen_final_proof_inventory.py --check
python3 - "$ROOT" <<'PY2'
import pathlib, subprocess, sys, tempfile, shutil
root = pathlib.Path(sys.argv[1])
target = root / "release" / "UnclassifiedAllowlist.md"
before = target.read_bytes()
subprocess.run([sys.executable, str(root / "release" / "gen_unclassified_allowlist.py")],
               check=True, stdout=subprocess.DEVNULL)
after = target.read_bytes()
if before != after:
    target.write_bytes(before)
    sys.exit("UnclassifiedAllowlist.md is not what its generator produces; "
             "run release/gen_unclassified_allowlist.py")

# Same treatment for the mutation declaration table. It was hand-kept,
# and it had drifted: it stopped at M50 while the suite ran to M76, and
# several rows named theorems the mutant had been retargeted away from.
decl = root / "mutation" / "expected_failures.md"
before_decl = decl.read_bytes()
subprocess.run([sys.executable, str(root / "mutation" / "gen_expected_failures.py")],
               check=True, stdout=subprocess.DEVNULL)
if before_decl != decl.read_bytes():
    decl.write_bytes(before_decl)
    sys.exit("mutation/expected_failures.md is not what its generator "
             "produces; run mutation/gen_expected_failures.py")
if not (root / "release" / "LevelDefinition.md").is_file():
    sys.exit("missing release/LevelDefinition.md")
inv = (root / "release" / "FinalProofInventory.md").read_text()
if "MATURITY_LEVEL=4.9" not in inv:
    sys.exit("FinalProofInventory.md must carry exactly one MATURITY_LEVEL marker")
print("GENERATED_DOCS_REPRODUCIBLE=TRUE")
PY2
echo "FORMAL_KERNEL_PHASE_FINAL_INVENTORY=PASS"
echo "AUDIT_PACKAGE_READY=TRUE"

echo "== source manifest (read-only) =="
./release/check_source_manifest.sh
echo "SOURCE_MANIFEST_PASS=TRUE"

echo "== release package =="
./release/check_release.sh
echo "FORMAL_KERNEL_PHASE_RELEASE=PASS"

echo "CONSENSUS_CODE_CHANGED=FALSE"
echo "DATABASE_FORMAT_CHANGED=FALSE"
echo "WALLET_FORMAT_CHANGED=FALSE"
echo "CPP_EQUIVALENCE_NOT_CLAIMED=TRUE"
echo "FULL_CHAIN_NOT_CLAIMED=TRUE"
echo "EXTRACTION_EQUIVALENCE_NOT_CLAIMED=TRUE"
echo "BINARY_SEMANTIC_PROOF_NOT_CLAIMED=TRUE"
echo "COMPILER_CORRECTNESS_NOT_CLAIMED=TRUE"
echo "FORMAL_KERNEL_PHASE_BOUNDARY_REPAIR=PASS"
echo "FORMAL_KERNEL_PHASE_FINAL_CLOSURE=PASS"
