# RIGOR Formal Verification Completeness Audit (P0-016)

Task: `TASK-RIGOR-FORMAL-VERIFICATION-COMPLETENESS-AUDIT-P0-016`  
Type: Independent formal-verification and implementation-consistency audit  
Status: **Audit complete. Remediation F-1…F-8 implemented 2026-08-25. F-9 not in scope.**  
Date: 2026-08-25  
Honesty flags (observed, not flipped):

```
equivalence_claimed = false
hash_kernel_shared_binary_with_cpp = false
refinement_proved = false
```

This review does not try to prove existing work correct. It asks whether the
formal stack actually supplies enough confidence for **mainnet consensus
security**. Prior coverage inventories (P0-001, RC0 freeze, Verdict) are
treated as evidence, not as authority.

---

## 0. Verdict

**No. The current formal-verification system does not provide sufficient
confidence for mainnet consensus security.**

What exists is a **strong model of a subset of RIGOR**, plus a **finite
differential gate**. What does **not** exist is a proof that the shipping
validator (`core/src/**`, especially `ConnectBlock` / `CheckTxInputs` /
`VerifyScript` / PoW) implements that model.

Mainnet consensus security today rests on:

1. C++ consensus code and its unit / functional / fuzz tests
2. Independent security audits of surfaces the model does not contain
   (Ed25519 domain P0-018; script-as-asset ownership P0-019)
3. A 422-vector Python ↔ C++ (and 215-vector Coq) lock on **draw / hash /
   schedule / reject-predicate** fragments

It does **not** rest on a machine-checked argument that “accepted blocks
preserve the specification invariants.” Treating the Coq corpus size
(~222 modules, ~2873 named results) as that argument would be a
**false-confidence failure**.

| Question | Answer |
|---|---|
| Does the formal model match the real implementation? | **No, not as a theorem.** Closest on σ / C / SHA-256d / W(k). Divergent on ConnectBlock composition, mint authorization, scripts, PoW, window walk, supply scalars. |
| Are critical consensus properties actually proven? | **Some, of the model only.** RES cap and schedule, C/winner order-independence, A-6 non-vacuity, dual-pool on `apply_block`. Not: mint↔draw coupling in production, script execution, PoW, fork choice. |
| Are there unverified assumptions? | **Yes.** `cm_draw_won` is an unconstrained boolean. Hash collision resistance is assumed. SIMD SHA-256 ≠ Coq SHA-256. Window list is given, not constructed. |
| Spec ↔ reference ↔ production gaps? | **Yes, structural.** See §2. Dual Coq models even disagree with each other. |

**Mainnet freeze implication:** protocol rules may be frozen by RFC/C++ lock.
Formal verification **cannot** be cited as a freeze-authority for consensus
correctness until the findings in §3.1–§3.2 are closed or explicitly
excluded from the public claim.

Allowed public sentence remains:

> Formally verified protocol invariants with explicit verification boundaries.

Forbidden, and currently **tempted by catalog naming**:

> Formally verified blockchain / mainnet freeze certified / Coq = runtime.

---

## 1. What is guaranteed, assumed, and still unsafe

### 1.1 Guaranteed (machine-checked, about the **model**)

These are real `Qed` results in the protocol-core cone. They do not bind
`rigord`.

| Property | Artifact | Bound |
|---|---|---|
| SHA-256d is an executable function on `list byte`; FIPS short messages | `RigorSha256.v`, `CanonicalHashKernel.v` | Bytes in Coq, not collision-resistance |
| σ preimage is 100 bytes, offsets 0/32/64/96, no `target_prev` | `CanonicalDrawEncoder.v`, `SigmaPoW01.v` | Layout, not C++ `ComputeSigmaHash` identity |
| Convergence_Root is permutation-invariant; `N < 2` → 32 zeros | `OrderIndependence.v`, `ConvergeRoot.v` | Candidate **list already in hand** |
| Winner is permutation-invariant under positive weights and distinct txids | `select_winner_order_independent` | Same |
| Eligibility filter is non-vacuous; pre-A-6 `T = 2^240` was vacuous | `Selection.v` | Constant T, not PoW |
| A-9-C unpaid-txid exclusion; at most one win per txid in the model | `WindowExclusion.v` | Abstract `already_paid` list |
| `draw_candidate_root` hashes the eligible subset; no `N < 2` shortcut | `DrawCandidateRoot.v` | Digest of a list, not coinbase/Merkle/header |
| RES `W(k)` finite-sum identity `schedule_sum = 99450437`; `max_issuance_within_cap` | `ResSchedule.v` | Schedule arithmetic |
| Connect-path cap reject **under abstract `cm_draw_won`** | `ResMintObligation.v` | Flag is not Selection/owner/PCORB |
| Dual-pool: ordinary RES in ≥ out; block net ≤ n·W(h) | `DualPoolConservation.v` | `apply_block` of `TxAbs` |
| ORU issuance linear in block count; no absolute cap | `SupplyConservation.v` | Model subsidy |
| Script **shape** on accepted `apply_block` | `ScriptShape.v` / `ScriptShapeLift.v` | Templates, not `EvalScript` |
| Pooling class is `asset_kind_of_script` (Task 4) | `AssetFromScript.v` | Shape classifier, not interpreter |
| 215 golden lemmas recomputed by `vm_compute` | `Kernel/*Harness.v` | Finite L1↔L2 |
| Protocol-core syntactic scan: 0 `Axiom` / `Admitted` | `_CoqProject` sources | Does not cover vacuous `True` / `reflexivity` lemmas |

### 1.2 Assumed (not proved, often implicit)

| Assumption | Where it enters | If false |
|---|---|---|
| `cm_draw_won` equals the production draw winner + owner check | `ConnectMintSpec`; H-1 theorems | Mint to wrong owner, or skip mint, outside the theorem |
| Production SHA-256 (often SIMD `SHA256AutoDetect`) equals Coq SHA-256 | Gate uses portable Transform, not live `rigord` hasher | Consensus split on hash, theoretically |
| Window / eligible list given to Coq is the list `BuildConvergenceDrawSet` builds | `WindowExclusion`, DCR | Same σ math, different F → different C/winner |
| `rs_res_supply` tracks UTXO RES | Explicitly **not** identified (`we_do_not_claim_rs_res_supply_equals_utxo_res`) | Scalar cap vs spendable supply diverge |
| Script bytes that pass shape are spent by the intended ownership predicate | No `Ed25519_Verify` / `SignatureHash` in Coq | P0-018/P0-019 class bugs |
| Honest nodes see the same block and UTXO | Network / mempool / eclipse out of model | Fork without a model violation |
| SHA-256 is collision-resistant | Fairness idealized | Grinding / collision attacks |
| Coq 8.15.0 kernel is trusted | Verdict TCB | Standard |

### 1.3 Still unsafe (production consensus, outside the proof)

| Surface | Production locus | FV status |
|---|---|---|
| `ConnectBlock` composition (scripts → mint → subsidy → Merkle/header) | `core/src/validation.cpp` | Scaffold only; no refinement |
| Mint authorization (owner hash160, parent draw set, W(h)>0 H-1) | `ConnectEvaluateCoinbaseResMint` | **Not in Coq.** Model uses a boolean. |
| `VerifyScript` / Ed25519 / sighash domain | `interpreter.cpp` | **Not modeled.** P0-018 is C++-level. |
| Asset/ownership from scriptPubKey (Option A vs B) | `rigor_classify.cpp`, spend | Shape only. P0-019 remains open for freeze. |
| Coin-view window walk, undo, `FinalizeWindowedDrawSet` | `convergence_logic.*` | Rule modeled; walk not. |
| DCR inserted into coinbase / tx Merkle / header / PoW | ConnectBlock + RPC | Explicit non-claim |
| Difficulty / ASERT / `CheckProofOfWork` / OpuHash | header validation | **NOT VERIFIED** |
| 100-block RES maturity | `Coin::RequiresMaturity` | C++ tests only |
| RFC-018 fee burn in ConnectBlock | `nFeesOruResTransferBurn` | Separate `fee-burn/` model, not ConnectBlock refinement |
| P2P, mempool, eclipse, DoS | net | Out of scope |
| Fork choice / chainwork tie-break | `chain.cpp` | Not a Coq theorem of C++ |

---

## 2. Layer gap analysis

Four artifacts are routinely collapsed in speech. They are not the same program.

```
SPEC-000…007 / RFC          mathematical specification
        ↓  (SPEC-006 claims a mapping; several names do not exist)
formal_verification/spec/   Python executable spec (L1)
        ↓  (215/422 Coq-enforced; named surface_splits)
formal_verification/coq/    Gallina model (L2)  — two models, see C-1
        ↓  (NO refinement theorem)
core/src/**                 production node (L3)
```

### 2.1 Mathematical specification vs Coq

[SPEC-005](../specification/SPEC-005-CONSENSUS-INVARIANTS.md) lists invariants
that “MUST remain true.” Mapping to proofs:

| Invariant | Spec statement | Coq status |
|---|---|---|
| CI-001 Chain continuity | Parent hash, consecutive height | Partial header fields on `BlockHeaderAbs`; not a chain theorem of C++ |
| CI-002 Single active chain | Unique tip by chainwork | **Not proved** for the node |
| CI-003 Deterministic validation | Same (S,B) ⇒ same accept/reject | Model `apply_block` is a function. C++ not refined. `Proofs/Determinism.v` is `congruence` on `Execute`. |
| CI-010 UTXO consistency | Spend existing outpoints | `ReplayReorgCore` model |
| CI-011 No double spend | Outpoint once | Model; BIP30-class txid reuse residual |
| CI-012 Deterministic transition | Unique successor | Model |
| CI-013 Replay equivalence | Replay B(1..h) = S(h) | Model; not platform C++ |
| CI-014 Reorg consistency | Reorg = sequential replay | Model rollback stack, not Core reorg policy |
| CI-020 ORU ≥ 0 | | Model subsidy path |
| CI-021 ORU recurrence S+R−C | | Split: ConnectBlockMint + fee-burn model; not one ConnectBlock theorem |
| CI-022 RES recurrence S+W·M | | Under abstract M = `cm_draw_won` |
| CI-023 RES cap | | **Model VERIFIED** + NEG vectors |
| CI-024 Single RES mint | M ∈ {0,1} | Model count; C++ by “one coinbase, one OP_RES_MINT”. `Proofs/Safety.unique_mint` does **not** prove this (see H-2). |
| CI-030 Draw determinism | | Model + vectors. Window construction C++. |
| CI-031 Single winner | | Model argmin. |
| CI-032 Draw independent of PoW | T frozen | **Model yes.** PoW itself unverified. |
| CI-033 Valid mint ⇒ valid draw | | **Not proved for production.** Core gap. |

[SPEC-006](../specification/SPEC-006-RUNTIME-FORMAL-MAPPING.md) asserts Coq
families `chain_validity_theorems`, `THM-05`, and a `PoWState` object.
**None of those names exist in `formal_verification/coq/`** (repo-wide search,
this audit). The mapping document describes a correspondence that the
development does not contain. That is finding **C-4**.

### 2.2 Python reference vs Coq

L1 (`rigor_consensus_spec.py`) is the byte SSOT for draw / σ / roots /
schedule and for dual-pool / mint-shape / cap **predicates**.

Living lock (`golden_lock.json`): **422** shared / **215** Coq-enforced.

| Family | Vectors | Coq-enforced | Implication |
|---|---:|---:|---|
| WindowExclusion | 59 | 59 | Strong L1↔L2 on the **filter** |
| DrawCandidateRoot | 36 | 36 | Strong on the digest |
| Inequality | 25 | 25 | Strong |
| NegativeCore / ICVG / SMW / EdgeCross | 62 | 62 | Strong on named reject **booleans** |
| SigmaEncoder | 10 | 10 | Layout |
| RunSelection | 84 | **7** | Winner relation mostly Python+C++ only |
| SigmaPoW01 | 50 | **5** | σ digest mostly not in Coq harness |
| ConvergenceRoot | 25 | **9** | |
| HashKernel | 29 | **2** | |
| ReorgBind / BlockTrace / Replay / WindowIncomplete / SupplyScaffold / Sha256Fips | 42 | **0** | Python+C++ or separate FIPS lemmas |

Named L1 vs L2 **surface splits** (NEG-014/015, SMW-010/011, XFAM-003…005,
Z-height obligation rows) are locked as intentional: Coq often pins a
boolean conjunct, not `apply_block` itself (`hash32_eq_dec` is `Qed`-opaque
to `vm_compute`). A green harness lemma is **not** “Coq executed the same
function as Python.”

### 2.3 Python / Coq vs production C++

Closest surfaces (finite CLI / golden cases):

- `ComputeSigmaHash` vs encoder
- `ComputeConvergenceRootFromEligibilityTxids` vs `ConvergeRoot`  
  Gate uses the **pure** function, **not** `ComputeConvergenceRootFromConvergenceSet` (coin-view wrapper).
- `GetResMintReward` vs `get_res_mint_reward` (parity test `k = 0..251`)
- Selection on frozen vectors

Not corresponding:

| Production | Model |
|---|---|
| `ConnectEvaluateCoinbaseResMint` (owner, parent draw set, script parse, W(h)) | `cm_draw_won : bool` |
| `CheckTxInputs` + `UpdateCoins` + script | `apply_tx` on `TxAbs` |
| `VerifyScript` Ed25519 | absent |
| `CheckProofOfWork` / ASERT | absent (`PoWState` named only in SPEC-006) |
| SIMD SHA-256 in `rigord` | Coq + portable CLI + OCaml extract |
| Window built from coin view / undo | List parameter |
| RFC-018 burn in `ConnectBlock` | `fee-burn/` separate state machine |
| Covenant retired (no OP_CVG_CLAIM) | `CvgCovenant.v` still present; model **over-approximates** |

`Phase5Equivalence` records `draw_closure_status = DCS_Impossible`. That
conclusion is still correct for **full refinement**. The catalog’s frozen
*reasons* are not: it still proves
`dcc_has_draw_canonical_encoder = false` and
`dcc_has_runtime_hash_kernel = false` after `CanonicalDrawEncoder.v` and
`RigorSha256.v` exist (`DrawBridgeProgress.v` says the opposite). Finding **C-5**.

### 2.4 Two Coq models (not one)

| Machine | State | Connect / mint | Used by |
|---|---|---|---|
| **Legacy account** `legacy_account_model/` | Global `ORU_supply` / `RES_supply` scalars; no UTXO | `ConnectBlock.v` **scaffold** | `Proofs/*`, `Execution.v`, `Properties.v` |
| **ReplayReorgCore** | UTXO list + independent supply scalars | `apply_block` / `valid_block_connect` | Monetary cone, harness negatives |
| **fee-burn/** | Circulating ORU + fee class | Own `apply_block_circulating` | RFC-018 properties |

`ConnectBlock.v` still **silently skips** a mint when
`RES_supply + W(h) > MAX` (keeps prior state). ReplayReorgCore and C++
**reject**. The development therefore contains two contradictory mint-cap
semantics. Finding **C-1**.

`legacy_account_model/State.v` labels itself “NOT THE C++ UTXO
IMPLEMENTATION.” `Proofs/Safety.unique_mint` and
`Proofs/Determinism.selection_deterministic` are `congruence` on a
Gallina `match` — they prove that a function returns at most one value,
which is true of every Coq function.

---

## 3. Findings

Severity for this audit:

| Severity | Meaning |
|---|---|
| **Critical** | False confidence or contradictory models that can hide a consensus fork / issuance bug |
| **High** | Consensus-critical production surface with no proof and weak differential lock |
| **Medium** | Structural debt that inflates coverage or stalls honest reading |
| **Low** | Hygiene, CI, optional strengthening |

### 3.1 Confirmed vulnerabilities

These are confirmed defects of the **verification system** (and, for C-1,
an internal contradiction). They are not newly discovered C++ mint bugs;
production ConnectBlock already rejects cap overshoot. They **are**
defects if the Coq tree is treated as evidence that mainnet is safe.

#### C-1. Dual Coq connect paths with opposite cap-overshoot semantics — **Critical**

**Root cause:** Historical scaffold `ConnectBlock.v` was never retired after
ReplayReorgCore adopted fail-closed `valid_res_cap`.

```42:50:formal_verification/coq/ConnectBlock.v
        if Z.gtb (s.(RES_supply) + RES_MINT_AMOUNT b.(height)) MAX_RES_SUPPLY then
          s
        else
          {| ORU_supply := s.(ORU_supply);
             RES_supply := s.(RES_supply) + RES_MINT_AMOUNT b.(height);
             ...
```

Overshoot → **no state change, block still “connects”** in the scaffold.
Production: reject (`bad-rigor-res-max-supply` / mint-amount path).
ReplayReorgCore: `apply_block = None`.

P0-001 / gap analysis P0-2 claim this was “done.” It was done in **one**
model. The other model still teaches the unsafe historical behaviour.
Anyone discharging a lemma over `connect_block_scaffold` can “prove”
cap safety that production does not implement that way.

**Classification:** Confirmed verification-integrity defect.

#### C-2. Winner-must-mint / CI-033 proved only for an unconstrained boolean — **Critical** (coverage)

**Root cause:** `ConnectMintSpec` has `cm_draw_won : bool` with no σ, no
txid, no owner, no PCORB. OpaqueBoundary is honest:

> H-1 / winner-must-mint theorems quantify over this unconstrained
> boolean; they are Closed *under that abstraction*.

Production authorization is `ConnectEvaluateCoinbaseResMint`:

- at most one `OP_RES_MINT` on coinbase
- maturity height in script
- parent draw set height = nHeight − 1
- parent outcome has a winner
- `CheckResRewardOwner(winner.res_payout_hash160, payout.scriptPubKey)`
- `nValue == GetResMintReward(height)`
- ConnectBlock H-1 only while `W(h) > 0`

None of those conjuncts appear in the H-1 Coq theorems. A block can
satisfy every proved mint-obligation lemma and still pay the wrong
owner, or a C++ bug can pay without a draw and the model cannot even
**state** the disagreement.

**Classification:** Confirmed completeness hole on the issuance
authorization path. This is the property that, if false in C++, prints
RES.

#### C-3. SPEC-006 maps consensus objectives to theorem families that do not exist — **High**

**Root cause:** Specification program invented a mapping layer
(`chain_validity_theorems`, `THM-05`, `PoWState`) without a corresponding
Coq development. Repo search of `formal_verification/coq` for those
identifiers returns **zero** hits.

Readers of SPEC-006 can believe PoW and chain validity are in the Coq
cone. They are not. Difficulty / ASERT / `CheckProofOfWork` have **no**
Coq module.

**Classification:** Confirmed documentation/specification integrity
failure. Not a C++ bug by itself; it is a false map from spec to proof.

#### C-4. Phase-5 impossibility catalog is stale relative to artifacts it pretends to describe — **High**

`DrawClosureImpossibilityCatalog.v` still contains:

```coq
Lemma draw_encoder_missing_on_current_architecture :
  current_draw_condition.(dcc_has_draw_canonical_encoder) = false.

Lemma draw_hash_kernel_missing_on_current_architecture :
  current_draw_condition.(dcc_has_runtime_hash_kernel) = false.
```

`DrawBridgeProgress.v` (same directory) sets those artifacts to **true**.
Comments warn not to update the catalog. Machine-checked lemmas therefore
assert a **false description of the current tree**. The *conclusion*
`DCS_Impossible` (no full refinement) remains right; the *premises* are
wrong.

**Classification:** Confirmed soundness issue for catalog theorems that
are about “current architecture,” not about an 018GG freeze snapshot
named as such in the lemma statement.

#### C-5. Coverage inflation and freeze-ready naming — **High** (false confidence)

Corpus shape (`_CoqProject`): **158** Phase-4F/4G/5 files vs **~62**
protocol-core / fee-burn / kernel files (~72% catalogs). Prior inventory:
~1750 of ~2873 named results are Phase status/`True` bookkeeping.

Concrete naming hazards:

| Name | Actual meaning |
|---|---|
| `FormalVerificationCertification` / `CS_ReadyForRelease` | Artifacts consolidated; verdict **must** be `RV_Partial`; equivalence false |
| `p5_mainnet_freeze_established = true` | Catalog flag aliasing PROOF-031 paperwork |
| `draw_closure_status_well_defined : forall s, True` | Vacuous, labelled VACUOUS-PLACEHOLDER |
| `same_transaction_same_block_same_burn` | `reflexivity` |
| `unique_mint` / `selection_deterministic` | `congruence` |

A count of “2873 theorems” or “READY_FOR_RELEASE” presented without the
honesty flags is a **mainnet-messaging vulnerability**. Website copy is
currently careful; the Coq identifiers are not.

**Classification:** Confirmed process/integrity defect.

#### C-6. Formal stack cannot see bugs that later P0 audits found in consensus — **High** (completeness evidence)

Independent audits already in-tree:

- P0-018 Ed25519 signed-message domain (sighash / network tag / scriptCode)
- P0-019 script-as-asset freeze (Option A `SHA256(pk)` vs mint `HASH160(pk)`)

Coq has **no** `SignatureHash`, **no** Ed25519 verify, **no** Option A/B
ownership equality theorem. The 215-vector harness would have stayed
green through both classes of defect. That is empirical proof that
“green FV gate ⇒ mainnet consensus safe” is false.

**Classification:** Confirmed completeness result, not a new C++ CVE in
this task.

---

### 3.2 Potential risks

#### H-1. Window / F construction is the real draw consensus, and it is C++-only

Coq proves: same list ⇒ same C and same winner. Production risk is
**building the list**: coin view, undo, `ConvergenceWindowTail`,
eligibility flags, already-paid walk, `N < 2` vs DCR. Gate step 4 does
not check the coin-view wrapper. A wrapper bug forks the network with
every proved order-independence lemma still true.

#### H-2. `rs_res_supply` ⟂ `utxo_res`

OpaqueBoundary constructs a state where the scalar is 0 and UTXO holds 1
RES. Dual-pool and cap theorems can hold of the scalar while spendable
RES disagrees — or the reverse. Production tracks value in coins;
runtime supply exports are observational. A silent scalar/UTXO split is
an accounting fork, not a Coq contradiction.

#### H-3. Script interpreter, P2WSH residual, and classifier drift

Shape lift is real. Execution is not. P0-019 notes explorer/Coq
classifier disagreements vs C++ `ASSET_INVALID` default. Any second
classifier is a consensus-adjacent wallet/explorer risk; a Coq
classifier that is not differentially locked to `AssetTypeFromScriptPubKey`
can “prove” pooling of a script C++ rejects (or the converse).

#### H-4. Hash kernel: three implementations, production uses a fourth

Coq SHA-256, Python hashlib, portable `CSHA256` CLI, and live
`SHA256AutoDetect()` SIMD. Tripartite gate does not bind the SIMD path.
Bitcoin Core `SelfTest` is the SIMD assurance. `hash_kernel_shared_binary_with_cpp = false`
is correct. Residual: a SIMD/portable split would not fail the FV gate.

#### H-5. CI does not compile Coq

`.github/workflows/formal-verification-gate.yml` runs the Python
differential checklist. Comment: do not apt-install Coq; Ubuntu is not
8.15.0. `Print Assumptions` / `coqchk` / harness rebuild are **local**
(`FV_COQ=1`). A PR can break every load-bearing `.v` file and still pass
CI if it does not break JSON vectors.

#### H-6. Covenant over-approximation

`CvgCovenant.v` / `count_cvg` remain. C++ has no `OP_CVG_CLAIM`, no
`ASSET_CVG`. Authors document this as the safe direction (model accepts
more than the node). Residual: theorems that quantify over covenant
fixtures do not apply to genesis-reachable C++ states; someone reviving
the opcode inherits an authorization hole the comments already flag.

#### H-7. RFC-018 fee-burn and ConnectBlockMint are parallel machines

`fee-burn/Properties.v` is a clean model of burn share and circulating
ΔORU. `validation.cpp` implements burn inside ConnectBlock fee
accounting. No refinement; a mismatch in floor/rounding or
“which outputs count as RES-transfer” is outside both dual-pool and
fee-burn theorems simultaneously.

#### H-8. Golden-vector families with 0 Coq-enforced rows include replay/reorg

`ReorgBind` (12), `Replay` (3), `BlockTrace` (12) are Python-only
scaffolds. Replay/reorg **theorems** exist in ReplayReorgCore; they are
not locked to production undo. Claiming “reorg verified” from either
side alone is overstated.

#### M-1. RunSelection / SigmaPoW01 under-enforced in Coq

77/84 selection vectors and 45/50 σ vectors never enter the Coq build.
SHA-256 `vm_compute` cost is the stated reason. Regression of winner
tie-break or σ byte order could be caught by Python+C++ and missed by
Coq — acceptable only if the C++ golden tests actually run in CI.
`selection_golden_tests.cpp` is on the workflow path list; full
`test_bitcoin` is **not** required (`SKIP_TEST_BITCOIN=1`).

#### M-2. Maturity, timestamp, Merkle, coinbase scriptSig commit of C

All implementation-level. Whitepaper “self-proving chain”
(coinbase → Merkle → header → PoW) stops in Coq at C/DCR/σ.

---

### 3.3 Design concerns

#### D-1. Path A (finite differential) is the maintained bridge; full refinement is correctly judged not fundable this cycle

[`RIGOR_L2_L3_REFINEMENT_FEASIBILITY.md`](../formal/RIGOR_L2_L3_REFINEMENT_FEASIBILITY.md)
is right: CompCert/VST against `core/src/**` is a different program.
The design concern is **operational**: if refinement will not happen,
mainnet security process must be **C++ audit + tests**, with Coq as an
oracle for a **named subset**, not as a substitute for ConnectBlock
review.

#### D-2. Three honest layers are still one trust story in practice

Honesty flags on the website are good. SPEC-006, Phase “Mainnet Freeze”
modules, and “2873 theorems” work against that honesty. The architecture
needs **one** load-bearing cone list (already sketched in
`FORMAL_COQ_LOAD_BEARING_THEOREMS.md`) as the only citable theorem set.

#### D-3. Model over-approximation vs under-approximation is not uniform

Covenant: model ⊇ node (safe if theorems are “all accepted model txs
satisfy P” — **unsafe** if theorems are “exists a model tx that the node
must accept”). Cap scaffold: model ⊈ node (unsafe). Asset-from-script:
recently aligned at shape layer. The development has no global
over/under-approx lemma.

#### D-4. Legacy account model should not remain on the default `_CoqProject` proof path for monetary claims

Keeping it for history is fine. Importing it from `Proofs/Safety.v` as
if it constrained issuance is not.

---

### 3.4 Non-issues

| Item | Why it is not a defect |
|---|---|
| `equivalence_claimed = false` (and siblings) | Correct. Public site and freeze docs repeat them. |
| 0 `Axiom` / `Admitted` in comment-stripped protocol-core sources | Real, and worth keeping. Does not imply useful theorems. |
| RES schedule / cap arithmetic | Load-bearing and closed under global context (sampled historically). |
| A-6 non-vacuity + proof that old T was vacuous | Unusual and valuable. |
| Order-independence of C and winner **on a given set** | Real anti-timing-fork lemma for that function. |
| Dual-pool on `apply_block` (model) | Real; just not C++. |
| Website Formal Verification page wording | Within the allowed sentence; flags displayed. |
| No absolute ORU cap | Honest; tail emission is design. |
| SHA-256 not collision-proved | No blockchain Coq development proves that; FIPS vectors are the right bar. |
| `res_exact_cap_admitted` name | Qed lemma; “admitted” means exact-cap allowed, not `Admitted`. |

---

### 3.5 Optional improvements

| Item | Value |
|---|---|
| Stratified extra `RunSelection` / `SigmaPoW01` Coq-enforced rows (N=0/1/2, duplicate-adjacent, height W) | Medium |
| Coin-view wrapper vs pure root test-only cases | Medium |
| `Print Assumptions` in CI on a **small** load-bearing set (not 2873 names) | High process value |
| Collapse Phase-4/5 Catalog/Audit pairs to one status module | Maintainability |
| Extract encoder/root like hash (second extract, still not `rigord`) | Low vs cost |
| Coq field for 100-block maturity | Low fork-risk vs mint auth |
| Shared production hash `.o` | Feasibility doc **rejects**; do not reopen |

---

## 4. Proof gaps and missing invariants

### 4.1 Missing invariants that mainnet actually needs

These are the properties a consensus-security argument must have.
Status is relative to **production**, not to a Gallina function of the
same name.

| ID | Invariant | Needed for | Status |
|---|---|---|---|
| INV-MINT-AUTH | RES mint output owner = draw winner owner ∧ amount = W(h) ∧ parent F/C/outcome | Issuance | **Missing** in Coq; C++ + tests |
| INV-MINT-UNIQUE | At most one `OP_RES_MINT` per accepted block | CI-024 | C++ structural; Coq `unique_mint` does not state it |
| INV-NO-BODY-RES | Body txs cannot create RES | Dual pool | C++ `CheckTxInputs`; Coq dual-pool on tags/scripts |
| INV-DRAW-F | F is exactly the windowed eligible set from committed parent coinbase C | Draw | C++ walk; Coq assumes the list |
| INV-BIND | Coinbase commitment / Merkle / header bind DCR or C | Self-proving | C++ only |
| INV-SCRIPT-OWN | Create script commitment = spend predicate (Option B) | Ownership | P0-019 open; no Coq |
| INV-SIGHASH | Signature domain unique per (tx, network, asset meaning) | Replay/cross-protocol | P0-018; no Coq |
| INV-POW | Header hash ≤ target(ASERT(parent)) | Sybil / cheap headers | **Missing** |
| INV-SUPPLY-COIN | Cumulative RES scalar = sum of RES UTXO + burned/unspendable accounting | CI-022/023 operational | Explicitly non-claimed |
| INV-FEE-BURN | ConnectBlock burn share = RFC-018 function of RES-transfer ORU fees | Circulating ORU | Parallel model |
| INV-MATURITY | Minted RES unspendable for N blocks | Theft of fresh mint | C++ only |
| INV-DET-NODE | Two `rigord` binaries, same chain, same accept/reject | CI-003 | Tests, not proof |

### 4.2 Unnecessary complexity

The Phase-4F/4G/5 Catalog/Audit factory (~158 files) does not add
monetary or draw theorems. It records statuses, often as `bool := true`
and `Proof. reflexivity`. Cost:

- Inflated theorem counts
- Stale flags (C-4)
- Reviewer time
- Import graphs that make `coqchk` of the full tree expensive

**Safest correction:** freeze and eventually **delete or archive** the
catalog tree from the default `_CoqProject`, leaving Verdict +
`DrawBridgeProgress.v` + `OpaqueBoundary.v` + load-bearing cone.
Do not add sibling Catalog/Audit pairs.

Legacy `ConnectBlock.v` + `Proofs/*.v` + account `State.v` is a second
unnecessary kernel. Keep as `legacy/` excluded from monetary CI, or
delete after exporting any unique lemma (A-6 lives in `Selection.v`, not
here).

### 4.3 Additional verification that **would** raise mainnet confidence

In decreasing security return, **not** including CompCert-of-bitcoind:

1. **Mint-authorization differential:** golden vectors that feed
   `ConnectEvaluateCoinbaseResMint` + ConnectBlock H-1 (wrong owner,
   two mints, W=0 winner, N<2 mint, parent-set height mismatch). Coq
   should either consume the same winner txid or drop H-1 from the
   “verified issuance” claim.
2. **Classifier lock:** every `asset_kind_of_script` byte pattern vs
   `AssetTypeFromScriptPubKey` (closes H-3 / P0-019 drift).
3. **Window-walk vectors:** constructed F from a miniature coin view,
   not a pre-filtered list (closes H-1).
4. **Sighash / Ed25519 domain vectors** (closes C-6 / P0-018) — tests,
   not a Coq crypto proof.
5. **PoW compact-target check** as a small Coq + C++ parity module, or
   an explicit public “PoW is NOT in the FV boundary” banner on every
   consensus-security page.
6. **CI Coq 8.15** on the **non-Phase** cone only (~62 files), plus
   `Print Assumptions` on the load-bearing list.

---

## 5. Remediation plan

Security, minimal consensus surface, and long-term robustness beat
catalog preservation. Pre-mainnet: no compatibility burden.

### Fix F-1 — Quarantine or delete legacy `ConnectBlock.v` scaffold (addresses C-1)

| Field | Content |
|---|---|
| **Security rationale** | Two mint-cap semantics in one tree. Reviewers and future lemmas will pick the wrong one. |
| **Affected components** | `formal_verification/coq/ConnectBlock.v`, `Proofs/*`, `Properties.v`, `Execution.v` importers, `_CoqProject` |
| **Expected code changes** | Remove `connect_block_scaffold` from any monetary import. Either delete the file or move under `legacy_account_model/` and stop exporting cap lemmas. Point remaining draw proofs at `Selection` / `Execution` only. |
| **Consensus impact** | **None** (Coq-only). Production already rejects. |
| **Backward compatibility** | None required. Update docs that cite `ConnectBlock.v` as ConnectBlock-aligned. |
| **Testing** | `make -f Makefile.coq` protocol-core; `Print Assumptions` on `max_issuance_within_cap`, `cap_overshoot_rejects_apply_block`. |
| **Deployment** | Docs + Coq rebuild. No node release coupling. |

### Fix F-2 — Stop citing H-1 as verified issuance until `cm_draw_won` is the Selection winner (addresses C-2)

| Field | Content |
|---|---|
| **Security rationale** | Issuance safety is owner × amount × parent draw, not a boolean. |
| **Affected components** | `ReplayState.v` / `TxAbs` (may need σ/weight or a winner id), `ResMintObligation.v`, `StateTransition.v`, OpaqueBoundary, NegativeCore vectors, C++ tests for `ConnectEvaluateCoinbaseResMint` |
| **Expected code changes** | **Preferred (safest, smaller model):** do **not** expand `TxAbs` into a full interpreter. Instead (a) rename/relabel Coq theorems so they cannot be cited as CI-033; (b) add a **production** golden family `MintAuth` (wrong owner, omit mint, extra mint, W=0, underflow N) in L1+L3, Coq-enforced only on the W(h)/count booleans that already exist. **Larger option:** add `cm_winner_txid` constrained by `select_winner` on an abstract F supplied with the block — still not owner/PCORB, but closes “claimed win ≠ scored win.” Owner encoding stays C++ (P1-2 of the gap analysis was right). |
| **Consensus impact** | Labelling-only: none. Wiring Selection into the flag: model-only unless C++ is changed (must not be). |
| **Backward compatibility** | N/A. |
| **Testing** | New MintAuth vectors in `phase3_golden_vectors.json` + `res_proof_math_tests` / fuzz `res_mint_eval` already present — **require them in CI** (`test_bitcoin` not skipped for this family). |
| **Deployment** | Gate lock bump. Honesty flags stay false. |

### Fix F-3 — Repair SPEC-006 or demote it (addresses C-3)

| Field | Content |
|---|---|
| **Security rationale** | A mapping to nonexistent theorems is a specification lie. |
| **Affected components** | `docs/specification/SPEC-006-RUNTIME-FORMAL-MAPPING.md` |
| **Expected code changes** | Replace invented families with **actual** theorem names from `FORMAL_COQ_LOAD_BEARING_THEOREMS.md`. Mark CO-1/PoW/CI-033 as **NOT VERIFIED**. Delete `PoWState` or mark “does not exist.” |
| **Consensus impact** | None. |
| **Backward compatibility** | Spec errata only. Pre-mainnet allowed. |
| **Testing** | Grep that every Coq identifier in SPEC-006 exists in `_CoqProject` modules. |
| **Deployment** | Doc PR. |

### Fix F-4 — Snapshot-date the impossibility catalog or delete stale lemmas (addresses C-4, C-5)

| Field | Content |
|---|---|
| **Security rationale** | Machine-checked false statements about “current architecture.” |
| **Affected components** | `Phase5Equivalence/DrawClosureImpossibilityCatalog.v` and Audit; optionally entire Phase tree |
| **Expected code changes** | Rename lemmas to `draw_encoder_missing_on_018GG_freeze`. Or delete encoder/hash-missing lemmas and keep a single `refinement_proved = false`. **Recommended stronger:** drop Phase-4/5 from default `_CoqProject`; keep `DrawBridgeProgress.v` + honesty flags. Rename `CS_ReadyForRelease` → `CS_ArtifactsConsolidated`; `p5_mainnet_freeze_established` → `p5_phase5_paperwork_recorded`. |
| **Consensus impact** | None. |
| **Testing** | Coq rebuild of remaining cone. |
| **Deployment** | Doc: theorem-count claims use protocol-core only (~62 modules). |

### Fix F-5 — Bind classifiers and sighash with tests, not Coq crypto (addresses C-6, H-3, INV-SCRIPT-OWN, INV-SIGHASH)

| Field | Content |
|---|---|
| **Security rationale** | FV gate stayed green through P0-018/019-class holes. Those holes are consensus. |
| **Affected components** | `AssetFromScript.v` vs `script/rigor_classify.cpp`; `interpreter.cpp` sighash; existing P0-018/019 remediations |
| **Expected code changes** | Byte-level differential table: scriptPubKey → asset enum, Coq `asset_kind_of_script` vs C++. Sighash domain vectors from P0-018 must be **consensus tests in CI**, independent of Coq. Implement P0-019 Option B if that audit’s freeze recommendation is accepted (separate task). |
| **Consensus impact** | Classifier lock: none if already equal. Option B: **consensus change**, allowed pre-mainnet, must be explicit. |
| **Backward compatibility** | None on mainnet (no chain). |
| **Testing** | New `script_classifier_coq_parity` test; sighash vectors. |
| **Deployment** | If Option B: coordinated with wallet/explorer (P0-019). |

### Fix F-6 — Window-construction and coin-view wrapper tests (addresses H-1)

| Field | Content |
|---|---|
| **Security rationale** | Proved order-independence is worthless if F is wrong. |
| **Affected components** | `convergence_logic.*`, `cpp_sigma_root_cli` gate step 4, golden families |
| **Expected code changes** | Extend CLI or BOOST tests: `ComputeConvergenceRootFromConvergenceSet` ≡ pure function on the eligibility list the wrapper emits. Miniature chain: window edge, undo, already-paid, N=0/1/2. |
| **Consensus impact** | None if tests-only. |
| **Testing** | Functional suite; add to pre-mainnet checklist (not optional `SKIP_TEST_BITCOIN`). |
| **Deployment** | CI. |

### Fix F-7 — Public and CI boundary for PoW (addresses INV-POW, C-3)

| Field | Content |
|---|---|
| **Security rationale** | Headers are the Sybil barrier. Completely outside FV, sometimes mapped as if inside. |
| **Affected components** | SPEC-006, Formal Verification site, `HOW_TO_VERIFY_RIGOR.md`, header validation tests |
| **Expected code changes** | Docs: PoW/ASERT/OpuHash = **NOT VERIFIED** by Coq. Optional later: compact-target parse + comparison Coq module matching `CheckProofOfWork` on golden headers. Do **not** claim hashcash hardness. |
| **Consensus impact** | None for docs. |
| **Testing** | Existing PoW tests must stay in the **security** gate, not only FV gate. |
| **Deployment** | Messaging. |

### Fix F-8 — Coq in CI for the load-bearing cone (addresses H-5)

| Field | Content |
|---|---|
| **Security rationale** | Unchecked proofs are comments. |
| **Affected components** | `.github/workflows/formal-verification-gate.yml`, container with Coq 8.15.0 |
| **Expected code changes** | Job `coq-protocol-core` installing pinned 8.15.0 (not Ubuntu default), building **non-Phase** `_CoqProject` subset, running `tools/audit_assumptions.py` on the load-bearing list. Phase catalogs excluded (cost + C-5). |
| **Consensus impact** | None. |
| **Testing** | The job is the test. |
| **Deployment** | CI minutes (~15 min historically for full tree; subset should be less). |

### Fix F-9 — Identify or isolate supply scalars (addresses H-2)

| Field | Content |
|---|---|
| **Security rationale** | Cap theorems on a scalar that need not equal UTXO RES are not operational CI-023. |
| **Affected components** | `ReplayState.v`, `SupplyConservation.v`, OpaqueBoundary, C++ runtime supply (observational) |
| **Expected code changes** | **Safest:** prove `rs_res_supply = utxo_res` as an inductive invariant of `apply_block` from genesis, **or** stop using `rs_res_supply` in cap theorems and cap on `utxo_res` only. Do not leave both and cite either as “the” supply. |
| **Consensus impact** | Coq-only unless C++ accounting is wrong (then fix C++, not the model). |
| **Testing** | Invariant lemma + NegativeCore row that forbids diverged states on the success path. |
| **Deployment** | Coq. |

### Fixes **not** recommended

| Idea | Why not |
|---|---|
| CompCert/VST of `validation.cpp` this cycle | Feasibility: architectural, not a missing lemma. Wrong cost/benefit pre-mainnet. |
| Flipping `equivalence_claimed` after more catalogs | Catalogs do not create equivalence. |
| Modeling full `EvalScript` in Coq | Largest inherited surface; residual P2WSH even if “minimal.” Tests + classifier lock first. |
| Shared SIMD hash in `rigord` | Rejected in SHARED-HASH-FEASIBILITY; portable CLI is the right gate. |
| Proving SHA-256 collision resistance | Impossible at this TCB; do not pretend. |

---

## 6. Mainnet readiness assessment

| Gate | Ready? | Notes |
|---|---|---|
| Coq model internal consistency (protocol-core, 0 Admitted) | **Conditional yes** | After F-1 (legacy scaffold) and ignoring Phase catalogs |
| Correspondence finite gate 422/215 | **Yes as a fragment lock** | Not ConnectBlock |
| Mint authorization proved | **No** | F-2 |
| Script/ownership/sighash in FV | **No** | Rely on P0-018/019 + tests (F-5) |
| PoW in FV | **No** | F-7 |
| Spec↔proof map honest | **No** | F-3 |
| CI compiles proofs | **No** | F-8 |
| Public claim safe | **Yes if flags kept** | Unsafe if “2873 theorems / READY_FOR_RELEASE / mainnet freeze catalog” leak into freeze language |
| **FV as sufficient mainnet consensus evidence** | **No** | This task’s question |

**Operator recommendation**

1. Do **not** treat RC0 FV freeze or Phase-5 “mainnet freeze” catalogs as a
   consensus-security sign-off.
2. Keep the three honesty flags **false** through launch.
3. Execute F-1, F-3, F-4, F-8 before any external “formally verified”
   campaign (integrity of the story).
4. Execute F-2, F-5, F-6, F-7 as the actual security work — mostly
   **production tests and the already-open P0-018/019 remediations**,
   not more Gallina catalogs.
5. Cite Coq only for: RES schedule/cap arithmetic, draw algebra on a
   given candidate set, dual-pool **model**, A-6 non-vacuity.

Until F-2 and F-5/F-6 are closed, the honest security posture is:

> RIGOR has a formally verified **model** of monetary schedule and draw
> algebra, differentially locked on a finite vector set. Shipping
> consensus correctness is established by C++ review, tests, and
> independent audits, not by refinement.

That is enough to be a serious pre-mainnet project. It is **not** enough
to call consensus “formally verified” for mainnet.

---

## 7. Traceability

| This finding | Related prior work |
|---|---|
| Dual models / cap skip | Gap analysis P0-2 “done” only in ReplayReorgCore; `ConnectBlock.v` leftover |
| `cm_draw_won` | OpaqueBoundary; gap P1-2; CURRENT_STATE §2.1 |
| No refinement | Verdict §3.1; Phase-5 `DCS_Impossible`; feasibility note |
| Vector split 422/215 | `golden_lock.json`; GOLDEN-VECTOR-DISCREPANCIES.md |
| Catalog inflation | Gap P2-2; load-bearing theorems doc |
| P0-018/019 outside FV | Those audits; this audit’s C-6 |
| CI without Coq | `formal-verification-gate.yml`; gap P2-1 partial |

Independent of P0-001: this audit **re-checked** `_CoqProject` shape,
legacy `ConnectBlock.v` cap behaviour, OpaqueBoundary `cm_draw_won`,
SPEC-006 identifier existence, Phase-5 encoder-missing lemmas vs
`DrawBridgeProgress.v`, production `ConnectEvaluateCoinbaseResMint`,
CI workflow, and website claim wording.

---

## 8. Classification summary

| Class | IDs |
|---|---|
| Confirmed (verification-integrity / completeness) | C-1 … C-6 |
| Potential risks | H-1 … H-8, M-1, M-2 |
| Design concerns | D-1 … D-4 |
| Non-issues | Honesty flags; 0 Admitted; schedule/cap; A-6; order-independence on a set; site wording |
| Optional | Extra harness rows; extract encoder; maturity field; catalog collapse (also F-4) |

No production consensus *rules* were changed for F-1…F-8 (classifier/mint-auth/root
locks are existing C++ tests). F-9 is not in scope. Honesty flags remain false.
