# Independent Audit Simulation Report

Adversarial review of the RIGOR critical-consensus kernel formal
verification package (P0-106 … P0-112), performed as an independent
senior formal-verification auditor.

**Claim under audit:** formally verified critical consensus invariants
(consensus kernel only).

**Auditor posture:** attempt to invalidate the claim. No implementation
assistance was given and no kernel file was modified by this audit.
The only file added is this report.

---

## Scope

Reviewed:

- Coq proofs (`proofs/`, `Supply/`, `Script/`, `Pow/`, `Core/`)
- Executable kernel (`Kernel/*Validator.v`, `KernelInterface.v`, `KernelSoundness.v`)
- Extraction pipeline (`extraction/`)
- Binary artifact (`kernel_binary/`, `rigor-kernel-checker`)
- TCB boundary (`Kernel/RefinementBoundary.v`, `evidence/TCBInventory.md`,
  `evidence/RefinementBoundary.md`, `evidence/KernelCone.md`)
- Release package (`release/`)
- Mutation evidence (`mutation/`, `evidence/MutationReport.md`)

Method: independent re-derivation. The auditor did not rely on the
author's green CI output; every conclusion below was reproduced by
direct computation in `coqtop`, by linking against the extracted OCaml
module, or by executing the shipped binary on auditor-constructed
inputs.

---

## Findings

### F-01

**ID:** F-01
**Severity:** HIGH
**Category:** Extraction soundness / refinement boundary overclaim

**Description:**
`extraction/ExtractKernel.v` imports `ExtrOcamlZInt` and
`ExtrOcamlNatInt`, which map Coq `Z` and `nat` onto OCaml's native
63-bit `int`. The Coq standard library carries an explicit disclaimer
on both files. The proved theorems quantify over unbounded `Z`; the
extracted program computes in a wrapping 63-bit ring. Extraction is
therefore **not semantics-preserving** for this cone, and the divergence
is silent.

Two counterexamples were constructed and executed.

*(a) UTXO — binary accepts what the model rejects.*
Ten outputs each of value `MAX_MONEY = 10^18`. Each output is
individually in range; the sum is `10^19`.

```
Coq  (vm_compute):  sum_values = 10000000000000000000
                    money_range_b sum = false
                    check_utxo t      = false     -> INVALID
Binary:             rigor-kernel-checker          -> VALID
```

The native sum wraps to `776627963145224192`, which re-enters
`money_range`. The binary admits a transaction that `utxo_soundness`
does not cover.

*(b) ASERT — sign inversion of the very invariant being advertised.*
With `te = -10^14`, `tau = 43200`:

```
Coq  (vm_compute):  exp_q_raw te tau = -151703703703703
Extracted OCaml:    exp_q_raw te tau = +61800278630897
Binary on that q:                     -> VALID
```

`te * ASERT_RADIX` overflows and changes sign, so the extracted code
reports a **positive** exponent for a negative time error. This is a
direct contradiction of `negative_time_error_preserved`, the theorem
that `kernel_binary/test_vectors/asert_negative_time.json` is named
after.

**Impact:**
The public claim ("formally verified critical consensus invariants")
is **not** invalidated — the Coq theorems remain correct over `Z`.
What is invalidated are two documentation statements that place
extraction inside the proof-covered region:

- `evidence/RefinementBoundary.md`, "Proof coverage" list:
  "Layer 2 → Layer 3 (extraction of the same functions)" listed as
  "Covered by Coq proofs and `Print Assumptions` Closed".
- `evidence/ClaimsBoundary.md`: "The proof covers Spec → Coq →
  validator → extraction."

Both are stronger than the evidence supports. The refinement boundary
is drawn one layer too far out: the correct stop is **Layer 2**
(Coq validators), not Layer 3 (extracted OCaml).

Additionally `Audit/KernelNonClaims.v` sets
`kernel_execution_verified = true`. Given F-01 that name overstates;
what is true is that the extracted code was *generated from* verified
definitions, not that its execution is verified.

**Required action:**
1. Move the proof boundary to Layer 2 in `RefinementBoundary.md` and
   `ClaimsBoundary.md`; list `Z`/`nat` → `int` as a **trusted,
   unverified** step in `TCBInventory.md`.
2. Either extract to an arbitrary-precision target (e.g. `zarith`) and
   re-audit, or state the bounded-integer precondition explicitly and
   restrict the artifact's advertised input domain.
3. Rename or retire the `kernel_execution_verified` flag.

---

### F-02

**ID:** F-02
**Severity:** HIGH
**Category:** Release provenance / reproducibility

**Description:**
`release/VersionManifest.md` records
`Git commit: 71ffd83d49a2c3dce3824fe8e89221ab5b68bcef`. That commit
contains **none** of the audited material:

```
git ls-files formal_verification/rigor_kernel          -> 0
kernel .v files present in worktree                    -> 26
kernel .v files present at 71ffd83                     -> 0
```

The only paths matching `rigor_kernel` in that commit are unrelated C++
files (`core/src/rigor_kernel/keccak.*`, `pow_digest.*`). The entire
verified cone is untracked. `release/ReproductionGuide.md` and
`release/AuditorChecklist.md` both open with `git clone <repository>`,
which would yield an auditor zero proofs.

**Impact:**
The release package cannot be independently reproduced from its own
provenance record. A third party cannot establish *which* sources the
`PASS` flags refer to. This does not affect proof correctness but
defeats the audit-preparation purpose of P0-112.

**Required action:**
Commit the cone, then regenerate `VersionManifest.md` so the recorded
commit actually contains the audited sources. Consider recording a
content hash (e.g. a manifest of `sha256` per `.v`) in addition to the
commit id.

---

### F-03

**ID:** F-03
**Severity:** MEDIUM
**Category:** Test-vector adequacy

**Description:**
All six golden vectors are positive:

```
6   "expect": "VALID"
```

There is no negative vector in the suite. Consequently a degenerate
checker that unconditionally returns `true` would pass
`kernel_binary/run_vectors.sh` and print `BINARY_VECTOR_TESTS_PASS=TRUE`.
The suite has no power to detect an accept-everything regression, and
in fact did not detect either divergence in F-01.

**Impact:**
`BINARY_VECTOR_TESTS_PASS=TRUE` is a much weaker signal than its name
suggests. The theorem→binary trace in `evidence/BinaryTrace.md` is
confirmed only on happy-path inputs.

**Required action:**
Add negative and boundary vectors per domain (expected `INVALID`),
including the two F-01 counterexamples, so the suite is falsifiable.

---

### F-04

**ID:** F-04
**Severity:** MEDIUM
**Category:** Theorem strength / index presentation

**Description:**
Two of the seven advertised top-level theorems are content-free as
stated.

*`pow_valid_deterministic`* is `forall h, pow_valid h 0 = pow_valid h 0`.
The auditor proved that `hash_le_target` requires `0 < target`, hence
`pow_valid h 0` is unsatisfiable for every `h` and every digest
function. The theorem is therefore an equality between two identical
false propositions — it holds for any `H` whatsoever, including a
deliberately wrong one.

*`script_correctness`* unfolds to
`exists a, classify s = a /\ classify_script s = classify_script s /\
classify_asset s = classify_asset s`. The auditor discharged the same
shape for a deliberately broken classifier that maps **every** script
to `A_RES`:

```
Theorem evil_classifier_also_satisfies_script_correctness : ...
Proof. intros s. exists (evil_classify s). repeat split. Qed.   (* compiles *)
```

The genuine content for these two domains lives elsewhere and is real:
`pow_valid_rejects_above_target`, `pow_valid_b_reflect`,
`recognizer_pairwise_disjoint`, `witness_never_falls_to_mint_or_converge`.
Mutations M7 and M8 confirm that the substantive lemmas do have teeth.

`release/TheoremIndex.md` states the trivial statements honestly, but
`release/ProofEvidenceIndex.md` and `evidence/BinaryTrace.md` elevate
them to the headline PoW/Script row.

**Impact:**
A reader who checks only the top-level theorem list will over-estimate
what is proved about PoW and script classification.

**Required action:**
Promote the substantive lemmas to the headline rows, or annotate the
two trivial ones as function-ness lemmas rather than the domain's
soundness result.

---

### F-05

**ID:** F-05
**Severity:** MEDIUM
**Category:** Trace integrity (theorem ↔ validator linkage)

**Description:**
`Kernel/PowValidator.v` imports only `Pow.CompactBits`. It never
imports `Pow.AbstractHash` or `Pow.PowModel`. Its `pow_digest_z` field
is a caller-supplied integer with no proved relationship to
`H (pow_preimage h)`. `check_pow_sound` therefore proves facts about
the *supplied* integer, not about a PoW digest.

The same pattern applies to ASERT: `check_asert` never evaluates
`asert_next_target`, so the `asert_negative_time.json` vector does not
exercise `pow_asert_correctness`; `check_asert_deterministic` is a
re-export, not a use.

`evidence/BinaryTrace.md` nonetheless draws the edges
`pow_valid_deterministic → check_pow_sound` and
`pow_asert_correctness → check_asert_sound`.

**Impact:**
Those two trace edges are asserted by document, not established by
proof. The binary's PoW check is an integer comparison on untrusted
input; the digest computation is entirely outside the artifact.

**Required action:**
Either state in `BinaryTrace.md` that the PoW/ASERT binary rows cover
only the target-comparison and operator-split fragments, or connect
`check_pow` to `pow_valid_b` (which *is* proved reflective via
`pow_valid_b_reflect`).

---

### F-06

**ID:** F-06
**Severity:** LOW
**Category:** Evidence weight

**Description:**
`Kernel/RefinementBoundary.v` defines all three `RefinementBoundary`
fields as `True`. `kernel_boundary_closed` is therefore `True`, proved
by `exact I`. It is counted as one of the nine "Closed under the global
context" theorems enforced by `run_kernel_critical.sh` and is cited as
the "machine-checked marker" for the TCB boundary in
`evidence/RefinementBoundary.md`.

**Impact:**
The marker carries no semantic content and cannot fail. Counting it
alongside the substantive theorems inflates the apparent proof count.

**Required action:**
Present it as documentation, not as machine-checked evidence, or give
the fields real propositional content.

---

### F-07

**ID:** F-07
**Severity:** LOW
**Category:** TCB completeness

**Description:**
`evidence/TCBInventory.md` correctly lists the execution-critical
components (Coq kernel, extraction mechanism, OCaml compiler, RTS, host
toolchain, OS, CPU) and correctly excludes node/network/wallet/RPC/
database/pool/crypto. Omitted build- and evidence-path dependencies:

- `python3` — used by `mutation/lib.sh`, `kernel_binary/run_vectors.sh`
  (decides pass/fail) and `release/check_release.sh`
- `make` / `coq_makefile`
- `bash` and coreutils (`grep`, `sha256sum`, `mktemp`)
- `git` (provenance recording)
- filesystem and binary loader (asked for explicitly; currently only
  implied by "Operating system")

`run_vectors.sh` invoking `python3` to read the expected result means a
Python fault could silently turn a failing vector into a pass.

**Impact:**
The evidence-producing path is slightly wider than documented.

**Required action:**
Add the above to the "Trusted but not formally verified" table.

---

### F-08

**ID:** F-08
**Severity:** INFO
**Category:** Evidence hygiene

**Description:**
`release/check_release.sh` rewrites `release/VersionManifest.md` on
every invocation, and it is invoked by `run_kernel_critical.sh`. The
gate that validates the evidence also mutates it.

**Impact:**
The manifest is perpetually dirty and its timestamp reflects the last
CI run rather than the audited state. Combined with F-02, provenance is
self-referential.

**Required action:**
Generate the manifest to a separate output artifact, or make the
refresh explicit and opt-in.

---

### F-09

**ID:** F-09
**Severity:** INFO
**Category:** Mutation methodology

**Description:**
Three of the ten mutations edit a **theorem statement** rather than a
model rule: M2 (`<=` → `<` in the conclusion of
`res_issuance_soundness`), M3 (RHS of `oru_conservation_extended`), M6
(`<=` → `=` in `compact_decode_lower_bound`). These demonstrate that a
strictly different statement is not provable — a useful but weaker
property than detecting a changed consensus rule. M1, M4, M5, M7, M8,
M9, M10 are genuine rule mutations.

**Impact:**
Mutation strength is somewhat lower than a 10/10 headline implies.

**Required action:**
Label statement mutations separately from rule mutations in
`mutation/expected_failures.md`.

---

### F-10

**ID:** F-10
**Severity:** MEDIUM
**Category:** Mutation coverage gaps

**Description:**
Against the mutation classes named in the audit brief, the following
are absent from M1–M10:

| Domain | Missing class |
|---|---|
| Supply | boundary height (`h = 0`, `h < 0`, epoch 250/251 edge); overflow |
| ORU | burn omission on the `DomORU` branch specifically; hidden-mint path |
| RES | schedule jump between adjacent epochs; cumulative-cap bypass |
| PoW | target edge case (`target <= 0`); compact **encode** error |
| ASERT | shift rounding (`num_shifts` direction); arithmetic overflow |

The overflow class is the one that would have caught F-01.

**Impact:**
No mutant survived, so the suite is sound as far as it goes, but the
`MUTATION_TEST_COMPLETE=TRUE` flag reflects 10 designed cases rather
than systematic coverage.

**Required action:**
Extend the suite with the classes above, in particular an overflow
mutation exercised through the extracted binary rather than only
through `coqc`.

---

## Cross-cutting observation

`run_kernel_critical.sh` reports every flag as `PASS` on the current
tree. It did **not** detect F-01, F-02 or F-03. The gate confirms
internal consistency (proofs close, mutants die, vectors match their
own declared expectations) but contains no check that would fail if the
extracted binary disagreed with the Coq model, or if the recorded
provenance were empty.

---

## What survived the attack

Stated plainly, because an adversarial report should also record what
could not be broken:

- **No hidden assumptions.** Independent `grep` found zero `Axiom`,
  `Admitted`, `admit`, `Parameter`, `refinement_proved`,
  `equivalence_proved` across all 26 `.v` files. The only `Variable`
  occurrences (`H : list byte -> list byte` in `AbstractHash.v`,
  `PowModel.v`, `PowCorrectness.v`) are section variables that
  generalise on section close — they are not axioms, which
  `Print Assumptions` confirms.
- **All nine advertised theorems are genuinely Closed** under the
  global context, re-verified independently by the auditor.
- **No mutant survived.** 10 `EXPECTED_FAILURE`, 0 `mutation_passed`,
  reproduced.
- **The forbidden-claim discipline holds.** No occurrence of full-node,
  full-Bitcoin, network, wallet, crypto-security, compiler-correctness,
  or C++-equivalence claims in `release/PublicClaims.md`,
  `release/VerificationScope.md`, or `evidence/ClaimsBoundary.md`.
  `cpp_equivalence`, `full_chain_equivalence`, `node_correctness`,
  `network_correctness`, `wallet_correctness`, `crypto_security` are
  all machine-checked `false`.
- **The substantive kernel content is real.** `utxo_soundness`,
  `oru_conservation_extended`, `no_hidden_inflation`,
  `recognizer_pairwise_disjoint`, `pow_valid_rejects_above_target`,
  `negative_time_error_preserved`, `compact_decode_lower_bound`,
  `res_schedule_finite` all have genuine content, confirmed by the fact
  that mutating the underlying rules breaks compilation.
- **The headline public claim is defensible as worded**: "formally
  verified critical consensus invariants (consensus kernel only)"
  describes the Coq layer, and the Coq layer holds.

---

## Reproduction of the auditor's counterexamples

```
# (a) UTXO overflow: Coq says false, binary says VALID
#     tx with 10 outputs of value 1000000000000000000 (= MAX_MONEY)
coqtop -R . RigorKernel -batch -l probe.v      # check_utxo t = false
./kernel_binary/rigor-kernel-checker overflow_utxo.json   # VALID

# (b) ASERT sign inversion
#     te = -100000000000000, tau = 43200
#     Coq  exp_q_raw = -151703703703703
#     OCaml exp_q_raw = +61800278630897
./kernel_binary/rigor-kernel-checker asert_overflow.json  # VALID
```

Neither input was added to the repository; both are reconstructible
from the values above.

---

## Final Verdict

**AUDIT_SIMULATION_PASS_WITH_OBSERVATIONS**

No CRITICAL finding. The formal core is sound: assumptions are closed,
no axioms or admits exist, mutation resistance is demonstrated, and the
public claim as worded — *formally verified critical consensus
invariants (consensus kernel only)* — is supported by the Coq layer and
survives adversarial attack.

Two HIGH findings must be resolved before the package is handed to an
external auditor as-is:

- **F-01** — the refinement boundary is drawn one layer too far out.
  Extraction to 63-bit `int` is not semantics-preserving, demonstrated
  by two executed counterexamples. Two documents currently claim proof
  coverage through extraction; that specific wording is an overclaim
  and must be corrected. The boundary belongs at Layer 2.
- **F-02** — the release provenance record points at a commit
  containing none of the audited files, so the package is not
  independently reproducible from its own manifest.

Neither finding contradicts a proved theorem. Both concern the
*boundary* between what is proved and what is asserted — which is
precisely the territory this release package exists to police.

| Finding | Severity |
|---|---|
| F-01 extraction not semantics-preserving; boundary overclaim | HIGH |
| F-02 release provenance does not contain audited sources | HIGH |
| F-03 golden vectors 100% positive; suite not falsifiable | MEDIUM |
| F-04 two headline theorems are content-free as stated | MEDIUM |
| F-05 PoW/ASERT trace edges asserted, not proved | MEDIUM |
| F-10 mutation coverage gaps (incl. overflow class) | MEDIUM |
| F-06 boundary marker proves `True` | LOW |
| F-07 TCB inventory omits build/evidence tooling | LOW |
| F-08 release gate rewrites its own evidence | INFO |
| F-09 3/10 mutations are statement, not rule, mutations | INFO |

**CRITICAL findings: 0.**
