# Verified Compiler Path

Intended route from the Coq kernel checkers to a machine-checked
compiler, and what is actually runnable **today**.

**Status (binding):**

> The verified-compiler path is **documented**. It is **not** in the
> current checking TCB. The default executable specification remains
> the faithful OCaml extract.
>
> Full binary semantic equivalence is **not** claimed.

`EXTRACTION_FORMAL_PROOF=source_identity_only`
`EXTRACTION_EQUIVALENCE_NOT_CLAIMED=TRUE`
`PRODUCTION_REFINEMENT_NOT_CLAIMED=TRUE`
`COMPILER_CORRECTNESS=false`
`BINARY_SEMANTIC_EQUIVALENCE=false`
`VERIFIED_COMPILER_FORCED_SWITCH=FALSE`
`verified_compiler_in_current_tcb = false`

## Why we do not force a switch

| Blocker | Why it is real on this tree |
|---|---|
| CompCert is not on the host PATH | Host `ccomp` is ABSENT. Sidecar switch has `ccomp` 3.13 (INRIA Non-Commercial; not vendored) |
| CompCert license | Official INRIA CompCert is not a file we can vendor into this repository. Decision: `L-RESEARCH-SIDECAR-2026-09-12` in `release/CertiCoqToolchain.md` (research sidecar only; no production ship) |
| Coq has no native C backend | `Extraction Language C` is not a Coq command. C / Clight requires CertiCoq (or an equivalent generator). It cannot be fed to `ccomp` from stock Coq |
| CertiCoq / CakeML | Sidecar has CertiCoq 0.9+8.19. CakeML would require rewriting the cone in HOL |
| Inductive `Z` | The faithful extract keeps Coq `Z`/`nat`. A verified C backend would need a separate integer model |

Default path stays: Coq validators → `extracted/Extract.v` → inductive OCaml → `ocamlopt` (untrusted compiler) → `extracted-kernel-checker`.

## Intended closed loop (not current TCB)

```
Coq check_*
    → CertiCoq 0.9+8.19 boxed-Z Clight (sidecar; present)
    → sidecar ccomp 3.13 (present; host ABSENT)
    → research vector_driver (named GC / clang TCB)
    → INVALID-001..005
```

Engineering path is present. Gallina–Clight and Clight–assembly
theorems are **not** instantiated. This loop is **not** in the
checking TCB.

Equivalent future backends, if they become available and licensed:

1. **CompCert (preferred):** generate Clight or CompCert-C from the six
   checkers, compile with `ccomp`, keep `Print Assumptions` Closed.
2. **CertiCoq:** Coq → C / Clight with a published pipeline, then CompCert.
3. **CakeML:** re-specify the fragment in HOL/CakeML. This is a new cone,
   not a drop-in for `formal_verification/rigor_kernel`.

None of those steps exists in CI today.

## What runs now (hand-written C99 fragment rehearsal)

```
cd formal_verification/rigor_kernel
./scripts/verified_compiler_path.sh
```

| Step | Artifact |
|---|---|
| Coq fact | `Kernel/VerifiedCompilerBoundary.v` — `negative_vectors_rejected_in_coq` (`vm_compute`, Closed) |
| C source | `verified_compiler/fragment_checker.c` — hand-written C99 subset of the *comparable fragment* (UTXO money range, ORU inequalities, `W(h)` mint bound, integer PoW compare, ASERT toward-zero `/`) |
| Compiler | `ccomp` if present; otherwise **untrusted** `gcc -std=c99` |
| Executable | `verified_compiler/fragment-checker` (or `$RIGOR_KERNEL_OUTDIR`) |
| Vectors | INVALID-001…005 plus five positive smokes, plus a documented `long long` overflow gap |
| Report | `evidence/VerifiedCompilerPathReport.md` |

If `ccomp` is missing the script still completes and prints:

```
VERIFIED_COMPILER=ABSENT
C_FRAGMENT_REHEARSAL=PASS
COMPILER=gcc
COMPILER_CORRECTNESS=false
BINARY_SEMANTIC_EQUIVALENCE=false
```

That PASS means: the C fragment rejects the same five invalid vectors.
It does **not** mean CompCert compiled the Coq extract.

## Trust root (current)

| Layer | Status |
|---|---|
| Coq kernel / `coqc` 8.15.0 | Trusted checker of the cone |
| `negative_vectors_rejected_in_coq` | Proved in Coq (`vm_compute`) |
| OCaml extract + `ocamlopt` | Untrusted; source identity only |
| `fragment_checker.c` | Hand-written C99 subset; **not** extracted |
| `gcc` fallback | Untrusted host compiler |
| `ccomp` (when present) | CompCert is a verified C compiler *for CompCert C*. Using it on this file still does **not** prove the C equals `check_*` |

When `ccomp` is used, the TCB *shrinks for that C file's compilation*,
and still includes: Coq, the C-to-checker correspondence (unproved),
libc/`strtoll`, OS, CPU.

## Known limits

- Fixed-width `long long` vs unbounded Coq `Z`. The rehearsal documents this with `asert_llong_overflow` (`te = -2^50`, `tau = 2^50`): Coq `Z.quot` is `-65536` (VALID); `te * 65536` overflows signed `long long`.
- Script classification is **not** in the C fragment (byte-list ladder).
- Full `GetNextWorkASERT` nBits and keccak PoW hash are **not** in the fragment.
- `ConnectBlock` / signatures / UTXO set lookup are **not** in the fragment.
- Stock Coq has **no** native C backend. C / Clight needs CertiCoq (or equal). The research sidecar produced CertiCoq Clight; that generator is **not** in the checking TCB.
- No instantiated Clight semantics proof, no applied CompCert refinement theorem (`PHASE7_PIPELINE_INSTANTIATED=false`).

## Future switch conditions (all required)

1. A licensed or CI-installable verified compiler (`ccomp` or CertiCoq/CakeML) is pinned and reproducible.
2. A CompCert-C / Clight generator from the Coq checkers exists (not a second hand-written oracle).
3. The generator's correspondence is stated as a Coq theorem or an explicit remaining gap.
4. Negative vectors still return `INVALID`.
5. Public flags stay `COMPILER_CORRECTNESS=false` and
   `BINARY_SEMANTIC_EQUIVALENCE=false` until a real compiler-correctness
   *and* source-refinement theorem is Closed.

Until then: **document the trust root; do not switch.**

3.1 implementation plan (Phase 0–8 done; listed TCB, not Level 5;
document freeze used the **narrow** sidecar sentence, not the
plan §10 instantiated-theorem paragraph):
`release/CertiCoqCompCertPlan.md`. Pin + license:
`release/CertiCoqToolchain.md` (`PIN_VERIFY=PASS`).
Surface: `certicoq/surface/` (`SURFACE_BUILD=PASS`,
`SURFACE_819_BUILD=PASS`, `OBSERVE_AGREE=PASS`,
`ORU_CLIGHT=PASS`, `PHASE5_CLIGHT=PASS`, `PHASE6=PASS`,
`PHASE7=PASS`, `PHASE8=PASS`, `PHASE9=ABORT`,
`PHASE10=ABORT_LEVEL5`,
`TASK018=CLIGHT_TO_CMINOR_CLOSED`).
That plan forbids promoting this rehearsal C file into the
CertiCoq path. The rehearsal `check_res` is also stale versus
RFC-020 (`draw_ok` / `coinbase` polarity).

## High-cost launch checklist (do not start without these)

This is the same gate as `release/OpenProperties.md` (OP-B1 / OP-E1).
It is a **start condition**, not a current claim.

| Prerequisite | Why it is blocking today |
|---|---|
| CompCert **license** (or a redistributable CI build of `ccomp`) | Decision written: `L-RESEARCH-SIDECAR-2026-09-12`. Still not vendored. Host `ccomp` remains ABSENT; sidecar switch has 3.13.1 |
| CI image with pinned `ccomp` **or** CertiCoq | Reference host has `ccomp: ABSENT` |
| Clight / CompCert-C **generator from Coq `check_*`** | Stock Coq has no C backend; `fragment_checker.c` is hand-written |
| Integer correspondence | Faithful extract uses unbounded `Z`; C rehearsal uses `long long` (`asert_llong_overflow`) |
| Extra theorems | At least: generator correspondence, or an explicit listed gap per omitted checker (script ladder, keccak, full ASERT nBits) |
| Resource estimate | License + CI pinning + generator: on the order of a dedicated workstream, not a one-day script change |
| Review rule | Do not flip `verified_compiler_in_current_tcb` or `COMPILER_CORRECTNESS` until items 1–5 exist *and* `Print Assumptions` on the new correspondence is Closed |

Forced switch without that list would be an overclaim.

Open properties: `release/OpenProperties.md`.
