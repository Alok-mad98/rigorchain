# CertiCoq → CompCert Phase 6 report

Measured 2026-09-13. Replay: `certicoq/scripts/ccomp_link.sh`.

`COMPILER_CORRECTNESS=false`
`BINARY_SEMANTIC_EQUIVALENCE=false`
`verified_compiler_in_current_tcb=false`
`CERTICOQ_GCC_SMOKE=not_run`

This is an engineering binary, not a Level 5 theorem. Phase 7
is correspondence. Do not treat this file as
`verified_compiler_in_current_tcb = true`.

Decision id: `L-RESEARCH-SIDECAR-2026-09-12`.
CompCert 3.13 is INRIA Non-Commercial. `ccomp` is not on the
host PATH and is not a production compiler.

## Result

| Token | Value |
|---|---|
| `CCOMP` | PRESENT (sidecar only) |
| `CCOMP_VERSION` | CompCert C verified compiler, version 3.13 |
| `GC` | LINKED |
| `PHASE6` | PASS |
| `PHASE6_VECTORS` | PASS |
| `INVALID_001_005` | REJECTED |
| `SMOKES` | PASS |
| Host `/usr/bin/coqc` | 8.15.0 |
| Host `ccomp` | ABSENT |
| Checker C compiled by | sidecar `ccomp` (not gcc, not clang) |
| GC / prims / driver compiled by | `/usr/bin/clang` 14.0.0 (TCB) |

## What was compiled by `ccomp`

Each Phase 5 Clight unit produced `.s` and `.o` under
`certicoq/generated/ccomp/` (gitignored):

| Unit | Object | Bytes |
|---|---|---:|
| `check_oru` | `oru.o` | 12568 |
| `check_utxo` | `utxo.o` | 28352 |
| `check_pow` | `pow.o` | 46128 |
| `check_asert` | `asert.o` | 31224 |
| `check_res` | `res.o` | 25920 |
| `check_script` | `script.o` | 100696 |
| `validate_kernel` | `validate.o` | 193200 |
| `phase6_run` | `phase6.o` | 204456 |

`ccomp` accepted every unit. Warnings only (fall-through
returns; function-pointer stores into `value`). No unit was
rewritten to please CompCert. No `gcc` fallback.

ASERT stays boxed `Z` in the Clight that `ccomp` compiled.

## Linked binary

Path: `certicoq/generated/ccomp/vector_driver` (gitignored).
ELF x86-64, dynamically linked.

Objects in the **link** (named, as required):

| Object | Producer | Role |
|---|---|---|
| `phase6.o` | sidecar `ccomp` | CertiCoq Clight of `phase6_run` |
| `gc_stack.o` | clang | CertiCoq GC (TCB) |
| `prim_int63.o` | clang | CertiCoq prims (TCB) |
| `prim_floats.o` | clang | CertiCoq prims (TCB) |
| `coq_c_ffi.o` | clang | CertiCoq FFI (TCB) |
| `vector_driver.o` | clang | trusted print/exit glue (TCB) |
| `libm` (`-lm`) | system | `sqrt` for `prim_floats` (TCB) |

The seven per-checker `.o` files are **not** in this link
(`body` would collide). They are the required “`ccomp` each
unit” objects. The binary evaluates the same six checkers
because `phase6_run` calls them on the Observe literals.
`phase6.o` undefined symbol: `garbage_collect` only.

## Driver (TCB)

`certicoq/driver/vector_driver.c` is not the checker. It does
not `strtoll` boxed `Z`. It calls `make_tinfo` / `body` and
reads the 12 bool fields of `phase6_result`.

Records are the Gallina literals in `certicoq/surface/Observe.v`
(same INVALID-001…005 and smokes as
`Kernel/VerifiedCompilerBoundary.v` / `kernel_binary/test_vectors/`).

Coq `bool := true \| false`: `true` is constructor 0 = `Val_int(0)`
= 1 (matches `Hello.f.c`).

## Runtime verdicts

| Vector | Binary |
|---|---|
| INVALID-001 `check_utxo` | REJECTED |
| INVALID-002 `check_oru` | REJECTED |
| INVALID-003 `check_res` | REJECTED |
| INVALID-004 `check_asert` | REJECTED |
| INVALID-005 `check_pow` | REJECTED |
| `invalid_script` | REJECTED |
| six positive smokes | PASS |

## Phase 8 differential (document freeze)

The Phase 6 `vector_driver` and Coq `Observe.v` / `vm_compute`
use the same named literals (INVALID-001…005, `invalid_script`,
six positive smokes). Binary verdicts matched Coq
(`PHASE6_VECTORS=PASS`). The boxed-`Z` path did **not** hit the
named `asert_llong_overflow` case (`te = -2^50`, `tau = 2^50`
is a machine-`long long` gap; this artifact does not compile
ASERT as `long long`).

This is agreement on that Observe set, not a byte-compare of
the faithful OCaml extract against every kernel vector, and
not an instantiated compiler-correctness theorem.

Phase 8 froze claims with the **narrow** sidecar sentence
(`release/FinalClaimsBoundary.md`). No new Category A. No
M129+. Maturity stays 4.9.

## What this does not claim

- Compiler correctness / binary semantic equivalence
- That CompCert's theorem has been instantiated on this Clight
  (Phase 7)
- `ConnectBlock` / C++ / `rigord`
- That clang-compiled GC is verified
- That the seven unit `.o` files were dynamically linked into
  the one binary (they were separately `ccomp`'d)
