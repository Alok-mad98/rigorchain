# TASK-018 result — Closed Clight → CompCert probe

Measured 2026-09-13. Sidecar Coq 8.19.2, CompCert 3.13.
Cone `/usr/bin/coqc` 8.15.0. Host `ccomp` must stay ABSENT
outside the sidecar subshell.

```
CLIGHT_TO_CMINOR_CLOSED = PASS
CLIGHT_TO_ASM_CLOSED = NOT
closed_compcert_check_oru = NOT ESTABLISHED
LEVEL_5 = NOT CLAIMED
COMPILER_CORRECTNESS = false
BINARY_SEMANTIC_EQUIVALENCE = false
verified_compiler_in_current_tcb = false
REFINEMENT_PROVED = false
CPP_KERNELINPUT_EQUIVALENCE = NOT PROVED
```

## What Closed

All six SHA-pinned checkers, after `clightgen -clight` of the
existing CertiCoq C (no CertiCoq regen):

`Cshmgen.transl_program` then `Cminorgen.transl_program`
compute to `OK`, with `Qed` and `Print Assumptions` =
**Closed under the global context**.

`print_Clight` was bypassed (`Compiler.v` not required).

## What did not Close

- Official first pass `SimplLocals.transf_program` computes to
  `Error` (`bad temporary`) on every `check_*` Clight.
- `Selection.sel_program` does not compute (oracle Parameters).
- Therefore Clight → Asm is **not** Closed.
- `closed_compcert_check_oru` was not added.

Detail: `audit/formal_bridge/compcert_execution_blockers_v1.md`.
