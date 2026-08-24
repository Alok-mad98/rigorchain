# Golden-vector discrepancies (post-verdict expansion)

**Current status**: [FORMAL-VERIFICATION-STATUS.md](./FORMAL-VERIFICATION-STATUS.md)

**Date**: 2026-08-20 (`TASK-FV-DISCREPANCY-AND-LOCK-MAINTENANCE-001`; honesty unchanged)
**Set**: `formal_verification/phase3_golden_vectors.json` (tag `tightening-replay-txid-2026-08-18`)
**Living lock**: **422** shared / **215** Coq-enforced (`golden_lock.json`). RC0 freeze was **414 / 207**; 402 / 195 was pre-`EdgeCross`. Do not cite either as current.

No byte-level disagreement was found between the Python spec, the Coq kernel (enforced subset plus `CanonicalHashKernel.v` FIPS lemmas), the extracted OCaml hasher, the portable C++ `CSHA256` CLI, and the generated C++ golden cases.

A green gate means the named Python predicate and the named Coq boolean agree on the frozen vector. It does **not** mean Coq executed the same function Python did, and it does **not** flip `equivalence_claimed`, `hash_kernel_shared_binary_with_cpp`, or `refinement_proved`.

Named splits are pinned in `formal_verification/golden_lock.json` → `surface_splits`. `scripts/check_golden_lock.py` fails if a named row is dropped, moved, or demoted. Generators refuse to rewrite JSON / harness files until that lock matches.

## Disposition vocabulary

| Disposition | Meaning |
|-------------|---------|
| **keep** | Intentional. Opaque, compilation-unit, or unary-height. Do not close this cycle. |
| **not_this_cycle** | Cost or flag hygiene. Optional later work; not a bug. Bump the lock only with a dedicated task. |

## Count-level splits

Python L1 evaluates **every** row (`tests/test_phase3_oracle.py`). Coq L2 evaluates a **strict subset**: 215 `Lemma golden_*` in `Kernel/GoldenHarness.v` (187) + `Kernel/NegativeCoreHarness.v` (28). JSON `coq_enforce: true` = **190** is **not** the L1↔L2 lock.

| Split | Why it exists | Acceptable? | Eliminate? |
|---|---|---|---|
| **215** Coq-enforced vs **422** full set | SHA-256 `vm_compute` is expensive. Extra `SigmaPoW01` / `RunSelection` / `ConvergenceRoot` / `HashKernel` samples are `coq_enforce: false` and checked in Python + C++ only. | Yes. Cheap families stay 100% enforced (Inequality, WindowExclusion, SigmaEncoder, DrawCandidateRoot, NegativeCore, InvalidCvg, ScriptMintNeg, EdgeCross). | **not_this_cycle.** Stratified extras only; do not force every SHA-256 row into the harness. |
| JSON `coq_enforce: true` = **190** vs **215** lemmas | Original kernel rows omit the key: `CR-000`…`006`, `CR-010`, `CR-011`, `SIG-000`…`004`, `INQ-000`…`003`, `WIN-000`…`006` (25 IDs). `gen_coq_harness.py` still includes them because their module is in `kernel_mods`. Replay / ReorgBind / SupplyScaffold / BlockTrace also omit the key but are `python_only`. | Yes. The lock is lemma count, not the JSON flag. | **not_this_cycle.** Adding the key is hygiene; it must bump `json_coq_enforce_true` and `omit_coq_enforce_key_enforced`. Do not cite 190 as the L1↔L2 lock. |
| `Sha256Fips` (8) vs `CanonicalHashKernel.v` | FIPS single-SHA is proved on the kernel, not duplicated as GoldenHarness rows. Harness index marks the 8 vectors `python_only`. | Yes. Avoids a second SHA-256 `vm_compute` copy of the same padding edges. | **not_this_cycle.** |

Must-enforce families (gate fails if any row is dropped or demoted to `python_only`):

| Family | Vectors | Coq-enforced |
|--------|---------|--------------|
| NegativeCore | 16 | 16 |
| InvalidCvg | 14 | 14 |
| ScriptMintNeg | 12 | 12 |
| DrawCandidateRoot (incl. DCR-024…035) | 36 (12 negatives) | 36 (12) |
| EdgeCross | 20 | 20 |

Bump `golden_lock.json` only with a new vector / discrepancy task.

## L1 Python vs L2 Coq execution (same reject, different surface)

These are documented correspondence splits, not bugs. **keep** rows are not closed by making `hash32_eq_dec` `Defined` or by merging `Types.Tx` into NegativeCoreHarness.

| Family / rows | Python (L1, full row) | Coq harness (L2) | Why it exists | Acceptable? | Eliminate? |
|---|---|---|---|---|---|
| **NegativeCore** `NEG-000`…`007` (dual-pool / mint-shape) | `eval_negative_core` → `valid_dual_pool_on_ins` / `res_mint_shape` | Same booleans. Cheap `vm_compute`. | Same predicate. | Yes. | Aligned (no split). |
| **NegativeCore** `NEG-008` / `009` / `012` / `013` (H-1 / unauthorized / without-winner) | Obligation path names at header height | `winner_mismatch_at` / `unauthorized_mint_at` / `without_winner_at` at **Z** height (`get_res_mint_reward h`) | Unary `hdr_height` of large heights would blow `vm_compute`. Schedule is already a Z function. | Yes. Same reject, Z vs nat height. | **keep.** |
| **NegativeCore** `NEG-010` / `011` (cap) | `valid_res_cap` | `negb valid_res_cap` on abstract state / block | Same boolean; not a full `apply_block`. | Yes. | Aligned on the cap conjunct (no extra split). |
| **NegativeCore** `NEG-014`…`015` (mixed ordinary + illegal mint body) | Also runs L1 `apply_block_model` (full connect) | Pins the **body dual-pool conjunct** that makes `apply_block` return `None` | `hash32_eq_dec` is a `Qed`-sealed `list_eq_dec` in `Hash32Order.v` (not a `Parameter`, still opaque to `vm_compute`). Unfolding 32-byte equality inside `gather_inputs` / `apply_block` would explode the harness. | Yes. Same reject, different surface. | **keep.** Do not `Defined` the equality to “close” this. |
| **InvalidCvg** `ICVG-000`…`013` | `eval_invalid_cvg` | Same classify / root / hash256d checks | SHA-256 rows are still cheap enough for `vm_cast_no_check`. Not a script interpreter. | Yes. | Aligned. |
| **ScriptMintNeg** `SMW-000`…`009` | `eval_script_mint_neg` | `classify_script_reject` / `classify_pcorb_reject` / `classify_spend_cvg2`. Shape subset is also an `apply_tx` conjunct (`ScriptShape.v`). | Script-vs-claimed-asset rows stay harness-only by design. | Yes. | Aligned on the script/PCORB/spend booleans. |
| **ScriptMintNeg** `SMW-010` / `SMW-011` (cross) | Records extras: `dual_pool_reject` (SMW-010) and `mint_shape_would_pass` (SMW-011) | `vm_compute`s **only** the script / PCORB-shape boolean | Monetary halves are already L2-locked by NegativeCore. GoldenHarness uses `Types.Tx`; NegativeCoreHarness uses `TxAbs` — one compilation unit cannot import both. | Yes. Same overall reject, thinner L2 surface. | **keep.** |
| **DCR negatives** `DCR-024`…`035` | Full eligible-root / e2e / σ-perturb Python | Full eligible-root / e2e / σ-perturb Coq | No execution split. | Yes. | Aligned. |
| **EdgeCross** `XFAM-000`…`002`, `012`…`015` (script/PCORB+dual-pool) | `eval_edge_cross`: script/PCORB reject **and** dual-pool reject **and** `valid_script_shape_tx` | Same conjuncts in `NegativeCoreHarness.v` | Lives in the TxAbs unit so both conjuncts are cheap. | Yes. | Aligned. |
| **EdgeCross** `XFAM-003`…`005` (root+mint/cap) | Full `eval_invalid_cvg` **and** `eval_negative_core` (mint-shape / `valid_res_cap`) | Digest / length mismatch ∧ `claimed ≠ W(h)` or cap arithmetic (`ResSchedule`) | TxAbs mint-shape / `valid_res_cap` stay the NegativeCore family (same `Types.Tx` vs `TxAbs` clash). | Yes. Same reject, thinner mint surface. | **keep.** |
| **EdgeCross** `XFAM-006`…`008` (window+DCR+sel) | Exclusion + `run_selection` + DCR of survivors vs unpaid set | `exclude_already_paid` ∧ `ConvergeRoot` ∧ `DrawCandidateRoot` ∧ `select_winner` | No execution split. | Yes. | Aligned. |
| **EdgeCross** `XFAM-016`…`017` (window+obligation) | Window remainder **and** `eval_negative_core` connect_obligation | Same window checks **and** `without_winner_at` / `winner_mismatch_at` (Z-height) in `GoldenHarness.v` | Does not claim `cm_draw_won` is the Selection winner (P1-2). | Yes. | Aligned on both conjuncts. |
| **EdgeCross** `XFAM-009`…`011`, `018`…`019` (schedule+mint/cap) | `eval_negative_core` mint-shape / cap at epoch last, `W=0`, last positive `W`, first-epoch last/start | Dual-pool / mint-shape booleans plus `get_res_mint_reward` jump / `unauthorized_mint_at` / `cap_overshoot_at` (**Z-height**; no unary `hdr_height` of ~52M / 210k) | Same unary-height reason as NEG obligation rows. | Yes. | **keep.** |

## Python-only families (L1 + L3, no harness lemma)

Locked at `families.*.coq_enforced = 0`. Promoting any of these into GoldenHarness is a new vector task (and, for SHA-256 families, a cost decision).

| Family | Vectors | Why it exists | Acceptable? | Eliminate? |
|--------|---------|---------------|-------------|------------|
| SupplyScaffold | 5 | RFC-017 schedule vs oracle scaffolding | Yes | **not_this_cycle** |
| BlockTrace | 12 | Structural traces, not a kernel function | Yes | **keep** (no kernel lemma to pin) |
| WindowIncomplete | 2 | Incomplete-window structure | Yes | **keep** |
| ReorgBind | 12 | Replay/reorg bind scaffolding | Yes | **keep** |
| Replay | 3 | Replay scaffold | Yes | **keep** |
| Sha256Fips | 8 | FIPS lives in `CanonicalHashKernel.v` | Yes | **not_this_cycle** (see count-level row) |

## Other intentional splits (L1 / L3)

| Split | Why it exists | Acceptable? | Eliminate? |
|---|---|---|---|
| `WindowExclusion` C++ cases filter hex strings | They check the A-9-C *rule* against the JSON. `FinalizeWindowedDrawSet` is covered separately by `convergence_consensus_tests.cpp` (`a9c_already_paid_*`). A divergence between the pure filter and the block-level builder would not be caught by the hex-filter cases alone. | Yes. Finite hex-filter lock, not a window-walk refinement. | **keep.** Optional densification is Gap P1-4, not this catalog. |
| Hash kernel | Coq `RigorSha256` and extracted `hash256_cli` are the same kernel. Portable C++ `cpp_hash_cli` is production `CSHA256` (C++ Transform). Python `hashlib` remains independent. `test_hash_kernel_tripartite.py` fails the gate on any split. SIMD paths are still Core `SelfTest`, not this gate. This is not a shared C++/Coq binary. | Yes. `hash_kernel_shared_binary_with_cpp` stays false. | **keep.** |
| σ / ConvergenceRoot CLI | Portable `cpp_sigma_root_cli` links production `ComputeSigmaHash` and `ComputeConvergenceRootFromEligibilityTxids` (not the coin-view wrapper). Gate step 4 checks `SigmaEncoder` / `SigmaPoW01` / `ConvergenceRoot` against the JSON. Finite, not refinement. | Yes. | **keep.** |

## Encoding traps that *looked* like discrepancies and are not

1. **SetHex vs raw hash hex.** Selection golden tests mix `ParseHex256` (Bitcoin display / `SetHex`, reversed) for opaque txids and `HashHexToUint256` (forward hash bytes) for σ outputs. Encoder / `SigmaPoW01` expansion vectors store **raw** 32-byte hex (`bytes.hex()`), matching `txid.begin()` memcpy order. Mixing the two orientations is a test bug, not a consensus bug.
2. **Height endianness.** Height 256 is LE `[0, 1, 0, 0]`, not BE. Pinned by `le32_bytes_256` and `test_le32_height_256_not_be`.
3. **`target_prev`.** Not a σ field. `PCORBHash_target_prev_ignored` and `canonical_encoder_has_no_target_prev_slot` lock that.

## Residual risk

A C++ bug outside the hashed preimage and the `already_paid` filter — for example an off-by-one in `BuildConvergenceWindowTail` when walking undo data, or a signed height truncation — can still agree with every vector here and fork on mainnet. That is the Verdict §3.1 boundary.

## Maintenance

1. Edit this file in the same task that adds or closes a split.
2. Update `golden_lock.json` `surface_splits` (IDs, family, harness kind, disposition).
3. `generate_phase3_vectors.py` and `gen_coq_harness.py` must refuse to write if the lock would fail. The gate regenerates into a temp dir and `cmp`s; `check_golden_lock.py` is step 2b.
4. After a lock bump or regen, run `bash formal_verification/scripts/run_fv_release_checklist.sh`. Which files force regen vs lock bump: [FV-RELEASE-CHECKLIST.md](./FV-RELEASE-CHECKLIST.md) §4.
5. Do not try to erase **keep** rows by unfolding `hash32_eq_dec` or by merging `Types.Tx` / `TxAbs`.
