# Protocol Freeze Migration Chain

**Purpose:** an auditable record of every re-freeze of
`.runtime/freeze/protocol_freeze_snapshot.json`.

A protocol freeze is only worth something if breaking it leaves a trace. This file, together
with the `amendments` array inside the snapshot itself and the `FREEZE_AMENDMENTS` constant
in `sim/freeze/freeze_integrity_checker.py`, is that trace. All three must agree, and
`freeze_amendment_chain_intact` enforces it.

---

## Link 1 → 2 — SECURITY-001-A6 re-freeze (2026-08-02)

| | |
|---|---|
| **Authorised by** | `TASK-SECURITY-001-CONSENSUS-REMEDIATION-CONTROL`, Q2 approval |
| **Superseded snapshot** | `8ed448a28ffea3637c2e400124ddd3e200ce44550b9ec59c50c6ceed7b6b70cd` |
| **New snapshot** | `c6a8bfb5f3e62c25265073cfb39acbdf1e6eb7553604f82e5692e0a8a1b94e34` |
| **Archived copy of the superseded file** | `.runtime/freeze/archive/protocol_freeze_snapshot.pre-SECURITY-001-A6.json` |
| **SHA-256 of that archive file** | `c30651234f7334619b0ec7f12e7d031462712c660d6e493a9ffbb6d2f46b04ca` |
| **Amendment reference** | [`docs/rfcs/RFC-014-v1.1.md`](../rfc/RFC-014-v1.1.md) |
| **Evidence** | [`SECURITY-001-A-FINAL-REPORT.md`](./SECURITY-001-A-FINAL-REPORT.md) |

### Changed parameter hashes

| Field | Before | After |
|-------|--------|-------|
| `protocol_params_hash` | `6a756b2ff54cd5858aa8258eb00e973f35de501937ba5bb6dc9c423764aa8c1f` | `35f3f2a11806b3dc637d8b204f2aebcb5f775b9758b1abf06e6f27777a940219` |
| `production_release_manifest_sha256` | `54e656a9c3202f060ca7ade083457d90700dc7af674c9cf045b5ec153ef2e389` | `31f463420ce0396450e19322e1ff3a1d6d165472537a41eb1a3d2ef400c12504` |
| `version_manifest_sha256` | `8f2a9c973f5921cba6cc514a4360378ed94c091bdbfcbad147d4da3f91022005` | unchanged |
| `system_hash` | — | unchanged |

### Two distinct causes, recorded separately

This re-freeze absorbed changes from **two unrelated tasks**. They are recorded as separate
amendment entries so that A-6 is not credited with a change it did not make.

#### Amendment `SECURITY-001-A6` — consensus

`protocol_params.res_probability_target_hex`

```
old: 0001000000000000000000000000000000000000000000000000000000000000   (= 2^240)
new: 00000000002af31dc4611873bf3f70834acdae9f0f4f534f5d60585a5f1c1a3c   (= floor(2^240 / 1e8))
```

RFC-014 v1.0 §2.1 fixed `T = 2^240` by reasoning at a normalised weight `f = 1`, but the
implemented weight is the convergence amount in satoshis. At the minimum admissible weight
`T · f / 2^256 = 1525.9`, so `H < T · f` held for every candidate and the RES eligibility
filter was vacuous. Combined with `MIN_TXS_FOR_RES = 1`, a block producer including a single
convergence transaction won the draw with certainty.

The corrected value restores the intended `p = 2^-16` per input at 1 ORU. This is the
correction of a unit error, **not** a change of monetary policy — every RFC-014 §5.4 issuance
figure is preserved.

**Consensus impact:** CHANGED. Chain-invalidating from height 1. Genesis blocks unaffected —
see ``CHAIN_REGENERATION_POLICY.md`` §3.

#### Amendment `RELEASE-006-PRODUCT-CLOSURE-001` — packaging, not consensus

`production_release_manifest_sha256`

```
old: 54e656a9c3202f060ca7ade083457d90700dc7af674c9cf045b5ec153ef2e389
new: 31f463420ce0396450e19322e1ff3a1d6d165472537a41eb1a3d2ef400c12504
```

`core/src/release/production_release_manifest.json` moved `native_ci_status` from `pending`
to `completed`, populated the installer and wallet artifact hashes, and added the AppImage
hash and workflow references. Owned by `TASK-RELEASE-006-PRODUCT-CLOSURE-001`.

**Consensus impact:** NONE. It is recorded here only because it was present in the working
tree when the A-6 re-freeze was taken. A snapshot that absorbed it silently would have
misattributed a packaging change to a consensus remediation.

### Verification after re-freeze

```
$ python3 sim/freeze/freeze_integrity_checker.py
RESULT: PASS      (12/12 checks)
```

| Check | Result |
|-------|--------|
| `freeze_snapshot_present` | PASS |
| `freeze_snapshot_integrity` | PASS |
| `version_manifest_unchanged` | PASS |
| `production_release_manifest_unchanged` | PASS |
| `system_hash_unchanged` | PASS |
| `protocol_params_unchanged` | PASS |
| `freeze_amendment_chain_intact` | PASS (new) |
| `res_probability_target_matches_source` | PASS (new) |
| `res_probability_target_not_vacuous` | PASS (new) |
| `protocol_version_unchanged` | PASS |
| `explorer_version_unchanged` | PASS |
| `equivalence_not_claimed` | PASS |

---

## Why silent refresh is now impossible

The cheapest way to make a failing freeze gate go green has always been to re-run the tool
with `--write-snapshot`. That is what made the gate weak, and it is what allowed the A-6
constant to sit behind a green gate for as long as it did.

`write_snapshot()` now refuses to overwrite a snapshot whose `snapshot_sha256` is not named
by an entry in `FREEZE_AMENDMENTS`. Demonstrated immediately after this re-freeze:

```
$ python3 sim/freeze/freeze_integrity_checker.py --write-snapshot
refusing to overwrite the freeze snapshot.
  existing snapshot_sha256: c6a8bfb5f3e62c25265073cfb39acbdf1e6eb7553604f82e5692e0a8a1b94e34
  no entry in FREEZE_AMENDMENTS supersedes it.

Re-freezing is a governance act. Append an amendment naming this hash in
`superseded_snapshot_sha256`, with the parameter changed, the old and new
values, the authorising task, and the reason. Then re-run.
[exit 1]
```

To re-freeze, an operator must first write down what changed and why, in code, naming the
hash being superseded. The record is created *before* the write, not after, and
`freeze_amendment_chain_intact` fails if a later snapshot loses that history.

---

## Procedure for the next re-freeze

1. Append an entry to `FREEZE_AMENDMENTS` in `sim/freeze/freeze_integrity_checker.py` with
   `superseded_snapshot_sha256` set to the current snapshot's hash, plus `id`, `authority`,
   `reference`, `parameter`, `old_value`, `new_value`, `reason`, `consensus_impact`.
2. Archive the current snapshot to `.runtime/freeze/archive/` and record its file hash.
3. Run `python3 sim/freeze/freeze_integrity_checker.py --write-snapshot`.
4. Run the checker with no arguments and confirm all checks pass.
5. Append a link to this document with both hashes.
