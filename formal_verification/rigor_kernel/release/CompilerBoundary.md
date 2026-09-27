# Compiler Boundary

Future Level 5 needs an instantiated verified-compiler path.
A **research sidecar** has generated CertiCoq Clight and compiled
it with sidecar `ccomp` 3.13. Correspondence theorems are **not**
instantiated. Every Level-5 item below remains **OPEN**.

**Public claim (unchanged):** Formally verified critical consensus
invariants inside a Coq consensus kernel model, with explicit
extraction and runtime trust boundaries.

`COMPILER_CORRECTNESS=false`
`VERIFIED_COMPILER_FORCED_SWITCH=FALSE`
`BINARY_SEMANTIC_EQUIVALENCE=false`
`verified_compiler_in_current_tcb = false`

## Engineering present, proof OPEN

| Item | Status |
|---|---|
| CertiCoq Clight of six `check_*` + `validate_kernel` | Present (sidecar; boxed `Z`) |
| Sidecar `ccomp` 3.13 compile + named-GC link | Present (`PHASE6=PASS`) |
| Gallina–Clight correspondence instantiated | **OPEN** (`PHASE7_PIPELINE_INSTANTIATED=false`) |
| Clight–assembly CompCert AST theorem applied | **OPEN** (`COMPILER_CLOSED_ROUTE=BLOCKED_BY_TCB`, TASK 3.1A). : Clight → Cminor Closed. TASK 3.1B: unlift one-step only; no checker simulation. Asm oracles remain |
| Verified compiler in the checking TCB | **OPEN** |
| Memory model | **OPEN** |
| Integer semantics (unbounded `Z` vs machine integers) | **OPEN** (artifact uses boxed `Z`) |
| Calling convention | **OPEN** |
| ABI proof | **OPEN** |
| Machine semantics | **OPEN** |

This file does **not** put CompCert or CertiCoq into the 8.15
auditor TCB. Host `ccomp` stays ABSENT.

Related: `release/VerifiedCompilerPath.md`,
`release/CertiCoqCompCertPlan.md`,
`release/FinalClaimsBoundary.md` (narrow sidecar note).
The Coq machine in `Execution/KernelMachine.v` is a
**model-level** small-step skeleton. It is not machine code.
The rehearsal `fragment_checker.c` is not the compile target.
