# Formal verification status (freeze)

**Date**: 2026-08-18 freeze baseline; living lock 2026-08-20 (historical verification record, 422/215) — RC0 FV freeze then 414/207; honesty unchanged from 2026-08-16  
**Role**: **Single current status page** for the correspondence boundary. Pre-release checks: [FV-RELEASE-CHECKLIST.md](./FV-RELEASE-CHECKLIST.md). Operator freeze: [FORMAL-VERIFICATION-FREEZE.md](./FORMAL-VERIFICATION-FREEZE.md). Citable freeze declaration: [RIGOR_FORMAL_VERIFICATION_CURRENT_STATE.md](../formal/RIGOR_FORMAL_VERIFICATION_CURRENT_STATE.md) §0.  
**Does not supersede** [FORMAL-VERIFICATION-VERDICT.md](./FORMAL-VERIFICATION-VERDICT.md) on *what the Coq model proves*.

Dated round notes ([R2](./FORMAL-VERIFICATION-STATUS-2026-08-16-R2.md), [R3](./FORMAL-VERIFICATION-STATUS-2026-08-16-R3.md)) are **historical** snapshots, not the freeze page. Tightening: [FORMAL-VERIFICATION-TIGHTENING-2026-08-18.md](./FORMAL-VERIFICATION-TIGHTENING-2026-08-18.md). Assessments: [SHARED-HASH-FEASIBILITY-2026-08.md](./SHARED-HASH-FEASIBILITY-2026-08.md), [LIGHTWEIGHT-CORRESPONDENCE-SIGMA-ROOT-2026-08.md](./LIGHTWEIGHT-CORRESPONDENCE-SIGMA-ROOT-2026-08.md). σ/root CLI: [SIGMA-ROOT-CLI-2026-08.md](./SIGMA-ROOT-CLI-2026-08.md).

---

## Honest status (one line)

The Coq **model** is strongly verified; **correspondence** is a finite differential gate (**422** shared vectors, **215** Coq-enforced, tripartite hash, production σ/root CLI); this is **not** a refinement proof. The 2026-08-18 operator freeze recorded 324 / 117. RC0 FV freeze recorded 414 / 207.

`equivalence_claimed` remains **false**.  
`hash_kernel_shared_binary_with_cpp` remains **false**.  
`refinement_proved` remains **false**.

Canonical product tree: [`https://github.com/rigorchain/rigorchain`](https://github.com/rigorchain/rigorchain).
Wallet is SQLite (`rigor-wallet.sqlite`), not Berkeley DB. The C ABI
`libbitcoinconsensus` is **deleted**, not renamed — do not document
`librigorconsensus`. Identity sync: [`../security/RIGOR_FORMAL_VERIFICATION_IDENTITY_SYNC_P0_029.md`](../security/RIGOR_FORMAL_VERIFICATION_IDENTITY_SYNC_P0_029.md).

Coq lock: `formal_verification/coq/DrawBridgeProgress.v` (re-export of
`formal_verification/coq/Phase5Equivalence/DrawBridgeProgress.v`).

---

## Operator entry

Primary command (from this repository):

```bash
# from repository root
bash formal_verification/scripts/run_fv_release_checklist.sh
```

That is freeze artifacts plus `SKIP_TEST_BITCOIN=1` `run_differential_gate.sh` (lock check is gate step 2b). A green run reports `FV release checklist: PASS` and the three honesty flags as **false**. Full table: [FV-RELEASE-CHECKLIST.md](./FV-RELEASE-CHECKLIST.md). Direct gate: `bash formal_verification/scripts/run_differential_gate.sh`. Help: `bash formal_verification/scripts/run_differential_gate.sh --help`.

Pre-mainnet **security** gates (window / solo / maturity functional suites) are a separate operator page: [../security/PRE-MAINNET-CHECKLIST.md](../security/PRE-MAINNET-CHECKLIST.md). This FV status page remains maintenance-only and does not run those suites.

---

## Freeze (this cycle is closed)

**Maintenance mode only:** fix regressions. Do not expand scope (new vectors, new Coq theories, new CLIs, refinement, shared production hash) without a new task.

The 2026-08-18 operator freeze remains the scope gate. Dated 2026-08-20 model-core tasks, the RC0 FV freeze (historical verification record, then **414 / 207**), and selective densify (historical verification record) were new tasks; they did **not** flip honesty flags. Living counts: **422** / **215**. Citable declaration: [CURRENT_STATE §0](../formal/RIGOR_FORMAL_VERIFICATION_CURRENT_STATE.md).

---

## Gates at a glance

| Gate | What it is | Not |
|------|------------|-----|
| Differential (`run_differential_gate.sh`) | Python oracle, generated artifacts, hash, σ/root | Refinement |
| Tripartite hash (step 3) | Portable `CSHA256` × hashlib × extracted Coq | Shared production hash; SIMD |
| σ/root CLI (step 4) | Production `ComputeSigmaHash` + `ComputeConvergenceRootFromEligibilityTxids` vs existing JSON families | Coin-view wrapper; Coq extract of encoder/root |

Living (2026-08-20 + historical verification record): **422** shared vectors; **215** Coq-enforced; σ/root CLI in the gate. The RC0 FV freeze recorded 414 / 207. The 2026-08-18 operator freeze recorded 324 / 117. Optional `COQ=1` rebuilds the harness; optional `test_bitcoin` BOOST if a binary is present.

**Explicit non-claims:** no Coq⇄C++ refinement; no shared production hash binary in `rigord`; no network / mempool / P2P layer; no grinding-cost theorem about real SHA-256.

---

## Tightening 2026-08-18

Small maintenance pass. Honesty flags were not touched.

1. **Replay-model fix (mandatory).** Created outputs are now `(tx_id, vout)` with `vout` starting at 0 per transaction. The shared `hash32_placeholder` + running-index shortcut is gone, so distinct transactions no longer collide and spend-deletion is unconditional when `outpoint_txid op <> tx_id tx` (`utxo_spend_removes_output_distinct_txid`). The fully universal statement remains false only under **txid reuse** (BIP30-class), which is now the stated residual — not a modelling shortcut.
2. **Nine high-risk golden vectors** (315 → **324** shared; 108 → **117** Coq-enforced): window+reorg repay/forks, W=32 exclusion edges, permutation no-double-win, `f=0` ineligibility, encoder height `W`.
3. **Gate robustness.** `run_differential_gate.sh` prints vector counts, regenerator hints on mismatch, and the three honesty flags on PASS. `COQ=1` also compiles `ReplayReorgCore/ReplayInvariants.v`.

---

## Test-only σ / root CLI (2026-08-18)

Gate step 4 builds `cpp_sigma_root_cli` over production `ComputeSigmaHash` and `ComputeConvergenceRootFromEligibilityTxids` (portable Transform). Existing `SigmaEncoder` / `SigmaPoW01` / `ConvergenceRoot` families must match. This does **not** replace BOOST, does **not** cover the coin-view wrapper, and does **not** flip honesty flags. Note: [SIGMA-ROOT-CLI-2026-08.md](./SIGMA-ROOT-CLI-2026-08.md).

---

## What has been proved in Coq (high-level)

All of the following are theorems about the **Coq model**, with a zero-axiom discipline on the protocol development (`Print Assumptions` → Closed under the global context). They do not by themselves prove that `core/src/**` matches the model.

| Area | Where | What it is |
|------|--------|------------|
| Executable SHA-256 / SHA-256d | `RigorSha256.v`, `RigorHash.v`, `CanonicalHashKernel.v` | Byte-level hash in Coq; FIPS short messages and padding edges; SHA-256d is `sha256 ∘ sha256` |
| Draw encoder layout | `CanonicalDrawEncoder.v` | 100-byte σ preimage; offsets 0/32/64/96; LE32 height |
| Selection determinism / kernel | `Proofs/Determinism.v`, `Kernel/*` | At most one winner for given inputs; kernel wrappers |
| A-9-C no-double-win (model) | `WindowExclusion.v` | `already_paid` exclusion; `txid_wins_at_most_once` on `paid_closed_trace` |
| Draw candidate root (model) | `DrawCandidateRoot.v` | Eligible-subset hash; e2e with Selection/Window; coinbase bind is C++. See 2026-08-20 addenda. |
| Replay / supply (model) | `ReplayReorgCore/*`, `Phase4FConsensus/*` | Deterministic replay and supply lemmas *in the model*; created outpoints use `tx_id` |
| Golden harness | `Kernel/GoldenHarness.v` + `Kernel/NegativeCoreHarness.v` | **215** vectors recomputed by `vm_compute` (187 kernel + 28 NegativeCoreHarness) |

Fairness remains **idealized** (`Proofs/FairnessIdealized.v`): not a theorem about real SHA-256.

The Verdict lists the full theorem inventory. Do not read Phase-5 freeze catalogs (`DCS_Impossible`) as having been flipped; they still correctly say *full* Coq⇄C++ refinement is not closed.

---

## Golden vectors and differential status

| Layer | Count / check | Role |
|-------|----------------|------|
| Shared JSON | **422** vectors (`phase3_golden_vectors.json`, tag `tightening-replay-txid-2026-08-18`) | Python + generated C++ cases |
| Coq-enforced | **215** `vm_compute` lemmas | Cheap families: Inequality, WindowExclusion, SigmaEncoder, DrawCandidateRoot (incl. negatives), short HashKernel, **NegativeCore**, **InvalidCvg**, **ScriptMintNeg**, **EdgeCross** |
| FIPS SHA-256 | `CanonicalHashKernel.v` + `Sha256Fips` JSON | Padding edges including 55/56/63/64/65 |
| Python oracle | `tests/test_phase3_oracle.py` | Full 422 |
| Artifact freshness | gate step 2 | Generators vs **working tree** (not `git diff` HEAD) |

Intentional splits (Coq-enforced vs full set, hex-filter vs `FinalizeWindowedDrawSet`) are recorded in [GOLDEN-VECTOR-DISCREPANCIES.md](./GOLDEN-VECTOR-DISCREPANCIES.md). Growing a finite set is not equivalence.

---

## Tripartite hash check (scope)

**Entry**: `bash formal_verification/scripts/run_differential_gate.sh` (step 3 hash; step 4 σ/root).

Three implementations are compared bit-for-bit on HashKernel / Sha256Fips golden vectors plus a random sample:

| Side | Artifact | What it is |
|------|----------|------------|
| Coq | `extracted/hash256_cli` (from `RigorSha256`) | Extracted Coq kernel (OCaml). TCB: extractor, `ocamlopt`, `nat` as `int`. |
| C++ portable | `extracted/cpp_hash_cli` | Production `core/src/crypto/sha256.cpp`, **C++ Transform only** (no SSE/SHANI). SHA-256d = SHA256(SHA256(x)), same composition as `CHash256`. |
| Python | `hashlib` | Independent OpenSSL-backed hash. |

A split among these layers **fails the gate**. That is automated divergence detection, not a shared C++/Coq object and not a proof of equality on all inputs.

**SIMD** (SSE4 / SHANI / AVX2) is **out of this check**. Those paths must still match the C++ Transform via Bitcoin Core `SHA256AutoDetect` / `SelfTest`.

### Portable CLI vs `test_bitcoin`

| Check | Covers | Does not cover |
|-------|--------|----------------|
| Gate step 3 (`cpp_hash_cli`) | `CSHA256` portable Transform on FIPS + HashKernel + random sample | BOOST tests, SIMD |
| Gate step 4 (`cpp_sigma_root_cli`) | Production `ComputeSigmaHash` + `ComputeConvergenceRootFromEligibilityTxids` on `SigmaEncoder` / `SigmaPoW01` / `ConvergenceRoot` | Coin-view `ComputeConvergenceRootFromConvergenceSet`, winner/window, SIMD, Coq extract of encoder/root |
| `test_bitcoin --run_test=selection_golden_tests` | BOOST cases compiled into that binary: encoder layout, A-9-C hex filter, σ, SHA-256d, and **`sha256_fips_golden` if the binary is rebuilt after that test was added** | Anything not linked into that binary |
| `test_bitcoin --run_test=convergence_consensus_tests` | Window builder / `already_paid` at the consensus-logic level | Hash FIPS |

If the gate prints **`test_bitcoin` STALE (no `sha256_fips_golden`)**:

- Hash FIPS bytes are **already** checked by the portable CLI. The gate may still PASS.
- σ / ConvergenceRoot golden families are **already** checked by step 4 (`cpp_sigma_root_cli`). They do not wait on a rebuilt `test_bitcoin`.
- BOOST `sha256_fips_golden` is **not** in that binary. Rebuild `test_bitcoin` (operator full `make` in `core/`) to cover it.

```bash
cd core
# operator: make -j$(nproc)
./src/test/test_bitcoin --run_test=selection_golden_tests,convergence_consensus_tests
```

Skip the optional BOOST suites: `SKIP_TEST_BITCOIN=1 bash formal_verification/scripts/run_differential_gate.sh`.

---

## Remaining gaps (explicit non-claims)

1. **No refinement proof** — no mechanized `Coq function = C++ function`.
2. **No shared C++ hash binary** — `cpp_hash_cli`, `cpp_sigma_root_cli`, and `hash256_cli` are test drivers.
3. **No grinding-cost / adversarial-advantage theorem** about real SHA-256.
4. **No SHA-256 uniformity** — fairness stays idealized.
5. **Network / mempool / P2P** are not in the model.
6. **Phase-5 catalogs** still report full equivalence closure as impossible.
7. **Replay residual** — spend-deletion is still refutable under *txid reuse* (a transaction whose `tx_id` equals a spent outpoint's txid). That is no longer a shared-placeholder artefact.
8. **Script classification** — Abstract CVG2 container is modeled (`InvalidCvg.v`) and is **not** wired into `apply_tx`. Witness v0/v1 / `IsResScript` / spend-CVG2 / `bad-rigor-script-type` remain C++ + docs. Replay `out_script` is opaque and unread (`we_do_not_claim_script_derives_asset`).
9. **Coinbase / Merkle / header bind of `draw_candidate_root`** — Coq hashes the eligible list (`draw_candidate_root_scope_is_eligible_digest`); inserting the digest into coinbase / Merkle / header is C++.
10. **Abstract `cm_draw_won`** — not the Selection winner, not owner/PCORB (`we_do_not_claim_cm_draw_won_is_owner_pcorb`).
11. **`rs_res_supply` scalar** — not the UTXO RES projection (`we_do_not_claim_rs_res_supply_equals_utxo_res`).

Architecture freeze snapshot: [RIGOR_FORMAL_VERIFICATION_CURRENT_STATE.md](../formal/RIGOR_FORMAL_VERIFICATION_CURRENT_STATE.md) §8 / §9.

---

## Mid-term assessments (2026-08)

Honesty flags were **not** flipped. Full notes: [SHARED-HASH-FEASIBILITY-2026-08.md](./SHARED-HASH-FEASIBILITY-2026-08.md), [LIGHTWEIGHT-CORRESPONDENCE-SIGMA-ROOT-2026-08.md](./LIGHTWEIGHT-CORRESPONDENCE-SIGMA-ROOT-2026-08.md), [SIGMA-ROOT-CLI-2026-08.md](./SIGMA-ROOT-CLI-2026-08.md).

Operator summary:

- Production shared Coq/C++ hash binary: **reject**. Extract is OCaml; node uses `SHA256AutoDetect` SIMD; flag stays false.
- Tripartite portable `CSHA256` × hashlib × extracted Coq: **keep** (finite, not refinement).
- SIMD extra hash gate: **defer** (`SelfTest()` already binds Transform).
- Lightweight σ / root correspondence: **test-only CLI is in the gate** (`cpp_sigma_root_cli`, step 4). Finite golden families only. **Not** a refinement. See [SIGMA-ROOT-CLI-2026-08.md](./SIGMA-ROOT-CLI-2026-08.md).
- Do **not** treat `ComputeConvergenceRootFromConvergenceSet` as the pure function (coin view).
- Residual: finite vectors + BOOST for window/`already_paid`; Coq encoder/root extract still deferred. Stale `test_bitcoin` no longer blocks σ/root for the JSON families.

---

## Operator reproducibility

From this repository:

**1. Coq targets** (Coq 8.15.0):

```bash
cd formal_verification/coq
coq_makefile -f _CoqProject -o Makefile.coq
make -f Makefile.coq -j$(nproc)
```

Minimum bridge lock: `CanonicalHashKernel.v`, `Kernel/GoldenHarness.v`, `Phase5Equivalence/DrawBridgeProgress.v`, `ReplayReorgCore/ReplayInvariants.v`.

**2. Pre-release checklist** (needs Python 3, `g++`; OCaml optional but used if `ocamlopt` is present):

```bash
bash formal_verification/scripts/run_fv_release_checklist.sh
# optional: FV_COQ=1  FV_EXTRACT=1  FV_TEST_BITCOIN=1
```

That wraps freeze artifacts plus `SKIP_TEST_BITCOIN=1` `run_differential_gate.sh`. Table: [FV-RELEASE-CHECKLIST.md](./FV-RELEASE-CHECKLIST.md). Direct gate: `bash formal_verification/scripts/run_differential_gate.sh`. Help: `bash formal_verification/scripts/run_differential_gate.sh --help`

**3. Boundary** — this file plus [FORMAL-VERIFICATION-FREEZE.md](./FORMAL-VERIFICATION-FREEZE.md) and the release checklist. Do not infer equivalence from a green gate.

**4. Key-file check** (safe from any cwd; do not `cd` into `coq/` and then `ls formal_verification/coq/...`):

```bash
bash formal_verification/scripts/check_freeze_artifacts.sh
```

---

## Addendum — 2026-08-20 (historical verification record)

Honesty flags were **not** flipped. This is a dated model expansion, not a new freeze.

- Coq `DrawCandidateRoot.v` is now a core kernel component, symmetric with `ConvergeRoot.v`: SHA-256d of sorted concatenation, **no** `N < 2` zero shortcut.
- Proved (model): length 32, empty set ≠ zeros, order independence, eligible subset ⊆ F, agreement with `C` when |F| ≥ 2. Windowed/A-9-C form in `WindowExclusion.v`. `select_winner_order_independent` is unchanged.
- Shared JSON **324 → 332**; Coq-enforced harness **117 → 125** (eight `DCR-*` rows). Tag unchanged (`tightening-replay-txid-2026-08-18`).
- Status of this component: **modeled + proven properties** (was runtime-tested only). Still not a C++ refinement.

Living counts: [`../formal/RIGOR_FORMAL_VERIFICATION_CURRENT_STATE.md`](../formal/RIGOR_FORMAL_VERIFICATION_CURRENT_STATE.md).

---

## Addendum — 2026-08-20 (historical verification record)

Honesty flags were **not** flipped.

- Integration lemmas: winner is a member of the hashed eligible set (`winner_in_draw_eligible_set`, `execute_winner_in_draw_eligible`); same candidate set ⇒ same root and winner (`draw_and_winner_order_independent`); A-9-C root is stable under permutation of candidates and of the paid list; all-paid exclusion yields the empty-concat root.
- Strengthened `|eligible| ≥ 2` relation: `draw_candidate_root` equals `ConvergeRoot` *of the eligible subset* (`draw_candidate_root_is_C_of_eligible_when_large`); they agree on F itself only when everyone is eligible.
- Golden: **332 → 340** shared; **125 → 133** Coq-enforced (`DCR-008`…`015`: incomplete window, genesis empty, all-paid exclusion, 3-candidate perms, σ/winner e2e, subset perm). `select_winner_order_independent` unchanged.

---

## Addendum — 2026-08-20 (historical verification record)

Honesty flags were **not** flipped.

- End-to-end model theorems: `draw_pipeline_e2e` (eligible list, DCR, and winner aligned); `execute_e2e_aligned` / `kernel_draw_e2e_aligned` (Execute uses `ConvergeRoot F` as σ’s C, hashes the same eligible list); `exclusion_e2e_consistent` and `exclusion_e2e_order_independent` (A-9-C). `select_winner_order_independent` unchanged and Closed.
- Coinbase / RPC commitment of `draw_candidate_root` remains **implementation-level** (C++ `ComputeConvergenceDrawCandidateRoot`). The Coq digest is not a coinbase-bind theorem.
- Golden: **340 → 348** shared; **133 → 141** Coq-enforced (`DCR-016`…`023`: same-DCR winner flip via seed, C-change winner flip, A-9-C drop-winner / singleton C=0, T_one empty eligible, height flip).

---

## Addendum — 2026-08-20 (historical verification record)

Honesty flags were **not** flipped.

- Negative golden family `NegativeCore` (`NEG-000`…`015`): ordinary RES inflation, ORU→RES / RES→ORU mixing, mint `W(h)±1` and two-output mint, genesis-W at epoch 1, winner mint-count 0/2 (H-1), hard-cap one-atom overshoot and mint-at-cap, schedule-exhausted mint, without-winner mint, mixed ordinary + illegal mint bodies.
- Python oracle evaluates the same dual-pool / mint-shape / cap / obligation predicates as Coq `StateTransition` / `ConnectBlockMint`. Mixed-body rows also run L1 `apply_block_model`.
- Coq: `Kernel/NegativeCoreHarness.v` (16 cheap `vm_compute` lemmas). `apply_block` itself is not executed in Coq (`hash32_eq_dec` is opaque); the harness pins the dual-pool/cap/obligation booleans that make those blocks fail.
- Golden: **348 → 364** shared; **141 → 157** Coq-enforced.

---

## Addendum — 2026-08-20 (historical verification record)

Honesty flags were **not** flipped.

- Negative/perturbation DCR family `DCR-024`…`035` (12 Coq-enforced rows) contrasts the 24 positive `DCR-000`…`023` vectors: candidate-set tamper (insert/delete/replace), eligibility-flag permutation, permutation of a tampered set, duplicate txids (not collapsed), empty/singleton DCR ≠ `N<2` zeros / C, A-9-C hashing full F while selecting the unpaid winner, and σ-input perturbation (1-bit-flipped `C`, flipped `B_{h-1}`, flipped `LE32(h)`).
- Golden: **364 → 376** shared; **157 → 169** Coq-enforced.

---

## Addendum — 2026-08-20 (historical verification record)

Honesty flags were **not** flipped. This is a documentation freeze of the model-core closed loop, not a new proof.

- `docs/formal/RIGOR_FORMAL_VERIFICATION_CURRENT_STATE.md` is the architecture freeze snapshot: Classification table, Gap/Roadmap completed items, and the §8 guaranteed / not-guaranteed boundary (**refinement**, **script**, **coinbase bind** of `draw_candidate_root` remain **not** guaranteed).
- `formal_verification/README.md` aligned: G-1 / G-2 / file count **207**; σ encoder is LE32 height / no `target_prev`; tie-break is txid memcmp (not XOR); inequality is `σ < T · f`.
- Living counts at that addendum: 376 shared / 169 Coq-enforced (then-current). Living lock after EdgeCross / RC0 freeze: **414 / 207**. Honesty flags remain **false**.

---

## Addendum — 2026-08-20 (historical verification record)

Honesty flags were **not** flipped.

- Negative family `InvalidCvg` (`ICVG-000`…`013`, 14 Coq-enforced rows) complements DCR-024…035: invalid Convergence-tx abstract form (amount &lt; 1 ORU, nValue=0, RES-tagged CVG2, RES2 / P2WPKH script, two CVG2 outputs), committed `C` ≠ recomputed, unsorted / truncated / extra-byte concat, claimed root length ≠ 32, duplicate txids in F (`C([a,a])` is not N&lt;2 zeros), and N&lt;2 claims that bind the DCR digest as `C`.
- Coq: `InvalidCvg.v` (abstract CVG2 container; not a script interpreter) + `Kernel/GoldenHarness.v`. Python oracle `eval_invalid_cvg`.
- Remaining P0-4-adjacent: P1-1 full script classification (`IsResScript`, spend-CVG2, `bad-rigor-script-type`); coinbase bind of C/DCR is still C++.
- Golden: **376 → 390** shared; **169 → 183** Coq-enforced.

---

## Addendum — 2026-08-20 (historical verification record)

Honesty flags were **not** flipped.

- Negative family `ScriptMintNeg` (`SMW-000`…`011`, 12 Coq-enforced rows) locks abstract script / mint-witness reject form while classification still lives mainly in C++: Witness v0 claimed as RES, v1 RES2 claimed as ORU, v1 program length 20, P2PKH / P2SH / witness v2, PCORB stack too short or wrong field size, spend-CVG2, plus dual-pool and mint-shape crosses (SMW-010 / SMW-011).
- Coq: `ScriptMintWitness.v` (not an interpreter) + `Kernel/GoldenHarness.v`. Python oracle `eval_script_mint_neg`. Dual-pool / mint-shape extras on the two cross rows are L1; L2 pins the script / PCORB half. Field-semantic PCORB bind and P2WSH execution stay L3.
- Golden: **390 → 402** shared; **183 → 195** Coq-enforced. `_CoqProject` **209 → 210**.

---

## Addendum — 2026-08-20 (historical verification record)

Honesty flags were **not** flipped.

- Finite SMW *shape* predicates are now a conjunct of `apply_tx` (`ReplayReorgCore/ScriptShape.v` / `ScriptShapeLift.v`): non-empty created scripts must be `IsAllowedScript` or zero-value `OP_RETURN`; spent CVG2 is rejected; `tx_pcorb = Some stack` must parse PCORB field sizes. Empty `out_script` remains the DualPool fixture placeholder (not C++ `IsAllowedScript`). `tx_pcorb = None` skips the witness conjunct (existing mint fixtures).
- **Not lifted:** `IsResScript` vs `out_asset` (SMW-000/001/005/010 stay vector-only), P2WSH execution, PCORB field semantics, `CheckTransaction`. `we_do_not_claim_script_derives_asset` is unchanged. Dual-pool / cap / H-1 theorems are preserved (`apply_tx_rejects_without_dual_pool` still fires first).
- `_CoqProject` **210 → 212**. Golden vector counts unchanged (**402** / **195**).

---

## Addendum — 2026-08-20 (historical verification record)

Honesty flags were **not** flipped.

- Gate pins correspondence counts in `formal_verification/golden_lock.json` (**402** shared / **195** Coq-enforced at this addendum; living lock after EdgeCross is **414 / 207**). `scripts/check_golden_lock.py` fails if JSON, harness index, or `Lemma golden_*` counts drift, or if NegativeCore / InvalidCvg / ScriptMintNeg / DCR-024…035 lose a row or are demoted to `python_only`.
- `gen_coq_harness.py` refuses `coq_enforce: false` on those must-enforce families. `generate_phase3_vectors.py` refuses to write a JSON whose family counts do not match the lock.
- L1 vs L2 execution splits (Python full row vs Coq boolean / opaque `hash32_eq_dec`) are recorded in [GOLDEN-VECTOR-DISCREPANCIES.md](./GOLDEN-VECTOR-DISCREPANCIES.md).
- `SKIP_TEST_BITCOIN=1 COQ=1` is the operator check that the new families stay in the Coq harness rebuild.

---

## Addendum — 2026-08-20 (historical verification record)

Honesty flags were **not** flipped. This declares residual abstract surfaces; it does not close them.

- Coq `ReplayReorgCore/OpaqueBoundary.v`: machine-checked *non-claims* that `out_script` does not classify assets, `cm_draw_won` is not owner/PCORB/Selection, and `rs_res_supply` is not `utxo_res`.
- `DrawCandidateRoot.v` §9: the Coq claim is the eligible-list digest only (`draw_candidate_root_scope_is_eligible_digest`). Coinbase / tx-Merkle / header bind remain C++.
- Classification table and CURRENT_STATE §8/§9 aligned with the code. Closed theorems were not reopened. `_CoqProject` **208 → 209**.

---

## Addendum — 2026-08-20 (historical verification record)

Honesty flags were **not** flipped. Phase-5 `DCS_Impossible` was **not** rewritten. No consensus or Coq catalog change.

- Assessment: [`../formal/RIGOR_L2_L3_REFINEMENT_FEASIBILITY.md`](../formal/RIGOR_L2_L3_REFINEMENT_FEASIBILITY.md). Full L2↔L3 identity is architectural (no C++ semantics, no shared production hash, SIMD AutoDetect, coin-view / script / mint-auth / coinbase bind have no matching L2 type).
- Maintained bridge is Path A (finite differential). Mechanized local refinement (Path B2) and architecture replacement (Path C) are **not funded** this cycle. Gap / Roadmap entries match that conclusion.

---

## Addendum — 2026-08-20 (historical verification record)

Honesty flags were **not** flipped.

- Negative family `EdgeCross` (`XFAM-000`…`011`, 12 Coq-enforced rows) densifies the finite differential gate with cross-family combination rejects: illegal script shape + dual-pool (P2PKH / witness v2 / spend-CVG2), malformed `C` + wrong mint amount / hard-cap overshoot, A-9-C window remainder × DCR × selection (empty / singleton / drop-winner), and schedule boundaries (epoch switch, `W(h)=0`, last positive `W`) × mint-shape / cap.
- Python oracle `eval_edge_cross`. Coq: script/schedule rows in `Kernel/NegativeCoreHarness.v` (both script-shape and dual-pool/cap booleans); root/window rows in `Kernel/GoldenHarness.v` (`ConvergeRoot` / `WindowExclusion` / `DrawCandidateRoot` / `select_winner` ∧ `ResSchedule` arithmetic). Root+mint L2 pins digest mismatch and `claimed ≠ W(h)` / cap arithmetic (TxAbs mint-shape remains the NegativeCore family).
- Golden: **402 → 414** shared; **195 → 207** Coq-enforced. `golden_lock.json` must-enforce adds EdgeCross 12/12. Fail-closed.

---

## Addendum — 2026-08-20 (historical verification record)

Honesty flags were **not** flipped. No vector was added or removed. Opaque / compilation-unit splits were **not** closed.

- [GOLDEN-VECTOR-DISCREPANCIES.md](./GOLDEN-VECTOR-DISCREPANCIES.md) is the living catalog: each known L1 vs L2 split has why / acceptable / eliminate (disposition **keep** or **not_this_cycle**). Living counts **414 / 207** (402 / 195 was pre-`EdgeCross`).
- `golden_lock.json` `surface_splits` pins NEG-014/015, SMW-010/011, XFAM-003…005, Z-height obligation/schedule rows, the 25 omit-`coq_enforce` kernel ids, and python-only families. `check_golden_lock.py` fails if a named split is dropped, moved, or demoted.
- `generate_phase3_vectors.py` and `gen_coq_harness.py` refuse to rewrite artifacts until the lock matches. The oracle checks the same JSON-side lock before evaluating.

---

## Addendum — 2026-08-20 (historical verification record)

Honesty flags were **not** flipped. No vector was added. `apply_tx` was **not** changed. Refinement was **not** attempted.

**Did (model-internal, current abstraction):**

- Block-level script-shape trace: `apply_txs_script_shape` / `apply_block_succeeds_implies_script_shape` — every accepted block’s body passed the finite shape conjunct already on `apply_tx`.
- Citable dual-pool projection: `apply_block_res_net_bounded_by_scheduled_mints` (body RES net ≤ n·W(h); mint-free body cannot increase RES).
- Machine-checked opaque non-claims: `we_do_not_claim_apply_tx_consults_is_res_script`, `we_do_not_claim_cvg2_script_is_pooling_class`.

**Did not (scope fence):**

- Wire `IsResScript` vs `out_asset` into `apply_tx` (would claim script derives asset).
- Interpreter / P2WSH / PCORB field semantics / `CheckTransaction`.
- Created-CVG2 min-amount or Convergence-tx class on `apply_tx`.
- P1-2: Selection winner into `cm_draw_won`.
- Any L2↔L3 refinement claim.

---

## Addendum — 2026-08-20 (historical verification record)

Honesty flags were **not** flipped. No vector was added. This is the citable RC0 FV freeze, not a new proof.

- Freeze declaration: [`../formal/RIGOR_FORMAL_VERIFICATION_CURRENT_STATE.md`](../formal/RIGOR_FORMAL_VERIFICATION_CURRENT_STATE.md) §0 — model guarantees, differential-gate guarantees, explicit non-guarantees.
- Classification §2.4 now includes **NegativeCore 16** so the family table sums to **414** (the prior omit summed to 398).
- Cross-doc living lock at that addendum: **414** shared / **207** Coq-enforced; `_CoqProject` **212**; `golden_lock.json` tag `tightening-replay-txid-2026-08-18`. Living lock after densify: **422 / 215**. 402 / 195 remains the pre-`EdgeCross` snapshot.
- Honesty: `equivalence_claimed` / `hash_kernel_shared_binary_with_cpp` / `refinement_proved` remain **false** in the Coq lock and in STATUS / FREEZE / CURRENT_STATE / Gap / Roadmap.

---

## Addendum — 2026-08-20 (historical verification record)

Honesty flags were **not** flipped.

Eight Coq-enforced `EdgeCross` rows (`XFAM-012`…`019`) densify residual crosses that the first 12 did not cover:

- **Shape × dual-pool:** P2SH, v1-len20, nonzero `OP_RETURN`, and 5-item PCORB, each with ordinary RES inflation (in-model `apply_tx` conjuncts). Complements XFAM-000…002 / SMW-002/004/011.
- **Window × mint obligation:** A-9-C empty F + without-winner mint; drop unpaid winner + H-1 omit. Pins window remainder **and** the Z-height obligation boolean. Does **not** wire Selection into `cm_draw_won` (P1-2).
- **First-epoch boundary × cap:** last height of epoch 0 and first height of epoch 1, each `W(h)+1` mint-shape **and** one-atom cap overshoot. Complements XFAM-011 (last paying height) and NEG-007 (wrong-epoch amount only).

**Not added:** SHA-256-heavy `RunSelection` / `SigmaPoW01` extras (P1-4 cost); `IsResScript` vs `out_asset`; coinbase bind of DCR; `W(h)=0` × cap (attempted amount is 0, cap does not fire); duplicate NEG-007.

Golden: **414 → 422** shared; **207 → 215** Coq-enforced (GoldenHarness 185→187, NegativeCoreHarness 22→28). `golden_lock.json` EdgeCross **20/20**. Fail-closed.

---

## Addendum — 2026-08-20 (historical verification record)

Honesty flags were **not** flipped. No vector or Coq theorem was added.

- Pre-release checklist: [`FV-RELEASE-CHECKLIST.md`](./FV-RELEASE-CHECKLIST.md). Runner: `bash formal_verification/scripts/run_fv_release_checklist.sh` (freeze artifacts + `SKIP_TEST_BITCOIN=1` gate; lock check is gate step 2b).
- CI `.github/workflows/formal-verification-gate.yml` now runs that runner and watches `docs/formal/**`, `GOLDEN-VECTOR-DISCREPANCIES.md`, and the checklist. `COQ=1` / GoldenHarness rebuild stays **local** (Coq 8.15.0).
- What to re-run after which file change is in the checklist §4. Living lock unchanged (**422 / 215**).

---

## Document map

| Document | Role |
|----------|------|
| **This file** | **Single current status page** — correspondence boundary and operator entry |
| [FV-RELEASE-CHECKLIST.md](./FV-RELEASE-CHECKLIST.md) | Pre-release commands, regen/lock triggers, CI vs local |
| [FORMAL-VERIFICATION-FREEZE.md](./FORMAL-VERIFICATION-FREEZE.md) | Freeze checklist: date, commands, maintenance mode |
| [FORMAL-VERIFICATION-VERDICT.md](./FORMAL-VERIFICATION-VERDICT.md) | What the Coq model proves / does not prove |
| [FORMAL-VERIFICATION-VERDICT-ADDENDUM-2026-08-16.md](./FORMAL-VERIFICATION-VERDICT-ADDENDUM-2026-08-16.md) | Post-verdict gap closures (historical + freeze pointer) |
| [GOLDEN-VECTOR-DISCREPANCIES.md](./GOLDEN-VECTOR-DISCREPANCIES.md) | Intentional splits, not bugs |
| [FORMAL-VERIFICATION-TIGHTENING-2026-08-18.md](./FORMAL-VERIFICATION-TIGHTENING-2026-08-18.md) | Replay-txid fix + 9 vectors |
| [SHARED-HASH-FEASIBILITY-2026-08.md](./SHARED-HASH-FEASIBILITY-2026-08.md) | Shared-hash options; production binary rejected; flag stays false |
| [LIGHTWEIGHT-CORRESPONDENCE-SIGMA-ROOT-2026-08.md](./LIGHTWEIGHT-CORRESPONDENCE-SIGMA-ROOT-2026-08.md) | σ + root correspondence assessment |
| [SIGMA-ROOT-CLI-2026-08.md](./SIGMA-ROOT-CLI-2026-08.md) | Test-only production σ/root CLI in the gate (not refinement) |
| [Architecture snapshot](../formal/RIGOR_FORMAL_VERIFICATION_CURRENT_STATE.md) | RC0 FV freeze (§0, then 414/207) + living Classification **422 / 215** |
| [L2↔L3 refinement feasibility](../formal/RIGOR_L2_L3_REFINEMENT_FEASIBILITY.md) | Why full refinement is not reachable under current architecture; Path A/B/C; flags stay false |
| `formal_verification/README.md` | Layer map and theorem pointers in the corpus |
| [R2](./FORMAL-VERIFICATION-STATUS-2026-08-16-R2.md) / [R3](./FORMAL-VERIFICATION-STATUS-2026-08-16-R3.md) | **Historical** round logs — not the freeze page |
