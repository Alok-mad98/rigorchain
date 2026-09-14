# Kernel Completeness Report (P0-126~130)

Kernel safety coverage expanded.
Consensus kernel model completeness strengthened.

**Public claim (unchanged):**

Formally verified critical consensus invariants
inside a Coq consensus kernel model,
with explicit extraction and runtime trust boundaries.

`FORMAL_KERNEL_PHASE_COMPLETENESS=PASS`

## 1. Existing theorem inventory

Category A before this phase (19): `utxo_soundness`,
`no_hidden_inflation`, `oru_conservation_extended`,
`res_issuance_soundness`, `recognizer_pairwise_disjoint`,
`pow_valid_rejects_above_target`, `compact_decode_lower_bound`,
`negative_time_error_preserved`, `exp_q_clamped`, `factor_range`,
`res_schedule_finite`, `res_total_issuance_capped`,
`block_transition_utxo_soundness`, `recognizer_trace_disjoint`,
`target_monotonic`, `pow_acceptance_monotonic`,
`time_error_direction_preserved`, `epoch_supply_transition_sound`,
`kernel_sound`.

Category B before this phase: 31 supporting lemmas (classifier
packaging, extraction source identity, u256 helpers, P0-120 safety
corollaries, `script_trace_deterministic`,
`hash_less_target_equivalent`).

Canonical table: `release/TheoremClassification.md`.

## 2. New theorem inventory

| Theorem | File | Category | Honest statement |
|---|---|---|---|
| `transaction_value_conservation` | `Kernel/Transaction/Validity.v` | A | `ValueFlow` ⇒ inputs ≥ outputs + fee |
| `transaction_no_negative_value` | `Kernel/Transaction/Validity.v` | A | `ValueFlow` ⇒ all legs and fee non-negative |
| `transaction_fee_separation` | `Kernel/Transaction/Validity.v` | A | fee is residual, not a mint |
| `block_acceptance_sound` | `Kernel/Block/Acceptance.v` | A | accepted block has UTXO + PoW + supply + script conjuncts |
| `economic_no_hidden_creation` | `Kernel/EconomicInvariant/Economic.v` | A | new = old + subsidy + fees − burn |
| `fees_only_transfer` | `Kernel/EconomicInvariant/Economic.v` | A | fees appear once as a transfer term |
| `burn_only_removal` | `Kernel/EconomicInvariant/Economic.v` | A | burn is a removal term |
| `subsidy_schedule_bound` | `Kernel/EconomicInvariant/Economic.v` | A | subsidy ≤ `ORU_INITIAL_BLOCK_SUBSIDY` |
| `script_step_deterministic` | `Script/Semantics/Execution.v` | A | unique next kernel exec state |
| `script_execution_preserves_class` | `Script/Semantics/Execution.v` | A | step yields `classify_script` |
| `script_invalid_state_rejected` | `Script/Semantics/Execution.v` | B | reject stays reject |
| `pow_pipeline_sound` | `Pow/Acceptance.v` | A | decode + abstract compare. No SHA3 |
| `asert_boundary_complete` | `Pow/ASERTMatrix.v` | A | neg / zero / pos / large use `Z.quot` |

Block acceptance is **model composition** only. It does not prove a
node, a reorg, or ConnectBlock.

## 3. Category A/B/C update

| Class | Before this phase | After this phase | Delta |
|---|---|---|---|
| A | 19 | 31 | +12 |
| B | 31 | 32 | +1 |
| C | 13 | 13 | 0 |

Secondary A-set in `evidence/TheoremClassification.md` is identical.

## 4. Mutation coverage (M1–M23)

| ID | Incorrect rule | Expected failure |
|---|---|---|
| M1–M17 | prior deepening / inventory cases | unchanged |
| M18 | allow output value > input value | `transaction_value_conservation` |
| M19 | remove one acceptance condition (PoW) | `block_acceptance_sound` |
| M20 | fees mint coins | `economic_no_hidden_creation` |
| M21 | nondeterministic opcode transition | `script_step_deterministic` |
| M22 | invert pipeline acceptance | `pow_pipeline_sound` |
| M23 | `Z.quot` → `Z.div` on `matrix_quot` | `asert_boundary_complete` |

Runner: `mutation/run_all_mutations.sh` (23 `EXPECTED_FAILURE`).
M18–M23: `mutation/mutate_completeness.sh`.

## 5. Trust boundary unchanged

`EXTRACTION_FORMAL_PROOF=source_identity_only`
`EXTRACTION_EQUIVALENCE_NOT_CLAIMED=TRUE`
`PRODUCTION_REFINEMENT_NOT_CLAIMED=TRUE`
`COMPILER_CORRECTNESS=false`
`BINARY_SEMANTIC_EQUIVALENCE=false`
`VERIFIED_COMPILER_FORCED_SWITCH=FALSE`
`CPP_EQUIVALENCE_NOT_CLAIMED=TRUE`
`FULL_CHAIN_NOT_CLAIMED=TRUE`

Not claimed: C++ refinement, binary semantic equivalence, verified
compiler, SHA3 / hash implementation, full Script VM, runtime
`S_RES` rejection (OP-D1), reorg / common-prefix (OP-R1), a node.

`release/AUDIT_PACKAGE.md` is byte-identical
(`AUDIT_PACKAGE_UNCHANGED=TRUE`).

## 6. OpenProperties update

`release/OpenProperties.md` still lists liveness, fairness,
finality, reorg, compiler correctness, extraction refinement, and
binary equivalence as **open**. This phase adds intra-model
composition only. It does not move those items to Closed.

See the P0-126~130 note in that file.
