# Kernel Deepening Report (P0-120~125)

Formal kernel scope expanded. Kernel invariant coverage
strengthened. The public claim is unchanged.

**Public claim (unchanged):**

Formally verified critical consensus invariants
inside a Coq consensus kernel model,
with explicit extraction and runtime trust boundaries.

`FORMAL_KERNEL_PHASE_DEEPENING=PASS`

## 1. New theorem inventory

| Theorem | File | Category | Statement (honest) |
|---|---|---|---|
| `block_transition_utxo_soundness` | `proofs/UTXO_Soundness/UTXOTransitionSoundness.v` | A | Accepted `apply_block_transition` keeps unique inputs, `utxo_wf`, and output `money_range` |
| `script_trace_deterministic` | `Script/Semantics/ScriptTrace.v` | B | One classification step from a script is a function. Not a Script VM |
| `recognizer_trace_disjoint` | `Script/Semantics/ScriptTrace.v` | A | Mint and converge markers are distinct opcode classes |
| `hash_less_target_equivalent` | `Pow/TargetRelation.v` | B | `meets_target` unfolds to `hash_le_target`. Abstract digest only |
| `target_monotonic` | `Pow/TargetRelation.v` | A | Larger target still accepts a digest that met a smaller positive target |
| `pow_acceptance_monotonic` | `Pow/TargetRelation.v` | A | `difficulty_ok` is monotonic in the target. No SHA3 claim |
| `time_error_direction_preserved` | `Pow/ASERTBoundary.v` | A | Negative / zero / positive time-error use `Z.quot`; witness distinguishes `Z.div` |
| `epoch_supply_transition_sound` | `Supply/SupplyTransition.v` | A | Any epoch prefix accumulates `<= RES_MAX_SUPPLY`. Not a runtime `S_RES` reject |

Supporting kernel-subset definitions (not public-claim theorems):
`ScriptState`, `ScriptStep`, `ScriptTrace`, `meets_target`,
`exp_q_boundary`, `epoch_reward` / `epoch_minted` /
`supply_after_epochs`.

## 2. Category change

| Class | Before this phase | After this phase | Delta |
|---|---|---|---|
| A | 13 | 19 | +6 |
| B | 29 | 31 | +2 |
| C | 13 | 13 | 0 |

Canonical table: `release/TheoremClassification.md`.
Secondary A-set is identical: `evidence/TheoremClassification.md`.

No theorem was promoted from an existing weaker statement by
rewriting its meaning. New statements live in new files.

## 3. Mutation coverage (M1–M17)

| ID | Incorrect rule | Expected failure |
|---|---|---|
| M1–M12 | unchanged prior cases | unchanged |
| M13 | remove `transition_inputs_unique` guard | `block_transition_utxo_soundness` |
| M14 | mint marker classified as converge | `recognizer_trace_disjoint` |
| M15 | reverse `meets_target` comparison | `target_monotonic` |
| M16 | `Z.quot` → `Z.div` on `exp_q_boundary` | `time_error_direction_preserved` |
| M17 | inflate `epoch_minted` | `epoch_supply_transition_sound` |

Runner: `mutation/run_all_mutations.sh` (17 `EXPECTED_FAILURE`).

## 4. Trust boundary unchanged

`EXTRACTION_FORMAL_PROOF=source_identity_only`
`EXTRACTION_EQUIVALENCE_NOT_CLAIMED=TRUE`
`PRODUCTION_REFINEMENT_NOT_CLAIMED=TRUE`
`COMPILER_CORRECTNESS=false`
`BINARY_SEMANTIC_EQUIVALENCE=false`
`VERIFIED_COMPILER_FORCED_SWITCH=FALSE`
`CPP_EQUIVALENCE_NOT_CLAIMED=TRUE`
`FULL_CHAIN_NOT_CLAIMED=TRUE`

Not claimed: C++ refinement, binary equivalence, compiler
correctness, SHA3 / hash implementation, full Bitcoin Script VM,
runtime `S_RES` rejection (OP-D1), reorg / common-prefix (OP-R1).

`release/AUDIT_PACKAGE.md` is byte-identical
(`AUDIT_PACKAGE_UNCHANGED=TRUE`).

## 5. Public claim unchanged

The allowed public sentence is exactly the one at the top of this
file. Allowed commentary for this phase:

- Kernel invariant coverage strengthened.
- Formal kernel scope expanded.

Do not describe this phase as a Bitcoin-node, full-node, C++,
binary-equivalence, or compiler-correctness result.
