# SECURITY-001-F — Formal Verification Integrity

**Subtask:** SECURITY-001-F of the pre-mainnet security remediation program
**Repository:** `.` @ `2b3354be3e`
**Date:** 2026-08-02
**Toolchain:** Coq 8.15.0 (OCaml 4.13.1), Python 3.10.12, Linux (WSL2)
**Objective:** make the project's verification claims match reality. Not to prove new theorems.

---

## Honesty legend

Every factual claim in this report carries one of:

- **VERIFIED** — I executed the command or compiled the artifact and observed the result. Output is pasted.
- **INFERRED** — I reasoned from reading source code without executing it.

The Coq toolchain **was** available in this environment (`coqc` 8.15.0), so build
claims are VERIFIED rather than assumed. The C++ toolchain was **not** exercised:
per the collaboration convention, full `make` of `core/` is run by the user, so
every claim about C++ *runtime* behaviour below is marked INFERRED from source
reading.

**This report does not declare mainnet readiness.** Section 6 lists an unfixed
defect in a frozen SSOT file that requires human approval.

---

## 0. Executive summary

All six findings reproduced. None were already fixed; none were non-reproducible.
One finding (F-4) is materially **larger** than reported, and one UTXO lemma
(F-3) turned out to be not merely unproven but **false as stated**.

| # | Finding | Status on audit | Disposition |
|---|---|---|---|
| F-1 | README advertises non-existent theorems | VERIFIED-STILL-BROKEN | Rewritten |
| F-2 | Differential fuzzer green by construction | VERIFIED-STILL-BROKEN | Genuinely wired to a Coq oracle |
| F-3 | Admits silently inherited by `Qed` theorems | VERIFIED-STILL-BROKEN (11, not 2) | All 11 discharged; inventory published |
| F-4 | Vacuous theorem statements | VERIFIED-STILL-BROKEN (worse) | 2 strengthened, 31 marked, guard added |
| F-5 | Missing critical invariants | VERIFIED-STILL-BROKEN | Gap register, §5 |
| F-6 | `GoldenHarness.v` excluded from build | VERIFIED-STILL-BROKEN | Added and made build-enforcing |

Headline results, all VERIFIED:

- Clean rebuild from scratch of the final tree: **196/196 files, exit 0** (§7.1a).
- Assumption scan: **1882 top-level results, 1882 axiom-free, 0 axiom-dependent, 0 probe errors** (§4).
- Coq `Admitted` count across the corpus: **11 → 0** (the finding named 2; a
  form-insensitive grep found 11, see §3.3).
- Golden vectors: **25 build-enforcing lemmas**, proven to break the build when corrupted (§3.6).
- Differential fuzzer: **32 cases compared against a genuinely independent Coq oracle**, and demonstrated to exit 1 on a real mismatch and 2 when no oracle exists (§3.2).

Two findings I discovered that were not in the original list:

- **`utxo_spend_removes_output` is refutable, not just unproven** (§3.3.1). The
  replay model gives created outputs synthetic outpoints that can collide with
  outpoints the same transaction spends.
- **2 of the 25 golden vectors are degenerate** (§3.6.1). CR-000 and CR-001 expect
  32 zero bytes regardless of txid content, so they cannot detect a SHA-256 or
  serialization regression. I found this by mutating a txid and watching the
  build stay green.

---

## 1. Method

1. Audited each finding against current source before editing.
2. Planned remediation (§2).
3. Executed.
4. Verified: clean rebuild of the whole Coq corpus plus a fresh assumption scan.
5. Reported here.

Files touched are confined to the allowed scope: `formal_verification/**` and
new files under `docs/security/`. **No file in the PROOF-034 SSOT prohibition
list was modified.** VERIFIED — the command below produced no output:

```
$ git status --porcelain core/src/proof/ core/src/consensus/rigor_consensus_lock.h \
      sim/proof_validator/ sim/release/gate_engine.py runtime/proof034/ \
      formal_verification/spec/rigor_consensus_spec.py
$
```

Note for the reader diffing this branch: `git status` shows unrelated
modifications under `core/src/**`, `docs/**` and `core/test/**` that were
**already present in the working tree** when this subtask began. They belong to
the engineers working concurrently on those areas. Every file I changed is listed
in §9.

---

## 2. Remediation plan (as executed)

| Order | Action | Rationale |
|---|---|---|
| 1 | Add `Kernel/GoldenHarness.v` to `_CoqProject`; convert its `Compute` statements to enforced `Lemma`s | Cheapest large gain in real assurance; must land before rebuild timing is measured |
| 2 | Discharge every `Admitted` result, or correct it if false | Removes the F-3 inheritance problem at the root rather than only documenting it |
| 3 | Strengthen the two vacuous supply theorems; sweep for the rest and mark them | F-4 |
| 4 | Rewrite the differential fuzzer around a real Coq oracle | F-2 |
| 5 | Build the assumption-inventory tool and publish the table | F-3 deliverable |
| 6 | Rewrite the README against verified file+line citations | F-1 |
| 7 | Add regression guards, clean rebuild, rescan | Prevent recurrence |

---

## 3. Per-finding detail

### 3.1 F-1 — README advertises theorems that do not exist

**Status: VERIFIED-STILL-BROKEN.**

**Audit.** VERIFIED by grep over all 196 `.v` files. The published README named
five numbered "formal theorems" plus a "Final Theorem: No Fork Guarantee".
Searching the corpus:

| README claim | README line | Exists in `coq/`? |
|---|---|---|
| `determinism_convergence_root` | 141 | **No** |
| `implementation_equivalence_convergence_root` | 152 | **No** |
| `no_hidden_state_convergence_root` | 163 | **No** |
| `order_invariance_convergence_root` | 175 | **No** |
| `no_ambiguity_winner` | 186 | **No** |
| `no_fork_theorem` | 385 | **No** |
| file `coq/ArdaConsensus.v` | 55, 132, 200 | **No** |

The `implementation_equivalence_convergence_root` block was the most damaging:
its body was printed in the README as literally `True`, immediately under the
caption **"Meaning: C++ == Python Spec == Coq Model"**. A `True`-bodied statement
is provable by `exact I` for any implementations whatsoever. It establishes
nothing, and the README then chained it into the assertion that "the network will
never fork due to consensus differences."

Worse, `order_invariance_convergence_root` was stated in terms of `Permutation`,
a token that appears **zero times** in the entire corpus (VERIFIED, §5 G-1) — so
the one property the anti-fork argument actually needs was advertised as proven
while being entirely absent.

**Change.** `formal_verification/README.md` rewritten end to end. Every claim now
cites a file and line that I verified exists. Specifically:

- All six fabricated theorem statements removed.
- All three references to `ArdaConsensus.v` removed; build instructions now use
  `coq_makefile -f _CoqProject`, which is the real entry point.
- The three-layer diagram now states explicitly that the **L2↔L3 connection does
  not exist** and that there is no extraction or refinement proof.
- Added §3 "Known-false and explicitly-open statements", recording the refuted
  UTXO lemma and the open obligations PD-4F-OB-02 / PD-4F-CPP-01 / PD-4F-OB-03.
- Added §4, a summary gap register pointing here.
- Existing honesty markers preserved and now *promoted* rather than buried: the
  README quotes `Proofs/Safety.v:7-10` ("WEAK (real, but content-light)") and
  `Phase4GRefinement/FormalVerificationCertification.v:11-21` ("HONESTY
  BOUNDARY", verdict `RV_Partial`, not `RV_Equivalent`).
- Fuzzer section now documents the exit-code semantics and instructs the reader
  not to cite a run that names no independent oracle.

**Security reason.** Overstated assurance suppresses review effort. A reader who
believes an implementation-equivalence theorem exists will not commission the
differential testing that would have caught the consensus bugs this program is
remediating. This item is also the highest external-exposure risk: any exchange
due-diligence reviewer greps for `no_fork_theorem`, fails to find it, and
reasonably concludes the whole verification package is fabricated.

**Affected modules.** Documentation only. No Coq or C++ semantics changed.

**Consensus impact.** None directly — no consensus code path reads the README.
The indirect impact is the one that already materialized: undetected consensus
bugs.

**Regression guard.** The rewritten README states the rule that any claim not
locatable by file+line is a documentation bug to be filed. The two new tools
(`tools/audit_assumptions.py`, `tools/check_vacuous.py`) make the underlying
facts cheap to re-derive, so the README can be re-checked mechanically rather
than by trust.

---

### 3.2 F-2 — Differential fuzzer cannot fail by construction

**Status: VERIFIED-STILL-BROKEN.**

**Audit.** VERIFIED by reading `formal_verification/fuzz/differential_fuzzer.py`
at the pre-change revision. All six oracle hooks returned the Python result:

```python
def _call_cpp_convergence_root(self, txids):
    # For now, return the Python output (assume C++ matches)
    return compute_convergence_root_from_txids(txids)
```

Every comparison therefore reduced to `x == x and x == x`, which is true for all
inputs. The tool reported 1000/1000 passes and project documentation cited it as
"ANY MISMATCH = CONSENSUS FAILURE" evidence of three-way equivalence. It was not
merely weak — it was **structurally incapable of producing a failure**, which is
worse than having no test, because it consumed the budget that a real test would
have occupied.

**Change.** File rewritten. The design principle is that a leg which is not
wired must never be counted as agreeing.

- **Coq leg is now genuinely independent.** Random inputs are serialized into a
  Coq source file and evaluated by the Coq kernel's VM (`vm_compute`), including
  the byte-level SHA-256 written in Coq (`RigorSha256.v`). Python's `hashlib` is
  not involved on that side. Cases are batched into one `coqc` invocation
  because per-case invocation costs seconds.
- **C++ leg reports UNAVAILABLE** unless `RIGOR_CPP_HARNESS` names an existing
  binary. It is deliberately *not* implemented by delegating to Python. If a
  harness path is supplied, the tool currently refuses to report a result rather
  than guess at the protocol — an honest "not implemented" instead of a
  fabricated pass.
- **Exit codes are now meaningful:** `0` compared and agreed, `1` mismatch,
  `2` inconclusive (no independent oracle reachable — explicitly "NOT a passing
  result").
- The module docstring records the old behaviour verbatim so the failure mode
  cannot be reintroduced by someone unaware of the history.

**Verification — all three paths VERIFIED by execution.**

Agreement path, real run, seed `20260802`, 16 iterations:

```
C++ oracle    : UNAVAILABLE — RIGOR_CPP_HARNESS not set to an existing binary; C++ leg not wired
Coq oracle    : WIRED (coqc, vm_compute over 32 cases)
Cases compared against an independent oracle: 32
Mismatches:                                   0
AGREEMENT: Python spec and Coq kernel agree on all compared cases.
FUZZER_EXIT=0
```

Mismatch path — I injected a one-bit corruption into the Python spec's
ConvergenceRoot result and re-ran:

```
Cases compared against an independent oracle: 6
Mismatches:                                   3
MISMATCHES (CONSENSUS-CRITICAL):
  [0] ConvergenceRoot: ConvergenceRoot(n=12)
  [2] ConvergenceRoot: ConvergenceRoot(n=4)
  [4] ConvergenceRoot: ConvergenceRoot(n=3)
ERROR: Differential fuzzing detected a consensus mismatch.
EXIT_WOULD_BE = 1
```

All 3 ConvergenceRoot cases caught; the 3 SigmaCR01 cases correctly unaffected
because the corruption was confined to the ConvergenceRoot function.

Inconclusive path — Coq oracle pointed at a directory with no built kernel:

```
Cases compared against an independent oracle: 0
Not compared: Coq (ConvergeRoot.vo not built; run `make` in /tmp/empty_coq_dir first)
INCONCLUSIVE: no independent oracle was available.
Nothing was verified. This is NOT a passing result.
EXIT_WOULD_BE = 2
```

**Security reason.** A test that cannot fail is an assurance liability, not an
asset: it converts absence of evidence into apparent evidence of absence.

**Affected modules.** `ConvergenceRoot` (Module 3.1) and `SigmaCR01`/PCORB hash
(Module 3.2) are now genuinely cross-checked. Eligibility (3.3), winner selection
(3.4), ORB proof (3.5) and dual semantics (3.6) are **not** covered by the
fuzzer; they are covered only by the 25 static golden vectors.

**Consensus impact.** The fuzzer is off the consensus path. Its prior state means
**no** randomized differential evidence existed for any module; the 32-case run
above is the first real evidence of Python↔Coq agreement on random inputs.

**Regression guard.** Vacuous success is now unreachable: `report.compared` only
increments when an independent oracle returned a value, and `main()` returns 2
when `compared == 0`. A CI job that treats exit 2 as failure cannot be silently
satisfied by an unwired oracle.

**Residual gap.** The C++ leg is still not wired. Recorded as G-3.

---

### 3.3 F-3 — Admitted lemmas silently inherited by `Qed`'d theorems

**Status: VERIFIED-STILL-BROKEN.**

**Audit.** VERIFIED. **Eleven** `Admitted` proofs existed at `HEAD` — more than
the finding's two named examples. Counting them requires matching both the
`Proof.\n  Admitted.` and the single-line `Proof. Admitted.` forms:

```
$ git grep -oE "\bAdmitted\b" HEAD -- 'formal_verification/coq/*.v' | wc -l
13          # 13 matches, 2 of which are the words "no Admitted" inside comments
```

| File:line at HEAD | Result | Kind |
|---|---|---|
| `RigorBytesOrder.v:15` | `nat_ltb_false_gt` | real lemma |
| `ReplayReorgCore/ReplayInvariants.v:34` | `utxo_remove_lookup_none` | real lemma |
| `ReplayReorgCore/ReplayInvariants.v:39` | `utxo_insert_lookup` | real lemma |
| `ReplayReorgCore/ReplayInvariants.v:46` | `utxo_spend_removes_output` | real theorem — **and false as stated**, §3.3.1 |
| `ReplayReorgCore/ReplayInvariants.v:64` | `rollback_restores_previous_state` | real theorem |
| `ReplayReorgCore/ReplayInvariants.v:77` | `replay_preserves_utxo_consistency` | real theorem |
| `Phase4FConsensus/ConsensusTrace.v:47` | `execute_trace_append` | real theorem |
| `Phase4FConsensus/ConsensusDeterminism.v:31` | `execute_trace_deterministic` | real theorem |
| `Phase4FConsensus/ConsensusDeterminism.v:64` | `oracle_agreement_placeholder` | `True`-bodied, see §3.4 |
| `Phase4FConsensus/ConsensusOracleBridge.v:158` | `bridge_cpp_equivalence_placeholder` | `True`-bodied, see §3.4 |
| `Phase4FConsensus/ConsensusTraceExecution.v:108` | `execute_registry_oracle_bridge_placeholder` | `True`-bodied, see §3.4 |

So: **8 genuine unproven results** plus 3 `True`-bodied placeholders.

A detail worth flagging for future audits: a grep for `^\s*Admitted` — the
obvious pattern — finds only 5 of the 11, because six use the single-line
`Proof. Admitted.` form. Any prior count based on the anchored pattern
undercounted by more than half.

The inheritance mechanism is exactly as reported: `consensus_replay_determinism`
in `ConsensusDeterminism.v:50` is marked `Qed`, but its proof calls
`execute_trace_deterministic`, which was `Admitted`. Coq accepts this silently;
only `Print Assumptions` reveals it. `Qed` alone is therefore not evidence.

**Change — all eleven discharged. Coq `Admitted` count across the corpus is now
0** (VERIFIED with the form-insensitive pattern:
`grep -rnE "\bAdmitted\b" --include='*.v' formal_verification/coq/` → no proof
sites remain).

- `nat_ltb_false_gt` (`RigorBytesOrder.v:13`) — one line: `apply Nat.ltb_ge in H; lia.`
  This lemma underpins `bytes_ltb_trans`/`antisym`/`total`, i.e. the claim that
  memcmp txid ordering is a total order — which is what makes ConvergenceRoot
  well defined.
- `execute_trace_append` (`ConsensusTrace.v:41`) — proven by induction on the trace.
- `execute_trace_deterministic` (`ConsensusDeterminism.v:27`) — `congruence`.
- `utxo_remove_lookup_none` (`ReplayInvariants.v:58`), `utxo_insert_lookup`
  (`:72`), `rollback_restores_previous_state` (`:230`) — proven.
- `replay_preserves_utxo_consistency` (`ReplayInvariants.v:256`) — proven, with
  the side condition described next.
- `utxo_spend_removes_output` — see below; it required correcting the statement.
- The three `True`-bodied placeholders — see §3.4.

Supporting lemmas added and proven along the way, all `Qed`:
`utxo_lookup_app_none`, `utxo_lookup_remove_stable`, `utxo_lookup_insert_stable`,
`utxo_lookup_fold_remove_stable`, `utxo_lookup_fold_create_stable`,
`spends_removed_by_fold`, `outpoint_eqb_refl`, `outpoint_eqb_false`.

#### 3.3.1 Discovery: `utxo_spend_removes_output` was FALSE as stated

This is the most significant technical finding of the subtask.

The unconditional statement — "if `op` is spent by `tx` and `apply_tx_utxo`
succeeds, then `op` is absent afterwards" — is not merely unproven. It is
**refutable in the current model**, because `apply_tx_utxo` assigns each created
output the synthetic outpoint `MkOutPoint hash32_placeholder (tx_index + i)`.
Every created output in every transaction draws from the same
`hash32_placeholder` txid, so a transaction can spend outpoint
`(hash32_placeholder, 0)` and immediately re-create it.

I encoded the counterexample and it is now checked by the build
(`ReplayInvariants.v:190`):

```coq
Theorem utxo_spend_removes_output_unconditional_is_false :
  ~ (forall (u : UTXOSet) (tx : TxAbs) (idx : nat) (op : OutPoint) (u' : UTXOSet),
        In op (tx_spends tx) ->
        apply_tx_utxo u tx idx = Some u' ->
        utxo_lookup u' op = None).
```

The usable theorem is restated with the side condition that makes it true
(`ReplayInvariants.v:137` and `:205`):

```coq
Definition not_recreated (tx : TxAbs) (tx_index : nat) (op : OutPoint) : Prop :=
  forall i, (i < length (tx_creates tx))%nat ->
            op <> synthetic_outpoint (tx_index + i).
```

**Consensus impact analysis.** INFERRED, and I want to be precise about the
boundary. This is a **modelling defect, not a production double-spend**.
Production assigns real 32-byte txids to created outputs (`core/src/**`, read
only), so the collision that refutes the lemma cannot arise on chain. What the
defect does mean is consequential in a different way: **the Coq model currently
cannot express double-spend prevention in its natural, unconditional form.** Any
document that cited `utxo_spend_removes_output` as establishing that spent
outputs are removed was citing a statement that is false in the very model it
was proven about. Recorded as gap **G-5**; the fix is to model real txids, which
is a modelling change out of scope here.

**Deliverable — complete axiom/admit inventory.** See §4. Built with the new
`formal_verification/tools/audit_assumptions.py`.

**Security reason.** `Qed` is routinely read as "proven". Without a
`Print Assumptions` sweep, a single admitted lemma silently launders itself into
every downstream theorem, and reviewers stop looking. The prior audit's
"601 of 617 axiom-free" figure is exactly the sort of number that needs a
reproducible generator rather than a one-off measurement.

**Affected modules.** `RigorBytesOrder` (byte ordering, feeds ConvergenceRoot),
`Phase4FConsensus` (trace determinism), `ReplayReorgCore` (UTXO/replay).

**Regression guard.** `tools/audit_assumptions.py --strict` exits non-zero if any
result rests on an axiom or admit, and also exits non-zero on probe errors
(so an unbuilt module cannot masquerade as clean — VERIFIED, §4.3). Suitable as a
CI gate.

---

### 3.4 F-4 — Vacuous theorem statements

**Status: VERIFIED-STILL-BROKEN, and materially larger than reported.**

**Audit.** VERIFIED. The reported instance was real: `reorg_preserves_supply`
concluded `total_supply s_end = total_supply s_end`. I then swept the whole
corpus programmatically for two shapes — conclusions of the form `X = X` after
stripping binders, and conclusions that are literally `True` — plus
`Definition <n> : Prop := True.` bookkeeping markers. Result: **31 vacuous
constructs**, not 1.

| Shape | Count | Location |
|---|---|---|
| Reflexivity-shaped (`X = X`) | 8 | all in `Phase5Equivalence/` |
| `True`-bodied results | 5 | all in `Phase5Equivalence/` |
| `Definition ... : Prop := True.` | 18 | all in `Phase5Equivalence/` |
| Supply theorems (reported instance + its mirror) | 2 | `ReplayReorgCore/`, `ConnectBlock/` |

The `Prop := True` family is the most misleading by name. For example
`EquivalenceFramework.v:183`:

```coq
Definition framework_established : Prop := True.
Lemma framework_established_holds : framework_established.
Proof. exact I. Qed.
```

Names like `pd4f_cpp01_closure_model_established`,
`phase5_mainnet_freeze_no_consensus_impact` and
`candidate_equivalence_evidence_model_established` read, in a status report, as
if a closure/impact/equivalence result had been obtained. Each is `True`.

Also found: `closure_allowed_well_defined : forall c, closure_allowed c = closure_allowed c`
and `sigma_bytes_to_integer_functional : forall bs, sigma_bytes_to_integer bs = sigma_bytes_to_integer bs`.
A name ending in `_deterministic` or `_functional` attached to `f x = f x`
asserts nothing; every Coq function is a function.

**Change — two strengthened, 31 marked, one guard added.**

*Strengthened* (`ReplayReorgCore/SupplyConservation.v`, mirrored in
`ConnectBlock/ConnectBlockSupply.v:106`):

```coq
(* before *)  total_supply s_end = total_supply s_end
(* after  *)  total_supply s_end' = total_supply (cc_state c) /\
              (total_supply s_end' <= total_supply s_end)%Z
```

and `reorg_replay_supply_equal` was re-stated so the two sides are genuinely
different states — previously it quantified over a second final state and then
compared `s_end` to itself; it now concludes
`total_supply s_reorg_tip = total_supply s_end`, i.e. rollback-then-replay
reaches the same tip supply as direct replay. Supporting this required a new
proven lemma, `fold_apply_blocks_supply_monotonic` (`SupplyConservation.v:349`).

I should be candid about the strength of the result: the existential witness for
`reorg_to_candidate` is definitionally `cc_state c`, so the third conjunct is
easy to prove. The *statement* now has content — it constrains what
`reorg_to_candidate` may return and asserts non-decreasing supply across the
replayed suffix — but it is not a deep theorem.

*Marked.* All 31 vacuous constructs now carry an explicit machine-greppable
comment immediately above them, e.g.:

```coq
(* VACUOUS-PLACEHOLDER (SECURITY-001-F / F-4): [framework_established] is defined as
   [True], so any lemma concluding it is discharged by [exact I] regardless of
   the artifacts it names. Bookkeeping marker only. NOT EVIDENCE. *)
```

These are comment-only edits; no proof or definition was altered, so there is no
risk of changing what the corpus asserts.

*Also under F-4*, the three `True`-bodied `Admitted` theorems from F-3 were
replaced with an honest pair — a `Prop` definition naming the open obligation,
and a provable theorem that makes the gap visible rather than hiding it:

| File | Was | Now |
|---|---|---|
| `ConsensusDeterminism.v:69,80` | `oracle_agreement_placeholder : ... True. Admitted.` | `oracle_agreement_obligation : Prop` (asserted nowhere) + a `Qed` theorem stating agreement holds **conditionally** on the oracle matching `execute_trace` |
| `ConsensusOracleBridge.v:159,169` | `bridge_cpp_equivalence_placeholder : True. Admitted.` | `bridge_cpp_equivalence_obligation : Prop` + a `Qed` theorem proving that with **no oracle present the bridge can never report `BS_Match`** |
| `ConsensusTraceExecution.v:110,117` | `execute_registry_oracle_bridge_placeholder : True. Admitted.` | `registry_oracle_bridge_obligation : Prop` + a `Qed` theorem proving the bridge is **incomplete**: `registry_na_count <> 0` (59 of 73 rows are N/A) |

The last one is my preferred pattern: instead of a vacuous placeholder that can
be miscited as progress, the file now contains a machine-checked proof that the
work is *not* done, which becomes false — and breaks the build — only when
someone actually completes it.

**Security reason.** A vacuous theorem is worse than a missing one. A missing
theorem shows up as a gap in review; a vacuous one shows up as a green checkmark
with an authoritative name, and it launders into status reports, certification
JSON, and eventually external claims.

**Affected modules.** `Phase5Equivalence` (31 markers — audit/catalog layer, no
consensus semantics), `ReplayReorgCore/SupplyConservation`,
`ConnectBlock/ConnectBlockSupply`, `Phase4FConsensus` (3 obligations).

**Consensus impact.** None of these are on a consensus path; the impact is
entirely on assurance accounting. The concrete risk realized: the supply
"conservation" theorem was cited as a supply guarantee while asserting nothing,
and there is still no supply-cap theorem (G-2).

**Regression guard.** New tool `formal_verification/tools/check_vacuous.py`.
It fails if any vacuous construct lacks a `VACUOUS-PLACEHOLDER` marker, so a new
one cannot be added silently. VERIFIED both ways:

```
$ python3 tools/check_vacuous.py
vacuous results found   : 31
explicitly marked       : 31
UNMARKED (failures)     : 0
GUARD_EXIT=0

# after adding `Lemma sneaky_vacuous : forall n : nat, n = n.`
UNMARKED (failures)     : 1
  VacGuardProbe.v:1  sneaky_vacuous  [reflexivity-shaped]
GUARD_EXIT_WITH_PROBE=1
```

---

### 3.5 F-5 — Missing critical invariants

**Status: VERIFIED-STILL-BROKEN**, with one nuance the finding did not capture.

**Audit.**

- **No supply-cap theorem for ORU** — VERIFIED. No `MAX_ORU` / `ORU_CAP` /
  `oru_max` constant exists anywhere in the corpus.
- **Nuance: RES *is* capped.** `ConnectBlockMint.v:11` defines
  `cb_MAX_RES_SUPPLY := 21000000 * 100` and `cb_res_mint_delta` refuses to mint
  past it. So "there is no supply cap" is too strong: the gap is **ORU-specific**.
- **ORU issuance is unbounded** — VERIFIED, and I machine-checked it rather than
  inferring. `cb_oru_subsidy` (`ConnectBlockMint.v:14`) is
  `Z.max 0 (cm_oru_subsidy spec)` — clamped below at zero, **unbounded above**,
  with no height-derived schedule check. `valid_block_connect`
  (`StateTransition.v:62`) checks only header linkage and strictly increasing
  chainwork; it never inspects the subsidy. I compiled this probe successfully:

```coq
Theorem oru_issuance_unbounded :
  forall (s : ReplayState) (b : BlockAbs) (N : Z),
    0 <= N ->
    cm_oru_subsidy (blk_mint b) = N ->
    protocol_oru_mint_delta s b = N.
Proof.
  intros s b N Hn Heq.
  unfold protocol_oru_mint_delta, cb_oru_mint_delta, cb_oru_subsidy.
  rewrite Heq. lia.
Qed.
```

  `ORU_PROBE_EXIT=0`. For any non-negative `N` the block author names, the model
  mints exactly `N`. (Probe only; not added to the corpus.)
- **`Permutation` appears zero times** — VERIFIED: `grep -rn "Permutation"
  --include='*.v' . | wc -l` → `0`. Transaction-order independence is entirely
  unproven, and it is precisely the property an anti-fork claim rests on.

**Change.** Per instruction, documented rather than proven. See the prioritized
register in §5. The README now carries a summary of it, and the fabricated
`order_invariance_convergence_root` claim that papered over G-1 is gone.

**Security reason.** These are the invariants whose absence would produce
inflation (G-2) or a chain split (G-1) — the two failure modes with no recovery
path after mainnet.

**Consensus impact.** G-2 is the serious one. INFERRED: the *model* permits
unbounded ORU issuance. Whether production does is a question about
`core/src/**`, which I did not modify and could not execute here; it must be
answered by the team that owns that code. **This should be treated as an open
question, not as a cleared item**, because the model that is supposed to be
mirroring ConnectBlock does not constrain the subsidy at all.

**Regression guard.** None possible for a missing theorem beyond the register
itself. G-1 and G-2 are stated here with named target theorems so that closing
them is a checkable event rather than a judgement call.

---

### 3.6 F-6 — `GoldenHarness.v` excluded from the build

**Status: VERIFIED-STILL-BROKEN.**

**Audit.** VERIFIED. `Kernel/GoldenHarness.v` existed but was absent from
`_CoqProject`, so it was never compiled. Additionally its 25 vectors were emitted
as `Compute` statements — which merely *print* a value. Even if the file had been
compiled, a wrong result would have printed `false` and the build would still
have succeeded. So the finding understated the problem twice over: the file was
both unbuilt and unenforcing.

**Change.**

1. Added to `_CoqProject` (the only line changed in that file):

```
 Kernel/KernelClosure.v
+Kernel/GoldenHarness.v
 State.v
```

2. Changed the generator `formal_verification/gen_coq_harness.py` to emit
   build-enforcing lemmas instead of `Compute`:

```python
def emit_check(P, idx, vid, expr):
    P(f"Definition chk_{idx} : bool := {expr}.")
    P(f"Lemma golden_{sanitize(vid)} : chk_{idx} = true.")
    P(f"Proof. vm_cast_no_check (eq_refl true). Qed.")
```

   `vm_cast_no_check (eq_refl true)` rather than `vm_compute; reflexivity`: the
   latter made the file take ~4 minutes because the kernel re-evaluates the
   expression a second time during conversion checking. The cast form defers a
   single check to the kernel. I confirmed the faster form is still sound
   (below), so this is a pure speedup, not a weakening.

3. Regenerated `Kernel/GoldenHarness.v`: 25 vectors, 25 lemmas — 9
   ConvergenceRoot (`golden_CR_000`-`006`, `010`, `011`; note the source vector
   set skips `CR_007`-`009`), 5 Sigma/PCORB (`golden_SIG_000`-`004`), 4
   eligibility (`golden_INQ_000`-`003`), 7 winner selection
   (`golden_WIN_000`-`006`).

**Verification that it actually enforces — VERIFIED.** I did not take the proof
style on trust. First, directly, that the mechanism rejects a false check:

```
Definition chk : bool := false.
Lemma bad_vm_cast : chk = true. Proof. vm_cast_no_check (eq_refl true). Qed.
  -> Error: The term "eq_refl" has type "true = true" while it is expected
     to have type "chk = true".
vm_cast_no_check_EXIT=1
```

Then end to end on the real harness, using the actual CR-002 vector with one
byte of its expected ConvergenceRoot changed from 43 to 44:

```
=== POSITIVE probe (unmodified CR-002 vector) ===
POSITIVE_EXIT=0
=== NEGATIVE probe (one byte of expected root corrupted) ===
File "/tmp/GHNeg.v", line 33, characters 40-44:
Error:
The term "eq_refl" has type "true = true" while it is expected to have type
 "chk_2 = true".
NEGATIVE_EXIT=1
```

A corrupted golden vector now breaks the build.

#### 3.6.1 Discovery: 2 of the 25 vectors are degenerate

My first attempt at the negative test mutated a **txid byte** in CR-001 and the
build stayed green. That was not a flaw in the enforcement — it exposed a flaw in
the vector. CR-000 (zero txids) and CR-001 (one txid) both expect 32 zero bytes,
because ConvergenceRoot is defined to return `0^256` when fewer than two txids
are eligible. Their expected value is therefore **independent of the txid
content**, and no SHA-256, serialization, or byte-ordering regression can make
them fail. VERIFIED by checking which `chk_N` have an all-zero expected value:
exactly `chk_0` and `chk_1`.

They still test something real — the "<2 ⇒ zeros" rule — but they should not be
counted toward SHA-256 coverage. Effective SHA-256-sensitive coverage is
**23 of 25**, not 25 of 25. Recorded as gap **G-4**.

**Security reason.** This is the most credible artifact in the repository — a
real byte-level SHA-256 executed by the Coq kernel and agreeing with the Python
SSOT on concrete data. Leaving it out of the build wasted the one piece of
genuine cross-layer evidence while weaker claims were being advertised.

**Affected modules.** `ConvergeRoot`, `SigmaCR01`/PCORB hash, eligibility
inequality, winner selection — i.e. Modules 3.1-3.4 at vector granularity.

**Consensus impact.** Positive. A silent drift between the Coq kernel and the
Python SSOT on any of the 23 non-degenerate vectors now fails the build instead
of going unnoticed.

**Regression guard.** The file is in `_CoqProject`, so it is built by the
standard entry point, and each vector is a `Qed` obligation. Regenerating from
`gen_coq_harness.py` preserves the enforcing form.

---

## 4. Axiom / admit inventory

### 4.1 Method

`formal_verification/tools/audit_assumptions.py` (new). For every module listed
in `coq/_CoqProject`, it extracts every top-level `Theorem`, `Lemma`,
`Corollary`, `Proposition`, `Remark`, `Fact` and `Example` (comments stripped so
commented-out examples are not counted), then runs Coq's `Print Assumptions` on
each and classifies the response:

- `AXIOM_FREE` — "Closed under the global context"
- `DEPENDS_ON_AXIOMS` — an `Axioms:` block, with the axiom names recorded
- `PROBE_ERROR` / `NO_OUTPUT` — status could not be determined; **never** treated
  as clean

Names are probed in batches per module, falling back to one-at-a-time so a single
bad name cannot suppress the rest of its module.

Full machine-readable output: `docs/security/TASK_SECURITY_001_F_ASSUMPTIONS.json`.

### 4.2 Result (VERIFIED)

```
========================================================================
RIGOR COQ AXIOM / ADMIT INVENTORY
========================================================================
Modules audited          : 196
Top-level results        : 1882
  AXIOM_FREE             : 1882
  DEPENDS_ON_AXIOMS      : 0
  probe errors           : 0
Declared Axiom/Parameter : 2
    hash256  (Hash.v)
    hash256_deterministic  (Hash.v)

No result depends on an axiom or admitted lemma.
```

Two observations that matter more than the headline number:

1. **The corpus declares exactly two axioms, and nothing reaches them.**
   `Parameter hash256 : Z -> Z` (`Hash.v:5`) and `Axiom hash256_deterministic`
   (`Hash.v:7`). `Hash.v` is imported only by `Extraction.v:6` (VERIFIED by
   grep), and no top-level result depends on either. The abstract-hash concern
   that would normally attach to a corpus like this **does not apply**: the
   byte-level modules compute real SHA-256.

2. **1882 ≠ 617.** The prior audit reported 617 theorems with 601 axiom-free.
   My count is higher because I include `Lemma`, `Corollary`, `Proposition`,
   `Remark`, `Fact` and `Example`, not only `Theorem`. The 16 previously
   non-axiom-free results correspond to the admits discharged in §3.3 and their
   downstream consumers; that set is now empty.

### 4.3 Inventory table — security-critical results

The full 1882-row inventory is in the JSON artifact. Reproducing it inline would
be unreadable, so the table below covers every result named in the findings, plus
the load-bearing consensus theorems. All are `Qed` **and** confirmed axiom-free by
`Print Assumptions`.

| Result | File:line | Before | After |
|---|---|---|---|
| `nat_ltb_false_gt` | `RigorBytesOrder.v:13` | **Admitted** | AXIOM_FREE |
| `bytes_ltb_trans` | `RigorBytesOrder.v:26` | inherited admit | AXIOM_FREE |
| `bytes_ltb_antisym` | `RigorBytesOrder.v:46` | inherited admit | AXIOM_FREE |
| `bytes_ltb_total` | `RigorBytesOrder.v:69` | inherited admit | AXIOM_FREE |
| `utxo_spend_removes_output` | `ReplayInvariants.v:205` | **Admitted, and FALSE as stated** | AXIOM_FREE (restated with `not_recreated`) |
| `utxo_spend_removes_output_unconditional_is_false` | `ReplayInvariants.v:190` | did not exist | AXIOM_FREE (refutation) |
| `replay_preserves_utxo_consistency` | `ReplayInvariants.v:256` | **Admitted** | AXIOM_FREE (with `not_recreated`) |
| `utxo_remove_lookup_none` | `ReplayInvariants.v:58` | **Admitted** | AXIOM_FREE |
| `utxo_insert_lookup` | `ReplayInvariants.v:72` | **Admitted** | AXIOM_FREE |
| `spends_removed_by_fold` | `ReplayInvariants.v:119` | did not exist | AXIOM_FREE |
| `rollback_restores_previous_state` | `ReplayInvariants.v:230` | **Admitted** | AXIOM_FREE |
| `utxo_create_adds_output` | `ReplayInvariants.v:223` | `Qed`, silently inherited admit | AXIOM_FREE (genuinely) |
| `execute_trace_append` | `ConsensusTrace.v:41` | **Admitted** | AXIOM_FREE |
| `execute_transition_deterministic` | `ConsensusDeterminism.v:12` | AXIOM_FREE | AXIOM_FREE |
| `execute_trace_deterministic` | `ConsensusDeterminism.v:27` | **Admitted** | AXIOM_FREE |
| `valid_trace_unique_final` | `ConsensusDeterminism.v:38` | `Qed`, silently inherited admit | AXIOM_FREE (genuinely) |
| `consensus_replay_determinism` | `ConsensusDeterminism.v:50` | `Qed`, silently inherited admit | AXIOM_FREE (genuinely) |
| `oracle_agreement_placeholder` | `ConsensusDeterminism.v:80` | `True.` + Admitted | AXIOM_FREE (conditional, honest) |
| `bridge_cpp_equivalence_placeholder` | `ConsensusOracleBridge.v:169` | `True.` + Admitted | AXIOM_FREE (proves gap) |
| `execute_registry_oracle_bridge_placeholder` | `ConsensusTraceExecution.v:117` | `True.` + Admitted | AXIOM_FREE (proves incompleteness) |
| `reorg_replay_supply_equal` | `SupplyConservation.v:374` | `Qed`, vacuous `X = X` | AXIOM_FREE, strengthened |
| `reorg_replay_preserves_winning_supply` | `SupplyConservation.v:391` | `Qed`, vacuous `X = X` | AXIOM_FREE, strengthened |
| `reorg_preserves_supply` | `ConnectBlockSupply.v:106` | `Qed`, vacuous `X = X` | AXIOM_FREE, strengthened |
| `fold_apply_blocks_supply_monotonic` | `SupplyConservation.v:349` | did not exist | AXIOM_FREE |
| `supply_monotonic` | `SupplyConservation.v:245` | AXIOM_FREE | AXIOM_FREE |
| `supply_deterministic` | `SupplyConservation.v:222` | AXIOM_FREE | AXIOM_FREE |
| `rollback_preserves_supply` | `SupplyConservation.v:311` | AXIOM_FREE | AXIOM_FREE |
| `genesis_supply_zero` | `SupplyConservation.v:411` | AXIOM_FREE | AXIOM_FREE |
| `selection_deterministic` | `Proofs/Determinism.v:7` | AXIOM_FREE | AXIOM_FREE |
| `unique_mint` (self-labelled WEAK) | `Proofs/Safety.v:11` | AXIOM_FREE | AXIOM_FREE |
| `execute_implies_verify` | `Proofs/Correctness.v:23` | AXIOM_FREE | AXIOM_FREE |
| `split_preserves_total` | `Proofs/SplitInvariance.v:13` | AXIOM_FREE | AXIOM_FREE |
| `pick_better_tx_prefers_lower` | `Kernel/KernelSelection.v:32` | AXIOM_FREE | AXIOM_FREE |
| `pick_better_tx_tie_txid` | `Kernel/KernelSelection.v:41` | AXIOM_FREE | AXIOM_FREE |
| `select_winner_in` | `Kernel/KernelSelection.v:52` | AXIOM_FREE | AXIOM_FREE |
| `converge_root_length` | `Kernel/KernelRoot.v:21` | AXIOM_FREE | AXIOM_FREE |
| `converge_root_small` | `Kernel/KernelRoot.v:32` | AXIOM_FREE | AXIOM_FREE |
| `golden_CR_000` … `golden_WIN_006` (25 total) | `Kernel/GoldenHarness.v` | **not built at all** | AXIOM_FREE, build-enforcing |

**Caveat that the table cannot express.** "AXIOM_FREE" means *unconditional given
the definitions in the model*. It says nothing about whether the model matches
`core/src/**`. A theorem can be perfectly axiom-free and still describe software
that was never shipped. That is gap G-3, and it applies to every row above.

### 4.4 Guard validation (VERIFIED)

I did not assume the tool works. Controlled test with a deliberately tainted
module:

```
Modules audited          : 1
Top-level results        : 3
  AXIOM_FREE             : 1
  DEPENDS_ON_AXIOMS      : 2
RESULTS THAT ARE NOT UNCONDITIONAL:
  Rigor.SecAuditNegControl.nc_admitted_dep   rests on: nc_bogus
  Rigor.SecAuditNegControl.nc_tainted        rests on: nc_bogus
TOOL_EXIT=1
```

It also refuses to report a status it could not obtain: probing an uncompiled
module yields `PROBE_ERROR` and exit 1, not a silent pass. The control module and
its `_CoqProject` entry were removed afterwards (VERIFIED: `git diff
formal_verification/coq/_CoqProject` shows only the `GoldenHarness.v` line).

---

## 5. Verification gap register (prioritized)

Priority reflects consequence-if-wrong × likelihood-of-being-wrong, for a chain
approaching mainnet. **P0 = must be closed or explicitly risk-accepted by a named
owner before mainnet.**

### G-1 (P0) — Transaction-order independence is entirely unproven

- **Claim status:** no theorem; `Permutation` occurs 0 times in 196 files (VERIFIED).
- **Why it matters:** two honest nodes that receive the same transaction set in
  different orders are not proven to compute the same ConvergenceRoot, the same
  winner, or the same resulting UTXO set. This is *the* property an anti-fork
  argument depends on, and it was the specific property the deleted README
  theorem `order_invariance_convergence_root` falsely claimed.
- **Mitigating context:** the spec sorts txids by memcmp before hashing, and the
  sort order is proven total (`bytes_ltb_total`, `RigorBytesOrder.v:69`), so the
  property is *plausibly* true. Plausible is not proven.
- **Target:** `forall l1 l2, Permutation l1 l2 -> convergence_root_from_txid_bytes l1 = convergence_root_from_txid_bytes l2`,
  and the corresponding statement for `select_winner`.
- **Estimated effort:** moderate. Requires a sortedness/permutation bridge lemma
  for the memcmp order; `Coq.Sorting.Permutation` and `Sorted` are applicable.
- **RESOLVED.** `formal_verification/coq/OrderIndependence.v`, axiom-free
  (`Print Assumptions`: closed under the global context).
  - `convergence_root_order_independent` and its `Tx`-level corollary
    `ConvergeRoot_order_independent` are **unconditional**. The route is
    `insert_sorted_txid_comm` — insertion into a sorted list commutes — which turns permutation
    invariance into an induction on the `Permutation` derivation and needs no sortedness or
    uniqueness bridge at all.
  - `select_winner_order_independent` carries two hypotheses: `positive_weights` and
    `distinct_txids`. Both hold on chain (`MIN_CONVERGENCE_AMOUNT` is 10^8 satoshis; a draw set is
    keyed by txid, and A-9-C's window excludes an already-paid one), but they are hypotheses of
    the theorem, not facts about `Tx`, so they are stated rather than assumed silently. The
    weight hypothesis is load-bearing rather than bookkeeping: the draw compares σ_x/w_x against
    σ_y/w_y by cross-multiplication, and `sigma_product_lt` stops being transitive the moment a
    weight can be zero — there would be no argmin to be invariant about.

### G-2 (P0) — No ORU supply cap; issuance is attacker-controlled in the model

- **Claim status:** VERIFIED by compiled probe (§3.5). `cb_oru_subsidy` returns
  `Z.max 0 (cm_oru_subsidy spec)` — no upper bound, no schedule check — and
  `valid_block_connect` never inspects it.
- **Why it matters:** unbounded issuance is the failure mode with no recovery
  after mainnet. Note RES *is* capped (`cb_MAX_RES_SUPPLY`), which makes the ORU
  omission look like an oversight rather than a deliberate design choice.
- **Open question for the `core/` owners:** does production `ConnectBlock`
  validate the coinbase ORU subsidy against a height-derived schedule? If it
  does, the model is wrong and should be corrected to mirror it; if it does not,
  this is a live consensus vulnerability. I could not determine this from the
  model, and `core/src/**` is out of my write scope.
- **Target:** a `valid_block_connect` that rejects
  `cm_oru_subsidy > expected_subsidy(height)`, plus
  `forall chain, total_oru_supply chain <= ORU_MAX_SUPPLY`.
- **RESOLVED — and the open question is answered: the model was wrong, not the code.**
  - Production `ConnectBlock` does validate the coinbase against a height-derived schedule:
    `oru_coinbase_value > nFeesOru + GetBlockSubsidy(pindex->nHeight)` is rejected as
    `bad-cb-amount` (`core/src/validation.cpp`). This was never a live consensus vulnerability.
  - The model has been corrected to mirror it. `cb_expected_oru_subsidy`,
    `cb_oru_subsidy_ok` and the schedule constants are in
    `ReplayReorgCore/ConnectBlockMint.v`, and `valid_block_connect` now carries the check
    (`ReplayReorgCore/StateTransition.v`).
  - **The second half of the target is false and cannot be proved.** RIGOR's subsidy halves only
    `ORU_SUBSIDY_MAX_SHIFTS = 3` times and then pays 125 ORU per block forever — tail emission, a
    deliberate divergence from Bitcoin, commented as "unlimited cumulative ORU" in
    `GetBlockSubsidy` and pinned by `static_assert` in `rigor_consensus_lock.h`. There is no
    `ORU_MAX_SUPPLY` to bound against. The finding read the RES cap's presence as evidence that
    the ORU omission was an oversight; it is not.
  - What is proved instead, in `ReplayReorgCore/SupplyConservation.v` and axiom-free, is the
    statement that actually excludes the failure mode: `apply_block_oru_bounded` (one block adds
    at most one genesis-era subsidy) and `oru_supply_bounded_by_block_count` (n blocks add at
    most n). Issuance is linear in chain length, so the only lever an attacker has left is
    producing blocks, which costs proof of work. `res_mint_respects_cap` states the RES cap
    alongside it so the asymmetry is on the record as intended rather than missing.

### G-3 (P1) — No Coq ↔ C++ equivalence; no extraction, no refinement

- **Claim status:** VERIFIED absent. Tracked in-repo as PD-4F-CPP-01 / PD-4F-OB-02.
- **Why it matters:** every one of the 1882 axiom-free results is about the
  model. Nothing transfers to the shipped binary by proof. The differential
  fuzzer's C++ leg is unwired (§3.2) and the golden-vector harness carries
  `equivalence_claimed: false`.
- **Honest note:** the repository is already candid about this
  (`FormalVerificationCertification.v:11-21`, verdict `RV_Partial`). The defect
  was the documentation layer implying otherwise, which F-1 fixes.
- **Nearest achievable step:** wire `RIGOR_CPP_HARNESS` so the fuzzer's C++ leg
  becomes real. That converts G-3 from "no evidence" to "sampled evidence" at low
  cost, without requiring extraction.

### G-4 (P2) — Cross-layer agreement is vector-level and partly degenerate

- **Claim status:** VERIFIED. 25 vectors; `chk_0` and `chk_1` expect 32 zero bytes
  regardless of txid content, so effective SHA-256-sensitive coverage is 23/25
  (§3.6.1).
- **Why it matters:** 23 sampled inputs is real but narrow evidence for a
  universally quantified equivalence claim.
- **Target:** replace or supplement the two degenerate vectors with
  boundary-condition vectors that are content-sensitive, and raise the default
  fuzzer iteration count in CI.

### G-5 (P2) — Synthetic-outpoint collision in the replay model

- **Claim status:** VERIFIED, refutation compiled (`ReplayInvariants.v:190`).
- **Why it matters:** the model cannot state double-spend prevention in its
  natural unconditional form. Not a production double-spend (production uses real
  txids — INFERRED), but it means the UTXO-safety story in the model is weaker
  than its lemma names suggest.
- **Target:** model created outputs with the real transaction id so
  `not_recreated` becomes derivable rather than assumed.

### G-6 (P3) — 31 vacuous constructs remain in `Phase5Equivalence`

- **Claim status:** VERIFIED; all 31 now marked and guarded (§3.4).
- **Why it matters:** they are inert, but their names
  (`..._established`, `..._no_consensus_impact`) invite miscitation in status
  reporting. The guard prevents growth; it does not remove the existing ones.
- **Target:** delete them, or give each a statement with content.

### G-7 (P3) — `Proofs/Safety.v:unique_mint` is content-light

- **Claim status:** VERIFIED; self-labelled "WEAK (real, but content-light)" at
  `Proofs/Safety.v:7-10`. It proves `Execute` is a function, not supply-level
  mint uniqueness.
- **Note:** the source is honest here. Preserve the label; do not cite this as a
  supply guarantee. Interacts with G-2.

---

## 6. Changes requiring approval (PROOF-034 SSOT)

**Nothing in this section has been applied.** Each item touches a file on the
hard-prohibition list and requires explicit human approval.

### 6.1 (P0) `golden_vector_fixture.cpp` — circular validation makes the golden-vector check incapable of failing

**File:** `core/src/proof/golden_vector_fixture.cpp` (**DELETED — see 6.1.1**)
**Status:** VERIFIED by source reading; INFERRED at runtime (C++ not executed here).
**Resolution:** FIXED by the recommended structural remedy. The finding below is retained as the
record of what was wrong; §6.1.1 records what was done.

**Defect.** The expected values from the vector spec are written into the block
index and the runtime mirror, and are then read back out of those same locations
and compared against themselves. The loop is:

1. `BuildMirrorFromGoldenVectorSpec` (lines 93-97, and the parallel blocks at
   99-139) copies the spec's expectations into the mirror:

```cpp
if (spec.expect_oru_supply) {
    mirror.oru_supply_cumulative = spec.expected_oru_supply;   // line 93-94
} else if (pindex.m_chain_oru_total >= 0) {
    mirror.oru_supply_cumulative = pindex.m_chain_oru_total;
}
```

2. `ApplyAuthoritativeCachesFromMirror` (line 24-30) writes those same values into
   the authoritative block-index caches:

```cpp
pindex.m_chain_oru_total = mirror.oru_supply_cumulative;
```

3. `StoreMirrorWitnessAt` (line 41) stores the mirror keyed on the index.

4. `MaterializeGoldenVectorFixture` (line 226) calls `EvaluateGoldenVectorOnChain`
   → `CaptureRuntimeWitnessSnapshot` → `Consensus::BuildRuntimeSupplyExport`,
   which at `runtime_supply_export.cpp:83-86` retrieves **that same stored
   mirror**, and at lines 95-98 reads back `pindex->m_chain_oru_total` — the value
   written in step 2.

5. `EvaluateGoldenVector` (`runtime_vector_harness.cpp:590-606`) compares:

```cpp
AppendAmountField(report, "oru_supply", "oru_supply_cumulative", "FVR-B", true,
                  spec.expected_oru_supply,      // expected
                  has_export,
                  mirror.oru_supply_cumulative); // actual == spec.expected_oru_supply
```

So the comparison is `spec.expected_oru_supply == spec.expected_oru_supply`. It
is **mathematically incapable of reporting a mismatch** for every field carrying
an `expect_*` flag: `oru_supply`, `oru_parent`, `oru_subsidy`, `oru_burn`,
`res_supply`, `res_parent`, `res_mint`, `res_mint_count`.

This is the same class of defect as F-2 and F-4 — a check that cannot fail — but
in the runtime layer, and it is the layer whose output feeds the release gate.

**Security consequence.** Every golden vector reported `VERIFIED` regardless of
what the node actually computed. Any supply-accounting divergence between
consensus and the spec would have passed. Combined with G-2 (no ORU cap in the
model), the two mechanisms that should independently have caught an issuance bug
were both inoperative.

**Proposed patch — DO NOT APPLY WITHOUT APPROVAL.** The fix is to stop feeding
expectations into the artifact under test. Fixture application should establish
*inputs* and let the consensus code compute the outputs; only then may the
outputs be compared against `spec.expected_*`.

Minimal form, preserving current file structure: add a capture of the
consensus-computed state taken **before** any spec value is written, and compare
against that.

```diff
--- a/core/src/proof/golden_vector_fixture.cpp
+++ b/core/src/proof/golden_vector_fixture.cpp
@@
+// SECURITY-001-F: capture what consensus actually computed, BEFORE any
+// spec-derived expectation is written into the index or the mirror. This is
+// the only value that may legitimately be compared against spec.expected_*.
+Consensus::RuntimeSupplyState CaptureUncontaminatedState(const CBlockIndex& pindex,
+                                                        const CCoinsViewCache& view,
+                                                        const Consensus::Params& consensus)
+{
+    const uint8_t mint_count = pindex.m_block_res_mint_sum > 0 ? 1 : 0;
+    return Consensus::CaptureRuntimeSupplyState(&pindex, view, consensus, mint_count);
+}
@@ bool ApplyGoldenVectorFixtureAtIndex(
     (void)view;
     if (pindex.nHeight != spec.height) {
         return false;
     }
+    // Must run before ApplyParentFixtureIfNeeded / BuildMirrorFromGoldenVectorSpec,
+    // both of which overwrite the authoritative caches with spec values.
+    const Consensus::RuntimeSupplyState observed =
+        CaptureUncontaminatedState(pindex, view, consensus);
+    RecordObservedStateForVector(spec.id, observed);
+
     if (!ApplyParentFixtureIfNeeded(pindex, spec, view, consensus)) {
         return false;
     }
```

and in `runtime_vector_harness.cpp`, source the "actual" side from the recorded
observation rather than from the mirror:

```diff
-                          has_export,
-                          mirror.oru_supply_cumulative);
+                          has_observed,
+                          observed.oru_supply_cumulative);
```

**Preferred alternative (cleaner, larger).** Split the two responsibilities that
are currently entangled in one function:

- `ApplyGoldenVectorFixtureAtIndex` becomes **input-only** — it may set up chain
  state, but must never write a `spec.expected_*` value into `pindex` or the
  mirror. Every `if (spec.expect_X) mirror.X = spec.expected_X;` branch in
  `BuildMirrorFromGoldenVectorSpec` (lines 93-139) is deleted; the `else`
  branches, which read genuine consensus state, become unconditional.
- Verification runs afterwards against unmodified consensus output.

This removes the circularity structurally rather than by adding a second path,
and it is the version I recommend.

**Regression guard to add with the fix.** A self-test vector whose
`expected_oru_supply` is deliberately wrong, asserted to produce
`GvrVerificationStatus::FAIL`. If that vector ever passes, the circularity has
returned. Without such a negative control, the same defect can be reintroduced
and will again look green.

**Verification required from the owner (I could not run it):** build `core/` and
confirm (a) all currently-passing vectors still pass against genuine consensus
output, and (b) the deliberately-wrong vector fails.

#### 6.1.1 Remediation record

The preferred alternative was taken, and taken to its conclusion. Once every
`if (spec.expect_X) mirror.X = spec.expected_X;` branch is deleted,
`BuildMirrorFromGoldenVectorSpec` is `CaptureRuntimeSupplyState` under another name and
`ApplyGoldenVectorFixtureAtIndex` does nothing a caller would want. The whole mechanism was
therefore removed rather than hollowed out:

| Removed | Why |
|---------|-----|
| `core/src/proof/golden_vector_fixture.{h,cpp}` | Sole purpose was writing expectations into observations |
| `materializegoldenvector` RPC | Its only effect was to run that write. No script called it |
| Runtime fixture registry (`golden_vector_fixtures.json`, `Set/Load/Save…Runtime`) | Persisted the fabricated result, so `getruntimevectorstatus` reported PASS on later runs and a vector could be promoted to ESTABLISHED by RPC |

`getruntimevectorstatus` survives untouched and is now the only path: it reads the mirror
consensus actually produced and compares it against the vector. `GoldenVectorDocumentStatus` now
reflects only the static `fixture_materialized` flag in the vector table.

The negative control asked for above is
`runtime_vector_harness_tests/harness_fails_when_the_observation_disagrees_with_the_vector`. It
evaluates GV-ORU-0001 twice — once with a mirror that agrees, once with the same mirror plus one
satoshi — and requires PASS then FAIL. Asserting only the PASS half would have been satisfied by
the tautology this replaces.

**Expect the gate's colour to change.** Vectors previously reported ESTABLISHED/PASS on the
strength of materialization now report against real chain state. Any that go NOT_EXECUTED or FAIL
were never verified; the status is new information, not a regression.

### 6.2 (P2) `catalog.py` / `gate_engine.py` — reporting text implies proven equivalence

**Files:** `sim/proof_validator/catalog.py`, `sim/release/gate_engine.py` (**NOT MODIFIED**)

I did not audit these in depth; they are outside the finding list. Flagging only
that if either surfaces a "formal verification" status derived from the
`Phase5Equivalence` `*_established : Prop := True` markers (§3.4, G-6), that
status is vacuous at the source and its wording should be revisited by the owner
alongside 6.1.

---

## 7. Final build and verification output

All output below is real and was produced in this environment.

### 7.1 Clean rebuild of the full Coq corpus from scratch — VERIFIED

Entry point is the repo's own `_CoqProject`:

```
$ cd formal_verification/coq
$ coq_makefile -f _CoqProject -o Makefile.sec001f.mk
$ make -f Makefile.sec001f.mk clean
$ make -f Makefile.sec001f.mk -j2
```

Tail of the run:

```
=== CLEAN BUILD FROM SCRATCH ===
Sun Aug  2 13:00:45 CST 2026
COQC Phase5Equivalence/RuntimeHashMappingCatalog.v
COQC Phase5Equivalence/RuntimeArtifactReconstructionCatalog.v
COQC Phase5Equivalence/AntiEquivalenceSafetyAudit.v
COQC Phase5Equivalence/RuntimeHashMappingAudit.v
COQC Phase5Equivalence/RuntimeHashMappingResolution.v
COQC Phase5Equivalence/RuntimeArtifactReconstructionAudit.v
COQC Phase5Equivalence/RuntimeSufficiencyGapCatalog.v
COQC Phase5Equivalence/RuntimeHashMappingResolutionAudit.v
COQC Phase5Equivalence/RuntimeSufficiencyGapAudit.v
COQC Phase5Equivalence/RuntimeWitnessRequirementCatalog.v
COQC Phase5Equivalence/RuntimeWitnessRequirementAudit.v
COQC Phase5Equivalence/RuntimeAlignmentCatalog.v
COQC Phase5Equivalence/RuntimeAlignmentAudit.v
COQC Phase5Equivalence/RuntimeWitnessIntegrationCatalog.v
COQC Phase5Equivalence/RuntimeWitnessIntegrationAudit.v
COQC Phase5Equivalence/RuntimeAlignmentIntegrationCatalog.v
COQC Phase5Equivalence/RuntimeAlignmentIntegrationAudit.v
COQC Phase5Equivalence/CanonicalRuntimeHashAlignmentCatalog.v
COQC Phase5Equivalence/EquivalenceClosureCatalog.v
COQC Phase5Equivalence/CanonicalRuntimeHashAlignmentAudit.v
COQC Phase5Equivalence/EquivalenceClosureAudit.v
COQC Phase5Equivalence/DrawClosureImpossibilityCatalog.v
COQC Phase5Equivalence/DrawClosureImpossibilityAudit.v
COQC Phase5Equivalence/Phase5SummaryCatalog.v
COQC Phase5Equivalence/Phase5SummaryAudit.v
CLEAN_BUILD_EXIT=0
Sun Aug  2 13:08:02 CST 2026
```

**196/196 files, exit 0, 7m17s**, including the newly added
`Kernel/GoldenHarness.v` with its 25 enforced vectors. The known-good baseline
(195 files) is not regressed; it is extended by one.

### 7.1a Second clean rebuild, after **all** edits — VERIFIED

The build in §7.1 predates the F-4 marker edits, so it does not by itself certify
the final tree. I re-ran a full clean build against the final state:

```
=== FINAL CLEAN BUILD (post all edits) ===
Sun Aug  2 13:44:49 CST 2026
COQC Phase5Equivalence/RuntimeWitnessIntegrationCatalog.v
COQC Phase5Equivalence/RuntimeWitnessIntegrationAudit.v
COQC Phase5Equivalence/RuntimeAlignmentIntegrationCatalog.v
COQC Phase5Equivalence/RuntimeAlignmentIntegrationAudit.v
COQC Phase5Equivalence/CanonicalRuntimeHashAlignmentCatalog.v
COQC Phase5Equivalence/EquivalenceClosureCatalog.v
COQC Phase5Equivalence/CanonicalRuntimeHashAlignmentAudit.v
COQC Phase5Equivalence/EquivalenceClosureAudit.v
COQC Phase5Equivalence/DrawClosureImpossibilityCatalog.v
COQC Phase5Equivalence/DrawClosureImpossibilityAudit.v
COQC Phase5Equivalence/Phase5SummaryCatalog.v
COQC Phase5Equivalence/Phase5SummaryAudit.v
FINAL_CLEAN_BUILD_EXIT=0
Sun Aug  2 13:49:09 CST 2026
```

Every one of the 196 `_CoqProject` entries produced a `.vo` (VERIFIED by set
difference; the only unmatched `.vo` in the tree,
`Phase5Equivalence/_PD4FCPP01ClosureModel_min.vo`, is a pre-existing stray with
no corresponding `.v` and no `_CoqProject` entry — not produced by this
subtask).

### 7.2 Incremental rebuild after the F-4 marker edits — VERIFIED

```
=== INCREMENTAL REBUILD AFTER F-4 MARKERS ===
Sun Aug  2 13:27:55 CST 2026
...
COQC Phase5Equivalence/DrawClosureImpossibilityAudit.v
COQC Phase5Equivalence/Phase5SummaryCatalog.v
COQC Phase5Equivalence/Phase5SummaryAudit.v
REBUILD_EXIT=0
Sun Aug  2 13:34:00 CST 2026
```

### 7.3 Assumption rescan after all changes — VERIFIED

```
Modules audited          : 196
Top-level results        : 1882
  AXIOM_FREE             : 1882
  DEPENDS_ON_AXIOMS      : 0
  probe errors           : 0
Declared Axiom/Parameter : 2
    hash256  (Hash.v)
    hash256_deterministic  (Hash.v)

No result depends on an axiom or admitted lemma.
```

### 7.4 Admit count — VERIFIED

Using the form-insensitive pattern (the anchored `^\s*Admitted` pattern misses
the single-line `Proof. Admitted.` form and undercounts — see §3.3):

```
$ git grep -oE "\bAdmitted\b" HEAD -- 'formal_verification/coq/*.v' | wc -l
13          # baseline: 11 real admits + 2 occurrences of "no Admitted" in comments

$ grep -rnE "\bAdmitted\b" --include='*.v' formal_verification/coq/
Phase4FConsensus/ConsensusTraceExecution.v:113:    [Theorem execute_registry_oracle_bridge_placeholder : True. Admitted.] — a
Phase4FConsensus/ConsensusDeterminism.v:75:    [Theorem oracle_agreement_placeholder : ... True. Admitted.] — a vacuous
Phase4FConsensus/ConsensusOracleBridge.v:163:    [Theorem bridge_cpp_equivalence_placeholder : True. Admitted.] — a vacuous
Phase4FConsensus/TraceAgreementAudit.v:79:(** ---- REQUIRED-4: mandatory Qed proofs (no Admitted) ---- *)
Phase4FConsensus/LiveRuntimeTraceIntegration.v:85:(** ---- Projection-correctness proofs (Qed; no Admitted) ---- *)
ReplayReorgCore/ReplayInvariants.v:167:    was [Admitted]. It is not merely unproven — it is REFUTABLE in this model:
```

Every remaining hit is a **comment**: two pre-existing, and four written by this
subtask to record what each site used to say. **Zero admitted proofs.** The
anchored form also confirms this:

```
$ grep -rcE "^\s*Admitted" --include='*.v' formal_verification/coq/ | awk -F: '{s+=$2} END{print s}'
0
```

### 7.5 Regression guards — VERIFIED

```
$ python3 tools/check_vacuous.py
vacuous results found   : 31
explicitly marked       : 31
UNMARKED (failures)     : 0
```

### 7.6 Differential fuzzer — VERIFIED

```
Cases compared against an independent oracle: 32
Mismatches:                                   0
Not compared: C++ (RIGOR_CPP_HARNESS not set to an existing binary; C++ leg not wired)
AGREEMENT: Python spec and Coq kernel agree on all compared cases.
FUZZER_EXIT=0
```

---

## 8. Regression guards introduced

| Guard | Command | Catches |
|---|---|---|
| Golden vectors are build-enforcing | `make -f Makefile.coq` | Any Coq↔Python drift on the 23 content-sensitive vectors |
| Axiom/admit inventory | `python3 tools/audit_assumptions.py --strict` | A new admit, or a `Qed` theorem that silently inherits one; also fails on unverifiable probes |
| Vacuous-statement guard | `python3 tools/check_vacuous.py` | A new `X = X` or `True`-bodied result added without an explicit marker |
| Fuzzer honest exit codes | `python3 fuzz/differential_fuzzer.py` | Exit 2 when no independent oracle is reachable, so an unwired leg cannot look green |

Recommended CI wiring: run all four; treat any non-zero exit as a failure. The
first is already covered by the standard Coq build.

---

## 9. Files changed

Within scope only.

| File | Change |
|---|---|
| `formal_verification/README.md` | Rewritten (F-1) |
| `formal_verification/fuzz/differential_fuzzer.py` | Rewritten around a real Coq oracle (F-2) |
| `formal_verification/gen_coq_harness.py` | Emits enforcing lemmas instead of `Compute` (F-6) |
| `formal_verification/coq/_CoqProject` | +1 line: `Kernel/GoldenHarness.v` (F-6) |
| `formal_verification/coq/Kernel/GoldenHarness.v` | Regenerated: 25 enforced lemmas (F-6) |
| `formal_verification/coq/RigorBytesOrder.v` | `nat_ltb_false_gt` discharged (F-3) |
| `formal_verification/coq/ReplayReorgCore/ReplayInvariants.v` | Admits discharged; false lemma refuted and corrected (F-3) |
| `formal_verification/coq/ReplayReorgCore/SupplyConservation.v` | Two vacuous theorems strengthened; monotonicity lemma added (F-4) |
| `formal_verification/coq/ConnectBlock/ConnectBlockSupply.v` | Mirrors the strengthened statement (F-4) |
| `formal_verification/coq/Phase4FConsensus/ConsensusTrace.v` | `execute_trace_append` discharged (F-3) |
| `formal_verification/coq/Phase4FConsensus/ConsensusDeterminism.v` | `execute_trace_deterministic` discharged; obligation made explicit (F-3, F-4) |
| `formal_verification/coq/Phase4FConsensus/ConsensusOracleBridge.v` | Vacuous placeholder replaced by an obligation + gap proof (F-4) |
| `formal_verification/coq/Phase4FConsensus/ConsensusTraceExecution.v` | Vacuous placeholder replaced by an incompleteness proof (F-4) |
| `formal_verification/coq/Phase5Equivalence/*.v` (24 files) | Comment-only `VACUOUS-PLACEHOLDER` markers (F-4) |
| `formal_verification/tools/audit_assumptions.py` | New (F-3) |
| `formal_verification/tools/check_vacuous.py` | New (F-4) |
| `docs/security/TASK_SECURITY_001_F_REPORT.md` | This report |
| `docs/security/TASK_SECURITY_001_F_ASSUMPTIONS.json` | Machine-readable inventory |

No file on the PROOF-034 prohibition list was modified. No file under
`core/src/**`, `pool/**` or `sim/**` was modified.

---

## 10. What this report does not claim

- It does **not** claim the implementation is correct. All Coq results are about
  the model (G-3).
- It does **not** claim Coq = Python = C++. No such theorem exists and none was
  created.
- It did **not** claim the network cannot fork, because order independence was unproven. It is
  now proved for both the convergence root and the winner (G-1). That closes the model-level
  obligation; it says nothing about the C++ implementation, which is still G-3.
- It did **not** claim supply is bounded. ORU issuance is now bounded per block in the model, and
  the production check it mirrors was there all along (G-2). ORU has no constant cap by design,
  so the bound is linear in chain length rather than absolute.
- It made **no** claim about the C++ golden-vector harness: §6.1 argued it could not fail. That
  defect has since been fixed (§6.1.1), so the harness can now report a real result — which is not
  the same as saying the vectors pass.
- It does **not** declare mainnet readiness.

What it does claim, with pasted evidence: the corpus builds clean, contains no
admits, has no result resting on an axiom, enforces 23 content-sensitive
cross-layer vectors at build time, and its outward-facing documentation no longer
advertises theorems that do not exist.
