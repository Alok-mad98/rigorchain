# Phase 7 TCB residues (listed passes)

Filled against the **installed** CertiCoq **0.9+8.19** and CompCert
**3.13** trees on this sidecar (2026-09-13), not against the
website. These rows are trusted. They are **not** in the 8.15
cone checking TCB (`verified_compiler_in_current_tcb = false`).

`COMPILER_CORRECTNESS=false`
`BINARY_SEMANTIC_EQUIVALENCE=false`
`PHASE7_PIPELINE_INSTANTIATED=false`
`PHASE9=ABORT`
`PHASE10=ABORT_LEVEL5`
`PHASE10_PARTIAL=true`
`CLIGHT_TO_CMINOR_CLOSED=true`
`CLIGHT_TO_ASM_CLOSED=false`

## CertiCoq 0.9+8.19 (read in-tree)

| Pass / object | Where read | Instantiated on `check_*`? | TCB? |
|---|---|---|---|
| `CertiCoq_pipeline` | `Compiler/pipeline.v` | no | composition not instantiated |
| `check_axioms` | same | no (plugin ran it; we have no Coq proof it returned `[]` for our terms) | yes |
| `compile_LambdaBoxMut` | `LambdaBoxMut/toplevel.v` | no end-to-end theorem found on our terms | yes |
| `compile_LambdaBoxLocal` | `LambdaBoxLocal/toplevel.v` | no | yes |
| `compile_LambdaANF` / CPS | `LambdaANF/toplevel.v` | Phase 10: CPS conversion `vm_compute` **Ret** on `check_oru` without `pipeline.v`; `exp` exported; `well_scoped` / `correct` **not** Closed. Full ANF opts timed out (600s) | yes until instantiated |
| `compile_Clight` / `Clight_trans_ANF` | `Codegen/toplevel.v` | **no `Theorem` in `Codegen/*.v`** | **yes — load-bearing** |
| `print_Clight` | `Compiler/pipeline.v` | **`Axiom` realized in OCaml** | **yes — load-bearing** |
| GC `gc_stack.o` | Phase 6 link | clang TCB | yes (I-8) |
| prim-int / prim-float / FFI | Phase 6 link | clang TCB | yes (I-8) |

There is no published one-liner in this tree of the form
“Gallina `check_oru` equals the generated `*.c` file”.

## CompCert 3.13 (used as `ccomp`)

| Item | Instantiated? | TCB? |
|---|---|---|
| `ccomp` binary on the generated `.c` files (Phase 6) | used as a compiler, not as a Coq theorem | the run is evidence; the theorem is not |
| CompCert `semantic_preservation` / `transf_c_program_correct` on a Coq AST of our files | : `Cshmgen`+`Cminorgen` Closed on six checker Clight ASTs (bypass `print_Clight`). `SimplLocals` = `Error` (`bad temporary`). `Selection.sel_program` does not compute (oracles). Clight → Asm **not** Closed. Phase 9: `transf_c_program` timed out. | **yes — load-bearing after Cminor** |
| clang link of GC + driver + `libm` | yes (TCB link) | yes |
| `vector_driver.c` | trusted glue | yes |

`ccomp` accepted CompCert-C (warnings only). That is not
`compcert_applies_to_generated_clight` as a Closed Coq theorem.

## Already Closed (not TCB)

| Item | Where |
|---|---|
| `surface_check_*_eq` / `surface_validate_kernel_eq` | cone `Kernel/SurfaceCorrespondence.v` Category B |
| No CertiCoq-fragment rewrite | Phase 4–5; no `rewrite_check_eq` obligation |
| INVALID-001…005 still `false` under `vm_compute` | `Observe.v` / Phase 6 binary |

## Consequence

Maturity stays **4.9**. OP-E1 / OP-B1 stay **open**. Phase 10
did not close them. Do not take a Level 5 number. The public
sentence is the engineering one: CertiCoq generated this
Clight; `ccomp` compiled it; Codegen, `print_Clight`, GC,
driver, and the uninstantiated CompCert AST theorem remain
trusted.
