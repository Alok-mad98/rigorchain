# TASK 3.1A result — CompCert Asm boundary audit

Measured 2026-09-13. Sidecar Coq 8.19.2, CompCert 3.13.
Native compiler **disabled** (fallback `vm_compute`).

```
COMPILER_CLOSED_ROUTE = BLOCKED_BY_TCB
CLIGHT_TO_CMINOR_CLOSED = PASS
CLIGHT_TO_ASM_CLOSED = NOT PROVED
LEVEL_5 = NOT CLAIMED
closed_compcert_check_oru = NOT ESTABLISHED
```

Detail: `audit/formal_bridge/compcert_execution_blockers_v2.md`.

SimplLocals first fail: `f_case_118` / `Sset $env_301` (param,
in `cenv`, not addr-taken). Unlift experiment makes SimplLocals
return OK on all `check_oru` internals; **no** Clight simulation.
Selection: `check_oru` Cminor has `Sswitch`; `compile_switch` is
a residual Parameter. `native_compute` is not a route.
