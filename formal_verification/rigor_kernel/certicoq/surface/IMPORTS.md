# Compile-surface import inventory (Phase 2)

Definitional dependencies of the six `check_*` functions and
`validate_kernel`. Theorem-only `Require`s of the cone are **not**
listed. `fragment_checker.c`, `Spend.v`, `Maturity.v`, and
`Kernel/Chain/*` are out of the first compile surface.

Surface modules live under `certicoq/surface/` with logical path
`RigorSurface`. They do not `Require RigorKernel`. Category A
proofs stay in the 8.15 cone.

`check_res` is the RFC-020 body mint (`negb coinbase`, no `draw_ok`).
Do not replace it with the stale C fragment.

## Surface files (this directory)

| Surface module | Role |
|---|---|
| `Core/Bytes.v` | `byte := nat`, `split_at` |
| `Core/Amount.v` | `COIN`, `MAX_MONEY`, `money_range_b` |
| `Supply/ORUModel.v` | `input_domain`, `oru_tx` |
| `Supply/KernelTx.v` | `kernel_tx`, `to_oru` (from `ORUConservation.v`) |
| `Supply/Poles.v` | `RES_MAX_SUPPLY`, poles, `res_mint_authorized`, `res_destroy_pole_sq` |
| `Supply/RESTx.v` | `res_tx`, `coinbase`, `mint_amount`, `burn_oru` (from `RESIssuance.v`) |
| `Script/ScriptTypes.v` | `Script`, `ScriptType`, `script_type_eqb` |
| `Script/Classify.v` | `classify_script` and its helpers (no theorems) |
| `Pow/Compact.v` | `compact_decode` / `compact_encode` and local helpers |
| `Pow/ASERTQ.v` | `ASERT_RADIX`, `exp_q_raw` (boxed `Z`; no `Uint256Model`) |
| `Kernel/TxBool.v` | `tx` / `valid_tx_b` (from `UTXOSoundness.v`) |
| `Kernel/UTXO.v` | `check_utxo` |
| `Kernel/ORU.v` | `valid_oru_tx_b`, `check_oru` |
| `Kernel/RES.v` | `check_res` |
| `Kernel/Script.v` | `ScriptState`, `check_script` |
| `Kernel/Pow.v` | `PowHeader`, `check_pow` |
| `Kernel/ASERT.v` | `ASERTInput`, `check_asert` |
| `Kernel/Validate.v` | `KernelInput`, `validate_kernel` |

## Cone sources the copies were taken from

| Gallina | Cone definition file | Surface home |
|---|---|---|
| `check_utxo` | `Kernel/UTXOValidator.v` | `Kernel/UTXO.v` |
| `valid_tx_b`, `tx`, `outpoint`, `txout`, `bytes_eqb`, `nodupb`, `inputs_fresh_b`, `sum_values` | `proofs/UTXO_Soundness/UTXOSoundness.v` | `Kernel/TxBool.v` |
| `check_oru`, `valid_oru_tx_b` | `Kernel/ORUValidator.v` | `Kernel/ORU.v` |
| `kernel_tx`, `to_oru` | `proofs/ORU_Conservation/ORUConservation.v` | `Supply/KernelTx.v` |
| `oru_tx`, `input_domain` | `Supply/ORUModel.v` | `Supply/ORUModel.v` |
| `check_res` | `Kernel/RESValidator.v` | `Kernel/RES.v` |
| `res_tx`, `coinbase`, `mint_amount`, `burn_oru` | `proofs/RES_Issuance/RESIssuance.v` | `Supply/RESTx.v` |
| `RES_MAX_SUPPLY`, `RES_DESTROY_POLE`, `RES_DESTROY_SHIFT`, `res_destroy_pole_sq`, `res_mint_authorized` | `Supply/SupplySchedule.v` | `Supply/Poles.v` |
| `check_script`, `ScriptState` | `Kernel/ScriptValidator.v` | `Kernel/Script.v` |
| `Script`, `ScriptType`, `script_type_eqb` | `Script/ScriptTypes.v` | `Script/ScriptTypes.v` |
| `classify_script` (through the classifier; no post-L209 theorems) | `Script/ScriptClassifier.v` | `Script/Classify.v` |
| `check_pow`, `PowHeader` | `Kernel/PowValidator.v` | `Kernel/Pow.v` |
| `compact_decode`, `compact_encode` | `Pow/CompactBits.v` | `Pow/Compact.v` |
| `check_asert`, `ASERTInput` | `Kernel/ASERTValidator.v` | `Kernel/ASERT.v` |
| `ASERT_RADIX`, `exp_q_raw` | `Pow/ASERTModel.v` | `Pow/ASERTQ.v` |
| `validate_kernel`, `KernelInput` | `Kernel/KernelSoundness.v` | `Kernel/Validate.v` |
| `byte`, `split_at` | `Core/Bytes.v` | `Core/Bytes.v` |
| `COIN`, `MAX_MONEY`, `money_range_b` | `Core/Amount.v` | `Core/Amount.v` |

`RATE` / display destroy cost is **not** a `check_res` input. `Uint256Model`
is **not** a definitional dependency of `exp_q_raw`.
`is_supported_witness` is unused by `classify_script` and is omitted.

## Correspondence (cone, not this directory)

`Kernel/SurfaceCorrespondence.v` maps cone records onto these copies
and proves pointwise `bool` equality (`surface_check_*_eq`,
`surface_validate_kernel_eq`). Category B. The copies are a
different inductive family, so the equalities are not
`surface_check_oru = check_oru` at the function type.

Phase 3 observational witnesses (`Observe.v`, `PORT_8_19.md`) are
not definitional dependencies. They are compiled after the surface
on each host. Not in `_CoqProject`.

Phase 4–6 `Extract*.v` are CertiCoq commands for the six
`check_*` functions, `validate_kernel`, and `phase6_run`.
Not in `_CoqProject`. Not definitional dependencies.

Phase 7 correspondence lives in `certicoq/proofs/` (cone 8.15
docs; no plugin). Not a definitional dependency.

## Out of surface

- Entire `_CoqProject.kernel-critical` proof cone
- `rigor_kernel` record packaging in `Kernel/KernelInterface.v`
  (the compiled dispatcher is `validate_kernel`)
- `Spend.v`, `Maturity.v`, `Kernel/Chain/*`
- `fragment_checker.c`
- CertiCoq plugin / CompCert / sidecar 8.19 rewrite (Phase 3)
