# Formal verification freeze (2026-08-18)

**This cycle is closed.** Maintenance mode only: fix regressions. Do not expand
scope (new vectors, new Coq theories, new CLIs, refinement, shared production
hash) without a new task.

**Does not supersede** [FORMAL-VERIFICATION-VERDICT.md](./FORMAL-VERIFICATION-VERDICT.md)
on what the Coq model proves. Current correspondence status:
[FORMAL-VERIFICATION-STATUS.md](./FORMAL-VERIFICATION-STATUS.md).
Classification freeze snapshot (citable RC0 declaration §0):
[RIGOR_FORMAL_VERIFICATION_CURRENT_STATE.md](../formal/RIGOR_FORMAL_VERIFICATION_CURRENT_STATE.md).

---

## Freeze date

2026-08-18 (operator scope freeze). Living RC0 FV freeze: 2026-08-20 (historical verification record).

## Honesty (must stay false)

```
equivalence_claimed = false
hash_kernel_shared_binary_with_cpp = false
refinement_proved = false
```

Lock: `formal_verification/coq/DrawBridgeProgress.v`
(re-export of `formal_verification/coq/Phase5Equivalence/DrawBridgeProgress.v`).

## Baseline commands

From this repository (primary operator entry):

```bash
# from repository root
bash formal_verification/scripts/run_fv_release_checklist.sh
```

That is freeze artifacts plus `SKIP_TEST_BITCOIN=1` `run_differential_gate.sh`. A PASS line
must print the three honesty flags as **false**. Table: [FV-RELEASE-CHECKLIST.md](./FV-RELEASE-CHECKLIST.md).

Direct gate (same correspondence checks, no freeze-artifact wrapper):

```bash
bash formal_verification/scripts/run_differential_gate.sh
```

Optional:

```bash
bash formal_verification/scripts/run_differential_gate.sh --help
SKIP_TEST_BITCOIN=1 bash formal_verification/scripts/run_differential_gate.sh
FV_COQ=1 bash formal_verification/scripts/run_fv_release_checklist.sh   # Coq 8.15.0
bash formal_verification/scripts/check_freeze_artifacts.sh
```

Manual rebuild of the test CLIs (the gate already does this):

```bash
bash formal_verification/scripts/build_cpp_hash_cli.sh
bash formal_verification/scripts/build_cpp_sigma_root_cli.sh
```

## Frozen baseline (what a green gate means)

The 2026-08-18 operator freeze recorded **324** shared / **117** Coq-enforced. Living lock (historical verification record, 2026-08-20): **422** shared / **215** Coq-enforced. RC0 FV freeze (historical verification record) recorded **414 / 207**. Lineage: docs freeze historical verification record was then **376 / 169**; InvalidCvg **390 / 183**; ScriptMintNeg **402 / 195**; EdgeCross **414 / 207**; densify **422 / 215**. Honesty flags were not flipped.

| Check | In gate |
|-------|---------|
| Python oracle on the living shared set (**422**) | step 1 |
| Generated artifacts match working tree | step 2 |
| Golden lock (422/215; NegativeCore 16; InvalidCvg 14; ScriptMintNeg 12; DCR-024…035; EdgeCross 20; named `surface_splits`) | step 2b |
| Tripartite hash (`cpp_hash_cli` × hashlib × extracted Coq when present) | step 3 |
| Production σ / ConvergenceRoot CLI vs `SigmaEncoder` / `SigmaPoW01` / `ConvergenceRoot` | step 4 |
| Coq harness rebuild (**215** lemmas if `COQ=1`) | step 5 if `COQ=1` |
| BOOST `test_bitcoin` | step 6 if binary present |

It does **not** mean refinement, a shared production hash binary, SIMD equality,
network/mempool/P2P coverage, `ComputeConvergenceRootFromConvergenceSet`,
`IsResScript` vs `out_asset` (finite script *shape* **is** on `apply_tx` / accepted
`apply_block`), or coinbase bind of `draw_candidate_root`.

## Maintenance mode

Allowed without a new task: fix a red gate, stale generated artifacts, or a
docs contradiction with STATUS/Verdict.

Not allowed without a new task: new golden-vector campaigns, new Coq
developments, new CLIs, Coq extract of encoder/root, SIMD extra gates,
production extracted hash in `rigord`, flipping honesty flags.

R2/R3 dated notes are historical snapshots, not the freeze page.
