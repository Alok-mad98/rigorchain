# Reproduction

How to rebuild the critical-consensus kernel cone and re-check that
every top-level theorem is closed.

This is an audit reproduction of **kernel invariants**. It is not a
reproduction of C++ node behavior.

## Recorded toolchain

Captured on the P0-109 evidence host:

```
The Coq Proof Assistant, version 8.15.0
compiled with OCaml 4.13.1
```

```
Linux WORKSTATION 6.6.87.2-microsoft-standard-WSL2
#1 SMP PREEMPT_DYNAMIC Thu Jun  5 18:30:46 UTC 2025
x86_64 GNU/Linux
```

Re-record on your machine:

```
coqc --version
uname -a
```

Required: Coq **8.15.0** (matches the existing corpus). No MathComp,
CompCert, or extra Coq plugins.

## Full reproduction

```
git clone <this-repository>
cd formal_verification/rigor_kernel
coq_makefile -f _CoqProject.kernel-critical -o Makefile
make
./run_kernel_critical.sh
```

`run_kernel_critical.sh` will:

1. Reject `Admitted` / `admit` / `Axiom` / `Parameter` /
   `refinement_proved` / `equivalence_proved` in `*.v`
2. Rebuild the cone
3. Run `Print Assumptions` on the seven top-level theorems and
   `kernel_sound`
4. Run the ten mutation cases (`./mutation/run_all_mutations.sh`)
5. Extract OCaml (`./extraction/build.sh`)
6. Build `rigor-kernel-checker` and run golden vectors (test artifact)
7. Run binary harness mutations B1–B3
8. Print `FORMAL_BOUNDARY_LAYER=COQ_KERNEL_ONLY`,
   `EXTRACTION_FORMAL_PROOF=source_identity_only`,
   `PRODUCTION_REFINEMENT_NOT_CLAIMED=TRUE`,
   `BINARY_SEMANTIC_EQUIVALENCE=false`, and
   `FORMAL_KERNEL_PHASE_BOUNDARY_REPAIR=PASS` on success

## Print Assumptions (manual)

From `formal_verification/rigor_kernel/`:

```
coqc --version
uname -a

coqtop -R . RigorKernel -batch <<'EOF'
Require Import RigorKernel.proofs.UTXO_Soundness.UTXOSoundness.
Require Import RigorKernel.proofs.ORU_Conservation.ORUConservation.
Require Import RigorKernel.proofs.RES_Issuance.RESIssuance.
Require Import RigorKernel.proofs.Script_Correctness.ScriptCorrectness.
Require Import RigorKernel.proofs.Pow_Correctness.PowCorrectness.
Require Import RigorKernel.proofs.Pow_Correctness.ASERTCorrectness.

Print Assumptions utxo_soundness.
Print Assumptions no_hidden_inflation.
Print Assumptions oru_conservation_extended.
Print Assumptions res_issuance_soundness.
Print Assumptions script_correctness.
Print Assumptions pow_valid_deterministic.
Print Assumptions pow_asert_correctness.
EOF
```

Each command must print:

```
Closed under the global context
```

## Extraction and binary

```
cd formal_verification/rigor_kernel
./extraction/build.sh
./kernel_binary/build.sh
./kernel_binary/run_vectors.sh
```

Expected: `KERNEL_EXTRACTION=PASS`, `BINARY_ARTIFACT_BUILT=TRUE`,
`BINARY_VECTOR_TESTS_PASS=TRUE`.

## Mutation reproduction

```
cd formal_verification/rigor_kernel
./mutation/run_all_mutations.sh
```

Healthy output contains ten `EXPECTED_FAILURE` lines and must **not**
contain `mutation_passed`.

## Evidence files

After a successful run, these files must exist:

- `evidence/README.md`
- `evidence/ProofInventory.md`
- `evidence/KernelCoverage.md`
- `evidence/MutationReport.md`
- `evidence/Reproduction.md` (this file)
- `evidence/ClaimsBoundary.md`
- `evidence/BinaryTrace.md`
- `evidence/RefinementBoundary.md`
- `evidence/TCBInventory.md`
- `evidence/KernelCone.md`
- `evidence/ReproducibleBuild.md`
- `evidence/BoundaryMutationReport.md`
- `kernel_binary/TRUST_MANIFEST.md`
- `release/` (see `release/README.md` and `release/check_release.sh`)
