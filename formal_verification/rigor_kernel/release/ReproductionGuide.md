# Reproduction Guide

How a third-party auditor rebuilds the kernel cone and re-checks
theorems, assumptions, mutations, extraction, and vectors.

This reproduces **Coq kernel invariants**. It does not reproduce C++
node behavior and does not prove extraction or binary semantics.

Canonical long form: `../evidence/Reproduction.md` and
`../evidence/ReproducibleBuild.md`.

## Toolchain

Required: Coq **8.15.0**. Recorded host: OCaml 4.13.1, Linux `x86_64`.

```
coqc --version
ocamlc -vnum
uname -a
```

## Full reproduction

```
git clone <this-repository>
cd formal_verification/rigor_kernel
coq_makefile -f _CoqProject.kernel-critical -o Makefile
make
./scripts/extract_and_test.sh
./scripts/diff_verify.sh
./scripts/verified_compiler_path.sh
./release/independent_audit_replay.sh
./run_kernel_critical.sh
```

Expected terminal flags include:

```
ALL_ASSUMPTIONS=CLOSED
NO_ADMITTED=true
NO_AXIOMS=true
FORMAL_BOUNDARY_LAYER=COQ_KERNEL_ONLY
EXTRACTION_FORMAL_PROOF=source_identity_only
EXTRACTION_EQUIVALENCE_NOT_CLAIMED=TRUE
PRODUCTION_REFINEMENT_NOT_CLAIMED=TRUE
BINARY_SEMANTIC_EQUIVALENCE=false
FORMAL_KERNEL_PHASE_BOUNDARY_REPAIR=PASS
BINARY_VECTOR_TESTS_PASS=TRUE
DIFF_VERIFY=PASS
C_FRAGMENT_REHEARSAL=PASS
COMPILER_CORRECTNESS=false
INDEPENDENT_AUDIT_REPLAY=PASS
```

## Print Assumptions (manual)

From `formal_verification/rigor_kernel/`, driven by the Category A/B
table (do not maintain a second list by hand):

```
python3 scripts/check_theorem_classification.py \
  --md release/TheoremClassification.md \
  --also evidence/TheoremClassification.md \
  --emit /tmp/print_assumptions.v
coqtop -R . RigorKernel -R certicoq/surface RigorSurface -batch -l /tmp/print_assumptions.v
```

Each line must be:

```
Closed under the global context
```

## Claims the auditor must not infer

A successful reproduction does **not** establish C++ equivalence,
full-chain / full-node correctness, extraction semantic preservation,
binary semantic equivalence, network correctness, wallet
correctness, or cryptographic-primitive security.
