# RFC-008 Erratum — PCORB Witness Stack (11 Items, Reserved Zero Placeholders)

**Status:** ~~Consensus-critical erratum (F-WX-08 / F-WX-09 activation)~~ — **SUPERSEDED, MOOT**  
**Superseded by:** SECURITY-001-A / A-7 (`docs/security/SECURITY_001_A7_REPORT.md`)  
**Supersedes:** RFC-008 v1.2 changelog line “10-item RNG/PCORB witness stack layout”  
**Date:** 2026-05-26 (superseded 2026-08-03)

> **This erratum no longer has a subject.** It corrected the item count of the native RES
> **script path**, and fixed indices 9 and 10 of that path to zero. SECURITY-001-A / A-7 removed the
> script path in its entirety, so there is no 11-item stack to count and no reserved placeholders to
> freeze. A witness v1 RES output now has exactly one spending path, the 2-item Ed25519 key path.
>
> The script path was removed rather than corrected because its authorization — `VerifyConvergeProof`
> over the σ digest — did not commit to the spending transaction, so a broadcast claim could be
> copied into anyone else's transaction spending the same output to themselves. Items 6 and 7, the
> Ed25519 signature and its self-supplied verifying key, were deleted from the mint witness at the
> same time and for a related reason: the key was chosen by the witness, so any freshly generated
> keypair satisfied the check.
>
> The **mint** witness that remains is the six proof fields, indices 0–5 of the table below, and
> nothing else. The normative layout now lives in `consensus/res_proof.h` and
> `docs/mining/RES_BODY_MINT_SCHEMA.md`.
>
> **What survives from this erratum:** §3's doctrine, in a stronger form. There are no reserved bytes
> anywhere in the witness to interpret later, so a future extension cannot quietly repurpose garbage
> — it must define a new witness mode under a new RFC. §4's σ layout and Sigma-CR-01 `bind_commit`
> are unchanged; σ is still the draw digest, it is simply no longer signed.
>
> **What is void:** the 11-item count, the F-WX-08 / F-WX-09 MUST_BE_ZERO consensus rules, the
> `SCRIPT_ERR_WITNESS_RESERVED_NONZERO` error, the `ConvergeProofLeafScript` sentinel at index 8
> (Leaf-Freeze-01, see RFC-008 §16), and both implementation references in §5 — the interpreter
> branch and `res_witness_reserved_zero_tests.cpp` are deleted.
>
> The body below is retained unaltered as the historical record of a rule that was in force between
> 2026-05-26 and A-7. Do not implement from it.

---

## 1. Correction

The native RES **PCORB / script-path** witness stack has **eleven** items, not ten.

RFC-008 v1.2 §Revision History incorrectly states “10-item … unchanged.” Production consensus (`VerifyWitnessProgram`, `ParseConvergeProof`, `BuildPcorbBodyResMintWitness`) has always required **11** pushes on this path.

---

## 2. Normative 11-Item Layout (bottom → top)

| Index | Size (bytes) | Field |
|-------|--------------|-------|
| 0 | 32 | `convergence_txid` |
| 1 | 8 | `convergence_amount_sat` (LE64) |
| 2 | 32 | `bind_hash` |
| 3 | 8 | `height` (LE64) |
| 4 | 4 | `target_prev` (LE32) — **not** a flags field |
| 5 | 32 | `convergence_root` |
| 6 | 64 | Ed25519 σ signature |
| 7 | 32 | Ed25519 pubkey |
| 8 | 1 | `ConvergeProofLeafScript()` (`0x51` only, Leaf-Freeze-01) |
| 9 | 32 | **reserved32** — MUST_BE_ZERO |
| 10 | 64 | **reserved64** — MUST_BE_ZERO |

Consensus (`F-WX-08 / F-WX-09`): any non-zero byte in indices **9** or **10** → invalid (`SCRIPT_ERR_WITNESS_RESERVED_NONZERO`).

---

## 3. Future Extensions

- Bits or semantics in **reserved32/reserved64** require a **new RFC** and explicit soft-fork / activation record.
- **Forbidden** without RFC: interpreting placeholder bytes, partial flag parsing, aggregation metadata in placeholders.
- Preferred path for major features (e.g. aggregation): **new witness mode** or new stack layout, not silent use of garbage bytes.

---

## 4. Unchanged (RFC-008 v1.2 still binding)

- σ input layout and Sigma-CR-01 `bind_commit`
- `ConvergeProofLeafScript` 1-byte sentinel
- Field order for indices 0–8

---

## 5. Implementation Reference

- `core/src/script/interpreter.cpp` — `VerifyWitnessProgram`, 11-item branch
- `core/src/test/res_witness_reserved_zero_tests.cpp` — consensus tests

---

**END OF ERRATUM**
