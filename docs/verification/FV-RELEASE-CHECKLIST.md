# FV Release Checklist

**Document status:** Production documentation  
**Role:** Repeatable pre-release checks for formal-verification correspondence.  
**Does not claim:** Coq = C++ refinement, a shared production hash binary, or permission to flip honesty flags.

Living lock: **422** shared / **215** Coq-enforced (`formal_verification/golden_lock.json`, tag `tightening-replay-txid-2026-08-18`). RC0 freeze recorded 414 / 207.

Honesty (must stay **false**):

```
equivalence_claimed = false
hash_kernel_shared_binary_with_cpp = false
refinement_proved = false
```

Operator status: [FORMAL-VERIFICATION-STATUS.md](./FORMAL-VERIFICATION-STATUS.md). Freeze commands: [FORMAL-VERIFICATION-FREEZE.md](./FORMAL-VERIFICATION-FREEZE.md). Named L1 vs L2 splits: [GOLDEN-VECTOR-DISCREPANCIES.md](./GOLDEN-VECTOR-DISCREPANCIES.md).

---

## 1. One command (default release path)

From this repository:

```bash
bash formal_verification/scripts/run_fv_release_checklist.sh
```

That runs, in order:

1. `check_freeze_artifacts.sh` — key files present; honesty flags false; CURRENT_STATE still cites 414/207 (RC0) and 422/215 (living).
2. `SKIP_TEST_RIGOR=1 run_differential_gate.sh` — Python oracle, generated-artifact freshness, **`check_golden_lock.py` (gate step 2b)**, tripartite hash, production σ/root CLI.

A PASS line must print the three honesty flags as **false**.

| Extra | Command |
|-------|---------|
| Coq harness (8.15.0; GoldenHarness is slow) | `FV_COQ=1 bash formal_verification/scripts/run_fv_release_checklist.sh` |
| BOOST `test_rigor` if a binary exists | `FV_TEST_RIGOR=1 bash formal_verification/scripts/run_fv_release_checklist.sh` |
| Re-extract OCaml hash kernel | `FV_EXTRACT=1 bash formal_verification/scripts/run_fv_release_checklist.sh` |

Do **not** apt-install Ubuntu `coq` for this gate. Pin **8.15.0** locally.

---

## 2. Commands that must be green

### Always (correspondence / docs release)

| Step | Command | Must show |
|------|---------|-----------|
| Freeze artifacts | `bash formal_verification/scripts/check_freeze_artifacts.sh` | `freeze artifact check: PASS` |
| Golden lock | `python3 formal_verification/scripts/check_golden_lock.py` | `golden lock: PASS` · `vectors=422` · `Coq-enforced lemmas=215` · `honesty flags: all false` |
| Differential gate | `SKIP_TEST_RIGOR=1 bash formal_verification/scripts/run_differential_gate.sh` | `differential gate: PASS` and the three flags **false** |

Lock check is already inside the gate (step 2b). Running it alone is useful after a lock bump, before a full gate.

### When Coq sources or harness lemmas changed (local, Coq 8.15)

Gate step 5 (`COQ=1`) compiles these three, in order:

| Target | When it is required | Cost |
|--------|---------------------|------|
| `ReplayReorgCore/ReplayInvariants.v` | Replay / UTXO / dual-pool / obligation model | Cheap |
| `Kernel/NegativeCoreHarness.v` | NegativeCore, EdgeCross script/PCORB/schedule rows | Cheap (~seconds if `.vo` deps exist) |
| `Kernel/GoldenHarness.v` | SHA-256 / root / window / DCR / InvalidCvg / ScriptMintNeg / window-obligation rows | Expensive (`vm_compute`) |

Equivalent one-liners from `formal_verification/coq/`:

```bash
coqc -q -R . Rigor ReplayReorgCore/ReplayInvariants.v
coqc -q -R . Rigor Kernel/NegativeCoreHarness.v
coqc -q -R . Rigor Kernel/GoldenHarness.v
```

A full `_CoqProject` rebuild is **not** required to ship correspondence. It is required only when claiming a protocol-core theorem still Closed (`Print Assumptions`).

---

## 3. What a green run does *not* mean

Same bound as the freeze page: not refinement; not a shared `rigord` hash object; not SIMD equality; not coin-view `ComputeConvergenceRootFromConvergenceSet`; not `IsResScript` vs `out_asset`; not coinbase bind of `draw_candidate_root`.

---

## 4. When to regenerate vs bump the lock

Generators **refuse to write** unless `golden_lock.json` already describes the set they would emit. Order for a count change: **bump lock → regenerate → gate**.

Checked-in generated artifacts:

| Artifact | Generator |
|----------|-----------|
| `formal_verification/phase3_golden_vectors.json` | `python3 formal_verification/generate_phase3_vectors.py` |
| `coq/Kernel/GoldenHarness.v`, `NegativeCoreHarness.v`, `coq_harness_index.json` | `cd formal_verification && python3 gen_coq_harness.py` |
| `core/src/test/selection_golden_bridge.inc` | `python3 formal_verification/tools/gen_cpp_bridge_header.py` |

The gate regenerates those into a temp dir and `cmp`s the working tree (step 2).

### Re-run the gate only (no lock bump)

Oracle, C++ portable CLIs, or Coq kernel *semantics* changed but vector **counts / IDs / splits** did not.

| Touched | Then |
|---------|------|
| `spec/rigor_consensus_spec.py` (same vector count) | Regen JSON + harness + bridge; gate. Expected bytes in JSON will move. |
| `coq/*.v` used by harness (Selection, WindowExclusion, DrawCandidateRoot, ConvergeRoot, Sigma*, ResSchedule, ScriptMintWitness, InvalidCvg, ReplayReorgCore, Hash32Order, …) | `FV_COQ=1` (or the three `coqc` targets above); gate. Regen harness only if `gen_coq_harness.py` emit changed. |
| `core/src/crypto/sha256.*`, `hash.h`, `consensus_rigor/sigma_hash.*`, `consensus/convergence_root_eligibility.*`, `convergence_logic.*` | Gate (steps 3–4 rebuild CLIs). |
| `core/src/test/selection_golden_tests.cpp` | Gate; BOOST if `FV_TEST_BITCOIN=1`. |
| Honesty lock `coq/DrawBridgeProgress.v` / `Phase5Equivalence/DrawBridgeProgress.v` | Freeze artifacts + lock check. **Do not set flags true.** |

### Bump `golden_lock.json` then regenerate

Vector **membership**, `coq_enforce`, family totals, must-enforce ranges, or `surface_splits` changed. Needs a dedicated vector / discrepancy task.

| Touched | Lock fields to edit |
|---------|---------------------|
| New / removed golden rows | `shared_vectors`, `families.*`, `must_enforce_families` id ranges, `json_coq_enforce_true`, harness lemma counts |
| EdgeCross / NegativeCore / InvalidCvg / ScriptMintNeg / DCR-024…035 | matching `must_enforce_families` |
| New named L1 vs L2 split | `surface_splits.rows` **and** [GOLDEN-VECTOR-DISCREPANCIES.md](./GOLDEN-VECTOR-DISCREPANCIES.md) |
| Promoting a python-only family into the harness | `families.*.coq_enforced`, drop it from `python_only_families` |

Do **not** cite JSON `coq_enforce: true` (190) as the L1↔L2 lock. The lock is **215** lemmas.

### Docs-only

| Touched | Then |
|---------|------|
| `docs/formal/RIGOR_FORMAL_VERIFICATION_CURRENT_STATE.md` living counts | `check_freeze_artifacts.sh` (requires 414/207 **and** 422/215) |
| STATUS / FREEZE / this checklist / discrepancies | Freeze artifacts; CI should run (see §5) |
| Verdict theorem inventory | Do not silently rewrite |

---

## 5. CI vs local

Workflow: `.github/workflows/formal-verification-gate.yml`.

| Check | CI (ubuntu-latest) | Local operator |
|-------|--------------------|----------------|
| Freeze artifacts | **Yes** (via this checklist runner) | Yes |
| Gate steps 1–4 (oracle, regen `cmp`, lock, hash CLI, σ/root CLI) | **Yes** | Yes |
| `COQ=1` harness / GoldenHarness | **No** — Ubuntu `coq` is not 8.15.0 | Required after Coq/harness edits |
| BOOST `test_rigor` | **No** (no prebuilt binary; default skips) | `FV_TEST_RIGOR=1` when a binary exists |
| `EXTRACT=1` | **No** | Optional |

Path filter (PR/push) includes `formal_verification/**`, portable hash/σ/root C++ sources, `selection_golden_bridge.inc` / `selection_golden_tests.cpp`, STATUS/FREEZE, this checklist, `docs/formal/**`, and `GOLDEN-VECTOR-DISCREPANCIES.md`.

CI is enough to stop a silent lock / artifact / oracle / CLI regression. It is **not** enough to stop a Coq lemma that still type-checks in the generator but fails `vm_compute`. That is the local `FV_COQ=1` obligation.

---

## 6. Pre-release box

Copy and tick:

- [ ] `bash formal_verification/scripts/run_fv_release_checklist.sh` → freeze PASS + `differential gate: PASS`
- [ ] PASS line prints `equivalence_claimed=false`, `hash_kernel_shared_binary_with_cpp=false`, `refinement_proved=false`
- [ ] `golden lock: PASS` reports **422 / 215** and EdgeCross **20/20**
- [ ] If Coq or harness emit changed: `FV_COQ=1` (or the three `coqc` targets) green
- [ ] If vector counts or splits changed: lock bumped in the **same** task as regen; discrepancies catalog updated
- [ ] No honesty flag set to `true`
- [ ] STATUS / CURRENT_STATE living counts still match the lock
