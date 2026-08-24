# RIGOR Whitepaper Technical Traceability Matrix

**Task:** `TASK-RIGOR-PROTOCOL-CONSISTENCY-FINAL-AUDIT-P0-001`  
**Date:** 2026-08-20  
**Source under review:** `docs/whitepaper/RIGOR_PROTOCOL_WHITEPAPER.md` (bound protocol paper) plus live site `/documentation/whitepaper`  
**Honesty:** `equivalence_claimed = false`  
**Consensus / wallet / miner / RFC impact:** **NONE**

Related: [FORMAL_CLAIM_TRACEABILITY_MATRIX.md](./FORMAL_CLAIM_TRACEABILITY_MATRIX.md) (FV-line audit), [RIGOR_FORMAL_CLAIM_TRACEABILITY.md](./RIGOR_FORMAL_CLAIM_TRACEABILITY.md) (public claim table).

Status vocabulary:

| Status | Meaning |
|--------|---------|
| **VERIFIED** | Coq model theorem(s), Closed under stated assumptions |
| **VECTOR** | Finite golden / parity / differential-gate check |
| **PARTIAL** | Model *or* implementation evidence; not both as refinement |
| **DOC** | Specified in RFC / paper / registry; not a Coq theorem of that sentence |
| **NOT** | Explicitly outside the verification boundary |

---

## Script architecture

| Whitepaper claim | Implementation location | Formal artifact | Verification status |
|------------------|-------------------------|-----------------|---------------------|
| RIGOR minimizes script complexity rather than eliminating scripts | `docs/security/minimal-script-architecture.md`; `RIGOR_SCRIPT_TYPE_REGISTRY.md`; `core/src/consensus/tx_check.cpp` (`IsAllowedScript`); `core/src/script/interpreter.cpp` | `ScriptShape.v` / `apply_block_succeeds_implies_script_shape`; SMW / ICVG vectors | **PARTIAL** (shape VERIFIED + VECTOR; P2WSH interpreter **NOT** Coq) |
| Constrained scripts exist; no general-purpose VM; no arbitrary contracts | Registry; `VerifyWitnessProgram` (no BIP341); `SCRIPT_ERR_SECP256K1_DISABLED` | OpaqueBoundary non-claims; ScriptMintWitness abstract form | **DOC** + C++ enforcement; interpreter **NOT** |
| Witness v0: spendable ORU (P2WPKH / P2WSH / native 32-byte Ed25519) | `rigor_classify.cpp`; `standard.cpp`; `interpreter.cpp` | SMW rows (v0 claimed as RES reject) | **DOC** + VECTOR (reject form); spend semantics **NOT** |
| Witness v1 RES key-path (`RES2` or native 32-byte; not CVG2) | `IsResScript`; `AssetTypeFromScriptPubKey` | `ScriptMintWitness.v` `IsResScript` (not wired as pooling class) | **PARTIAL** (`we_do_not_claim_script_derives_asset`) |
| Witness v1 CVG2: ORU-domain Convergence commitment, unspendable | `IsConvergenceScript`; omitted from `AddCoins`; spend reject | `InvalidCvg.v`; spend-CVG2 conjunct on `apply_tx` | **PARTIAL** (container + spend-CVG2 VERIFIED + VECTOR; not pooling class) |
| Witness v1 is not Bitcoin Taproot / BIP341 | Validation path comments; registry | — | **DOC** |
| P2PKH / P2SH / witness v2 rejected as new outputs | `bad-rigor-script-type` / nonstandard | SMW-00x | **VECTOR** + C++ |

---

## Dual assets

| Whitepaper claim | Implementation location | Formal artifact | Verification status |
|------------------|-------------------------|-----------------|---------------------|
| ORU and RES are native UTXO assets at the same protocol layer | `CTxOut` `(nValue, scriptPubKey)` only; class from script | Coq `AssetKind` / dual-pool model | **PARTIAL** (no C++ refinement) |
| Not colored coins, inscriptions, Ordinals, Runes, wrapped assets, or smart-contract tokens | No overlay indexer in consensus; no token VM | — | **DOC** (architecture) |
| Asset class inferred from `scriptPubKey`; no parallel asset field | `AssetTypeFromScriptPubKey` | `we_do_not_claim_script_derives_asset` (pooling still tagged) | **DOC** + honest model gap |
| Per-asset conservation (ORU fees; RES in ≥ out except mint) | `CheckTxInputs` / `cross-asset-input` | `dual_pool_conservation`; `apply_block_res_net_bounded_by_scheduled_mints` | **VERIFIED** (model) + **VECTOR** + runtime |
| ORU: coinbase subsidy 1000 → 500 → 250 → 125; no hard cap | `GetBlockSubsidy`; `ORU_MIN_BLOCK_SUBSIDY` | `oru_supply_bounded_by_block_count` | **PARTIAL** (bound VERIFIED; no absolute ORU cap by design) |
| RES: body mint to winner’s Witness v1 program, not miner coinbase | `ConnectBlock` mint path; `bad-rigor-res-owner-mismatch` | `winner_must_mint` under abstract `cm_draw_won` | **PARTIAL** |

---

## RES monetary policy

| Whitepaper claim | Implementation location | Formal artifact | Verification status |
|------------------|-------------------------|-----------------|---------------------|
| `k = height / 210000`; `W(k) = floor(40·RES_COIN·(k+1)^(−3/2))` for `0 ≤ k ≤ 250` | RFC-017; `Consensus::ResRewardForEpoch` in `rigor_consensus_lock.h`; Python `res_reward_for_epoch` | `res_reward_for_epoch` in `ResSchedule.v`; `reward_pin_table` | **VERIFIED** + **VECTOR** (`test_rfc017_schedule_parity.py` `k=0..251`) |
| `W(k) = 0` for `k > 250` (`RES_REWARD_LAST_EPOCH = 250`) | Same C++ `if (k > LAST)` | `schedule_terminates` | **VERIFIED** + **VECTOR** |
| `RES_COIN = 1_000_000`; `RES_MAX_SUPPLY = 21_000_000 RES` | `rigor_consensus_lock.h` static_assert | `RES_COIN`; `RES_MAX_SUPPLY` | **DOC** + compile-time lock + Coq definitions |
| `schedule_sum = 99450437` atoms; `210000 × schedule_sum < 21M` | C++ `static_assert(ResScheduleSum() == 99450437)` | `schedule_sum_value`; `max_issuance_within_cap` | **VERIFIED** + **VECTOR** |
| Occurrence demand-driven; amount deterministic if mint | Draw + `GetResMintReward` | Selection + schedule | **PARTIAL** (amount VERIFIED; winner flag abstract) |
| Cost floor `C / W(k)` weakly rises; `T` constant | RFC-017; `Selection.v` `E = 246` ⇒ `C = 1024` ORU | `cost_floor_nondecreasing_along_schedule`; `mint_cost_is_1024_oru` | **VERIFIED** (model) |
| 21M is a ceiling, not a fill promise | ConnectBlock `bad-rigor-res-max-supply`; schedule cannot exhaust cap | Cap dual-insurance in `ResMintObligation.v` | **VERIFIED** (model) + C++ reject |
| Formally verified *monetary invariants*, not C++↔Coq refinement | Ch.2 summary | Assumption boundary | **DOC** (honesty) |

Numeric lock (all four of RFC-017, C++, Coq, Python, Monetary Policy page):

| Symbol | Value |
|--------|-------|
| `RES_COIN` | `1_000_000` |
| `RES_REWARD_GENESIS` / `W(0)` | `40_000_000` atoms |
| `RES_REWARD_EPOCH` | `210_000` blocks |
| `RES_REWARD_LAST_EPOCH` | `250` |
| `schedule_sum` | `99_450_437` atoms |
| Max conceivable issuance | `20_884_591.770000` RES |
| `RES_MAX_SUPPLY` | `21_000_000` RES (`21_000_000_000_000` atoms) |

Pin table used on the website and in Coq: `k=0 → 40.000000`, `k=1 → 14.142196`, `k=10 → 1.096405`, `k=100 → 0.039407`, `k=250 → 0.010058`, `k=251 → 0`.

---

## Convergence consensus

| Whitepaper claim | Implementation location | Formal artifact | Verification status |
|------------------|-------------------------|-----------------|---------------------|
| Convergence is deterministic selection over public set `F`, not a lottery UI | Draw RFCs; `validation.cpp` / draw helpers | `select_winner_order_independent` | **VERIFIED** (model) + **VECTOR** |
| `Convergence_Root` = Hash256(concat(sort(eligible txids))); `\|F\|<2` → 32 zero bytes | `ComputeConvergenceRootFromEligibilityTxids` | `convergence_root_order_independent`; ICVG / CR vectors | **VERIFIED** + **VECTOR** |
| σ = Hash256(`txid \|\| C \|\| B_{h-1} \|\| LE32(h)`) (100 bytes) | SECURITY-001-A8; portable σ CLI | `SigmaPoW01` / CanonicalDrawEncoder | **VERIFIED** + **VECTOR** |
| Window `W = 32`; unpaid exclusion (A-9-C) | `CONVERGENCE_DRAW_WINDOW` | WindowExclusion `txid_wins_at_most_once` | **VERIFIED** + **VECTOR** |
| Shipping target `E = 246`, expected wins `n / 1024`, mint cost `C = 1024` ORU | RFC-017 / RFC-014 v1.2 as implemented; `params` / lock header | `mint_cost_is_1024_oru`; `res_probability_target_not_vacuous` | **VERIFIED** (model) |
| Teaching identity `2^240/2^256 = 1/65536` is **not** shipping `T` | Whitepaper **abstract** + Appendix B | `Selection.v` comments (E moved 240 → 246) | **DOC** in abstract/appendix; **Ch.5 body still writes 65536 as published identity** (residual) |
| Draw candidate root binds eligible list | Production root CLI in differential gate | `DrawCandidateRoot.v` / DCR vectors | **VERIFIED** + **VECTOR** (eligible-list digest; coinbase/Merkle/header bind **NOT**) |
| No Convergence → no RES | Mint reject `bad-rigor-res-without-winner` | H-1 / `winner_must_mint` | **PARTIAL** (`cm_draw_won` abstract) |

---

## Formal verification

| Whitepaper claim | Implementation location | Formal artifact | Verification status |
|------------------|-------------------------|-----------------|---------------------|
| Formally verified protocol invariants with explicit verification boundaries | Ch.8; `/documentation/formal-verification`; `verification/README.md` | This matrix; assumption boundary | **DOC** (citation rule) |
| Three layers must not collapse into “formally proven blockchain” | Ch.8 | Forbidden-list in HOW_TO_VERIFY / claim table | **DOC** |
| Living lock 422 shared / 215 Coq-enforced | `formal_verification/golden_lock.json`; gate script | Kernel harnesses | **VECTOR** |
| Historical 324/117 and RC0 414/207 are freeze snapshots, not the living lock | Ch.8 Layer 3 paragraph | STATUS / FREEZE docs | **DOC** |
| `equivalence_claimed = false` (and hash-kernel / refinement flags false) | Site `version.ts`; Coq `DrawBridgeProgress.v` | `equivalence_not_claimed` | **DOC** + Coq bool lock + UI fault |
| Coq model strongly verified; not shipping-binary identity | `_CoqProject`; verdict | Load-bearing theorem list | **VERIFIED** (model) |
| Differential gate: Python oracle, hash CLI, σ/root CLI | `run_differential_gate.sh` | HOW_TO_VERIFY_RIGOR.md | **VECTOR** |
| PROOF-034 is an empirical archive, not equivalence | `/experiment`, `/status` | PROOF-034 docs | **DOC** / **NOT** as proof |
| Complete C++ ↔ Coq ↔ Python equivalence | — | Phase-5 `DCS_Impossible` | **NOT** |
| SHA-256 collision resistance | Hash kernel + FIPS vectors | Executable model only | **PARTIAL** |
| Mining / difficulty / wallet / P2P | C++ node tests | — | **NOT** |

---

## How to read this matrix

A **VERIFIED** cell is a statement about the **Coq model** (and sometimes a finite vector). It is not a license to set `equivalence_claimed = true`.

Public reproduce path: [`verification/README.md`](../../verification/README.md), [`HOW_TO_VERIFY_RIGOR.md`](./HOW_TO_VERIFY_RIGOR.md).
