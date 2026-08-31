# RIGOR Formal Verification Progressive Closure Audit (P0-035)

**Task:** `TASK-RIGOR-FORMAL-VERIFICATION-PROGRESSIVE-CLOSURE-AUDIT-P0-035`  
**Mode:** Audit, classification, proof-route design, and feasibility. **No production consensus rules were modified.**  
**Date:** 2026-08-29  
**Companions:** [P0-016](./RIGOR_FORMAL_VERIFICATION_COMPLETENESS_AUDIT_P0_016.md), [P0-023 inventory](./RIGOR_FORMAL_VERIFICATION_CLOSURE_INVENTORY_P0_023.md), [P0-023 refinement gap](./RIGOR_CPP_FORMAL_REFINEMENT_GAP_P0_023.md), [P0-032](./RIGOR_W32_REORG_STABILITY_AUDIT_P0_032.md), [P0-033](./RIGOR_W36_SIX_HOUR_WINDOW_FEASIBILITY_AUDIT_P0_033.md), [P0-034](./RIGOR_WINDOW_PARAMETERIZATION_FORMAL_CLOSURE_P0_034.md), [FV status](../verification/FORMAL-VERIFICATION-STATUS.md), [FV verdict](../verification/FORMAL-VERIFICATION-VERDICT.md)

This TASK does **not** claim that RIGOR C++ is formally proved. A Coq `Qed` is a theorem about a **model**. Production consensus remains the C++ in `core/src/`.

```
FORMAL_VERIFICATION_STATUS          = PROGRESSIVE_CLOSURE_READY

COQ_ADMITTED                        = 0
COQ_AXIOM                           = 0
COQ_PARAMETER                       = 0

PROTOCOL_PARAMETER_FORMAL_COVERAGE  = MODEL_PARTIAL
SUPPLY_FORMAL_COVERAGE              = MODEL_PARTIAL
REORG_FORMAL_COVERAGE               = MODEL_PARTIAL
REPLAY_FORMAL_COVERAGE              = MODEL_PARTIAL
OPUSHASH_FORMAL_COVERAGE            = MODEL_PARTIAL   # native ScriptPubKey push encoding
OPUHASH_POW_FORMAL_COVERAGE         = NOT_COVERED     # OpuHash mix / CheckProofOfWork
INTEGER_FORMAL_COVERAGE             = MODEL_PARTIAL

CPP_REFINEMENT                      = NOT PROVED
EQUIVALENCE_CLAIMED                 = false
HASH_KERNEL_SHARED_BINARY_WITH_CPP  = false
FUZZ_COVERAGE                       = HARNESS_PRESENT_NOT_A_THEOREM

NEXT_FORMAL_TARGETS                 = MintSupplyConservation; ConnectBlockUnifiedSupply; ReorgIssuanceBound; WitnessCanonicalEncoding
NEXT_FUZZ_TARGETS                   = ScriptPubKey/mint parser; sighash preimage; integer/amount boundaries; OpuHash header; P2P/RPC
```

Honesty lock (unchanged by this TASK):

```
refinement_proved                   = false
equivalence_claimed                 = false
hash_kernel_shared_binary_with_cpp  = false
```

Frozen production parameters **not** changed by this TASK:

| Item | Frozen value / location |
|------|-------------------------|
| W | `CONVERGENCE_DRAW_WINDOW = 32` (`rigor_consensus_lock.h`) |
| T | `RES_PROBABILITY_TARGET_CONST` = `000000000055e63b88c230e77e7ee106959b5d3e1e9ea69ebac0b0b4be383479` |
| RES initial reward | RFC-017 `W(0) = 40 RES` (`ResRewardForEpoch` / `ResSchedule.v`) |
| OpuHash PoW | `OPUS_POW_BUFFER_SIZE = 65536`, `OPUS_POW_MIX_ROUNDS = 256` |
| Subsidy | `ORU_INITIAL_BLOCK_SUBSIDY = 1000 * COIN`, 3 halvings, then 125 ORU forever |
| Maturity | `COINBASE_MATURITY = 100` (regtest may use 1 — **parameter gap**) |
| ScriptPubKey / Ed25519 / sighash / classify / ConnectBlock | unmodified |

---

## 0. How to read this audit

| Label | Meaning |
|-------|---------|
| **Qed theorem** | Machine-checked in Coq. About the **model named in that file**. |
| **axiom / Parameter** | None found in live `.v` (comment-stripped). |
| **opaque definition** | Intentional abstraction (e.g. `cm_draw_won`, HASH160 not computed). |
| **Admitted** | None in live `.v` bodies. Historical comments mention removed placeholders. |
| **model-only theorem** | Qed, but the model is not proved equal to C++. |
| **runtime-linked theorem** | Qed plus a finite vector / CLI / golden JSON gate. Still not refinement. |
| **refinement theorem** | Coq definition = C++ function. **None exist.** |
| **catalog / scaffold** | Phase 4F / 4G / Phase 5 observation modules. Do **not** count lemma volume as coverage. |
| **FORMAL CANDIDATE** | Security-critical invariant that can be stated; not yet a load-bearing theorem (or only C++ tests). |
| **WEAK Qed** | True by `congruence` on a function (`f x = a → f x = b → a = b`). Constrains almost nothing. |

**Do not cite filename as proof.** `ConnectBlock.v` is schedule aliases. `Proofs/Safety.v` `unique_mint` is WEAK Qed on `Execute`. `Serialization.v` serializes a toy account-model `Tx` as a sum of integers, not `CDataStream`.

**Do not cite Phase 5 lemma counts.** `formal_verification/coq/_CoqProject` says the citable cone is `_CoqProject.protocol-core`. Phase 5 Equivalence is ~89 files / ~1,300 lemmas of catalogs, observation bridges, and freeze paperwork.

---

## 1. Formal verification baseline

### 1.1 Two cones

| Cone | Path | Role |
|------|------|------|
| **Live spend surface** | `formal_verification/rigor_v01/` (20 modules) | Classifier, predicates, Ed25519 boundary, sighash tags, mint create=spend, opcode namespace, W-generic window / T(W), W32 reorg geometry |
| **Protocol-core + catalogs** | `formal_verification/coq/` | SHA-256 kernel, RFC-017 schedule, draw/selection (legacy account model), ReplayReorgCore apply_block, fee-burn accounting, Kernel harnesses, Phase 4F/4G/5 catalogs |

Live C++ spend path (`predicate_verify.cpp`, `rigor_classify.cpp`, `validation.cpp`) is **intended** to match `rigor_v01`. It is **not** proved to match.

### 1.2 Syntactic closure (this tree)

Comment-stripped scan of `formal_verification/**/*.v` excluding `archive/`:

| Check | Result |
|-------|--------|
| `Admitted.` in code | **0** |
| `Axiom` at line start | **0** |
| `Parameter` at line start | **0** |
| Theorem + Corollary | 691 |
| Lemma | 2537 |

Raw counts **include catalogs**. Citable load-bearing surface is much smaller (rigor_v01 ~278 theorems; ReplayReorgCore ~174 theorems; ResSchedule + fee-burn + Kernel + protocol-core hashes). Treat the 3,200 figure as inventory size, not coverage.

Honesty flags are themselves Coq definitions in `Phase5Equivalence/DrawBridgeProgress.v`: `refinement_proved = false`, `equivalence_claimed = false`.

### 1.3 Module map — what is actually proved

#### A. rigor_v01 (KEEP, model-only unless noted)

| Module | What Qed actually says | Not proved |
|--------|------------------------|------------|
| `AssetClassification.v` | Unique `classify(script)` → `{ORU,RES,NONE,INVALID}`; Option B `program = pk`; unknown → INVALID | `classify` = `AssetTypeFromScriptPubKey`; HASH160; C++ `IsWitnessProgram` byte-identity |
| `ScriptAssetBoundary.v` | Same script → same ownership predicate; spendable templates disjoint | Canonical encoding uniqueness vs PUSHDATA / trailing bytes as a C++ parser theorem |
| `Ed25519Ownership.v` | Three spend paths, all `AuthEd25519`; size gates 64/32/32; RES v1 `pk == program` | RFC 8032 verification; libsodium |
| `SignatureDomainSeparation.v` | Tag injectivity `RGRORU01` / `RGRRES01` / `RGRMNT01`; preimage field-prefix injectivity; compact-size for `len < 253` | `HashWriter::GetHash` = SHA256d; `GetPrevoutsSHA256`; C++ compact-size for `len ≥ 253` |
| `ResMintCorrectness.v` | `mint_create_predicate = mint_spend_predicate` (same 20-byte field); two maturity layers; mint sighash tag | HASH160(pk); coinbase-only create; unique `GetResMintScript` encoding; reorg+replay composition |
| `NoInterpreterConsensus.v` | Exhaustive inductive: no VM constructor in the model | C++ has no hidden EvalScript (enforced by absence + tests, not refinement) |
| `AssetStateTransition.v` | INVALID create rejected; `total' = total + subsidy + mint − burn` on **abstract pools** | ConnectBlock; fee vs converge split |
| `RigorPredicateRegistry.v` / `RigorByteNamespace.v` / `RigorOpcodeNamespace.v` / `RigorUpgrade*` | Five live predicate IDs; bytes are markers not operations | C++ `RigorPredicateID` equality |
| `NetworkCapabilityBoundary.v` / `NetworkSecurityBoundary.v` | Capabilities are not consensus | P2P implementation |
| `VectorAgreement.v` | runtime-linked: shared vectors vs Coq-enforced subset | refinement |
| `WindowGeneric.v` / `WindowTarget.v` / `WindowSpecialization.v` / `W32ReorgStability.v` | Height-anchored window geometry; T(W)=⌊UNWINDOWED/W⌋; W32 shipping + W36 **candidate**; reorg interval = f(s,W); maturity 100 independent of W | C++ still uses `>> 5`; production W36 not landed; combined reorg+issuance |

#### B. Protocol-core (citable, still model-only)

| Module | What Qed actually says | Not proved |
|--------|------------------------|------------|
| `RigorSha256.v` / `CanonicalHashKernel.v` | Executable SHA-256 / SHA256d; FIPS length/composition lemmas; NIST pins | Shared binary with `CSHA256`; SIMD SHA |
| `ResSchedule.v` | `W(k)` non-negative, ≤ genesis, terminates, finite sum ≤ cap, height-only, nonincreasing | C++ `ResRewardForEpoch` byte-identity (shape match is audit, not Qed) |
| `ReplayReorgCore/ConnectBlockMint.v` | Mint amount = `get_res_mint_reward(header_height)`; winner-must-mint; cap overshoot rejects; ORU subsidy schedule | `cm_draw_won` is abstract (`OpaqueBoundary.v`); not Selection |
| `ReplayReorgCore/DualPoolConservation.v` | Ordinary txs cannot mint extra RES; scheduled mint increases RES by exactly `W(h)`; rollback restores scalars | RFC-018 fee burn; 100-block maturity; PCORB/owner; C++ ConnectBlock |
| `ReplayReorgCore/SupplyConservation.v` | apply_block supply evolution; rollback; ORU linear bound (no ORU cap — by design) | Unified with fee-burn circulating view |
| `ReplayReorgCore/ResMintObligation.v` | H-1 winner/mint/cap dual insurance on `apply_block` | Draw predicate; owner encoding |
| `ReplayReorgCore/ReplayDeterminism.v` | **WEAK Qed**: same `fold_apply_blocks` cannot return two different `Some` | Canonical block bytes → C++ state; orphan disk replay |
| `fee-burn/*` | RFC-018 share 1/2; circulating ΔORU = subsidy − converge − fee-burn; reorg records B not A+B | Not wired into DualPool `apply_block`; not `DisconnectBlock` |
| `ConnectBlock.v` | Aliases `RES_MINT_AMOUNT` / `MAX_RES_SUPPLY` | **Not** ConnectBlock |
| `ConnectBlock/ConnectBlockSupply.v` | Bridge: apply_block rejects where legacy scaffold would skip-on-overshoot | Scaffold clamp is historical, not production |
| `legacy_account_model/Selection.v` | T non-vacuous; `eligible` inequality; **W pinned to 32**; `window_is_a_power_of_two` | Generic W (that's `WindowTarget.v`, not this file); C++ `>> 5` |
| `Kernel/*` | Eligibility / σ / root harnesses vs golden/negative vectors | Coin-view wrapper; C++ `ComputeSigmaHash` except CLI differential |
| `Proofs/{Determinism,Safety,Correctness}.v` | WEAK uniqueness of `Execute`; `execute_implies_verify` on account-model draw | Supply-level mint uniqueness (file itself says OPEN) |
| `Proofs/FairnessIdealized.v` | P ∝ T·f under **defined** uniform digest | SHA-256 uniformity; grinding cost |
| `Serialization.v` | Toy integer “serialization” of account-model Tx | Production consensus encoding |
| `ScriptMintWitness.v` | Shape family; comments admit possible RES2/CVG2 historical encoding **≠ C++ wire** | Classifier refinement |
| `UInt512OverflowRefinement.v` / `SigmaIntegerRefinement.v` | σ·f < 2^512 under bounds; LE σ bytes → Z | VST of `Mul256x256Full`; CAmount paths |
| Phase 4F / 4G / Phase 5 catalogs | Observation / freeze paperwork; many congruence scaffolds | Load-bearing ConnectBlock / classify |

#### C. Opaque / external boundaries (intentional)

| Surface | Status |
|---------|--------|
| HASH160 (RIPEMD160∘SHA256) | Not implemented in Coq. Mint/ORU owner is a **20-byte field**. |
| Ed25519 verify | Size + dispatch only. Crypto = libsodium / RFC 8032. |
| `cm_draw_won` | Unconstrained flag. Selection lives in a **different** model. |
| `rs_res_supply` vs UTXO RES | Declared independent (`OpaqueBoundary.v`). |
| OpuHash mix | **No Coq module.** |
| ChaCha20 | C++ `crypto/chacha20.cpp` only. |

### 1.4 Correspondence (not refinement)

Living freeze (`FORMAL-VERIFICATION-STATUS.md`): **422** shared vectors, **215** Coq-enforced; tripartite hash CLI; σ/root CLI. This is a **finite differential gate**. It does not make `ClassifyRigorTxout = classify` or `ConnectBlock = apply_block`.

---

## 2. Protocol-core coverage matrix (frozen parameters)

Legend: **M** = model Qed; **R** = C++ refinement (none); **P** = C++ property/unit tests; **F** = fuzz harness exists; **G** = golden vectors.

| Item | Current Coq | C++ refinement | Fuzz / property test | Status |
|------|-------------|----------------|----------------------|--------|
| **W32** | M: generic window + W32 specialization (P0-032/034) | **No.** C++ `>> 5` + `static_assert(W==1<<5)` | P: `w32_reorg_stability_tests.cpp`, `window_parameterization_tests.cpp` | **MODEL QED; C++ still power-of-two encoding** |
| **T** | M: `T(W)=⌊UNWINDOWED/W⌋`; `T32_matches_shipping_shift_encoding`; non-vacuity in `Selection.v` | **No.** `uint256` vs Coq `Z` | P: params pin hex | **MODEL QED for formula; not uint256 refinement** |
| **RES reward** | M: `ResSchedule.v` height-only `W(k)`, cap sum, nonneg, nonincreasing | **No.** | P: reward schedule tests | **MODEL QED; not C++ `ResIntegerSqrt` refinement** |
| **OPUSHASH (ScriptPubKey push / witness program)** | M: `parse_witness_program` length/version gates; unknown → INVALID | **No.** | P: `res_orum_tests` PUSHDATA1 reject; classify freeze tests. F: `script.cpp`, `parse_script.cpp`, `transaction.cpp` | **MODEL PARTIAL; canonical uniqueness FORMAL CANDIDATE** |
| **OpuHash PoW** | **None** | **No.** | F: `pow.cpp` (nBits/difficulty; not mix-loop KATs as a Coq theorem). P: PoW unit tests | **NOT COVERED in Coq → F4/C** |
| **subsidy** | M: `cb_expected_oru_subsidy` 1000 ORU / 210k / 3 shifts; G-2 no ORU cap | **No.** | P: subsidy tests | **MODEL QED schedule; CAmount overflow NOT** |
| **maturity** | M: 100 in `ResMintCorrectness` / WindowGeneric independence | **No.** Regtest maturity 1 | P: mint maturity tests | **MODEL QED at 100; param gap vs regtest** |
| **mint** | M: create=spend field; H-1 obligation; DualPool exactly `W(h)` | **No.** HASH160 opaque | P+F: `res_orum_tests`, `res_mint_eval.cpp` | **MODEL PARTIAL; MintSupplyConservation FORMAL CANDIDATE** |
| **ORU** | M: dual-pool ORU step; converge cannot create RES; subsidy bound | **No.** | P: converge / classify | **MODEL PARTIAL** |
| **supply** | M: DualPool + SupplyConservation + fee-burn **as two models** | **No.** | P: supply / RFC-018 tests | **MODEL PARTIAL; unified ConnectBlock identity missing** |
| **reorg** | M: window geometry; DualPool rollback; fee-burn reorg scalars | **No.** | P: W32 reorg tests | **MODEL PARTIAL; issuance across boundaries FORMAL CANDIDATE** |
| **replay** | M: WEAK determinism; fee-burn same tx → same burn; W32 orphan ≠ canonical | **No.** | P: replay/reorg suites | **MODEL WEAK + PARTIAL** |
| **classification** | M: unique `classify` | **No.** (P0-023: NOT PROVED) | P: freeze tests; G: P0-024 vectors | **MODEL QED; refinement F2** |
| **sighash** | M: tag + field injectivity | **No.** digest is C++ JSON | G: `rigor_sighash_p0_018.json`. F: `signature_checker.cpp` | **MODEL PARTIAL; SHA256d F3** |
| **Ed25519** | M: encoding + unique auth constructor | **No.** | P: RFC 8032 / libsodium KATs. F: `key.cpp` | **EXTERNAL LIB BOUNDARY** |
| **ConnectBlock** | M: `apply_block` / obligations / DualPool | **No.** `ConnectBlock.v` is not the C++ function | P: validation tests. F: `block.cpp`, `transaction.cpp` | **MODEL PARTIAL; refinement impractical as a whole** |

---

## 3. T / RES reward curve — unformalized relations

Production identities (frozen):

```
UNWINDOWED = ⌊2^246 / MIN_CONVERGENCE_AMOUNT⌋
T(32)      = ⌊UNWINDOWED / 32⌋     # C++: UNWINDOWED >> 5
C          = 2^(256-E) ORU = 1024 ORU at E = 246
W_draw     = 32                     # draw window
W_reward(k)= ⌊40e6 · (k+1)^(-3/2)⌋  # RFC-017 schedule (different W)
maturity   = 100
RES cap    = 21e6 × 1e6 atoms
```

| Relation | Coq today | Gap |
|----------|-----------|-----|
| `W > 1` | `W_ok := W > 1` (`WindowGeneric.v`) | C++ `static_assert(W==32)` not this predicate |
| `T = ⌊UNWINDOWED / W⌋` | `T_floor_bracket_unique`, `T32_floor_pair` | C++ shift encoding not proved equal except at W=32 (`T32_matches_shipping_shift_encoding`) |
| `T(32)` with `W=36` is a different protocol | `T_pinned_old_W_is_not_generic`, `stale_T32_with_W36_overshoots` | Production must not land W=36 with T32 (P0-033/034) |
| Expected wins per candidate independent of W when T retuned | `expected_wins_neutral_integer` | Idealized count, not SHA-256 probability |
| Expected RES **atoms** per height | **Missing.** Reward `W(k)` and draw T are in different modules | **FORMAL CANDIDATE:** `expected_issuance(h) ≤ W_reward(h)` under T(W)·W ≈ UNWINDOWED |
| `reward(height)` deterministic / height-only | `get_res_mint_reward` is a function of `h` | Node-local independence is definitional, not a C++ purity proof |
| Reward ≥ 0, ≤ genesis, ≤ cap | `get_res_mint_reward_nonneg`, `res_reward_for_epoch_le_genesis`, `reward_below_cap` | CAmount overflow of `reward + subsidy` **NOT COVERED** (Coq `Z` unbounded) |
| Finite sum of schedule ≤ cap | `max_issuance_within_cap` | Assumes **every** height mints; actual issuance is draw-filtered |
| Reorg cannot extra-issue | DualPool rollback on ReplayState; fee-burn reorg on FeeBurnState | **Not composed** with T, W_draw, W_reward, maturity, C++ chain |
| Mint cost C independent of draw W | `mint_cost_independent_of_W` | Naming collision: C / W_reward(k) floor price is stated in comments, not a C++ theorem |
| Dummy subsidy ⊥ W | `oru_subsidy_model = 1000` (`WindowGeneric.v`) | **WEAK** — not `GetBlockSubsidy` |

**Next-phase Formal Closure (F1) — if proveable in the model without changing C++:**

1. `RewardDeterministic : get_res_mint_reward h` depends only on `h`.
2. `RewardPlusSubsidyNoNeg : 0 ≤ W_reward(h) ∧ 0 ≤ subsidy(h)`.
3. `A9CMass : T(W) * W ≤ UNWINDOWED` (already essentially `mass_near_unwindowed`).
4. `ScheduledIssuanceCeiling :` any legal apply_block sequence issues RES ≤ min(cap, Σ_h W_reward(h) · 1_won(h)) with `1_won ≤ 1`.
5. `ReorgDoesNotIncreaseIssuance` — see §7.

Items 4–5 are the real missing monetary theorems. Item 1–3 are nearly present.

---

## 4. OPUSHASH / native ScriptPubKey encoding

This TASK uses **OPUSHASH** in two senses. They must not be collapsed.

### 4.1 Sense A — native ScriptPubKey (TASK §4)

Intended chain:

```
ScriptPubKey
  → version opcode (OP_0 / OP_1 … OP_16)
  → push-length byte
  → program
  → AssetTypeFromScriptPubKey
  → predicate
```

**Coq (`parse_witness_program`):** rejects length ∉ [4,42]; requires `witness_version_of` on first byte; requires `length(rest) = plen`. Trailing bytes that break `length(rest)=plen` parse as `None` → INVALID. Wrong witness version that is still OP_1–OP_16 with a 32-byte program that is not native RES (CVG2/RES2) is INVALID. P2WSH v0/32 is INVALID (`p2wsh_cannot_enter_asset_domain`).

**Not proved:**

| Claim | Status |
|-------|--------|
| Same semantic program → only one canonical encoding | **FORMAL CANDIDATE.** C++ mint: `ParseResMintScript` requires `script == GetResMintScript(...)`. Coq mint parser accepts a **shape family** (locktime push widths). P0-023 already recorded this. |
| Different encoding cannot classify as the same asset | Partial: classifier is a function of bytes (`same_script_same_asset`). The converse (two encodings, one asset) is **exactly** the uniqueness gap. |
| Malformed / wrong length / trailing / unexpected opcode / wrong version → invalid | **Model Qed** for the Coq parser; **C++ tests** for mint PUSHDATA1 (`res_orum_tests.cpp`). **Not** `ClassifyRigorTxout = classify`. |

Classification: **FORMAL CANDIDATE** (canonical uniqueness + parser equality). Property tests already exist; fuzz (`script.cpp`, `parse_script.cpp`, `res_mint_eval.cpp`) is the right **B** companion, not a substitute.

### 4.2 Sense B — OpuHash PoW (frozen mix)

`pow_opus.cpp`: ChaCha20-filled 64 KiB buffer, 256 mix rounds, then `CheckProofOfWork`. **Zero Coq modules** mention OpuHash. Fuzz `pow.cpp` stresses difficulty/nBits, not a Coq mix specification.

Classification: **F4 / C** for the mix function (external crypto + impractical VST). **A** only for the *boundary* theorem “PoW digest is OpuHash, not SHA256d” — still unstated as Coq.

---

## 5. Mint / Create = Spend depth

Existing Qed: `mint_create_eq_spend` — both sides are the **same record field** `mint_pkh`.

| Required same-ness | Coq | C++ | Combined theorem |
|--------------------|-----|-----|------------------|
| Same owner field | Qed (`mint_pkh`) | `GetResMintScript` / `VerifyResMintSpend` 20-byte field | Model only; HASH160 opaque |
| Same asset identity | `mint_is_res` | `ASSET_RES` | Model |
| Same program | N/A (mint is not v1 program) | mint script ≠ witness v1 | — |
| Same chain_id | Sighash record includes `sh_chain_id`; mint tag `RGRMNT01` | genesis hash argument | Injectivity of preimage, not “create and spend used the same chain_id on this UTXO” |
| Same sighash domain | `mint_sighash_tag` | `SigVersion::RES_MINT` | Model |
| Same maturity rule | Two layers Qed; cannot bypass 100 | `nCoinbaseMaturity`; script lock | Regtest gap |
| Same amount constraints | DualPool: mint tx increases RES by exactly `W(h)` | ConnectEvaluateResMintTx | **Not composed** with create=spend |
| Coinbase-only create | **Gap** (P0-023) | `bad-txns-res-mint-noncoinbase` | FORMAL CANDIDATE |

**Create valid + spend valid + reorg + replay:** no single theorem. DualPool forbids extra RES on ordinary txs and bounds scheduled mints; it **explicitly** leaves maturity, owner, and fee burn out of scope. OpaqueBoundary: `cm_draw_won` is not Selection.

**Suggested F1 theorem (model, then refinement later):**

```
MintSupplyConservation:
  ∀ s b.
    valid_block_connect s b = true →
    rs_res_supply (apply_block s b) − rs_res_supply s
      = (if scheduled_mint b then get_res_mint_reward (height b) else 0)
    ∧  (* create/spend of the same mint outpoint cannot both credit supply *)
    no_double_credit (utxo s) (utxo s')
```

Search for: a create that is legal, a spend that is legal, and a reorg/replay that realizes **two** `W(h)` credits. If DualPool+UTXO spend-deletion already rules this out **inside apply_block**, the work is to **cite and compose**, not to invent a new rule. Residual: txid reuse / BIP30 (already named in FV status).

---

## 6. ConnectBlock / supply conservation

Desired:

```
old_state + block  --ConnectBlock-->  new_state
ΔRES = valid scheduled mint
ΔORU_ledger = subsidy − converge          # C++ m_chain_oru_total (RFC-002)
ΔORU_circulating = subsidy − converge − fee_burn   # RFC-018
```

| Piece | Where | Wired into one transition? |
|-------|-------|----------------------------|
| Subsidy schedule | `ConnectBlockMint.v` | Yes, in `apply_block` (ReplayState) |
| Valid mint / cap | `ResMintObligation.v` | Yes, `apply_block` |
| Converge / dual-pool burns | DualPool (ORU burn abstract) | Partial |
| Fee burn | `fee-burn/` **separate** `FeeBurnState` | **No** |
| Maturity | `ResMintCorrectness.v` | **Not** in DualPool |
| Draw / T | `Selection.v` / Kernel | **Not** in DualPool (`cm_draw_won` opaque) |
| C++ ConnectBlock | `validation.cpp` | **No refinement** |

Honest identity available today (model):

```
AssetStateTransition.total_supply_conservation:
  total' = total + oru_subsidy + valid_mint − oru_burn
```

This treats burns as one scalar. RFC-018 **ledger vs circulating** split is proved only in `fee-burn/BlockSupply.v`, on a different state type.

**Particular tests (C++ / fuzz vs Coq):**

| Case | Coq | C++ / fuzz |
|------|-----|------------|
| Invalid tx | apply_tx fails | CheckTransaction / CheckTxInputs |
| Duplicate mint | DualPool + obligation count | ConnectBlock mint count |
| Duplicate spend | ReplayInvariants UTXO delete | CCoinsView |
| Reorg | rollback lemmas | DisconnectBlock — **not refined** |
| Orphan | W32 `orphan_not_canonical` (geometry) | disk orphan handling **not** Coq |
| Replay | WEAK determinism | — |
| Malformed tx | ScriptShape / classify INVALID | fuzz `transaction.cpp` |
| Integer overflow | Unbounded Z | CAmount / `addition_overflow.cpp` |
| Empty / max block | not specialized | policy + consensus limits |
| Max amount / boundary height | schedule epoch lemmas | unit tests |

**F1 route:** define a **combined** abstract state `{ReplayState; FeeBurnState}` with a single `connect` that applies DualPool then fee-burn, and prove the two-line Δ identity. Still not C++ refinement (F2).

---

## 7. Reorg + supply + W + T (next-stage proof)

P0-032: W32 window **geometry** is stable under reorg (interval = f(source, W), hash-keyed cache, orphan ≠ canonical).  
P0-034: same geometry for generic W; T(W) pair.  
**Neither** proves “legal reorg cannot increase total RIGOR issuance.”

| Reorg class | Geometry (Qed) | Issuance (missing) |
|-------------|----------------|--------------------|
| Shallow | W32/generic interval preserved | Combined supply |
| Deep | relative window still f(s,W) | Combined supply |
| Across window boundary | contents change, bounds formula-stable | Could a disconnected mint be re-credited? |
| Across reward epoch | ResSchedule crossing lemmas exist **in isolation** | Not under reorg |
| Across maturity 100 | maturity ⊥ W | Spend-after-reorg maturity |
| Across T (none: T is constant) | T frozen | N/A unless W retune |

**F1 statement:**

```
ReorgIssuanceBound:
  disconnect(prefix) then connect(alternative)
  with both sides valid_block_sequence
  ⇒  res_supply(final) ≤ res_supply(genesis) + Σ_{h ∈ surviving} W_reward(h)
  ∧  no height credits more than one mint
```

Feasible **in ReplayReorgCore** if mint count is already ≤ 1 per block and rollback restores snapshots. Hard part: identifying **surviving** heights with C++ chainwork / fork choice (`select_best_fork` is a separate toy). Call this **refinement difficult** for C++, **refinement possible** for the replay model.

This is the highest security-value next theorem after classifier uniqueness.

---

## 8. Replay safety

| Claim | Coq | Honest reading |
|-------|-----|----------------|
| Same canonical block → same state transition | `apply_block_deterministic` / `Replay_Determinism` | WEAK: `apply_block` is a function. Does **not** mention canonical serialization. |
| Replaying orphan → no state mutation | `W32_orphan_not_canonical` | Geometry/cache model, not `ActivateBestChain` |
| Same tx cannot be realized twice | UTXO spend-delete; DualPool; fee-burn `apply_one` | Residual BIP30 txid reuse (FV status) |
| Mint / subsidy / burn / reward / asset creation | Split across DualPool, ResMintObligation, fee-burn | Not one replay theorem |

`Serialization.v` does **not** lock C++ block hex. Canonical hash kernel is SHA-256 of **Coq bytes**, differentially gated vs portable CSHA256, not `CBlock` serialization.

---

## 9. Integer / overflow

| Quantity | Coq | C++ production | Status |
|----------|-----|----------------|--------|
| W | `Z` / `nat` | `int` 32 | MODEL; no overflow story |
| T | `Z` | `uint256` | MODEL; hex pin in tests |
| Difficulty / nBits | little | `arith_uint256` | **NOT COVERED** (OpuHash) |
| Reward / subsidy / amount / fee | `Z` | `CAmount` = `int64_t` | MODEL nonneg; **CAmount overflow NOT COVERED** |
| Supply scalars | `Z` | `CAmount` / chain stats | MODEL |
| σ bytes | LE sum → `Z` (`SigmaIntegerRefinement`) | 32-byte hash | MODEL of FVR pin; `equivalence_claimed = false` |
| σ·f | `Z` mul; `σ<2^256 ∧ f<2^64 ⇒ product<2^512` | `uint512` `Mul256x256Full` | MODEL refinement of **spec**, not VST of `.cpp` |
| `>> 5` vs `/ 32` | `T32_matches_shipping_shift_encoding` | `static_assert(W==1<<5)` | Proved **at W=32 only** |
| Division | `Z.div` floor | C++ unsigned / `uint256` | Possible mismatch on signs; production values nonneg |
| Shift | `Z.shiftr` subsidy | `CAmount >>` | MODEL; C++ shift UB if mis-typed — **NOT COVERED** |

Fuzz present: `addition_overflow.cpp`, `multiplication_overflow.cpp`, `integer.cpp`, `scriptnum_ops.cpp`. These are **implementation** tests, not consensus theorems.

**Type-diff examples (Coq proved, C++ type differs):** T as `Z` vs `uint256`; amounts as `Z` vs `int64_t`; product as `Z` vs `uint512`; W as `Z` vs `int`.

---

## 10. Formal vs fuzz buckets

### A — Must be Formal (fuzz cannot replace)

- Classification correctness (unknown → INVALID, no ORU fallback)
- Supply conservation / mint uniqueness / cap
- Maturity invariant (mainnet 100)
- Reorg issuance bound
- W window identity + T(W) pair
- Reward determinism (height-only)
- Native ScriptPubKey semantic boundary
- ConnectBlock **model** transition (then refine)
- Signature **domain separation** (tags + chain_id)

### B — Formal + Fuzz

- Serialization / canonical encoding
- Tx / ScriptPubKey / mint parsers
- Sighash preimage construction
- Integer boundaries (CAmount, uint256, uint512)
- Reorg state transition (Disconnect/Connect)
- Witness version / length / trailing bytes

### C — Fuzz first

- P2P messages, malformed packets, peer state machines
- RPC parse, wallet import/export, address parse
- Config, disk/DB corruption, DoS surfaces
- OpuHash mix (until a Coq spec exists — then still fuzz)
- Network capability handshake (policy, not consensus)

### D — Ordinary tests

- GUI, logging, help text, non-consensus CLI

Existing fuzz directory: 100+ `core/src/test/fuzz/*.cpp` (Bitcoin-derived + `res_mint_eval.cpp`, `res_checktxinputs.cpp`). Presence of a harness ≠ a completed campaign. This audit does **not** claim continuous fuzz coverage.

---

## 11. Formal vs fuzz boundary principle

**Formal verification** proves a **rule always holds** in a stated model.  
**Fuzz** shows an **implementation** does not crash, over-read, or accept garbage on sampled inputs.

- Fuzz **must not** replace a consensus theorem (classifier, supply, mint uniqueness, T pair).
- A Coq model **must not** replace C++ tests (parser, ConnectBlock, libsodium).
- Catalog lemma counts **must not** replace either.

Required chain:

```
Protocol invariant
    → Coq theorem (named model)
    → C++ refinement (or honest “NOT PROVED”)
    → Property / unit test
    → Fuzz
    → Golden vectors
```

Today the chain is usually `invariant → Coq model → C++ tests → (some fuzz/goldens)` with the refinement step **missing**. Differential vectors sit beside refinement, not in place of it.

---

## 12. C++ refinement gap (largest moat)

P0-023 remains the correct freeze: `refinement_proved = false`.

| Pair | Feasibility | Notes |
|------|-------------|-------|
| `AssetTypeFromScriptPubKey` ↔ `classify` | **Possible** | Byte-vector lock; mint unique encoding is the hard conjunct |
| `VerifyRigorPredicate` ↔ predicate dispatch | **Possible** (dispatch) / **lib boundary** (Ed25519) | Structural match already audited |
| `ConnectBlock` ↔ `apply_block` | **Difficult** (full) / **Possible** (monetary slice) | validation.cpp size; fee-burn+mint+draw |
| `SignatureHash` ↔ `SighashFields` encode | **Possible** | Compact-size + SHA256d still open |
| Ed25519 ↔ RFC 8032 | **Impractical** (VST/libsodium) | Prove I/O contract only |
| `ResRewardForEpoch` ↔ `get_res_mint_reward` | **Possible** | Fixed-point `<< 32` vs `Z.sqrt` — feasible computational pin |
| `GetBlockSubsidy` ↔ `cb_expected_oru_subsidy` | **Possible** | Small function |
| Selection / `eligible` ↔ Coq `eligible` | **Difficult** | σ encoder + uint512 + window enumeration |
| OpuHash ↔ (no Coq) | **Impractical** | Need a spec first; then still ChaCha20 |

Do **not** grow Coq theorem counts as a substitute for the first row.

---

## 13. External crypto-library boundary

```
RIGOR consensus C++
    → crypto boundary (sizes, tags, failure = false)
    → libsodium / CSHA256 / ChaCha20
```

**Can prove (F1/F2, still not VST):**

- Signature, pk, sighash lengths (already `ed25519_encoding_ok`)
- Domain tags and `chain_id` position
- Verify failure ⇒ spend rejected (dispatch), if crypto returns bool
- SHA-256 **kernel** in Coq + tripartite differential (not shared binary)

**Must depend on external evidence (F4):**

- RFC 8032 test vectors / libsodium `crypto_sign_verify_detached`
- SHA-256 NIST + production SIMD vs portable
- ChaCha20 / OpuHash mix KATs + fuzz
- HASH160 (RIPEMD160) — not in Coq

**Do not** start a libsodium VST project as the next P0.

---

## 14. Roadmap (F1–F5)

Ranked by **security value × consensus criticality / (proof difficulty × coupling)**.

### Phase F1 — prove immediately in Coq (adapt Coq to frozen C++, no rule changes)

| ID | Target | Value | Difficulty | Coupling |
|----|--------|-------|------------|----------|
| F1.1 | `MintSupplyConservation` (compose DualPool + UTXO no-double-credit + coinbase-only create) | High | Medium | Low (model) |
| F1.2 | `ReorgIssuanceBound` on ReplayState | High | Medium | Low |
| F1.3 | Wire fee-burn into a **combined** connect identity (still model) | High | Medium | Medium |
| F1.4 | Witness/mint **canonical encoding** uniqueness (match `GetResMintScript` / `IsWitnessProgram`) | High | Medium | Medium |
| F1.5 | `ExpectedIssuanceCeiling`: T(W)·W mass × `W_reward(h)` (statement + bounds, not probability) | Medium | Low–Med | Low |
| F1.6 | Boundary: PoW function identifier ≠ SHA256d (definitional, if a Coq OpuHash stub is added **without** changing C++) | Medium | Low | Low |

### Phase F2 — needs C++ refinement (still no protocol change)

| ID | Target | Feasibility |
|----|--------|-------------|
| F2.1 | Byte-lock `classify` ↔ `AssetTypeFromScriptPubKey` | Possible |
| F2.2 | `GetResMintReward` / `GetBlockSubsidy` computational pin | Possible |
| F2.3 | Sighash preimage bytes ↔ `HashWriter` (digest still SHA256d) | Possible |
| F2.4 | Replace C++ `>> 5` with `/ W` **only when** a future retune lands; **not** this TASK | Behavior-preserving at W=32; do not touch now |
| F2.5 | Selection window enumeration ↔ `WindowGeneric` | Difficult |

### Phase F3 — Formal + Fuzz

Parser/canonical bytes, sighash preimage, CAmount/uint256/uint512 boundaries, DisconnectBlock/ConnectBlock monetary slice, mint PUSHDATA/trailing.

### Phase F4 — external crypto

RFC 8032 / libsodium KATs + fuzz; OpuHash/ChaCha20 KATs; HASH160 KATs. No VST.

### Phase F5 — not worth formalizing

GUI, logs, help, Phase 5 catalog expansion, full `validation.cpp` VST, P2P state machines (fuzz), wallet SQLite.

---

## 15. What this TASK did not do

- Did not modify W, T, RES reward, OpuHash, subsidy, maturity, ScriptPubKey, Ed25519, sighash, classify, or ConnectBlock semantics.
- Did not restore Script VM or Bitcoin compatibility.
- Did not add protocol rules to make Coq easier.
- Did not treat catalog theorem volume as coverage.
- Did not write `Formal verification = PASS`.

---

## 16. Verification (rebuild)

Commands required by this TASK:

```bash
cd formal_verification/coq
make -f Makefile.coq clean
make -f Makefile.coq -j$(nproc)

cd ../../core
make -j$(nproc)
make check
```

`Makefile.coq` is generated and ignored by the sandbox; rebuild must run with a full filesystem view.

### 16.1 Coq

| Check | Result |
|-------|--------|
| `make -f Makefile.coq clean && make -f Makefile.coq -j$(nproc)` | **PASS** (exit 0, wall ~16.4 min, `nproc=2`) |
| Comment-stripped `Admitted.` in live `.v` | **0** (before and after rebuild) |
| `Axiom` / `Parameter` at line start | **0** |
| `ExtractHashKernel.v` | Present on disk; **not** in `Makefile.coq` `VFILES`. Optional extraction, not the protocol cone. |

A `Qed` after this rebuild is still a theorem about a **model**. It does not flip `refinement_proved`.

### 16.2 C++

| Check | Result |
|-------|--------|
| `cd core && make -j$(nproc)` | **PASS** (exit 0; rebuilt node, qt, bench, fuzz, unit/qt tests) |
| `make check` | **PASS** (exit 0) |
| `qt/test/test_rigor-qt` | **PASS** |
| `test/test_rigor` | **PASS** (ran as `check-local`; includes `window_parameterization_tests`, `w32_reorg_stability_*`, `script_asset_ownership_freeze_tests`, `res_orum_tests`) |
| `test/util/rpcauth-test.py` | **PASS** (3 tests) |

No production consensus sources were edited by this TASK. Functional `test/functional` is **not** part of this `make check` target (Bitcoin-style: unit + qt + util). Unit coverage of the frozen W/T/classify/mint surface did run.

### 16.3 Rebuild interpretation

`COQ_ADMITTED = 0` after a clean full-tree `Makefile.coq` rebuild is a **syntactic** closure of the Coq project. Combined with `make check` PASS, the honest product status is still:

```
FORMAL_VERIFICATION_STATUS = PROGRESSIVE_CLOSURE_READY
CPP_REFINEMENT             = NOT PROVED
```

---

## 17. Success criteria and next TASKs

### 17.1 This TASK succeeds if

- Baseline, inventory, matrices, Formal/Fuzz split, refinement roadmap, and crypto boundary are honest.
- `COQ_ADMITTED = 0` after full `Makefile.coq` rebuild.
- Production consensus parameters unchanged.
- Status is **not** “C++ formally verified.”

### 17.2 Suggested next TASKs (do not implement here)

1. **TASK-RIGOR-MINT-SUPPLY-CONSERVATION-P0** — F1.1  
2. **TASK-RIGOR-REORG-ISSUANCE-BOUND-P0** — F1.2 / §7  
3. **TASK-RIGOR-CONNECTBLOCK-UNIFIED-SUPPLY-MODEL-P0** — F1.3 (model only)  
4. **TASK-RIGOR-WITNESS-MINT-CANONICAL-ENCODING-P0** — F1.4 + F3 fuzz  
5. **TASK-RIGOR-CLASSIFY-BYTELOCK-REFINEMENT-P0** — F2.1 (hard, high value)  
6. **TASK-RIGOR-OPUHASH-KAT-AND-FUZZ-P0** — F4/C (no VST)

### 17.3 One-line reminder

**Coq Qed ≠ RIGOR C++ formally proved.** Progressive closure means the next theorems are identified, admitted-free, and ordered — not that the node is a proved refinement of the model.

---

## Appendix A — rigor_v01 theorem classes (representative)

| Theorem | Class |
|---------|-------|
| `unique_classifier_is_classify` | model-only Qed |
| `invalid_cannot_enter_asset_domain` | model-only Qed |
| `mint_create_eq_spend` | model-only Qed (field equality) |
| `mint_cannot_bypass_coinbase_maturity` | model-only Qed |
| `every_spendable_path_is_ed25519` | model-only Qed (inductive exhaustiveness) |
| `distinct_paths_distinct_tags` | model-only Qed |
| `total_supply_conservation` | model-only Qed (abstract pools) |
| `T_floor_bracket_unique` | model-only Qed |
| `W32_reorg_preserves_boundary` | model-only Qed |
| `production_W36_not_landed` | model-only Qed (flag) |

## Appendix B — WEAK Qed (do not over-cite)

| Theorem | Why weak |
|---------|----------|
| `Replay_Determinism` | congruence on `fold_apply_blocks` |
| `selection_deterministic` / `unique_mint` (`Proofs/`) | congruence on `Execute` |
| `execute_transition_deterministic` (Phase 4F) | congruence on scaffold |
| `subsidy_independent_of_W` | dummy constant 1000 |

## Appendix C — inventory volume (do not cite as coverage)

| Directory | Files | Thm | Lem |
|-----------|------:|----:|----:|
| `coq/Phase5Equivalence` | 89 | 71 | 1296 |
| `coq/ReplayReorgCore` | 16 | 174 | 345 |
| `rigor_v01` | 20 | 278 | 63 |
| `coq/Kernel` | 7 | 27 | 231 |
| `coq/Phase4GRefinement` | 27 | 23 | 230 |
| `coq/` (root protocol files) | 23 | 71 | 150 |
| `coq/Phase4FConsensus` | 42 | 15 | 115 |
| `coq/fee-burn` | 7 | 19 | 86 |
| `coq/Proofs` | 6 | 7 | 6 |
| `coq/ConnectBlock` | 1 | 6 | 5 |
| `coq/legacy_account_model` | 5 | 0 | 10 |
