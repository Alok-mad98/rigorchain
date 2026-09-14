# Phase 9 result — Level 5 close attempt

Measured 2026-09-13. Sidecar Coq 8.19.2, CertiCoq 0.9+8.19,
CompCert 3.13. Cone `/usr/bin/coqc` 8.15.0. Host `ccomp` ABSENT.

`COMPILER_CORRECTNESS=false`
`BINARY_SEMANTIC_EQUIVALENCE=false`
`verified_compiler_in_current_tcb=false`
`PHASE7_PIPELINE_INSTANTIATED=false`
`PHASE9=ABORT`
`MATURITY_STAYS=4.9`

Decision: **Abort** (I-1 / plan §10). Do not take Level 5.
Do not flip public flags.

## CompCert AST

| Step | Result |
|---|---|
| Phase 5 Clight pin of `check_oru` | MATCH (`generated/SHA256`) |
| Phase 6 `vector_driver` replay | `PHASE6_VECTORS=PASS` |
| sidecar `clightgen -csyntax` | RC=0 (warnings only); `prog : Csyntax.program` |
| sidecar `clightgen -clight` | RC=0 (warnings only) |
| Compile generated AST + `TryTransf.v` | PASS (8.19) |
| `Eval vm_compute in compiled_is_ok` (`transf_c_program prog`) | **TIMEOUT 120s** (unix 124). No `OK`/`Error` constructor obtained |
| `transf_c_program_correct` applied | **no** |

`ccomp` compiling the same `.c` is not `transf_c_program = OK`.
Print/parse (`print_Clight` Axiom → C → `clightgen`) remains TCB
even if compute later finishes.

Generated AST lives under `certicoq/generated/clightgen-oru/`
(not source-locked). Replay: `certicoq/scripts/compile_phase9_ast.sh`.

## LambdaANF / Codegen

| Step | Result |
|---|---|
| `LambdaANFTry.v` (toy `Ehalt`, no pipeline) | PASS |
| `From CertiCoq Require Import LambdaANF.toplevel_theorems` | PASS (no `print_Clight` Axiom) |
| `exp` of Gallina `check_oru` | **absent** |
| `correct` / `correct_cc` instantiated on `check_oru` | **no** |
| `Theorem` in `Codegen/*.v` | **none** (re-read 2026-09-13) |

Replay: `certicoq/scripts/compile_phase9_lambdaanf.sh`.

## Why Level 5 is not taken

I-1 requires both (a) Gallina–Clight correspondence and (b)
CompCert on that Clight, Closed. Phase 9 obtained neither.
§10 forbids a Level 5 number while Codegen / `print_Clight`
are load-bearing TCB.

OP-E1 / OP-B1 stay **Not proved**. Cone
`Audit/KernelNonClaims.v` `compiler_correctness` stays false.
No new Category A. No M129+.
