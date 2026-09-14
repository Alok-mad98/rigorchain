# Final Claims Boundary

P0-130 re-check of every public statement. No new consensus or
binary or compiler claim.

**Allowed public phrase (TRUE):**

> Formally verified critical consensus invariants
> inside a Coq consensus kernel model,
> with explicit extraction and runtime trust boundaries.

## TRUE (Coq kernel model only)

| Claim | Meaning | Cite |
|---|---|---|
| Coq kernel invariants | Category-A Closed theorems | `TheoremClassification.md` |
| economic conservation | no hidden creation; fees transfer; burn removes | `economic_no_hidden_creation` et al. |
| UTXO soundness | valid tx applies; inputs consumed; no in-model double-spend | `utxo_soundness` |
| RES schedule bounds | `RATE(k)` finite; mint inequality `R * RATE(k) <= B * RES_COIN`; derived aggregate cap `61345467819135` | `res_schedule_finite`, `res_total_issuance_capped` |
| ORU conservation | no hidden ORU; domain conservation | `no_hidden_inflation`, `oru_domain_conservation` |
| script classification subset | pairwise disjoint recognizers | `recognizer_pairwise_disjoint` |
| PoW abstract target relation | digest above target rejected over abstract `H` | `pow_valid_rejects_above_target` |
| ASERT integer model | `Z.quot` sign / clamp / matrix | `negative_time_error_preserved`, `asert_boundary_complete` |
| ASERT fixed-point width | cubic stays positive and under `2^64`, so the unsigned cast of `poly >> 48` cannot wrap | `asert_poly_pos`, `asert_poly_fits_uint64` |
| ASERT exponent split | shift count and remainder agree exactly where they meet: `factor 65536 = 2 * factor 0` | `factor_wrap_continuous` |
| RES reorg rewind (model) | disconnect orphan mints then apply a different suffix equals never applying the orphans; surviving pool excludes orphans | `res_reorg_rewind_eq`, `res_reorg_surviving_pool` |
| Most-work tip selection (model) | accumulated work is the sum of `GetBlockProof` = `2^256/(T+1)` after SetCompact guards; among a finite set of suffixes the maximal-work tip is selected and a tie keeps the active tip. Not k-deep, not `ActivateBestChain` | `block_proof_eq_div`, `chain_work_uses_block_proof`, `select_tip_maximal`, `select_tip_keeps_active`, `select_tip_binary`, `select_tip_total` |
| Coinbase / RES-mint maturity and spend-side MAX_MONEY | coinbase and body RES mint are unspendable before depth 100; inputs above MAX_MONEY are rejected; pool add that would exceed MAX_MONEY is None. Not an ORU supply cap, not dust, not ConnectBlock | `immature_coinbase_rejected`, `mature_coinbase_at_100`, `res_mint_uses_same_maturity`, `plain_transfer_no_maturity`, `input_over_max_rejected`, `pool_add_overflow_none` |
| Script spendability by class (model) | converge / null-data / nonstandard are unspendable; witness v0 / v1 and mint require abstract `sig_ok`. Not a VM, not Ed25519 unforgeability, not ConnectBlock | `converge_unspendable`, `nulldata_unspendable`, `nonstandard_unspendable`, `witness_requires_sig_ok`, `witness_sig_ok_spends`, `classifier_then_spend` |
| ORU rewind + closed-form subsidy | disconnect orphan ORU flows then apply a different suffix; fold of heights 0..n is n·128 | `oru_reorg_rewind_eq`, `oru_reorg_surviving_pool`, `oru_subsidy_fold_closed` |
| runtime boundary model | projection stays in kernel domain; invalid stays invalid | `runtime_projection_preserves_kernel_domain`, `runtime_failure_preserved` |

## FALSE (must not be claimed)

| Claim | Status |
|---|---|
| C++ equivalence | FALSE (`cpp_equivalence = false`) |
| Bitcoin equivalence | FALSE |
| full node verification | FALSE (`node_correctness = false`) |
| network correctness | FALSE |
| wallet correctness | FALSE |
| database correctness | FALSE (out of cone; never modelled) |
| compiler correctness | FALSE (`COMPILER_CORRECTNESS=false`) |
| CPU correctness | FALSE (not modelled) |
| crypto security | FALSE (`crypto_security = false`) |

Also FALSE: binary semantic equivalence, native binary verified,
full-chain verification, extraction operational correspondence,
CompCert in TCB.

## Forbidden keyword scan (entire `formal_verification/`)

These tokens must not appear as a **positive** claim:

| Token | Rule |
|---|---|
| `equivalence_proved` | only `= false` / NOT_CLAIMED |
| `compiler_verified` | only NOT_CLAIMED / false |
| `binary_verified` | only `compcert_binary_verified_kernel = false` / NOT_CLAIMED |
| `cpp_refinement` | only `cpp_refinement_proved = false` / `*_not_proved` / NOT_CLAIMED |
| `full_chain_verified` | must not appear as true; `full_chain_equivalence = false` |

`release/final_adversarial_audit.sh` enforces the scan.

Older cones under `formal_verification/coq/` and `rigor_v01/` define
the same flags as **false**. That is NOT_CLAIMED, not a proof.

## Flags that stay false

```
COMPILER_CORRECTNESS=false
BINARY_SEMANTIC_EQUIVALENCE=false
NATIVE_BINARY_VERIFIED   (not raised)
CPP_REFINEMENT           (not raised)
FULL_NODE_VERIFIED       (not raised)
CRYPTO_SECURITY_PROOF    (not raised)
EXTRACTION_FORMAL_PROOF=source_identity_only
PRODUCTION_REFINEMENT_NOT_CLAIMED=TRUE
```

Machine-checked: `Audit/KernelNonClaims.v`.

## Research sidecar note (not a Level 5 claim)

Narrow sentence only. **Do not** use the plan §10 paragraph that
says “the Gallina–Clight correspondence is the instantiated
CertiCoq theorem” — Phase 7 did not instantiate that theorem.

> The six Coq kernel checkers and `validate_kernel` generated
> boxed-`Z` Clight via CertiCoq 0.9+8.19. Sidecar CompCert
> `ccomp` 3.13 compiled that C and linked it with a named GC.
> The Gallina–Clight and Clight–assembly correspondence
> theorems were not instantiated. Codegen, `print_Clight`, GC,
> FFI, and the vector driver remain TCB. This is not
> `ConnectBlock`, not `rigord`, not C++ equivalence, and not a
> verified compiler in the checking TCB. Maturity stays 4.9.

`COMPILER_CORRECTNESS` / `BINARY_SEMANTIC_EQUIVALENCE` /
`verified_compiler_in_current_tcb` stay **false**.
`Audit/KernelNonClaims.v` is unchanged.

Phase 9 (`PHASE9=ABORT`): `clightgen` read `check_oru` C back
to a `Csyntax.program`; `transf_c_program` did not
`vm_compute` in 120s. Codegen still has no theorem. That is
**not** a Level 5 close.

Phase 10 (`PHASE10=ABORT_LEVEL5`, `PHASE10_PARTIAL=true`):
erase / LambdaBoxLocal / LambdaANF CPS computed `Ret`
without `pipeline.v`. No in-memory `Clight.program`
(ANF opts timeout; CPS Codegen `translate_funs`). CompCert
not applied. §10 not written. Maturity stays 4.9.

TASK-018 (`CLIGHT_TO_CMINOR_CLOSED=PASS`,
`CLIGHT_TO_ASM_CLOSED` not taken): sidecar `vm_compute` of
`Cshmgen` then `Cminorgen` is `OK` + `Qed` + Closed on the
six SHA-pinned checker Clight ASTs. `SimplLocals` is
`Error`. `Selection.sel_program` does not compute. No
`closed_compcert_check_oru`. That is **not** a Level 5
close. `refinement_proved` / `cpp_equivalence` stay false.

TASK 3.1A (`COMPILER_CLOSED_ROUTE=BLOCKED_BY_TCB`): exact
SimplLocals fail is `f_case_118` / `Sset $env_301`. Unlift
is an experiment without Clight simulation. `compile_switch`
does not `vm_compute`. No `closed_compcert_check_oru`.
Level 5 is **not** claimed.

TASK 3.1B: `unlift_sset_only_params` is still not semantic
preservation. One-step `Sset`/`Sassign` simulation exists
under an explicit environment relation. `store_params` is
stuck on official Clight1/Clight2 entries.
`check_oru` function/checker simulation is **not** proved.
SimplLocals on normalized `check_oru` computes OK (not Closed).
No `closed_compcert_check_oru`. Level 5 is **not** claimed.

TASK 3.1C: `FORWARD_SIMULATION_ROUTE=NOT_FEASIBLE_WITHIN_SCOPE`.
Official Clight1/Clight2 can host `unlift_function_core`, but a
real `forward_simulation` needs inverse SimplLocals (`Mem.inject`,
unequal `free_list`, all callees). `f_case_118` has 12 `Scall`s.
No `closed_compcert_check_oru`. Level 5 is **not** claimed.

TASK 3.2A: C++ production IR ≠ Coq `KernelInput`. Construction
refinement is **not** proved. Decode trailing/`MAX_SIZE`
divergences stay documented. `cpp_equivalence` /
`refinement_proved` stay false. Level 5 is **not** claimed.

TASK 3.2B: Coq-side `ConnectBlockObservables → KernelInput`
projection is Qed/Closed. That is **not** C++ refinement.
`COQ_SIDE_PROJECTION=PROVED`.
`CPP_KERNELINPUT_EQUIVALENCE` / `REFINEMENT_PROVED` stay false.
Level 5 is **not** claimed. C++ left half remains MISSING.

TASK 3.2C: C++ observables mapping is DEFINED (audit Coq-to-Coq).
Production ConnectBlock does **not** inhabit it.
`CPP_INCOMING_OUTGOING_EXTRACTION` is the first left-half hole.
`REFINEMENT_PROVED` stays false. Level 5 is **not** claimed.

TASK 3.2D: incoming/outgoing extraction is DEFINED (audit sums).
C++ `CheckTxInputs` does **not** inhabit it.
`REFINEMENT_PROVED` stays false. Level 5 is **not** claimed.

TASK 3.2E: `CppTxOruFlow` inhabit interface is DEFINED.
`REAL_CPP_INHABIT` is **not** proved.
`REFINEMENT_PROVED` stays false. Level 5 is **not** claimed.

TASK 3.2F: `real_cpp_checktxinputs_inhabits_flows` is a defined
[Prop], **not** a proved theorem. Decode / UTXO assumptions stay
NOT PROVED. `REFINEMENT_PROVED` stays false. Level 5 is **not** claimed.

TASK 3.2G: `STRICT_VALID_BLOCK_DOMAIN` is DEFINED.
`cpp_block_decode_refines_coq_decode` is STATED, **not** proved.
Trailing / `MAX_SIZE` / dummy-vin stay documented.
`REFINEMENT_PROVED` stays false. Level 5 is **not** claimed.

TASK 3.2H: `cpp_utxo_representation_refines` is STATED, **not**
proved. `CppAbstractView` remains a prefix ghost; the coin map
is an audit stand-in only. `REFINEMENT_PROVED` stays false.
Level 5 is **not** claimed.

TASK 3.2I: `cpp_named_accumulators_refines` is STATED, **not**
proved. Fees / burn / res_mint / subsidy sites are named only.
`REFINEMENT_PROVED` stays false. Level 5 is **not** claimed.

TASK 3.2J: `cpp_bool_flags_abstract_refines` is STATED, **not**
proved. The four KernelInput bools are control-flow abstractions,
not `kernel_in` fields. `REFINEMENT_PROVED` stays false.
Level 5 is **not** claimed.

TASK 3.2K: interface phase is COMPLETE. Proof phase is **not**
started. All ten KernelInput fields are covered as DEFINED or
STATED. `REFINEMENT_PROVED` stays false. Level 5 is **not** claimed.

TASK 3.3A: decode refinement is **PARTIAL**. Coq-side domain
lemmas are Closed. C++ `operator>>` inhabit is **not** proved.
`REFINEMENT_PROVED` stays false. Level 5 is **not** claimed.

TASK 3.3A2: `remaining_operator_shift_matches_coq_decode` is
still **PARTIAL**. Tight production-stream witness and split
lemmas are Closed. C++ `::Unserialize(CBlock)` inhabit is
**not** proved. Decode stays PARTIAL. `REFINEMENT_PROVED`
stays false. Level 5 is **not** claimed.

TASK 3.3A3: the four 3.3A2 residuals are still **PARTIAL**.
Coq CAmount / header representation lemmas are Closed. C++
`ser_readdata64` / header LE / `UnserializeTransaction`
inhabit is **not** proved. Decode stays PARTIAL.
`REFINEMENT_PROVED` stays false. Level 5 is **not** claimed.

TASK 3.3A4: integer / hash residuals 2A/2B/2C/4A/4B are still
**PARTIAL**. Coq little-endian specs and signed/unsigned
MSB-clear lemmas are Closed. C++ `ser_readdata32/64` and
`uint256::Unserialize` inhabit is **not** proved. Decode
stays PARTIAL. `REFINEMENT_PROVED` stays false. Level 5 is
**not** claimed.

TASK 3.3A5: decode leaf residuals are **FROZEN** as named
TCB-classified reader facts (no Coq `Axiom`). The main
decode theorem is **not** closed under those facts.
`CPP_BLOCK_DECODE_REFINEMENT` stays **PARTIAL**.
`REFINEMENT_PROVED` stays false. Level 5 is **not** claimed.

TASK 3.3A6: under those leaf premises, Coq spec composition
of header / nValue / tx / CBlock is Closed. The main theorem
stays **STILL_PARTIAL** (CompactSize / CScript / vector /
`UnserializeTransaction` / production-bool inhabit).
`CPP_BLOCK_DECODE_REFINEMENT` stays **PARTIAL**.
`REFINEMENT_PROVED` stays false. Level 5 is **not** claimed.

TASK 3.3A7: CompactSize / CScript / vector Coq composition
is Closed. The three C++ inhabit residuals stay **PARTIAL**.
Main theorem **STILL_PARTIAL**. `REFINEMENT_PROVED` stays
false. Level 5 is **not** claimed.

TASK 3.3A8: transaction control flow / CBlock Coq composition
is Closed. The four C++ inhabit residuals stay **PARTIAL**.
Main theorem **STILL_PARTIAL**. `REFINEMENT_PROVED` stays
false. Level 5 is **not** claimed.

TASK 3.3A9: production-bool / loose `operator>>` Coq inhabit-glue
is Closed. The two C++ inhabit residuals stay **PARTIAL**.
Main theorem **STILL_PARTIAL**. `REFINEMENT_PROVED` stays
false. Level 5 is **not** claimed.

TASK 3.3A10: decode boundary is **FROZEN** at PARTIAL.
`NEXT_ACTIVE = 3.3B_UTXO_REPRESENTATION`. UTXO proofs are
**not** started. `REFINEMENT_PROVED` stays false. Level 5 is
**not** claimed.

TASK 3.3B: `cpp_utxo_representation_refines` is **PARTIAL**.
Coq snapshot / prefix / ORU-sum is Closed. C++ cache inhabit
is **not** proved. Decode stays **FROZEN**. `REFINEMENT_PROVED`
stays false. Level 5 is **not** claimed.
