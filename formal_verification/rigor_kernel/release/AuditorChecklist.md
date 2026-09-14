# Auditor Checklist

Use this list to decide whether the kernel cone is in the state this
package claims. Tick items only after performing them on a clean
checkout.

**Claim under audit:** Formally verified critical consensus invariants
(Coq consensus kernel only).

```
[ ] clone repository

[ ] verify Coq version

[ ] make kernel

[ ] Print Assumptions

[ ] verify no Admitted

[ ] verify no Axiom

[ ] run mutation tests

[ ] build extracted kernel

[ ] execute vectors

[ ] inspect claims boundary
```

## Suggested commands

From the repository root:

```
git clone <this-repository>
cd formal_verification/rigor_kernel
coqc --version          # expect 8.15.0
coq_makefile -f _CoqProject.kernel-critical -o Makefile
make
./run_kernel_critical.sh
```

Or stepwise:

```
# no Admitted / Axiom / Parameter in *.v
grep -nE 'Admitted|[[:space:]]admit[[:space:]]|^[[:space:]]*Axiom\b|^[[:space:]]*Parameter\b' \
  --include='*.v' -R .

# Print Assumptions — see ReproductionGuide.md

./mutation/run_all_mutations.sh
./scripts/extract_and_test.sh
./scripts/diff_verify.sh
./scripts/verified_compiler_path.sh
./release/independent_audit_replay.sh
less release/IndependentAuditReplay.md
less release/RefinementMapping.md
less release/VerifiedCompilerPath.md
less release/AUDIT_PACKAGE.md
less release/OpenProperties.md
less Kernel/SafetyExtensions.v
less Kernel/CryptoAssumptions.v
less evidence/DiffVerifyReport.md
./extraction/build.sh
./kernel_binary/build.sh
./kernel_binary/run_vectors.sh

# claims / TCB
less evidence/ClaimsBoundary.md
less evidence/TCBInventory.md
less release/PublicClaims.md
less release/VerificationScope.md
```

## Pass criteria

| Check | Pass signal |
|---|---|
| Coq version | 8.15.0 |
| `make` | cone builds |
| Print Assumptions | Closed under the global context for every Category A/B row |
| No Admitted / Axiom | `run_kernel_critical.sh` G-1 OK |
| Mutations | 41 `EXPECTED_FAILURE`, 0 `mutation_passed` |
| Extraction | `KERNEL_EXTRACTION=PASS` |
| Vectors | positive and negative vectors / `BINARY_VECTOR_TESTS_PASS=TRUE` |
| Differential verify | `DIFF_VERIFY=PASS` on the comparable fragment only |
| Crypto inventory | `Kernel/CryptoAssumptions.v`; not a security proof |
| C fragment rehearsal | `C_FRAGMENT_REHEARSAL=PASS`; `COMPILER_CORRECTNESS=false` |
| Open properties | `release/OpenProperties.md` listed, not proved |
| Independent replay | `INDEPENDENT_AUDIT_REPLAY=PASS` via `release/independent_audit_replay.sh` |
| Binary mutations | B1–B3 `EXPECTED_FAILURE` (harness detection only) |
| Claims | `cpp_equivalence=false`, `extraction_equivalence=false`, no full-node / crypto claim |
| Provenance | `ProofSourceManifest.md`; do not cite a commit that does not track `.v` files |
| Final inventory | `release/final_adversarial_audit.sh` → `FORMAL_FINAL_AUDIT_READY=TRUE` |

## Out of scope for this checklist

Do not treat a pass as verification of P2P, mempool, RPC, wallet,
database, miner implementation, or cryptographic primitive security.
