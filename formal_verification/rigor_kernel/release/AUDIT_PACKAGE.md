# Audit Package

**Unique auditor entry.** Start here. Other release files are
supporting detail, not a second claim surface.

This file does **not** strengthen the public claim.

**Public claim (only):** Formally verified critical consensus
invariants inside a Coq consensus kernel model, with explicit
extraction and runtime trust boundaries.

`EXTRACTION_FORMAL_PROOF=source_identity_only`
`EXTRACTION_EQUIVALENCE_NOT_CLAIMED=TRUE`
`PRODUCTION_REFINEMENT_NOT_CLAIMED=TRUE`
`COMPILER_CORRECTNESS=false`
`BINARY_SEMANTIC_EQUIVALENCE=false`
`VERIFIED_COMPILER_FORCED_SWITCH=FALSE`

`res_total_issuance_capped` / OP-X3 is a **schedule-side Coq-model
identity** (`RES_REWARD_EPOCH * Σ W_epoch ≤ RES_MAX_SUPPLY`). It is
**not** a runtime `S_RES` reject, not production-supply refinement,
and does **not** close OP-D1.

## Auditor minimal steps

1. Clone this repository and check out the commit that tracks
   `formal_verification/rigor_kernel/` (confirm
   `TRACKED_PROOF_SOURCES=TRUE` in `release/ProofSourceManifest.md`).
2. Read-only replay — do **not** run `freeze_source_manifest.sh`:

   ```
   cd formal_verification/rigor_kernel
   ./release/check_source_manifest.sh
   ./release/independent_audit_replay.sh --out-dir /tmp/rigor-kernel-audit
   ```

   Detail: `release/IndependentAuditReplay.md`.
   Script: `release/independent_audit_replay.sh`.
   Success: `INDEPENDENT_AUDIT_REPLAY=PASS`. The audited tree must
   stay unwritten.
3. Tamper the lock: change a listed `.v` or add an unlisted
   lock-eligible file. `./release/check_source_manifest.sh` must FAIL.
   Restore the tree afterwards.
4. Do **not** interpret a green run as C++ / ConnectBlock, full
   extraction refinement, verified-compiler, or binary-semantic
   proof. Those flags remain false / not claimed.

## Reproduce

From this cone, on a host with Coq 8.15.0 and OCaml 4.13.x:

```
cd formal_verification/rigor_kernel
./release/check_source_manifest.sh
./release/independent_audit_replay.sh --out-dir /tmp/rigor-kernel-audit
```

`independent_audit_replay.sh` does **not** write the audited tree.
Reports go to `--out-dir`. Success token:
`INDEPENDENT_AUDIT_REPLAY=PASS`.

Full CI (includes the twelve Coq mutations, binary harness, and
`check_release.sh`):

```
./run_kernel_critical.sh
```

Expected close: `FORMAL_KERNEL_PHASE_FINAL_CLOSURE=PASS`.

Publisher-only freeze (do **not** run this to “pass” a check):

```
./release/freeze_source_manifest.sh
```

Auditors verify the freeze with the read-only bidirectional lock:

```
./release/check_source_manifest.sh
```

A hash mismatch or an unlisted lock-eligible file must FAIL.

## Proof and boundary files

| Path | Role |
|---|---|
| `Kernel/SafetyExtensions.v` | Intra-block duplicate-input and per-mint RES corollaries (Category B) |
| `Supply/SupplySchedule.v` | `res_schedule_finite`, `res_total_issuance_capped` (Category A) |
| `Kernel/Chain/ReorgReplay.v` | RFC-020 RES rewind (`res_reorg_rewind_eq`, `res_reorg_surviving_pool`); not OP-R1 |
| `Pow/BlockProof.v` | GetBlockProof = `2^256/(T+1)` after SetCompact guards; not arith_uint256 refinement |
| `Kernel/Chain/ForkChoice.v` | Most-work finite-tip selection on GetBlockProof sums; not k-deep, not OP-R1 |
| `Kernel/Tx/Maturity.v` | Coinbase / RES-mint depth 100 and spend-side MAX_MONEY; not dust, not ConnectBlock |
| `Script/Semantics/Spend.v` | Spendability by class + abstract `sig_ok`; not a VM, not Ed25519, not ConnectBlock |
| `Kernel/KernelSoundness.v` | Bridge `kernel_sound` |
| `Kernel/ExtractionBoundary.v` | Source identity only |
| `Kernel/CryptoAssumptions.v` | Central crypto assumption inventory |
| `Kernel/VerifiedCompilerBoundary.v` | Compiler not in TCB |
| `Audit/KernelNonClaims.v` | Machine-checked `*_NOT_CLAIMED` / `= false` flags |
| `release/TheoremClassification.md` | Canonical A/B/C table |
| `release/UnclassifiedAllowlist.md` | Reverse-check allowlist for `Kernel/` and `proofs/` |
| `release/KernelCompletenessMatrix.md` | Six-domain coverage matrix |
| `release/OpenProperties.md` | Unproved properties + high-cost launch checklist |
| `release/VerifiedCompilerPath.md` | Documented rehearsal path; not switched |
| `release/RefinementMapping.md` | Comparable-fragment map; not a refinement proof |
| `release/PublicClaims.md` | Allowed and forbidden wording |
| `release/SourceManifest.sha256` | Frozen source lock (hash recorded in `VersionManifest.md`) |
| `release/VersionManifest.md` | Toolchain stamp + lock-of-lock hash |

## Category A (authoritative)

Canonical list: `release/TheoremClassification.md`.
`evidence/TheoremClassification.md` must have the **same** Category A
names. Current set (13):

| Theorem | File |
|---|---|
| `utxo_soundness` | `proofs/UTXO_Soundness/UTXOSoundness.v` |
| `no_hidden_inflation` | `proofs/ORU_Conservation/ORUConservation.v` |
| `oru_conservation_extended` | `Supply/FeeDomain.v` |
| `res_issuance_soundness` | `proofs/RES_Issuance/RESIssuance.v` |
| `recognizer_pairwise_disjoint` | `proofs/Script_Correctness/ScriptCorrectness.v` |
| `pow_valid_rejects_above_target` | `proofs/Pow_Correctness/PowCorrectness.v` |
| `compact_decode_lower_bound` | `Pow/CompactBits.v` |
| `negative_time_error_preserved` | `Pow/ASERTModel.v` |
| `exp_q_clamped` | `proofs/Pow_Correctness/ASERTCorrectness.v` |
| `factor_range` | `proofs/Pow_Correctness/ASERTCorrectness.v` |
| `res_schedule_finite` | `Supply/SupplySchedule.v` |
| `res_total_issuance_capped` | `Supply/SupplySchedule.v` |
| `kernel_sound` | `Kernel/KernelSoundness.v` |

P0-120 local corollaries (`apply_block_accepted_inputs_unique`,
`apply_block_rejects_duplicate_input`, `res_mint_bounded_by_genesis`,
`check_res_mint_bounded_by_genesis`) are **Category B**.
`pow_asert_correctness` is **Category C**: its statement holds of any function.

## Explicitly not claimed

| Flag / statement | Status |
|---|---|
| C++ / ConnectBlock equivalence | `PRODUCTION_REFINEMENT_NOT_CLAIMED=TRUE` |
| Full extraction / OCaml compiler refinement | `EXTRACTION_EQUIVALENCE_NOT_CLAIMED=TRUE`; formal proof is source identity only |
| Compiler correctness / binary semantic equivalence | `COMPILER_CORRECTNESS=false`, `BINARY_SEMANTIC_EQUIVALENCE=false` |
| Forced verified-compiler TCB switch | `VERIFIED_COMPILER_FORCED_SWITCH=FALSE` |
| Reorg / common-prefix / finality | OP-R1 **Still open** |
| Liveness / fairness / network | OP-L1 … OP-L4, OP-N1 **Not formalized** |
| Runtime `S_RES` reject | OP-D1 **Open by design** |
| Crypto primitive security | OP-C1; assumptions in `CryptoAssumptions.v` |
| Differential `MATCH` | Implementation agreement only; not independent correctness |

Machine-checked non-claims: `Audit/KernelNonClaims.v`.

## Open properties and high-cost work

- Inventory and reading rule: `release/OpenProperties.md`
- High-cost launch checklist (refinement, verified compiler, liveness):
  same file, plus `release/VerifiedCompilerPath.md`

A Closed nearby corollary is **not** a partial proof of an open
property.

## Package map

See `release/README.md` for the rest of the auditor file list.
Checklist: `release/AuditorChecklist.md`.
