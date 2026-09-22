# Boundary Correction

P0-114 correction of the P0-113 F-01 overclaim.

The formal argument stops at the Coq kernel. Extraction, compilation,
and the host binary are **outside** the proof boundary.

**Public claim (only):** Formally verified critical consensus invariants
(Coq consensus kernel only).

## 1. Verified layer

```
Spec
 ↓
Coq definitions
 ↓
Coq proofs
 ↓
Coq validators
```

Covered by `Qed` theorems and `Print Assumptions` Closed:

- Layer 0 — consensus specification text (P0-106 kernel IDs)
- Layer 1 — Coq kernel models and invariant theorems
- Layer 2 — Coq boolean validators (`check_*`) and `kernel_sound`

`Print Assumptions` Closed applies to this layer only.

## 2. Unverified layer

```
OCaml extraction
 ↓
compiler
 ↓
binary artifact
 ↓
runtime environment
```

These steps are mechanically useful for testing. They are **not**
covered by any Coq theorem in this cone.

## 3. Reason

- No verified compiler correctness theorem exists.
- No extraction refinement theorem exists.
- No binary semantic equivalence theorem exists.

`ExtrOcamlZInt` / `ExtrOcamlNatInt` map unbounded Coq `Z` / `nat` onto
OCaml native `int`. That mapping is trusted, not proved. P0-113 F-01
exhibited concrete divergences; those divergences are why extraction
is outside the proof boundary.

## Forbidden wording

Do not write:

- "Spec → Coq → validator → extraction covered"
- "Coq proof covers OCaml extraction"
- "Coq proof covers binary artifact"
- "theorem-to-binary refinement"
- "executable binary semantic preservation"

Correct wording:

- "Spec → Coq → validator covered"
- "Extraction is outside the proof boundary."
