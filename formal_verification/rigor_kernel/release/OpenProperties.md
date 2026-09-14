# Open Properties

Properties that this cone has **not** formally proved.

Nothing listed here is implied by a passing `run_kernel_critical.sh`.
Category-A theorems remain safety / invariant results inside a Coq
consensus kernel model.

**Public claim (unchanged):** Formally verified critical consensus
invariants inside a Coq consensus kernel model, with explicit
extraction and runtime trust boundaries.

`EXTRACTION_FORMAL_PROOF=source_identity_only`
`EXTRACTION_EQUIVALENCE_NOT_CLAIMED=TRUE`
`PRODUCTION_REFINEMENT_NOT_CLAIMED=TRUE`
`COMPILER_CORRECTNESS=false`
`BINARY_SEMANTIC_EQUIVALENCE=false`

Priority: P0 = blocks a stronger public claim; P1 = protocol-level
gap; P2 = engineering / ecosystem.

Cost class (P0-119 inventory): **LOW** = optional local kernel
enhancement; **MEDIUM** = dedicated engineering; **HIGH** =
CompCert / seL4-scale. Cost is **not** a promise of work.

| Cost | Items | Continue? |
|---|---|---|
| LOW | Further intra-model corollaries (like OP-X1…X3); documentation honesty | Optional. Do not inflate Category A |
| MEDIUM | Script VM cone (OP-S1); larger differential fragment (still not refinement) | New workstream if resourced |
| HIGH | Compiler correctness; extraction refinement; binary equivalence; C++ / full-node refinement; network liveness; reorg | Not the next increment of *this* cone |

Closed OP-X1…X3 stay Closed. They do not move an OPEN item to LOW
“almost proved”.

## P0-130 named HIGH registry

Must remain listed for adversarial audit. Status is **open** unless
marked Closed below.

| Item | Status | Cost |
|---|---|---|
| OP-R1 reorg / common-prefix | Still open | HIGH |
| OP-R2 implementation refinement | Still open (Coq→Coq Closed) | HIGH |
| OP-L1 liveness | Not formalized | HIGH |
| OP-D1 runtime `S_RES` / ConnectBlock rejection | Open by design (not a missing kernel bug) | HIGH if misread as a kernel hole |
| Compiler correctness | Not proved (`COMPILER_CORRECTNESS=false`) | HIGH |
| Binary semantic equivalence | Not proved (`BINARY_SEMANTIC_EQUIVALENCE=false`) | HIGH |
| CPU semantics | Not modelled | HIGH |
| Crypto proof | Assumptions only (`crypto_security = false`) | HIGH |

OP-L1 is the liveness entry. OP-D1 keeps its historical meaning
(runtime `S_RES` reject). Do not rename IDs.

CPU semantics is the hardware/ISA gap behind OP-B1 (compiler /
binary) and the TCB OPEN row.

## Inventory

### OP-L1 — Liveness

| | |
|---|---|
| Status | **Not formalized.** No liveness theorem in the cone |
| Cost | **HIGH** |
| Depends on | Scheduler / miner / network progress assumptions that are not modelled |
| Priority | P1 |
| Relation | Existing proofs are safety-shaped (`utxo_soundness`, conservation, issuance bounds). They do not say that a valid block is ever produced or that a transaction is ever confirmed |

### OP-L2 — Fairness

| | |
|---|---|
| Status | **Not formalized.** No miner / fee / draw fairness theorem |
| Cost | **HIGH** |
| Depends on | Hashrate, fee market, draw-eligibility interpretation (C-4 remains a hypothesis) |
| Priority | P1 |
| Relation | `pow_asert_correctness` is Category B function-ness of the ASERT *model*, not fairness of mining. Class A is `negative_time_error_preserved`. `rt_draw_ok` is an input bit, not a proved lottery |

### OP-L3 — Liveness under a network assumption

| | |
|---|---|
| Status | **Not formalized.** No P2P, delay, or partial-synchrony model |
| Cost | **HIGH** |
| Depends on | Network timing, eclipse resistance, relay policy |
| Priority | P1 |
| Relation | `network_correctness = false`. The kernel is a single-transition predicate, not a message automaton |

### OP-L4 — Finality under a network assumption

| | |
|---|---|
| Status | **Not formalized.** No k-deep / checkpoint / economic-finality theorem |
| Cost | **HIGH** |
| Depends on | Reorg policy, confirmation depth, attacker hashrate bound |
| Priority | P1 |
| Relation | No chain-selection semantics in the cone. Confirmations are a node / wallet concern |

### OP-R1 — Reorg / common-prefix safety

| | |
|---|---|
| Status | **Still open** |
| Cost | **HIGH** |
| Depends on | Honest-majority or other blockchain assumption; fork-choice rule; a model of two tips |
| Priority | P1 |
| Relation | RFC-020 RES rewind is a dual-supply issuance identity. Most-work selection (`select_tip_maximal`, `more_work_wins`, `chain_work_uses_block_proof`) is a finite-tip comparison on `GetBlockProof` increments; it has **no** k-deep bound and no honest-majority hypothesis. OP-X1 rejects a *single* candidate block whose collected inputs are not unique. Full reorg safety is **not** a corollary of any of these — blocker is the missing k-deep / honesty model |

### OP-X1 — Intra-block duplicate-input rejection (P0-120)

| | |
|---|---|
| Status | **Closed.** `apply_block_accepted_inputs_unique` / `apply_block_rejects_duplicate_input` in `Kernel/SafetyExtensions.v` |
| Cost | Done (was LOW) |
| Depends on | Existing `apply_block` / `nodupb_ok` (already in the cone) |
| Priority | Done (local safety only) |
| Relation | Category B (definitional unfolding of the `nodupb` guard). Main statement: `apply_block u b = Some u' → NoDup (collect_inputs b)`. Not ConnectBlock, not a reorg bound, not Category A |

### OP-E1 — Full extraction refinement

| | |
|---|---|
| Status | **Not proved.** Source identity only (`extraction_source_equivalence`). Phase 7 listed CertiCoq residues (`print_Clight` Axiom, Codegen unproved). Phase 9 Abort; Phase 10: CPS `exp` without `pipeline.v`, no mem Clight, no Gallina–Clight instance; §10 Closed sentence is **not** taken |
| Cost | **HIGH** |
| Depends on | Coq extraction correctness, integer mapping, OCaml operational semantics |
| Priority | P0 (must not be claimed) |
| Relation | `EXTRACTION_FORMAL_PROOF=source_identity_only`. Compiler and runtime stay outside the proof stop |

### OP-R2 — Concrete implementation refinement

| | |
|---|---|
| Status | **Still open.** Coq abstract → Coq concrete is Closed (`concrete_step_refines_abstract_step`). Implementation correspondence is not. TASK 3.2A: C++ `EvaluateConnect` builds `RigorConsensusKernelConnectInput` (supply slice). It does not construct `UnifiedTransition.KernelInput`. First named gap: `CPP_KERNELINPUT_CONSTRUCTION_REFINEMENT`. TASK 3.2B: Coq-side target `coq_connect_observables_project_to_kernelinput` is Qed/Closed (`COQ_SIDE_PROJECTION=PROVED`). TASK 3.2C: `cpp_connect_observables_related` is DEFINED. TASK 3.2D: incoming/outgoing extraction is DEFINED. TASK 3.2E: `cpp_checktxinputs_inhabits_flows` interface DEFINED. TASK 3.2F: inhabit [Prop] DEFINED; `REAL_CPP_INHABIT` NOT PROVED. TASK 3.2G: `STRICT_VALID_BLOCK_DOMAIN` DEFINED; decode relation STATED (NOT PROVED). TASK 3.2H: UTXO representation STATED (NOT PROVED). TASK 3.2I: named accumulators STATED (NOT PROVED). TASK 3.2J: bool flags STATED (NOT PROVED). TASK 3.2K: interface COMPLETE. TASK 3.3A: decode PARTIAL (Coq-side Closed; C++ operator>> remaining). TASK 3.3A2: remaining_operator_shift still PARTIAL; tight stream witness Closed; C++ Unserialize inhabit remains. TASK 3.3A3: four residuals still PARTIAL; Coq CAmount/header representation Closed; C++ integer-LE inhabit remains. TASK 3.3A4: 2A/2B/2C/4A/4B still PARTIAL; Coq LE spec + MSB-clear Closed; C++ ser_readdata inhabit remains. TASK 3.3A5: decode leaves FROZEN as TCB-classified reader facts (no Axiom); main decode theorem NOT CLOSED. TASK 3.3A6: leaf-TCB composition lift STILL_PARTIAL (spec composition Closed; CompactSize / UnserializeTransaction / inhabit remaining). TASK 3.3A7: CompactSize/script/vector Coq composition Closed; C++ inhabit PARTIAL; main theorem STILL_PARTIAL. TASK 3.3A8: tx control flow / CBlock Coq composition Closed; four inhabit residuals PARTIAL; main theorem STILL_PARTIAL. TASK 3.3A9: production-bool / loose operator>> Coq inhabit-glue Closed; C++ inhabit PARTIAL; main theorem STILL_PARTIAL. TASK 3.3A10: decode FROZEN at PARTIAL; NEXT_ACTIVE = 3.3B_UTXO_REPRESENTATION; UTXO proofs not started. TASK 3.3B: UTXO representation PARTIAL (snapshot/sum Coq Closed; C++ cache inhabit remaining). Synthesis / incoming inhabit still NOT PROVED |
| Cost | **HIGH** |
| Depends on | C++ `ConnectBlock`, extraction correctness, compiler correctness, ABI / memory model |
| Priority | P0 (must not be claimed) |
| Relation | `release/ConcreteRefinementBoundary.md`. PROVED: Coq-to-Coq projection. NOT CLAIMED: C++ implementation refinement, node refinement, compiler correctness |

### OP-P1 — Production-code refinement

| | |
|---|---|
| Status | **Not proved.** Differential verification of a comparable fragment only. Coverage expanded (structured gap vectors + random); status unchanged |
| Cost | **HIGH** (full ConnectBlock). Fragment-only extra vectors: MEDIUM |
| Depends on | C++ `ConnectBlock`, scripts, sigs, UTXO set, keccak PoW hash, full ASERT nBits |
| Priority | P0 (must not be claimed) |
| Relation | `PRODUCTION_REFINEMENT_NOT_CLAIMED=TRUE`. Map: `RefinementMapping.md`. Report: `evidence/DiffVerifyReport.md` |

### OP-C1 — Cryptographic primitive security

| | |
|---|---|
| Status | **Not proved.** Assumptions, not theorems |
| Cost | **HIGH** |
| Depends on | SHA3 / keccak / SHA256 collision and preimage; Ed25519 EUF-CMA |
| Priority | P0 (must not be claimed) |
| Relation | Inventory: `Kernel/CryptoAssumptions.v`. PoW digest is abstract `H` |

### OP-B1 — Compiler / binary semantic equivalence

| | |
|---|---|
| Status | **Not proved.** Verified-compiler path is documented, not switched. Phase 9: `clightgen` read `check_oru` C back; `transf_c_program` timed out (120s). Phase 10: no in-memory Clight from the Gallina pipeline (`translate_funs` / ANF timeout); `transf_clight_program` not applied. TASK-018: six SHA-pinned checker Clight ASTs compute `Cshmgen`+`Cminorgen` to `OK` (`Qed`, Closed). TASK 3.1A: `COMPILER_CLOSED_ROUTE=BLOCKED_BY_TCB` — SimplLocals first fail is `f_case_118` `Sset $env_301`; `compile_switch` is a residual Parameter on `Sswitch`; native_compute disabled. TASK 3.1B: `unlift` one-step `Sset`/`Sassign` under a stated relation; `store_params` stuck on official Clight entries; `check_oru` simulation **not** proved. TASK 3.1C: `unlift_function_core` `forward_simulation` is `NOT_FEASIBLE_WITHIN_SCOPE` (12 `Scall`s + single `function_entry`; `free_list` needs inverse SimplLocals). Clight → Asm is **not** Closed. Maturity stays 4.9 |
| Cost | **HIGH** |
| Depends on | CompCert / CertiCoq / CakeML plus a Clight (or equal) generator from Coq |
| Priority | P0 (must not be claimed) |
| Relation | `COMPILER_CORRECTNESS=false`, `BINARY_SEMANTIC_EQUIVALENCE=false`. Path: `VerifiedCompilerPath.md` |

### OP-N1 — Network / eclipse / relay safety

| | |
|---|---|
| Status | **Not formalized** |
| Cost | **HIGH** |
| Depends on | P2P graph, address manager, compact block relay |
| Priority | P2 |
| Relation | `network_correctness = false` |

### OP-W1 — Wallet / signing policy

| | |
|---|---|
| Status | **Not formalized** |
| Cost | **HIGH** (out of cone; do not start here) |
| Depends on | Wallet format, key hygiene, PSBT / descriptor policy |
| Priority | P2 |
| Relation | `wallet_correctness = false`. Out of this cone by construction |

### OP-S1 — Script VM / signature verification

| | |
|---|---|
| Status | **Not formalized** for the VM / Ed25519 / sighash half. Spendability by class + abstract `sig_ok`: Closed (`converge_unspendable` et al., M123–M128) |
| Cost | **MEDIUM**–**HIGH** (separate cone) |
| Depends on | Opcode interpreter, Ed25519 verify, sighash |
| Priority | P1 |
| Relation | Class A spend matrix is `Script/Semantics/Spend.v`. No VM. Classifier ≡ `rigor_classify.cpp` is open (C-7). Signatures unforgeable is OP-C1 |

### OP-D1 — Runtime `S_RES` / ConnectBlock rejection

| | |
|---|---|
| Status | **Open by design (C-3).** C++ does not reject on a running `S_RES` scalar |
| Cost | **LOW** as documentation; **HIGH** if someone wrongly treats it as a missing kernel proof |
| Depends on | Observational metrics vs consensus reject rules |
| Priority | P1 (documentation, not a missing kernel bug) |
| Relation | Related Closed items that do **not** close this entry: OP-X2 (per-mint genesis cap) and OP-X3 (schedule-side Σ W ≤ `RES_MAX_SUPPLY`). Kernel mint bound remains `W(h)`. That is **not** a running-`S_RES` reject |

### OP-X2 — Per-mint RES cap by genesis reward (P0-120)

| | |
|---|---|
| Status | **Closed.** `res_mint_bounded_by_genesis` / `check_res_mint_bounded_by_genesis` in `Kernel/SafetyExtensions.v` |
| Cost | Done (was LOW) |
| Depends on | `res_created` and `res_schedule_finite` (already in the cone) |
| Priority | Done (per-mint schedule cap only) |
| Relation | Category B. Any created mint, and any `check_res = true` state, satisfies `0 < mint_amount ≤ RES_REWARD_GENESIS`. Not a C++ `S_RES` reject, not a total-supply theorem (see OP-X3), and not draw-ownership (C-4) |

### OP-X3 — Schedule-side RES aggregate cap (P0-120 follow-up)

| | |
|---|---|
| Status | **Closed.** `res_total_issuance_capped` in `Supply/SupplySchedule.v` |
| Cost | Done (was LOW) |
| Depends on | Epoch-unfolded `W_epoch_sum` over epochs `0..RES_REWARD_LAST_EPOCH` (251 terms; not a per-height expansion) |
| Priority | Done (schedule identity only) |
| Relation | Category A. `res_schedule_total = RES_REWARD_EPOCH * W_epoch_sum 250 ≤ RES_MAX_SUPPLY`. Not a running `S_RES` reject (OP-D1). `res_cumulative_cap` in `RESIssuance.v` remains a non-negative height-sum and **does not** prove this upper bound |

### OP-A1 — ASERT approximation error against `2^x` (frontier 2.4)

| | |
|---|---|
| Status | **Not formalized.** Measured, not proved: exhaustive evaluation outside Coq puts the worst relative deviation of `factor e` from `65536 * 2^(e/65536)` at 0.0117% at `e = 57176`, inside the 0.013% quoted for aserti3-2d |
| Cost | **MEDIUM**, and gated on a dependency rather than on effort: it needs coq-interval, which is not in this toolchain |
| Depends on | A certified enclosure of `2^x` over a continuum. The stdlib reals give no tight envelope alone; monotonicity and the exact endpoints proved in 2.4 do not pin the interior. Not reachable by evaluation either — a per-point integer certificate is `a^65536 <= 2^e * 65536^65536`, comparing numbers over a million bits wide, at 65536 points |
| Priority | P2 |
| Relation | Deliberately below the width and wrap results it sits next to. The deviation from `2^x` is a property of the approximation, and every node evaluates the same cubic — the polynomial is the consensus rule, not an implementation of some other rule. A poor approximation would make difficulty track intended spacing imprecisely; it would not split the chain. The split-relevant arithmetic is covered by `asert_poly_pos`, `asert_poly_fits_uint64`, `factor_wrap_continuous` and `asert_next_wf` |

## P0-120 probe notes (what was *not* attempted as a proof)

Tried as low-cost candidates and **rejected as too expensive** for
this increment (no half-written proofs left in the cone):

| Candidate | Why not now |
|---|---|
| Common-prefix / k-deep reorg bound | Needs a fork-choice automaton and an honest-majority assumption. Not in the cone |
| Liveness / “a valid block is eventually produced” | Needs a scheduler / miner / network progress model |
| Fairness of draws / fees | Retired with RFC-019. C-4 `draw_eligibility_is_hypothesis` / `rt_draw_ok` is archived; mint is the integer inequality `R * RATE(k) <= B * RES_COIN`. Fee fairness remains unmodelled |
| Full extraction refinement | Needs Coq extraction + OCaml semantics (OP-E1) |
| Production ConnectBlock refinement | Needs C++ linking and a much larger fragment (OP-P1) |

## High-cost launch checklist

Preconditions to start the *expensive* items later. None of these
are claimed as current work.

### Full extraction / production refinement (OP-E1 / OP-P1)

1. A written refinement statement: Coq `check_*` vs extracted OCaml
   *and/or* vs a named C++ function, with explicit I/O types.
2. An integer-model proof or an explicit remaining gap for
   inductive `Z` vs native integers (P0-113 F-01).
3. For production: a comparable-fragment expansion that includes
   prevouts, scripts, signatures, keccak PoW hash, full ASERT nBits
   — or a listed gap for each omission.
4. Resource estimate: multi-week proof-engineer time plus a
   dedicated reviewer; not a one-script increment.
5. Public flags stay `EXTRACTION_EQUIVALENCE_NOT_CLAIMED=TRUE` and
   `PRODUCTION_REFINEMENT_NOT_CLAIMED=TRUE` until a Closed theorem
   exists.

### Forced verified-compiler switch (OP-B1)

See `release/VerifiedCompilerPath.md` (same list, expanded):

1. Licensed or CI-installable `ccomp` / CertiCoq / CakeML, pinned.
2. A CompCert-C / Clight *generator from the Coq checkers* (not a
   second hand-written C file).
3. Correspondence theorem or an explicit remaining gap.
4. Negative vectors still `INVALID`.
5. Resource estimate: compiler license + CI image + generator work
   before any TCB shrink is honest.
6. Flags stay `COMPILER_CORRECTNESS=false` and
   `BINARY_SEMANTIC_EQUIVALENCE=false` until both compiler
   correctness *and* source correspondence are Closed.

### Liveness / fairness / finality (OP-L1 … OP-L4)

1. A formal network or scheduler model (partial synchrony or
   equivalent) inside this cone or a new cone.
2. Explicit progress / hashrate / honesty assumptions, declared as
   hypotheses — not hidden.
3. Separate the new cone from this safety-only kernel; do not
   inflate Category A of the existing checkers.
4. Resource estimate: a new formalization workstream, not a
   corollary of `kernel_sound`.

## Reading rule

An open property must not be cited as “almost proved” because a
nearby kernel invariant is Closed. Safety in the Coq model is not
liveness, not finality, not C++ refinement, and not a verified
binary. P0-120 / follow-up Closed items are **local** schedule /
intra-block facts only. They do not partially address OP-R1.
RFC-020 dual-supply rewind (`res_reorg_rewind_eq`,
`oru_reorg_rewind_eq`) and the n·128 fold are issuance identities.
Most-work selection (`more_work_wins`) is a two-tip comparison on
abstract work. None of them close OP-R1. RFC-020 does not cheapen
the k-deep / honesty race.

P0-126~130 adds transaction value-flow, model-level block
composition, economic algebra, kernel-subset script steps, a PoW
pipeline on abstract `H`, and an ASERT `Z.quot` matrix. Those are
additional Coq-model safety facts. They do **not** close OP-R1,
OP-D1, OP-L1…L4, compiler correctness, or a node.

P0-131~135 adds a model-level `refines` interface and a small-step
kernel machine. That is a **Level 4 foundation**, not Level 5.
Compiler / ABI / machine-code items remain OPEN
(`release/CompilerBoundary.md`).

## Cost of closing these

Each open property above is priced, with the technical method and an
engineer-time estimate, in `release/FormalizationFrontier.md`. That
document also states where further formalization stops being worth its
cost, and why (collision resistance is not provable; verifying `coqc`
removes a smaller risk than the one C++ `ConnectBlock` still carries).

Maturity level is defined in `release/LevelDefinition.md` and asserted
only in `release/FinalProofInventory.md`.
