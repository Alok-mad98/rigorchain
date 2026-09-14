# KernelInput candidate generator (TASK-008-R)

Not a refinement proof. Not `MakeKernelInput`. Not Coq extraction.

```
consensus_trace.json
        |
        v
generate_kernel_candidate
        |
        v
KernelInputCandidate + candidate_hash
```

CLI: `tools/formal_bridge/generate_kernel_candidate.py`

## Input

A TASK-007 `consensus_trace.json` object. Required enough to read:

- `pow_result` (bool or null)
- `asert_result` (bool or null)
- `tx_results[]` with `function`, `inputs_checked`, `utxo_result`, `script_result`, `amount_result`, `fee_result`

Embedded `kernel_candidate` in the trace is **ignored**. The generator does not copy a hand-written record.

## Output

`KernelInputCandidate`:

| Field | Class | Rule |
|-------|-------|------|
| `ki_utxo_ok` | **DERIVED** | AND of `CheckTxInputs` rows with `inputs_checked` and `utxo_result`. Vacuous AND (no such row) → `true`. UTXO **set** is not in the trace (**MISSING** object). |
| `ki_script_ok` | **DERIVED** | AND of `script_result` on `CheckTransaction` / `VerifyRigorPredicate` / `VerifyWitnessProgram`, and `amount_result` on `CheckTransaction`. Vacuous AND → `true`. Witness stack is **MISSING**. |
| `ki_pow_ok` | **DIRECT** | `pow_result` copied. `null` stays `null` (header never reached / untraced). |
| `ki_asert_ok` | **DIRECT** | `asert_result` copied. `null` stays `null`. ASERT arithmetic is **MISSING**. |
| `ki_fees` | **DERIVED** | sum of `tx_results[].fee_result` (integer). |
| `ki_subsidy` | **MISSING** | always `null` — `EvaluateConnect` / `GetBlockSubsidy` not in the trace. |
| `ki_res_mint` | **MISSING** | always `null` — `EvaluateConnect` not in the trace. |
| `ki_incoming` | **MISSING** | always `null` — per-outpoint values not in the trace. |
| `ki_outgoing` | **MISSING** | always `null` — per-output values not in the trace. |
| `ki_burn` | **MISSING** | always `null` — two C++ burns, neither traced. |

No field is omitted. `field_class` repeats the table so MISSING stays visible.

`status` is `oracle_only` unless every `ki_*` is null (`not_reached`).

## candidate_hash

```
canonical_json(payload) → UTF-8 → SHA256 hex
```

`payload` is **only** the ten `ki_*` fields, keys sorted, `separators=(',', ':')`, `null` preserved.

`note`, `status`, `field_class`, and `candidate_hash` are **not** inside the hashed payload.

Same trace ⇒ same payload ⇒ same `candidate_hash`.

## Stability vs equivalence

| Statement | Status |
|-----------|--------|
| Generator is deterministic on a fixed trace | **BRIDGE_GENERATOR_STABILITY** (empirical) |
| Candidate equals Coq `KernelInput` at `ConnectBlock` | **CPP_KERNELINPUT_EQUIVALENCE = NOT PROVED** |

## Forbidden

Do not restore FORGE, MTH, historical RNG-as-protocol, ARDA, script-as-asset, or deleted dual-asset RFC into this object.
Do not edit `KernelInput.v` to match the generator.
