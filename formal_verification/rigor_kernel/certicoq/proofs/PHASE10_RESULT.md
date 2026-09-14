# Phase 10 result — partial pipeline, Level 5 still Abort

Measured 2026-09-13. Sidecar Coq 8.19.2, CertiCoq 0.9+8.19,
CompCert 3.13. Cone `/usr/bin/coqc` 8.15.0. Host `ccomp` ABSENT.

`COMPILER_CORRECTNESS=false`
`BINARY_SEMANTIC_EQUIVALENCE=false`
`verified_compiler_in_current_tcb=false`
`PHASE7_PIPELINE_INSTANTIATED=false`
`PHASE9=ABORT`
`PHASE10=ABORT_LEVEL5`
`PHASE10_PARTIAL=true`
`MATURITY_STAYS=4.9`

Decision: **do not take Level 5.** Do not write plan §10.
Do not flip public flags. This phase ran the five partial
steps. Neither Gallina–Clight nor Clight–assembly is
`Print Assumptions` Closed.

This sidecar does not have `native_compute` (configure
disabled it; eval falls back to `vm_compute`).

## 1. In-memory Clight (no `print_Clight`)

| Stage | `vm_compute` of `Ret`/`Err` | Time / RC |
|---|---|---|
| Quote `check_oru` (no `pipeline.v`) | compiled | ~4s, RC=0 |
| `compile_LambdaBoxMut` (erase) | **Ret** (`true`) | ~10s, RC=0 |
| `compile_LambdaBoxLocal` | **Ret** (`true`) | ~8s, RC=0 |
| `compile_LambdaANF_CPS` | **Ret** (`true`) | ~9s, RC=0 |
| `compile_LambdaANF` (opts) | **TIMEOUT** | 600s, RC=124 |
| `compile_Clight` after CPS (skip opts) | **Err** | ~20s, RC=0 |
| Full `compile` / `compiled_ok` | TIMEOUT (Phase 10 eval) | 600s, RC=124 |

CPS→Clight error string (only the `Err` payload):

```
LambdaANF_to_Clight: Failure in make_defs:translate_funs
```

Official CertiCoq runs `compile_LambdaANF` before Codegen.
That pass did not finish in 600s, so there is **no**
in-memory `Clight.program` from the Gallina pipeline.

Files: `certicoq/proofs/sidecar819/CheckOruQuote.v`,
`CheckOruNoPipeline.v`, `CheckOruStage*.v`.
Replay: `certicoq/scripts/compile_phase10_nopipeline.sh`,
`compile_phase10_stages.sh`.

`Axiom print_Clight` was not required. `Print Assumptions`
of the Phase 10 cone flags is Closed (documentation only).

## 2. CompCert on that Clight

Blocked. There is no memory `Clight.program` from step 1.
`transf_clight_program` was not applied. A `ccomp` exit
code is still not the theorem.

Phase 9 already measured `clightgen` → `transf_c_program`
timeout at 120s on the print/parse AST. That path is TCB
(`print_Clight` Axiom → C → `clightgen`) and is not step 1.

## 3. `exp_of_check_oru` without `pipeline.v`

| Item | Result |
|---|---|
| `Compiler.pipeline` loaded | **no** |
| `exp_of_check_oru_opt` (`anf_cps_stage`) | **Some** (`true`) |
| `LambdaANF.toplevel_theorems` in scope | yes |
| `unique_bindings` `vm_compute` → `True` | **no** (residual term) |
| `correct` / `correct_cc` on `check_oru` | **no** (needs `well_scoped`) |

`correct` / `correct_cc` are properties of ANF *transforms*,
not Gallina–Clight. Exporting the CPS `exp` does not close
Codegen.

## 4. Codegen — follow upstream

Installed `Codegen/*.v` (CertiCoq 0.9+8.19): **no `Theorem`**.
CertiRocq wiki (2026-09-13): λANF^C → Clight still
*Proof underway* (legacy CPS generator and full λANF^C).
This tree did **not** add a handwritten generator.

Cone: `certicoq/proofs/CodegenUpstream.v`.

## 5. Claims

I-1 requires both (a) Gallina–Clight and (b) CompCert on
that Clight, Closed. Phase 10 obtained neither.

| Claim | Status |
|---|---|
| Plan §10 instantiated sentence | **not written** |
| New compiler flag names | **not added** |
| Maturity 5 | **not taken** |
| `compiler_correctness` | stays `false` |
| OP-E1 / OP-B1 | stay **Not proved** |

Public sentence stays the Phase 8 narrow one. Maturity
stays **4.9**.
