# RIGOR Full Consensus Security Audit and Formal Verification Review

**Task:** `TASK-RIGOR-FULL-CONSENSUS-SECURITY-AUDIT-P0-010`  
**Type:** Independent consensus security audit and formal verification review  
**Date:** 2026-08-25  
**Scope:** Complete consensus boundary (validation, UTXO, script, assets, issuance, RES/ORU, OP_CONVERGE, OP_RES_MINT, constants, spec, reference, production, verification coverage)  
**Code changes:** none (audit-first; implementation is a later phase)  
**Honesty flags (observed, not flipped):**

```
equivalence_claimed = false
hash_kernel_shared_binary_with_cpp = false
refinement_proved = false
```

This review does not defend the current design. It asks whether the shipping validator, the specification, and the formal stack are sufficiently secure, deterministic, and robust for mainnet.

Companion canvas: [RIGOR-full-consensus-security-audit.canvas.tsx](/home/user/.cursor/projects/home-timlee-rigor-official-tree/canvases/RIGOR-full-consensus-security-audit.canvas.tsx)

---

## 0. Verdict

**The production C++ consensus path is not known to admit inflation, unauthorized RES mint, or spendable OP_CONVERGE outputs.**  
**The complete consensus boundary is not mainnet-ready.**

Those two sentences must stay separate. Treating the first as the second would be a false-confidence failure.

| Question | Answer |
|---|---|
| Can honest nodes ever reach different consensus results? | **Not by any confirmed production bug found in this review**, given identical binaries and complete block/undo data. Residual fork class: dual F construction (live view vs undo) without an equality gate on committed C. |
| Can invalid state transitions be accepted? | **No confirmed ConnectBlock inflation / mint-without-winner path.** Residual: `assert` instead of reject under `NDEBUG`; Bitcoin-style `assumevalid` (currently genesis / null on public nets); PoW not re-checked inside ConnectBlock. |
| Can assets be created, duplicated, or destroyed incorrectly? | **Creation/duplication: no confirmed path.** Destruction has extra undocumented channels (RFC-018 fee burn; `res_in > res_out`; unspendable v1 programs). |
| Does any hidden global consensus state exist? | **No hidden mint accumulator.** UTXO is monetary truth. Process-local caches of *derived* draw/PoW/script results exist; they must remain pure memoization. |
| Does the implementation preserve RIGOR principles? | **Mostly in production C++.** Spec and Coq lag: Sigma-CR-01 still in SPEC-003; Coq still models `AssetCVG`; Phase catalogs inflate “verified” surface. |

**Mainnet readiness:** **NO** for a full protocol freeze of spec + implementation + verification.  
**Conditional:** the C++ mint/script/UTXO core can be frozen *after* the High findings in §3.2 are closed. Formal verification **must not** be cited as freeze authority (`equivalence_claimed = false`).

---

## 1. Classification legend

| Class | Meaning |
|---|---|
| **Confirmed vulnerability** | Production consensus defect that can inflate, steal, mint without a draw, or split honest nodes with current shipping code |
| **Confirmed integrity issue** | Spec/code/Coq disagree, or a dual path exists that a future implementer or cache bug can turn into a fork |
| **Potential risk** | Plausible attack or divergence class; not demonstrated as currently exploitable |
| **Design concern** | Unnecessary surface or principle violation even if not immediately exploitable |
| **Non-issue** | Examined and found sound |
| **Optional improvement** | Hardening that is not required to close a finding |

Severity: **Critical** (theft/inflation/fork under realistic conditions) · **High** (wrong implementations or freeze-blocking dual truth) · **Medium** (fork class / economic trap / false proof) · **Low** (docs, dead code, test helpers).

---

## 2. What was examined (production SSOT)

Primary evidence is C++ under `core/src/`, not prior audit conclusions.

| Surface | Authority in this tree |
|---|---|
| ConnectBlock | `core/src/validation.cpp` (~2733–3364) |
| CheckTransaction | `core/src/consensus/tx_check.cpp` |
| Dual-pool / UTXO spends | `core/src/consensus/tx_verify.cpp` |
| Asset class | `core/src/script/rigor_classify.cpp` → `AssetTypeFromScriptPubKey` |
| Script | `core/src/script/interpreter.cpp` (no `EvalScript`) |
| RES mint gate | `core/src/consensus/rigor_res.cpp` `ConnectEvaluateCoinbaseResMint` |
| Draw / σ / winner | `core/src/consensus/convergence_logic.cpp`, `consensus_rigor/sigma_hash.h` |
| F / C | `core/src/consensus/convergence_root.cpp` |
| Constants | `core/src/consensus/rigor_consensus_lock.h`, `params.h` |
| PoW / ASERT | `core/src/pow.cpp`, `pow_opus.cpp` |
| Spec | `docs/specification/SPEC-000`…`SPEC-003` |
| Freeze | `docs/MAINNET_PROTOCOL_FREEZE_DECLARATION.md` |
| Coq | `formal_verification/coq/_CoqProject.protocol-core` |

Prior P0 audits (016, 018, 019, 011) were treated as **evidence**, not authority. Several of their runtime findings are already closed in code; several documentation/Coq findings are not.

---

## 3. Findings

### 3.1 Confirmed vulnerabilities (exploitable on shipping ConnectBlock)

**None.**

The following attacks were attempted as review hypotheses and are rejected by multiple independent gates:

| Hypothesis | Result |
|---|---|
| Mint RES with no parent winner | `bad-rigor-res-without-winner` / `bad-rigor-res-winner-mismatch` |
| Body transaction creates RES | `mixed-asset-output` |
| Extra coinbase `OP_RES_MINT` | `bad-rigor-res-amount` |
| Mint amount ≠ `GetResMintReward(h)` | `bad-rigor-res-amount` |
| Mint after schedule end (`W(h)=0`) | `bad-rigor-res-amount` |
| Spend `OP_CONVERGE` | `bad-txns-spend-convergence`; output never enters UTXO (`AddCoins` skip) |
| Unknown script as spendable value | `bad-rigor-script-type` / `ASSET_INVALID` |
| RES → ORU conversion | `illegal-res-to-oru-conversion` |
| PCORB 11-item witness as RES spend | `SCRIPT_ERR_WITNESS_PROGRAM_MISMATCH` |
| CVG2 as convergence ticket | not an allowed script |
| Inequality RHS from `nBits` | `ResolveConvergenceDraw` uses `RES_PROBABILITY_TARGET` only |

Absence of a confirmed exploit is **not** a mainnet go. It is the floor, not the ceiling.

---

### 3.2 Confirmed integrity issues

#### C-1 — SPEC-003 still specifies the superseded σ construction (High)

**What is true in production (Sigma-PoW-01 / SECURITY-001-A8):**

```
σ_i = Hash256( txid || C || B_{h-1} || LE32(h) )   // 100 bytes
CONVERGENCE_DRAW_WINDOW = 32
```

`SelectionDigest` in `convergence_logic.cpp:36–45` and `CalculateSigmaHash`.  
Freeze declaration §3.2 matches the code.

**What SPEC-003 CR-040 / CR-012 still write:**

```
bind_commit = Hash256(B_{h-k} ‖ LE32(target_prev))
σ_i         = Hash256(txid_i ‖ C ‖ bind_commit ‖ LE32(h+1))
k = 10
```

SPEC-000 §1.6 simultaneously lists A8 *and* leftover `bind_commit` / `k=10` rows.

**Root cause:** specification layer was not revised when A8 replaced Sigma-CR-01. Code and freeze text moved; SPEC-003 did not.

**Attack scenario:** a second client, pool, or Coq extract implemented from SPEC-003 produces a different C, different eligibility, and different winner than `rigord`. That is a network split the moment a non-Core validator exists.

**Why this is High, not Critical:** a single-implementation network running this tree does not split today. The freeze document already forbids re-introducing `bind_commit`. The defect is the *normative spec still teaching the forbidden construction*.

##### Remediation plan — C-1

| Item | Plan |
|---|---|
| Security rationale | One σ equation. Two written equations is a future fork. |
| Affected | `SPEC-000`, `SPEC-003` CR-012/CR-040/CR-046; comments in `rigor_math.h`, `params.h`, `convergence_logic.h` |
| Code changes | None required for consensus. Delete `bind_commit` / `k=10` from normative spec; replace with Sigma-PoW-01 and W=32. |
| Consensus impact | None if code unchanged. |
| Compatibility | N/A (pre-mainnet). Spec correction is mandatory before any second implementation. |
| Testing | Spec review checklist: grep `bind_commit`, `k = 10`, `target_prev` as σ input across `docs/specification`. |
| Deployment | Documentation-only; do not ship a “spec frozen” statement until this diff lands. |

---

#### C-2 — Dual construction of F (live overlay vs undo) with no ConnectBlock equality gate (Medium / High if they diverge)

ConnectBlock commits C from **live** `BuildConvergenceDrawSet` + `ListConvergenceRootCommitmentTxids` (block-entry view + in-block overlay).

The *next* block’s parent draw is rebuilt by **undo** `GetConvergenceDrawSetForBlock` → `BuildConvergenceDrawSetFromUndo`.

σ and winner at height h are computed from the reconstructed parent set, **not** by parsing the C already committed in the parent coinbase (beyond paid-winner cross-check).

Unit tests (`convergence_consensus_tests.cpp`) require candidate-for-candidate agreement. ConnectBlock does **not** assert `C_undo == C_coinbase` when paying a parent draw.

**Root cause:** two builders exist for operational reasons (template/live vs reorg/read). The identity is a test invariant, not a consensus predicate.

**Attack scenario (if builders diverge on some tx shape):** all honest Core nodes still agree with each other (they all use undo for the parent). The committed C would not be the C used in σ. An adversarial miner who understands the divergence could grind the undo-F winner while committing a different live-F C. That breaks the “PoW commits C, C binds the draw” story.

##### Remediation plan — C-2

| Item | Plan |
|---|---|
| Security rationale | C in the coinbase must be the C used in σ. Anything else is a hidden second draw. |
| Affected | `validation.cpp` ConnectBlock parent-draw path; `convergence_logic.cpp` both builders |
| Expected code | After `GetConvergenceDrawSetForBlock(parent)`, parse parent coinbase C and reject with a new reason (e.g. `parent-convergence-root-mismatch`) if it differs from `parent_draw_set.convergence_root`. Optionally drop the live builder from ConnectBlock and commit the same undo-equivalent overlay function. **Safer: one function, two data sources, plus equality gate.** |
| Consensus impact | If builders already agree on all valid blocks: **NONE** (reject set unchanged). If they disagree on some currently-accepted block: **hard fork / pre-mainnet rule tightening**. Discover that before genesis. |
| Compatibility | Pre-mainnet: tighten. Do not ship genesis until a differential test of live vs undo over generated blocks is green *and* the equality gate is in ConnectBlock. |
| Testing | Existing view/undo tests; new ConnectBlock test that mutates undo independently and expects reject; fuzz overlay vs undo. |
| Deployment | Must land before mainnet genesis. |

---

#### C-3 — Documented `bad-pcorb-carrier` reject does not exist (Medium)

`tx_verify.h:36` states body PCORB carriers are rejected with `bad-pcorb-carrier`. Grep of `core/src` finds **no** such reject reason. A `nValue==0` OP_RETURN with the PCOR tag is an allowed unspendable output.

**Not theft:** mint authorization is solely `ConnectEvaluateCoinbaseResMint`. PCORB cannot mint.

**Is a principle violation:** freeze and pipeline docs say the only commitment of F is coinbase scriptSig C; a second on-chain “proof” channel remains writable.

##### Remediation plan — C-3

| Item | Plan |
|---|---|
| Security rationale | Minimum consensus surface. A dead tag that still parses is a future footgun (A-7 class). |
| Affected | `CheckTransaction` / `CheckTxInputs`; `res_proof.cpp`; `res_mint_carrier.cpp`; comments |
| Expected code | Either (A) reject tagged PCORB OP_RETURN in body *and* coinbase, or (B) delete carrier helpers from the consensus library and the claim. **Prefer B + reject the 4-byte tag in any `OP_RETURN` if the bytes are cheap to test — or delete the tag constants from consensus headers.** Safest: stop compiling `res_proof.cpp` / `res_mint_carrier.cpp` into libbitcoin_consensus; reject `IsPcorbCarrier` outputs if the function remains. |
| Consensus impact | (A) is a new reject (pre-mainnet OK). (B) without reject is docs/link cleanup only. |
| Compatibility | Pre-mainnet: no chain to migrate. |
| Testing | Vector: coinbase and body with PCORB OP_RETURN must fail if (A); mint still only via `OP_RES_MINT`. |
| Deployment | Before genesis. |

---

#### C-4 — Coq still models a deleted CVG asset class (Medium)

C++ has no `OP_CVG_CLAIM`. Coq `ScriptMintWitness.v` maps that template to `SmwCVG`; `AssetFromScript.v` maps it to `AssetCVG`. `OpaqueBoundary.v` still contains `we_do_not_claim_script_derives_asset` while Task 4 *does* derive pooling class from script shape.

**Not a runtime fork.** It is a false-confidence / governance defect: citing Coq conservation lemmas as “the C++ asset model” proves the wrong function.

##### Remediation plan — C-4

| Item | Plan |
|---|---|
| Security rationale | Formal artifacts that over-approximate consensus become a second specification. |
| Affected | `ScriptMintWitness.v`, `CvgCovenant.v`, `AssetFromScript.v`, `StateTransition.v`, `OpaqueBoundary.v`; `RIGOR_FORMAL_VERIFICATION_CURRENT_STATE.md` |
| Expected code | Remove `SmwCVG` / `AssetCVG` from the protocol-core cone, or isolate them in an explicitly non-shipping directory with a theorem `we_do_not_claim_cvg_exists_in_cpp`. Update the non-claim about script-derived assets to match Task 4. |
| Consensus impact | None (Coq only). |
| Compatibility | N/A. |
| Testing | `make -f Makefile.coq` protocol-core; `coqchk` sample; inventory grep `AssetCVG`. |
| Deployment | Before any public “formally verified asset model” sentence. |

---

#### C-5 — SPEC ORU recurrence omits RFC-018 burn and unclaimed coinbase (Medium)

SPEC-000 / SPEC-002:

```
S_ORU(h+1) = S_ORU(h) + R(h+1) − C(h+1)
```

Production circulating ORU (UTXO) also decreases by:

1. RFC-018: `OruResTransferFeeBurnShare` of RES-transfer ORU fees (coinbase must not claim them)
2. Miner claiming strictly less than `oru_reward_limit`
3. `MAX_MONEY` / `MoneyRange` clipping on some aggregations (see D-2)

ConnectBlock **does not** enforce the SPEC recurrence as a reject (W1 removed `bad-oru-supply-negative`). UTXO remains truth. The specification is therefore **not** a description of production supply.

##### Remediation plan — C-5

| Item | Plan |
|---|---|
| Security rationale | A false recurrence will be implemented by the next client. |
| Affected | SPEC-000, SPEC-002, SPEC-003 CR-032; observational metrics comments |
| Expected code | Redefine `S_ORU` as the UTXO ORU sum, or write the full recurrence: `+R +claimable_fees −C −fee_burn −unclaimed`. State explicitly that ObservationalMetrics is not a consensus predicate. |
| Consensus impact | Docs only, unless someone re-introduces a supply-scalar reject. **Do not re-introduce W1 rejects.** |
| Testing | Spec examples matching a ConnectBlock fixture with RFC-018 burn. |
| Deployment | Spec track; no binary change. |

---

#### C-6 — ConnectBlock comments claim a runtime RES cap check that does not exist (Low)

RFC-005 step 14 comments in `validation.cpp` (~2996–3000) say cap is enforced at stale line numbers. `rigor_consensus_lock.h:247–248` states ConnectBlock does **not** re-check the cap; authority is `static_assert(ResScheduleSum() <= RES_MAX_SUPPLY / RES_REWARD_EPOCH)` plus at most one mint per block.

That arithmetic is **stronger** than a runtime running total *if and only if* “at most one mint per block of size `W(h)`” holds. That local rule is enforced. SPEC-003 CR-045 still requires a runtime `S_RES` reject.

##### Remediation plan — C-6

| Item | Plan |
|---|---|
| Security rationale | Dual stories (runtime cap vs compile-time identity) confuse reviewers into adding a global scalar (forbidden). |
| Affected | SPEC-003 CR-045; ConnectBlock comments |
| Expected code | Align SPEC-003 with the compile-time identity. Delete stale “STEP 14 will be verified at lines …” comments. Optional defense-in-depth: a ConnectBlock check that `res_mint_coinbase ∈ {0, GetResMintReward(h)}` (already true via `ConnectEvaluateCoinbaseResMint`). **Do not add `GetChainResMintTotal` as a reject.** |
| Consensus impact | Docs. Optional local check is redundant if mint gate stays. |
| Testing | Existing schedule `static_assert` / Coq `max_issuance_within_cap`. |
| Deployment | Docs. |

---

#### C-7 — Stale ownership comments still teach Option A (Medium for ecosystem, Low for this node)

Production Option B: witness v1 program **is** the 32-byte Ed25519 public key (`ResWitnessProgram` memcpy; spend requires `pk == program`). Owner on `OP_CONVERGE` / `OP_RES_MINT` is `HASH160(pk32)`.

`standard.h` / `convergenceres.h` still say `HASH160(RES program)`. Under Option A that was `HASH160(SHA256(pk))`, which the wallet does not produce. A miner following the comment can pay a consensus-valid, **unspendable** mint (covered by `script_asset_ownership_freeze_tests.cpp`).

P0-018 H-1 remains open at the comment layer.

##### Remediation plan — C-7

| Item | Plan |
|---|---|
| Security rationale | Unspendable RES is economic destruction, not inflation; still a consensus-adjacent footgun. |
| Affected | `standard.h`, `convergenceres.h`, GBT RPC comments, freeze table already correct |
| Expected code | Comment-only: `HASH160(pk32)`, and `HASH160(pk32)==HASH160(v1 program)` under Option B. |
| Consensus impact | None. |
| Testing | Existing freeze tests. |
| Deployment | Before public SDK / pool docs. |

---

### 3.3 Potential risks

| ID | Severity | Risk | Why not confirmed |
|---|---|---|---|
| P-1 | Medium | `assert(!coin.IsSpent())` in `AccumulateTxInputPools` (`tx_verify.cpp:192`). `NDEBUG` strips asserts. | Preceded by `HaveInputs`; Bitcoin-class residual, not a demonstrated inflation path. |
| P-2 | Medium | `g_draw_set_cache` / `g_window_contribution_cache` return memoized F without re-hashing against coinbase C. | Fail-closed on paid-winner index mismatch; keyed by block hash. Poisoning requires a buggy Remember. |
| P-3 | Low | `g_opus_pow_ok` positive PoW cache (`header_hash → nBits`). False positive would accept invalid work. | Inserted only after a successful OpuHash compare; process-local. |
| P-4 | Low | `g_scriptExecutionCache` false-positive insert-before-all-checks. | Insert is after the loop; salted hasher; ConnectBlock does not insert (`fCacheResults = fJustCheck`). Fence comment is correct and load-bearing. |
| P-5 | Medium | Live vs undo F (see C-2). | Tests exist; no counterexample constructed in this audit. |
| P-6 | Low | SIMD SHA-256 in `rigord` vs Coq/portable Transform. | Theoretical consensus split across hash implementations, not across identical binaries. |
| P-7 | Medium | Hashrate share α still captures ≈α of tickets (A-9-C removes exclusivity, not share). | Economic, not a validation bug. Documented in `PRE-MAINNET-CHECKLIST.md`. |
| P-8 | Low | `assumevalid`: wrong hash could skip scripts. | Mainnet `defaultAssumeValid = hashGenesisBlock`; `GetAncestor(h)==pindex` fails for h>0. Scripts run. |
| P-9 | Low | ConnectBlock does not re-run `ContextualCheckBlockHeader` (PoW/nBits). | Bitcoin-inherited; P2P path checks headers first. Documented in-code. |
| P-10 | Medium | Arbitrary 32-byte v1 programs classify as RES; `SHA256(pk)` is unspendable (freeze test). | Not inflation. Can lock funds sent to a hashed program. |

---

### 3.4 Design concerns

| ID | Concern | Recommendation |
|---|---|---|
| D-1 | `CONVERGENCE_DRAW_MIN_TXS_FOR_RES = 1` | Keep (RFC-015). Do not pretend W=32 is a decentralization proof. |
| D-2 | `MAX_MONEY = 21e6 * COIN` is a **consensus-critical per-output / per-tx-input-sum cap** while ORU issuance is uncapped (1000 ORU/block). After ~21 000 blocks, a tx whose ORU inputs sum above 21M ORU is invalid. | Either raise `MAX_MONEY` to a bound above any plausible UTXO consolidation (e.g. `std::numeric_limits<CAmount>::max()/2`) **before genesis**, or freeze 21M as an intentional per-tx ceiling and document it as a consensus rule. Silence is the bug. |
| D-3 | `res_in >= res_out` allows silent RES destruction | Document as intentional (lost coins) or require equality. |
| D-4 | `res_proof.cpp` / `res_mint_carrier.cpp` still linked | See C-3. Dead mint narrative. |
| D-5 | `expected_draw_candidate_root` computed then `(void)` in `VerifyCoinbaseConvergenceAndResLayout` | Delete the argument or commit DCR. Pipeline currently says DCR is not required — delete the dead parameter. |
| D-6 | Two Coq machines (legacy account vs ReplayReorgCore) | Do not cite `Proofs/*` or `legacy_account_model/` in security claims. |
| D-7 | `GetBlockSubsidy` falls back to `50 * COIN` if `nBlockSubsidyBase==0` | All shipped nets set the lock constant. Fail closed instead of Bitcoin-50. |
| D-8 | Coinbase `OP_CONVERGE` counts in C(t) but is never in F (list starts at tx 1) | Harmless ORU burn; document or reject coinbase converge. |
| D-9 | Phase-4F/5 catalogs ≈ 61% of named “theorems” | Public counts must use protocol-core only. |

---

### 3.5 Non-issues (examined, sound)

| Topic | Why it is sound |
|---|---|
| Script-as-asset in C++ | `CTxOut` is `(nValue, scriptPubKey)` only. Class from `AssetTypeFromScriptPubKey`. Unknown → `ASSET_INVALID`. |
| No EvalScript dual path | `VerifyScript` is mint / v0 P2WPKH / v1 key-path only. CVE-2024-52911 async script queue deleted. |
| Ed25519 sighash domain | Tagged `RGRORU01` / `RGRRES01` / `RGRMNT01` + explicit `chain_id` (genesis hash argument). No `g_chainparams` in `SignatureHash`. `SigVersion::BASE` deleted. |
| OP_CONVERGE | 22-byte canonical; skipped by `AddCoins`; spend rejected; classified ORU for pooling only. |
| OP_RES_MINT | Coinbase-only; owner = ticket HASH160; amount = `W(h)`; coupled to parent draw. |
| Body cannot create RES | `mixed-asset-output`. |
| Inequality not from PoW | `RES_PROBABILITY_TARGET` constant; E=246 windowed hex matches freeze. |
| Winner uniqueness | 512-bit cross-multiply; txid tie-break; at most one mint. |
| W1 supply scalars | Observational; not ConnectBlock rejects. |
| FVR mirrors | SPEC-003 §12: forbidden as predicates. Code matches. |
| Float on draw path | Integer `ResIntegerSqrt` / uint512 only. |
| chain_id global | Explicit argument / `PrecomputedTransactionData::m_chain_id`. |
| Mainnet bootstrap RES | `res_wallet_bootstrap_coinbase = 0`. |
| Parallel script workers | Deleted (CVE-2024-52911). |

---

### 3.6 Optional improvements

| ID | Change | Why optional |
|---|---|---|
| O-1 | Reject v1 programs that are not valid Ed25519 public keys (on-curve / encoding) | Closes P-10; is a consensus change; do it before genesis if at all. |
| O-2 | ConnectBlock equality of parent C (C-2) is **not** optional — listed here only if C-2 is deferred, which this audit does **not** recommend | — |
| O-3 | Single F builder | Simplification that also closes C-2. |
| O-4 | Remove Phase catalog modules from default `coqchk` publicity | Honesty, not consensus. |
| O-5 | Fail closed if `nBlockSubsidyBase==0` (D-7) | Tiny, pre-mainnet. |

---

## 4. Core principle review

### 4.1 Deterministic consensus

Production validation is a function of `(parent index + UTXO view + block)` for ConnectBlock. Candidate sets are sorted by txid for C. Winner order is integer. No local clock in the draw.

Residual non-determinism classes: cache bugs (P-2/P-3/P-4), live vs undo (C-2), hash implementation (P-6). None confirmed.

`Proofs/Determinism.v`-style Coq “Execute is a function” results do **not** prove C++ determinism.

### 4.2 No global consensus state

Monetary truth is the UTXO set. There is no account table and no ConnectBlock mint accumulator.

What exists and is acceptable **only as memoization**:

| Object | Role | Must not |
|---|---|---|
| `m_paid_winner_txid` | Disk cache of coinbase paid-winner push | Diverge from scriptSig (already fail-closed) |
| `g_draw_set_cache` / window cache | Rebuild F | Diverge from disk reconstruction |
| ObservationalMetrics | Explorer | Reject blocks |
| `g_scriptExecutionCache` | Script results | Insert before all checks |
| `g_opus_pow_ok` | Successful PoW | Cache failures as success |

W1/W5/W7 fences are correctly aimed. Do not re-introduce height-keyed witnesses or supply-scalar rejects.

### 4.3 Script-as-asset

Holds in production. Four spendable/burn templates. Ownership is one Ed25519 commitment per template. Coq still has a fifth class (`AssetCVG`) — C-4.

### 4.4 Minimal consensus surface

C++ script surface is small. The **repository** surface is not: PCORB parsers, CVGE commitment structs, `bad-convergence-cumulative` comments, Coq covenants, Phase catalogs. Minimum-risk design is deletion, not documentation that says “unused.”

---

## 5. Attack scenarios (residual)

These are the scenarios a mainnet adversary would actually try. None are confirmed working against this tree’s ConnectBlock; each maps to a finding.

| # | Adversary | Goal | Status |
|---|---|---|---|
| A1 | Block producer | Mint RES to self instead of ticket owner | **Blocked** (`bad-rigor-res-owner-mismatch`) |
| A2 | Block producer | Mint without eligible winner | **Blocked** |
| A3 | Block producer | Omit mint when winner exists and `W(h)>0` | **Blocked** (`bad-rigor-res-winner-mismatch`) |
| A4 | User | Spend OP_CONVERGE | **Blocked** (no UTXO + spend reject) |
| A5 | User | Create RES in a body tx | **Blocked** |
| A6 | Miner majority | Exclude others from F | **Mitigated not eliminated** (W=32; P-7) |
| A7 | Implementer | Follow SPEC-003 σ | **Would fork** (C-1) |
| A8 | Implementer | Follow HASH160(program) comments | **Unspendable mint** (C-7) |
| A9 | Miner | Craft txs so live F ≠ undo F | **Unknown / C-2** |
| A10 | Peer | Feed assumevalid-invalid chain | **Not default-mainnet** (P-8) |
| A11 | User | Send RES to SHA256(pk) v1 | **Funds locked** (P-10) |
| A12 | Wallet | Consolidate >21M ORU in one tx | **Rejected after ~21k blocks** (D-2) |

---

## 6. Formal verification gaps

P0-016 remains the correct FV completeness verdict. This section only restates what matters for **this** consensus audit.

### 6.1 Actually proved (model, Closed, protocol-core)

- RFC-017 schedule sum `99450437`; `max_issuance_within_cap`
- C / winner order-independence **given a candidate list**
- E=246 mint cost 1024 ORU; windowed T non-vacuous
- Dual-pool on `apply_block` of abstract txs
- `winner_must_mint` under abstract `cm_draw_won` — **not** production CI-033
- 215 golden harness lemmas (finite L1↔L2)
- Explicit non-claims in `OpaqueBoundary.v`

### 6.2 Assumed / not proved

| Gap | Production locus | Consequence if false |
|---|---|---|
| No L2↔L3 refinement | `ConnectBlock` | Coq success ≠ node correctness |
| `cm_draw_won` ≠ owner+PCORB+parent F | `ConnectEvaluateCoinbaseResMint` | Wrong owner / skipped mint outside the theorem |
| F list given, not built | `BuildConvergenceDrawSet` | Same σ math, different F |
| No Ed25519 / sighash | `interpreter.cpp` | P0-018 class bugs invisible to Coq |
| No PoW / ASERT / OpuHash | `pow.cpp` | Header acceptance unverified |
| `rs_res_supply` ≠ UTXO RES | observational vs coins | Scalar vs spendable diverge |
| SHA-256 collision resistance | all hashes | Fairness / grinding arguments |
| SIMD hasher ≠ Coq SHA | `rigord` | Theoretical split |

### 6.3 Must not be claimed

- “Formally verified blockchain”
- “Mainnet freeze certified by Coq”
- Theorem count ~2873 as protocol coverage (majority Phase paperwork)
- `winner_must_mint` as CI-033
- `p5_mainnet_freeze_established` as a proof

Allowed public sentence remains:

> Formally verified protocol invariants with explicit verification boundaries.

### 6.4 Verification improvements (priority)

1. **Do not expand Phase catalogs.** They harm honesty.
2. Close C-4 (delete or isolate `AssetCVG`).
3. Add a **C++** equality gate for C (C-2) — more valuable than another Coq lemma about a given list.
4. Pin production mint authorization with **vectors** (`res_proof_math_tests` F-2 family) as the CI-033 surrogate until a refinement exists.
5. Keep `run_differential_gate.sh` and `run_pre_mainnet.sh` as the operational bar; they are not a refinement.

---

## 7. Shipping constants (code SSOT)

| Constant | Value |
|---|---|
| `RES_PROBABILITY_TARGET` | `000000000055e63b88c230e77e7ee106959b5d3e1e9ea69ebac0b0b4be383479` (E=246, W=32) |
| `CONVERGENCE_DRAW_WINDOW` | 32 |
| `MIN_CONVERGENCE_AMOUNT` | 1 ORU |
| `CONVERGENCE_DRAW_MIN_TXS_FOR_RES` | 1 |
| `RES_REWARD_GENESIS` | 40.000000 RES |
| `GetResMintReward` | `W(⌊h/210000⌋)`, 0 after epoch 250 |
| `ResScheduleSum` | 99 450 437 atoms |
| `RES_MAX_SUPPLY` | 21 000 000 RES |
| ORU subsidy | 1000 → 125 ORU, 3 halvings, then constant |
| σ | Sigma-PoW-01, 100 bytes, parent hash at [64..95] |
| `nCoinbaseMaturity` | 100 |
| Mainnet `defaultAssumeValid` | genesis hash |
| Mainnet bootstrap RES | 0 |

Authority order when documents disagree: `rigor_consensus_lock.h` + `params.h` → Coq `Selection.v` → RFC-017 → freeze declaration → older SPEC/RFC prose.

---

## 8. Mainnet readiness assessment

| Gate | Status |
|---|---|
| No confirmed inflation / unauthorized mint | **PASS** |
| Script-as-asset fail-closed in C++ | **PASS** |
| Dual-asset conservation in CheckTxInputs | **PASS** |
| Sigma-PoW-01 in production | **PASS** |
| Winner–mint coupling | **PASS** |
| SPEC σ matches code | **FAIL (C-1)** |
| Single F construction / C equality gate | **FAIL (C-2)** |
| Dead PCORB narrative removed | **FAIL (C-3)** |
| Coq asset model = C++ | **FAIL (C-4)** |
| SPEC ORU recurrence complete | **FAIL (C-5)** |
| `MAX_MONEY` vs unbounded ORU decided | **FAIL (D-2)** |
| Owner-hash comments Option B | **FAIL (C-7)** |
| `equivalence_claimed` | **false (correct)** |
| Pre-mainnet functional suites | **operator gate, not re-run in this audit** |
| Historical chain / migration | **none (good)** |

**Go / No-Go:** **NO-GO** for mainnet genesis and for any statement that the “complete consensus system” is freeze-ready.

**Narrower statement that would be honest after C-1, C-2, C-3, C-7, D-2 are closed:**

> The shipping C++ validator’s mint, script, and UTXO rules are simple enough to freeze. Specification and Coq must be edited to that same object. Formal verification remains a bounded model plus a differential gate, not a refinement of `rigord`.

---

## 9. Recommended implementation order

Pre-mainnet, no compatibility constraint. Do **not** preserve dead paths.

| Order | ID | Work | Owner skill |
|---|---|---|---|
| 1 | C-2 | One F function + ConnectBlock `C_parent` equality | Consensus C++ |
| 2 | D-2 | Decide `MAX_MONEY` (raise or document as rule) | Consensus C++ / spec |
| 3 | C-3 | Delete or reject PCORB carrier | Consensus C++ |
| 4 | C-1, C-5, C-6, C-7 | Spec and comment alignment | Spec |
| 5 | D-5, D-7, D-8 | Dead args / subsidy fallback / coinbase converge | Consensus C++ |
| 6 | C-4, D-6, D-9 | Coq cone honesty | FV |
| 7 | P-1 | Replace remaining consensus `assert` with `Invalid` | Consensus C++ |
| 8 | O-1 | Optional v1 on-curve check | Consensus C++ (fork) |

Do not implement in this task. A later engineer should execute only against an approved subset of this plan.

---

## 10. Answers the task asked, compressed

**Security findings:** §3.  
**Severity:** tables in §3.2–§3.4.  
**Affected components:** ConnectBlock, classify, mint, draw builders, SPEC-003, Coq asset modules, `MAX_MONEY`.  
**Attack scenarios:** §5.  
**Formal verification gaps:** §6.  
**Recommended fixes:** remediation boxes + §9.  
**Mainnet readiness:** **NO-GO** for the complete boundary; production mint/UTXO/script core has **no confirmed exploitable consensus bug** in this review.

---

## 11. Audit independence note

Assumptions challenged:

- That SPEC-003 is an accurate description of σ.
- That compile-time RES cap identity plus comments equal CR-045.
- That Coq theorem cardinality implies ConnectBlock correctness.
- That W=32 “solves” miner control of F.
- That observational supply recurrences are consensus.
- That unused PCORB code is harmless.

Safest correction is **deletion and unification**, not additional parallel checks.

---

**Signed:** independent review, TASK-010, 2026-08-25.  
**Consensus impact of this document:** none (audit only).
