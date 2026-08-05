# SECURITY-001-A — Final Report

**Task:** `TASK-SECURITY-001-PRE-MAINNET-SECURITY-REMEDIATION-MASTER`, subtask A
**Control plan:** `TASK-SECURITY-001-CONSENSUS-REMEDIATION-CONTROL`, Phases 0–6
**Date:** 2026-08-02
**Baseline tag:** `security-001-a-remediation-start` (local, not pushed)

**`SECURITY_001_A_CLOSURE_READY = false`** — one item remains: the user-executed full C++
build and functional suite. See §8.

> **Governance decisions received 2026-08-02:**
> - **A-5 deferred.** Removed from SECURITY-001-A scope; tracked as
>   [`TASK-SECURITY-001-A5-ECONOMIC-GOVERNANCE`](./TASK-SECURITY-001-A5-ECONOMIC-GOVERNANCE.md).
>   No A-5 code written. **SECURITY-001-A now closes on A-1…A-4 and A-6 only.**
> - **Re-freeze authorised and executed.** Freeze checker now 12/12 PASS. Auditable chain in
>   [`PROTOCOL_FREEZE_MIGRATION_CHAIN.md`](./PROTOCOL_FREEZE_MIGRATION_CHAIN.md).

---

## 1. Summary

Subtask A covered the convergence draw and RES issuance consensus path. Seven findings were
raised (A-1 … A-7). Six are resolved in code; one (A-5) is resolved to a design document
awaiting a governance decision that is not the security lead's to make.

The dominant finding is **A-6**: `RES_PROBABILITY_TARGET` was specified at `2^240` under an
implicit weight of 1, but the implemented weight is denominated in satoshis. At the minimum
admissible weight the right-hand side of the draw inequality exceeded the digest range by a
factor of ≈1526, so `σ < T · f` was true for every candidate. The eligibility filter — the
mechanism that makes RES issuance probabilistic rather than discretionary — did nothing.

Combined with `MIN_TXS_FOR_RES = 1`, this meant a block producer including exactly one
convergence transaction won the draw with certainty. RES issuance was not lottery-governed;
it was miner-governed.

| Finding | Subject | Status |
|---------|---------|--------|
| A-1 … A-4 | (see `TASK_SECURITY_001_A_REPORT.md`) | RESOLVED — code + tests |
| A-5 | RES reward not bound to convergence owner | **DEFERRED — out of scope**, moved to `TASK-SECURITY-001-A5-ECONOMIC-GOVERNANCE` |
| A-6 | `RES_PROBABILITY_TARGET` unit error → vacuous eligibility filter | RESOLVED — full dependency chain synchronized |
| A-7 | (see `TASK_SECURITY_001_A_REPORT.md`) | ASSESSED — no change required |

---

## 2. The A-6 correction

| | Value |
|---|---|
| Before | `2^240` = `0001000000000000000000000000000000000000000000000000000000000000` |
| After | `floor(2^240 / MIN_CONVERGENCE_AMOUNT)` = `00000000002af31dc4611873bf3f70834acdae9f0f4f534f5d60585a5f1c1a3c` |
| Decimal | `17668470647783843295832975007429185158274838968756189581216062012` |
| `T · f_min / 2^256` before | `1525.87890625` — vacuous |
| `T · f_min / 2^256` after | `1.52587890625 × 10⁻⁵` — selective |

**Economic intent is unchanged.** RFC-014 §2.1 always intended `p = 2^-16` per input at 1 ORU,
and that is exactly what the corrected constant delivers: `p(n ORU) = min(1, n / 65536)`.
Every figure in the RFC-014 §5.4 sensitivity table is preserved. This was a correction of a
unit error, not a monetary policy decision — an important distinction, because the latter
would require governance and the former does not.

One property is new and explicitly accepted: at `n ≥ 65536` ORU the probability saturates at
1. This is inherent to the linear `p ∝ f` form fixed by RFC-014 §2.2. Eligibility is not
victory — `argmin H/f` still decides the winner among eligible candidates — so saturation
does not confer control.

### Three-layer agreement, machine-checked

| Layer | Definition | Location |
|-------|-----------|----------|
| C++ | `RES_PROBABILITY_TARGET_CONST` | `core/src/consensus/params.h:52` |
| Python | `(1 << 240) // MIN_CONVERGENCE_AMOUNT` | `formal_verification/spec/rigor_consensus_spec.py:57` |
| Coq | `Z.shiftl 1 240 / MIN_CONVERGENCE_AMOUNT` | `formal_verification/coq/Selection.v:26` |

`MIN_CONVERGENCE_AMOUNT = 10⁸` in all three (`rigor_consensus_lock.h:97`, unmodified SSOT).
Coq value equality with the C++ literal is proved by `vm_compute` + `Qed`, not asserted.

---

## 3. The invariant that was missing

The root cause is not that someone typed the wrong number. It is that **no artifact anywhere
in the stack checked the constant against the rule it parameterises.** Every gate compared
the constant to another declaration of the same constant, and they all agreed, because they
all derived from the same defective RFC clause.

The invariant now exists in four executable forms:

```
0 < RES_PROBABILITY_TARGET · MIN_CONVERGENCE_AMOUNT < 2^256
```

| Layer | Mechanism | Behaviour on violation |
|-------|-----------|------------------------|
| C++ | `res_proof_math_tests::security001a_eligibility_filter_is_not_vacuous` | test failure |
| Python | import-time `assert` in `rigor_consensus_spec.py` | module refuses to load |
| Coq | `res_probability_target_not_vacuous` (`Qed`, axiom-free) | build failure |
| Ops | `freeze_integrity_checker.res_probability_target_not_vacuous` | gate FAIL |

The freeze checker additionally now **reads `params.h` directly**
(`_res_probability_target_from_source`) and compares it against its own declared value, so a
divergence between the checker's snapshot and the source can no longer pass silently. That
was the specific blind spot that let gate G10 certify a defective constant.

The Coq layer also retains the old value as a named definition with two lemmas proving it
broken — `pre_a6_target_was_vacuous` and `pre_a6_every_digest_was_eligible`. Restoring
`2^240` now requires deleting a machine-checked proof that it is wrong.

---

## 4. Dependency chain synchronized (Phase 2)

| Artifact | Action |
|----------|--------|
| `core/src/consensus/params.h` | constant + non-vacuity note (done in earlier phase) |
| `core/src/kernel/chainparams.cpp` | six assignments via named constant; comments cite v1.1 |
| `core/src/test/*` (4 files) | literals replaced with the named constant; test weights re-based |
| `docs/rfcs/RFC-014-v1.1.md` | **new** — erratum with derivation, probability law, invariant |
| `docs/rfcs/RFC-014-frozen-convergence-target.md` | §2.1 marked superseded; banner added |
| `docs/rfcs/RFC-007-formal-verification-coq.md` | Coq listing corrected |
| `formal_verification/spec/rigor_consensus_spec.py` | constant + import-time assertion |
| `formal_verification/rigor_golden_vectors.json` | regenerated — see below |
| `formal_verification/coq/Selection.v` | constant + 4 new lemmas |
| `sim/freeze/freeze_integrity_checker.py` | value + source cross-check + non-vacuity check |
| `CONV_T_GREP_GATE_EVIDENCE.md` | **G10 withdrawn and rewritten** |
| `H_CONV_T_DIFF_REVIEW_001.md` | **REQUIRED-5 withdrawn and rewritten** |
| `docs/freeze/H_RES_003_ARCHITECTURE_VERDICT.md` | origin of the unit error annotated |
| `RELEASE_BASELINE_MANIFEST.md`, `RELEASE_MANIFEST_VALIDATION.md`, `RFC_FREEZE_INDEX.md`, `FORMAL_DELTA.md`, `CONV_T_ACTIVATION_DIFF_REPORT.md` | semantic anchors corrected |
| `explorer-indexer-mvp/src/importer.ts`, `docs/rigor-qt/FUTURE_GUI_RPC_SPEC.md`, `day0-smoke.sh` | frozen values corrected |
| 4 × `docs/formal/**` alignment audits | rows corrected |

### Golden vectors

Regenerated deterministically. **No expected output changed** — every `convergence_root`,
`sigma`, and `winner` is byte-identical. Only candidate weights (raised past the saturation
point) and annotations moved. This bounds A-6 precisely: it was an *eligibility* defect, not
a *selection* defect. Full detail and a tool bug found during regeneration:
``docs/formal/GOLDEN_VECTOR_REGENERATION_REPORT.md``.

### Governance evidence

Gate **G10** and **REQUIRED-5** were the two gates that certified the defective constant.
Both are now struck through with an explicit account of *why they passed*: G10 verified
wiring but never the value; REQUIRED-5's saturation check tested `T == 0` (always-false) but
never the upper bound (always-true). Both records retain their original text — this is a
correction of the audit trail, not a rewriting of it.

---

## 5. Coq synchronization (Phase 3)

`REBUILD_EXIT=0`. 16 dependent modules recompiled; 197 `.vo`; zero `Admitted.`; zero `admit.`.

No proof required repair, because the draw proofs are symbolic in `T` — which is also why
the Coq layer never objected to a bad `T`. Four new axiom-free lemmas supply the missing
obligation. `kernel_draw_sound`, `kernel_draw_deterministic`, `execute_winner_eligible`, and
`execute_implies_verify` were re-verified as "Closed under the global context" after the
change.

Detail and residual gaps: ``docs/formal/SECURITY-001-A6-COQ-REPORT.md``.

---

## 6. Test matrix (Phase 4)

| Layer | Command | Result |
|-------|---------|--------|
| Coq full rebuild | `cd formal_verification/coq && make -j$(nproc)` | ✅ `REBUILD_EXIT=0` |
| Coq assumption audit | `Print Assumptions` on 4 closure theorems + 4 new lemmas | ✅ all "Closed under the global context" |
| Golden vectors | `python3 formal_verification/test_runner.py` | ✅ 16/16 |
| Differential fuzz | `python3 formal_verification/fuzz/differential_fuzzer.py` | ✅ 32 cases, 0 mismatches |
| Freeze integrity | `python3 sim/freeze/freeze_integrity_checker.py` | ✅ 12/12 PASS after authorised re-freeze; see §8.2 |
| Freeze refresh guard | second `--write-snapshot` with no amendment | ✅ refused, exit 1 |
| C++ build | `make -j$(nproc)` | ✅ exit 0, zero `error:` |
| C++ unit tests | `./src/test/test_bitcoin` | ✅ **745/745, `*** No errors detected`** |
| Functional | `make check` | ✅ **TOTAL 3 / PASS 3 / FAIL 0 / ERROR 0** |

User-executed 2026-08-02 20:49–20:52, `cd ./core`, chain exit code 0. Logs:
`/tmp/a6_build.log`, `/tmp/a6_unit.log`, `/tmp/a6_check.log`.

```
Running 745 test cases...
*** No errors detected

Testsuite summary for Rigor Core 25.0.0
# TOTAL: 3   # PASS: 3   # SKIP: 0   # XFAIL: 0   # FAIL: 0   # XPASS: 0   # ERROR: 0
```

All four A-6-affected suites are confirmed present in the run: `res_proof_math_tests`,
`selection_golden_tests`, `convergence_consensus_tests`, `canonical_layer_tests`.

Two honest caveats on this row:

1. **The build was incremental, not from scratch.** `make` reported "Nothing to be done" —
   the objects were already current. This confirms sources and binaries are consistent and
   that the test binary was built from the corrected sources, but it is not a clean-room
   rebuild. If a from-scratch guarantee is wanted before mainnet, that is a separate
   `make clean && make` run.
2. **The differential fuzzer's C++ leg is not wired** (`RIGOR_CPP_HARNESS` unset). It
   compares Python ↔ Coq only, over ConvergenceRoot and SigmaCR01 only, and reported 0/32
   mismatches both before and after A-6 — it is not evidence about this defect in either
   direction. C++ evidence rests on the unit suite, the freeze checker's direct read of
   `params.h`, and `res_proof_math_tests::security001a_eligibility_filter_is_not_vacuous`
   (verified present at `core/src/test/res_proof_math_tests.cpp:576`).

The chainparams genesis asserts execute during unit-test construction, so this run is also
direct confirmation that all five hard-coded genesis hashes still hold under the corrected
constant — the empirical basis for §7's retraction of the "genesis regeneration required"
claim.

---

## 7. Chain data (Phase 5)

All pre-A-6 chain databases are invalid and must be archived and discarded. Any block
containing a RES mint was admitted under a different eligibility rule and will be rejected.

**Correction issued during this phase:** earlier A-6 documentation stated that genesis
regeneration was required. **That was wrong.** A genesis hash depends only on the block
header and coinbase; `RES_PROBABILITY_TARGET` never enters `CreateGenesisBlock`. All five
hard-coded genesis asserts still pass. RFC-014 §6.1's GENESIS-ONLY activation gate is
satisfied by restarting from the *existing* genesis, since the corrected constant is in force
from height 0. Three documents and one ops script were corrected accordingly.

This materially reduces migration cost. Policy, archival procedure, and downstream reset
list: ``docs/migration/CHAIN_REGENERATION_POLICY.md``.

---

## 8. Closure status

### 8.1 A-5 — DEFERRED, out of scope (resolved)

A-5 turns on a question no RFC answers: **is the convergence owner intended to receive the
RES?** That is a protocol-economics decision, not a security determination, and it decides
who earns the protocol's issuance.

Governance ruling: **defer**. A-5 is removed from SECURITY-001-A and tracked as
[`TASK-SECURITY-001-A5-ECONOMIC-GOVERNANCE`](./TASK-SECURITY-001-A5-ECONOMIC-GOVERNANCE.md).
No A-5 code was written and none may be until that task's deliverables are signed off. The
three-option analysis across eight impact dimensions remains in
[`SECURITY-001-A5-DESIGN.md`](./SECURITY-001-A5-DESIGN.md).

Scheduling note carried forward: if A-5 is later answered as "yes", it is chain-invalidating
and should ship in the same reset as A-6, so `CHAIN_REGENERATION_POLICY.md` should not be
executed until A-5 is decided.

### 8.2 Freeze baseline — RE-FROZEN (resolved)

Authorised and executed. The checker now reports **12/12 PASS**.

| | |
|---|---|
| Superseded snapshot | `8ed448a28ffea3637c2e400124ddd3e200ce44550b9ec59c50c6ceed7b6b70cd` |
| New snapshot | `c6a8bfb5f3e62c25265073cfb39acbdf1e6eb7553604f82e5692e0a8a1b94e34` |
| Archived predecessor | `.runtime/freeze/archive/protocol_freeze_snapshot.pre-SECURITY-001-A6.json` |
| Full chain | [`PROTOCOL_FREEZE_MIGRATION_CHAIN.md`](./PROTOCOL_FREEZE_MIGRATION_CHAIN.md) |

Two amendment entries were recorded, not one. The re-freeze absorbed a change from
`TASK-RELEASE-006-PRODUCT-CLOSURE-001` (packaging manifest, no consensus impact) that was
already in the working tree. Recording it separately keeps A-6 from being credited with a
change it did not make.

**Silent refresh is now structurally impossible.** `write_snapshot()` refuses to overwrite a
snapshot whose hash is not named by an entry in `FREEZE_AMENDMENTS`, so re-freezing requires
writing down what changed and why *before* the write succeeds. This was verified by
attempting a second unrecorded re-freeze immediately afterwards; the tool refused and exited
non-zero. A new `freeze_amendment_chain_intact` check fails if a later snapshot loses that
history.

This closes the specific weakness that let A-6 survive: the cheapest way to green a failing
freeze gate used to be re-running the tool.

### 8.3 Full build and functional suite — PASSED (resolved)

Executed by the user 2026-08-02. Build exit 0 with no compile errors; unit suite 745/745
`*** No errors detected`; `make check` 3/3 PASS, 0 FAIL, 0 ERROR. Evidence and caveats in §6.

### 8.4 Remaining gate before closure

All Phase 0–6 deliverables are complete and all test gates are green. The one item
outstanding is **not** technical:

> **A scoped commit has not been made.** Per the task's stop condition, no commit or push has
> been performed. The working tree is shared with SECURITY-001-F, the Rust pool stack, wallet
> work, and `TASK-RELEASE-006-PRODUCT-CLOSURE-001`, so the A-6 change set is not yet isolated
> in history. Attribution and a recommended commit sequence are in
> [`SECURITY-001-A-DIFF-SCOPE.md`](./SECURITY-001-A-DIFF-SCOPE.md).

`SECURITY_001_A_CLOSURE_READY` flips to `true` on approval to commit the scoped change set.

---

## 9. Residual items referred onward

Not defects in A-6; recorded so they are not lost.

1. **No formal link between the constant and an issuance rate.** The Coq supply modules prove
   *conservation*, not *rate*. A-6 changed realised issuance by ~10⁸ and the entire proof
   corpus was silent. RFC-014 §5.4's economics are unformalised. → SECURITY-001-F
2. **Differential fuzzer scope.** Does not cover the inequality; C++ leg unwired. → SECURITY-001-F
3. **Coq uses unbounded `Z` where C++ uses `uint512` cross-multiplication.** Pre-existing
   (`UInt512OverflowRefinement.v`), but A-6 *was* an overflow-class defect, which makes this
   gap more pointed than it previously appeared. → SECURITY-001-F
4. **`Execute` ignores its `T` parameter**, using the global constant. Correct for a consensus
   model, but it means the harness and `Execute` reach the filter by different paths. → SECURITY-001-F
5. **`slowtest` genesis has no hard-coded hash assert**, unlike the other five networks.
   Unrelated to A-6. → ops
6. **Historical runtime evidence** (`LIVE_RES_VISIBILITY_REPORT.md`,
   `RES_REORG_VALIDATION.md`, `WINNER_SELECTION_TRACE.md`, `PHASE-05.json`) records the old
   constant. These are accurate records of what the defective binary did and were
   deliberately left unedited; they belong to the chain data Phase 5 declares invalid.

---

## 10. Constitutional compliance

| Constraint | Status |
|-----------|--------|
| `CTxOut` storage unmodified | ✅ no change |
| No asset field outside script | ✅ no change |
| Asset type inferred from `scriptPubKey` | ✅ unchanged |
| 100% Bitcoin storage-format compatibility | ✅ unchanged |
| Block header format | ✅ unmodified |
| Transaction serialization | ✅ unmodified |
| PoW | ✅ unmodified |
| PROOF-034 SSOT (`rigor_consensus_lock.h`) | ✅ unmodified |
| `gate_engine` | ✅ unmodified |
| Score logic (`argmin H/f`, txid tie-break) | ✅ unmodified |
| Witness layout / `ConvergeProof` | ✅ unmodified (A-5 deliberately not implemented) |
| No admin key / upgrade backdoor introduced | ✅ |
| No public release, no deployment | ✅ |

---

## 11. Output

```
SECURITY_001_A_CLOSURE_READY = false
```

Scope after the 2026-08-02 governance decisions: **A-1…A-4 and A-6 only.** A-5 is no longer
part of this subtask.

**All technical work is complete and every test gate is green.** The flag remains `false`
solely because the change set has not been committed — see §8.4.

Resolved since the previous revision:

- ~~Governance decision on A-5 intent~~ → deferred to `TASK-SECURITY-001-A5-ECONOMIC-GOVERNANCE`
- ~~Authorization to re-freeze the protocol snapshot~~ → authorised, executed, 12/12 PASS
- ~~User-executed `make` / `test_bitcoin` / `make check`~~ → 745/745 and 3/3, exit 0

Remaining:

1. Approval to make the scoped commit
   ([`SECURITY-001-A-DIFF-SCOPE.md`](./SECURITY-001-A-DIFF-SCOPE.md))

No commit has been made. No push. SECURITY-001-B not started.
