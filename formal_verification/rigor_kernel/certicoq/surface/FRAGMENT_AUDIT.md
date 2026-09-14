# CertiCoq fragment audit (Phase 4–5)

Filled against **CertiCoq 0.9+8.19** on sidecar Coq **8.19.2**
(2026-09-13), not against memory. Replay:
`certicoq/scripts/compile_oru.sh` then
`certicoq/scripts/compile_phase5.sh`.
INVALID / smoke Coq evaluation:
`certicoq/scripts/observe_surface.sh`.

`COMPILER_CORRECTNESS=false`
`BINARY_SEMANTIC_EQUIVALENCE=false`
`verified_compiler_in_current_tcb=false`
`PHASE6_CCOMP_NOT_RUN=true`

This is the rewrite list. It is not a compiler-correctness theorem,
not a CompCert link, and not a reason to flip flags.

## Toolchain measurement

| Probe | Result |
|---|---|
| Commands | `CertiCoq Compile -O 0` on each of the seven names |
| Host | sidecar `coqc` 8.19.2 under `OPAMROOT=certicoq/.opam-root` switch `rigor-certicoq-8.19` |
| Cone `/usr/bin/coqc` after compile | **8.15.0** (unchanged) |
| Host `ccomp` | **ABSENT** |
| Plugin | `From CertiCoq.Plugin Require Import CertiCoq` — **not** in `certicoq/surface/_CoqProject` |
| Surface Gallina rewrite | **none** (no pointwise equality required) |
| Clight units | seven (six `check_*` + `validate_kernel`) |

Extract files (not in `_CoqProject`):

| File | Constant |
|---|---|
| `ExtractCertiCoq.v` | `check_oru` (Phase 4) |
| `ExtractUtxo.v` | `check_utxo` |
| `ExtractPow.v` | `check_pow` |
| `ExtractAsert.v` | `check_asert` |
| `ExtractRes.v` | `check_res` |
| `ExtractScript.v` | `check_script` |
| `ExtractValidate.v` | `validate_kernel` |

Generated files (gitignored except the pin):

| File | Bytes | Notes |
|---|---:|---|
| `clight-oru/RigorSurface.Kernel.ORU.check_oru.c` | 64435 | Phase 4 |
| `clight-utxo/RigorSurface.Kernel.UTXO.check_utxo.c` | 148329 | `bytes_eqb` / `sum_values` / `nodupb` |
| `clight-pow/RigorSurface.Kernel.Pow.check_pow.c` | 276919 | boxed `shiftl` / `testbit` |
| `clight-asert/RigorSurface.Kernel.ASERT.check_asert.c` | 181456 | boxed `Z.quot` / `Z.mul` |
| `clight-res/RigorSurface.Kernel.RES.check_res.c` | 139120 | boxed `Z.shiftl`; `rt_coinbase`; no `draw_ok` |
| `clight-script/RigorSurface.Kernel.Script.check_script.c` | 2400295 | `is_push_only_n` accepted |
| `clight-validate/RigorSurface.Kernel.Validate.validate_kernel.c` | 2962992 | dispatcher; contains all six domains |
| `generated/SHA256` | — | sha256 of all `clight-*/*.{c,h}` |

Every header is `body(struct thread_info *)` plus `#include <gc_stack.h>`.
Imported constants name as `RigorSurface.Kernel.*.*.c`, not `Hello.f.c`.

Pin (replay the two compile scripts): see `certicoq/generated/SHA256`.

## Surface-wide computational path

`rg` over `certicoq/surface/**/*.v` (excluding this audit and
`Extract*.v` / `Observe.v`):

| Check | Result |
|---|---|
| `Axiom` / `Parameter` / `Admitted` | **none** |
| `Extract Constant` | **none** |
| Records / inductives | first-order (`kernel_tx`, `oru_tx`, `input_domain`, `tx`, …) |
| `Z` representation | boxed `BinInt.Z` throughout. No `ExtrOcamlZInt`. No CompCert `long long` lowering |

`Extract*.v` are **not** in `certicoq/surface/_CoqProject`.
8.15 `build_surface.sh` must not see the plugin.

## Per-checker measurement (CertiCoq 0.9+8.19)

| Checker | Axiom / `Extract Constant` | Recursion | Boxed `Z` ops | CertiCoq | Rewrite |
|---|---|---|---|---|---|
| `check_oru` | none | none (`to_oru` projection) | `<=?` `<?` `=?` | **accepted** `ORU_CLIGHT=PASS` | none |
| `check_utxo` | none | structural `bytes_eqb`, `sum_values`, `nodupb` | `+`, `<=?` via `money_range_b` | **accepted** `utxo_CLIGHT=PASS` | none |
| `check_pow` | none | none in `Pow.v` | `/` `mod` `Z.shiftl` `Z.shiftr` `Z.log2` `Z.testbit` | **accepted** `pow_CLIGHT=PASS` | none |
| `check_asert` | none | none | `<?` `=?` `*` `Z.quot` | **accepted** `asert_CLIGHT=PASS` `ASERT_BOXED_Z=TRUE` | none |
| `check_res` | none | none | `<?` `<=?` `*` `Z.shiftl` | **accepted** `res_CLIGHT=PASS` `RES_NO_DRAW_OK=TRUE` | none |
| `check_script` | none | fuel `Fixpoint is_push_only_n`; structural `split_at` | none (`nat` / `list byte`) | **accepted** `script_CLIGHT=PASS` | none |
| `validate_kernel` | none | `match` dispatcher | (calls the six) | **accepted** `validate_CLIGHT=PASS` | none |

`check_res` keeps `negb (coinbase s)` and has no `draw_ok`.
`is_push_only_n` was accepted as-is; no equivalent recursor was required.

### Clight inspection (not `fragment_checker.c`)

Every unit includes `gc_stack.h`, `thread_info`, and `garbage_collect`.
`compile_phase5.sh` refuses `draw_ok`, `res_W(`, and `fragment_checker`.

ASERT Clight names boxed ops (`CoqdZArithdBinIntDefdZdquot_uncurried_*`,
`Zdmul_*`). It is not `check_asert(long long …)`.

RES Clight names `Z.shiftl` and reads `rt_coinbase`. No `draw_ok`.

Script Clight defines `is_push_only_n_uncurried_*` (fuel + rest).
Not a `T_NONSTANDARD` stub.

Dispatcher Clight contains the six domains (UTXO `TxBool`, ORU
`oru_in`/`oru_out`, RES `mint_amount`/`burn_oru`, Script
`is_push_only_n` / `Classify`, Pow compact, ASERT `Z.quot`).

`ccomp` of every unit, including this Clight, is Phase 6
(`PHASE6=PASS`, 2026-09-13). `gcc` was not used on checker C
(`CERTICOQ_GCC_SMOKE=not_run`).

## Coq evaluation after compile (not Clight eval)

`Observe.v` `vm_compute` on both hosts after Phase 5
(`OBSERVE_8_15=PASS`, `OBSERVE_8_19=PASS`, `OBSERVE_AGREE=PASS`):

| Vector | Result |
|---|---|
| INVALID-001 `check_utxo` | `false` |
| INVALID-002 `check_oru` | `false` |
| INVALID-003 `check_res` | `false` |
| INVALID-004 `check_asert` | `false` |
| INVALID-005 `check_pow` | `false` |
| `invalid_script` (empty vs `T_WITNESS_V0`) | `false` |
| six positive smokes | `true` |
| `validate_kernel` on the same records | same bools |

This is host evaluation, not a CertiCoq / CompCert theorem.

`invalid_script` is surface-only. The cone boundary has no
INVALID-00x for script.

## Rewrites this phase

None. No `check_*_pre = check_*` lemmas were required.

## What this does not claim

- Compiler correctness / binary semantic equivalence
- CompCert *theorem* on the generated Clight (Phase 7; Phase 6 linked a binary)
- `ConnectBlock` / C++ / `rigord`
- That generated C is source-of-truth (it is a build artifact)
