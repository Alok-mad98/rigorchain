# SECURITY-001-A — Working Tree Diff Attribution

**Date:** 2026-08-02
**HEAD:** `2b3354be3e` on `proof034-pre-final-mining-checkpoint`
**Purpose:** determine which of the 127 modified files belong to SECURITY-001-A and which
belong to other in-flight subtasks, so that a future commit can be scoped correctly.

**Method:** hunk-level classification. A hunk is attributed to SECURITY-001-A if its added or
removed lines reference `SECURITY-001-A`, `A-6`, `RES_PROBABILITY_TARGET`, `RFC-014 v1.1`,
`MIN_CONVERGENCE_AMOUNT`, or the corrected constant `2af31dc4611873…`. Files where the
keyword heuristic gave an ambiguous answer were read manually; those judgements are marked.

**Headline:** the working tree is **not** cleanly separable by file. One file has genuinely
mixed ownership and must be split by hunk. Three tracked files are deleted, and two classes
of file should never have been tracked at all.

---

## 1. Summary

| Bucket | Files | Commit scope |
|--------|-------|--------------|
| **A** — SECURITY-001-A | 42 modified | commit as SECURITY-001-A |
| **A-new** — SECURITY-001-A, untracked | 8 new | commit as SECURITY-001-A |
| **F** — SECURITY-001-F (formal verification) | 32 modified, 3 deleted, 2 new | separate commit |
| **E/pool** — Rust pool stack | 26 modified, 7 new | separate commit |
| **W** — wallet asset routing | 4 modified | separate commit |
| **MIXED** — requires hunk-level split | 1 | see §5 |
| **NOISE** — should not be tracked | 6 | see §6 |
| **UNRELATED** — release/ops evidence, untracked | 26 dirs/files | out of scope |

`git diff --stat` total: `127 files changed, 4614 insertions(+), 2573 deletions(-)`.
Untracked: 43 entries. Deleted: 3.

---

## 2. Bucket A — SECURITY-001-A (commit these together)

### Consensus code

| File | Findings |
|------|----------|
| `core/src/consensus/params.h` | A-6 — constant + non-vacuity note |
| `core/src/consensus/convergence_logic.cpp` / `.h` | A-1…A-4 |
| `core/src/consensus/convergenceres.h` | A-1…A-4 |
| `core/src/consensus/res_proof.cpp` / `.h` | A-1…A-4 |
| `core/src/consensus/rigor_math.h` | A-6 comment |
| `core/src/consensus/rigor_res.cpp` / `.h` | A-1…A-4 + A-6 comment |
| `core/src/consensus/selection.h` | A-6 comment |
| `core/src/kernel/chainparams.cpp` | A-6 — six network assignments |
| `core/src/validation.cpp` | A-1…A-4 (`bhash`/`target_prev` binding) + A-6 comment |
| `core/src/script/interpreter.h` | A-6 comment only (1 line) |
| `core/src/rpc/blockchain.cpp` | A-6 RPC doc strings (+1 unrelated `ORUM`→`ORU` typo fix, harmless) |

### Tests

`core/src/test/canonical_layer_tests.cpp`, `convergence_consensus_tests.cpp`,
`res_proof_math_tests.cpp`, `selection_golden_tests.cpp`

The "other-hunk" counts on these are a heuristic artifact: the re-weighted candidate amounts
(`100000 * COIN`) are A-6 work but contain none of the keywords. Manually confirmed as A.

### Formal verification owned by A

`formal_verification/coq/Selection.v`, `Kernel/KernelClosure.v`, `Kernel/KernelEligibility.v`,
`Proofs/Correctness.v`, `spec/rigor_consensus_spec.py`, `rigor_golden_vectors.json`

### Ops / freeze

`sim/freeze/freeze_integrity_checker.py`, `core/contrib/ops/closed-testnet/scripts/day0-smoke.sh`,
`explorer-indexer-mvp/src/importer.ts`

### Documents

`CONV_T_GREP_GATE_EVIDENCE.md`, `H_CONV_T_DIFF_REVIEW_001.md`, `CONV_T_ACTIVATION_DIFF_REPORT.md`,
`FORMAL_DELTA.md`, `RELEASE_BASELINE_MANIFEST.md`, `RELEASE_MANIFEST_VALIDATION.md`,
`RFC_FREEZE_INDEX.md`, `docs/rfcs/RFC-014-frozen-convergence-target.md`,
`docs/rfcs/RFC-007-formal-verification-coq.md`, `docs/freeze/H_RES_003_ARCHITECTURE_VERDICT.md`,
`docs/rigor-qt/FUTURE_GUI_RPC_SPEC.md`, and four `docs/formal/**` alignment audits.

### New files (untracked) belonging to A

```
docs/rfcs/RFC-014-v1.1.md
docs/security/SECURITY-001-A-FROZEN-BASELINE.md
docs/security/SECURITY-001-A5-DESIGN.md
docs/security/SECURITY-001-A-FINAL-REPORT.md
docs/security/SECURITY-001-A-DIFF-SCOPE.md          (this file)
docs/formal/GOLDEN_VECTOR_REGENERATION_REPORT.md
docs/formal/SECURITY-001-A6-COQ-REPORT.md
docs/migration/CHAIN_REGENERATION_POLICY.md
formal_verification/tools/regenerate_golden_vectors_a6.py
```

Note `docs/security/` and `docs/migration/` show as untracked *directories* — they contain
`TASK_SECURITY_001_A_REPORT.md` and the files above.

---

## 3. Bucket F — SECURITY-001-F (do not mix with A)

`formal_verification/fuzz/differential_fuzzer.py` was initially misattributed to A by the
keyword heuristic. Its own rewritten header settles it:

> `HISTORY / WHY THIS FILE WAS REWRITTEN (SECURITY-001-F, finding F-2)`

299 added / 340 removed — a full rewrite that introduces the `RIGOR_CPP_HARNESS` gating and
the honest "C++ leg not wired" reporting. **Bucket F.**

Also F:

- `formal_verification/gen_coq_harness.py`, `coq/Kernel/GoldenHarness.v`
- `coq/_CoqProject` — adds `Kernel/GoldenHarness.v` to the build. This is a genuine
  improvement: the golden harness is now *enforced* by the build rather than being an
  orphan file, which is why it recompiled during the A-6 rebuild and why its lemmas
  constitute real evidence.
- `coq/rigor.ml`, `coq/rigor.mli` — extraction output
- `coq/Phase4FConsensus/**` (4 files), `coq/Phase5Equivalence/**` (24 files),
  `coq/ReplayReorgCore/ReplayInvariants.v`, `SupplyConservation.v`,
  `coq/ConnectBlock/ConnectBlockSupply.v`, `coq/RigorBytesOrder.v`
- New: `formal_verification/tools/audit_assumptions.py`, `tools/check_vacuous.py`

### Deleted tracked files (F)

```
 D formal_verification/coq/Makefile.coq
 D formal_verification/coq/Makefile.coq.conf
 D formal_verification/coq/.Makefile.coq.d
```

These are `coq_makefile` output, superseded by the checked-in `CoqMakefile`. Deleting them is
correct — generated makefiles should not be tracked — but the deletion belongs in F's commit,
not A's, and `.gitignore` should be extended so they do not return.

---

## 4. Buckets E/pool and W — unrelated to A

**Pool (Rust), 26 modified + 7 new:** all of `pool/block-assembler`, `pool/mint-engine`,
`pool/stratum-server`, `pool/winner-engine`, `pool/pilot`, `pool/rigor-pool-pg`,
`pool/share-validator`. New files include `payout_script.rs`, `auth.rs`, `vardiff.rs`,
`leader_fence.rs`, `security_config.rs`, `mempool_preflight.rs`, `submit.rs`.

Relevant to A only in one direction: if A-5 is ever approved, `payout_script.rs` and
`stratum-server/src/payout.rs` are directly affected. Nothing to do now.

**Wallet, 4 modified:** `core/src/wallet/asset_domain.cpp`, `wallet/rpc/coins.cpp`,
`wallet/spend.cpp`, `wallet/wallet.cpp` — asset-type routing and change-type selection.

**Other:** `core/src/release/production_release_manifest.json`,
`core/test/functional/test_framework/rigor_block_builder.py`,
`docs/branding/RIGOR_PRODUCT_NAMING.md`, `docs/mining/RES_BODY_MINT_OPERATIONAL_SPEC.md`,
`docs/wallet/RIGOR_WALLET_DELTA_MAP.md`, `formal_verification/README.md`.

---

## 5. MIXED — must be split by hunk

### `core/src/wallet/rpc/rpcspend.cpp`

| Hunk | Owner | Content |
|------|-------|---------|
| `CCoinControl coin_control_asset` block + `SendMoney` call site | **W (wallet)** | asset-type routing, BECH32M change for RES |
| `coin_control.m_asset_type = ASSET_RES` in the mint path | **W (wallet)** | same work |
| `convergence_amount` RPC doc string | **A** | states the SECURITY-001-A exact-match rule |
| `target_prev` RPC doc string | **A** | A-6 constant correction |

The two documentation hunks are A; the three behavioural hunks are wallet work. Split with
`git add -p`, or accept the file into whichever commit lands first and note the overlap.

---

## 6. NOISE — files that should not be tracked

These are unrelated to any subtask but are polluting every diff and should be handled
separately from the remediation.

| Path | Problem |
|------|---------|
| `core/src/compute_rfc001_vectors` | **A tracked ELF-64 executable, unstripped, with debug_info.** A compiled binary in version control. |
| `docs/release/evidence/release006/M-08/.pgdata/pg_stat_tmp/db_0.stat` | PostgreSQL runtime statistics temp file |
| `…/pg_stat_tmp/db_13759.stat` | same |
| `…/pg_stat_tmp/db_16384.stat` | same |
| `…/pg_stat_tmp/db_16385.stat` | same |
| `…/pg_stat_tmp/global.stat` | same |

The `.pgdata` files are owned by `root`, are rewritten by a live PostgreSQL instance, and
change timestamp on every diff. A committed `.pgdata` directory may also contain database
contents that were never intended for the repository — worth a look before the next commit.

None of these paths are covered by `.gitignore`. Recommended additions:

```gitignore
core/src/compute_rfc001_vectors
**/.pgdata/
formal_verification/coq/Makefile.coq
formal_verification/coq/Makefile.coq.conf
formal_verification/coq/.Makefile.coq.d
```

---

## 7. Recommended commit sequence

Not executed. No commit has been made.

1. **Hygiene** — extend `.gitignore`; `git rm --cached` the ELF binary and `.pgdata` tree.
2. **SECURITY-001-F** — Coq corpus, harness wiring, fuzzer rewrite, generated-makefile deletions.
3. **SECURITY-001-A** — bucket A + the two doc hunks from `rpcspend.cpp`.
4. **Wallet** — asset routing including the three behavioural hunks of `rpcspend.cpp`.
5. **Pool** — Rust stack.

A before F is also defensible; F's `_CoqProject` change is what puts `GoldenHarness.v` into
the build, and A's report cites the harness recompiling as evidence, so F-then-A keeps the
evidence chain readable in history.
