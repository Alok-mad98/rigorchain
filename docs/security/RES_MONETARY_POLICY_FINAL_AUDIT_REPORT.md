# RES Monetary Policy — Final Audit Report (RFC-017)

**Task:** `TASK-RIGOR-RES-MONETARY-POLICY-FINAL-VERIFICATION-P0-001`  
**Date:** 2026-08-20  
**Scope:** Alignment of C++ / Coq / Python / golden vectors / public docs  
**Consensus changes:** **NONE** (no inconsistency requiring a code fix)  
**`equivalence_claimed`:** `false`

| Field | Value |
|-------|-------|
| Overall result | **PASS** |
| Differential `W(k)` for `k=0..251` | **100% identical** (C++ ≡ Python ≡ Coq pins) |
| Cap identity | **Aligned** (`99450437`; max issuance `< RES_MAX_SUPPLY`) |
| Cost-floor monotonicity | **DONE** in Coq (`reward_nonincreasing`, `cost_floor_nondecreasing_along_schedule`) |
| Highest residual risk | Post-schedule winner-must-mint liveness (not RC0-reachable; not inflation) — see prior monetary-policy security audit |

---

## 1. Executive summary

RFC-017 ships a **constant draw target `T`** and a **height-scheduled mint reward**

```
k = height / RES_REWARD_EPOCH
W(k) = 0                                           if k > 250
W(k) = floor(W₀ · (k+1)^(-3/2))                    otherwise
```

with `W₀ = 40.000000 RES`, `RES_REWARD_EPOCH = 210000`, and six-decimal `RES_COIN`.

This final verification confirms:

1. Every consensus mint-amount path reads **only** `GetResMintReward(height)`.
2. The finite-sum cap identity is the same number in C++, Coq, and Python.
3. Adjacent-epoch rewards are nonincreasing on `k ∈ [0,250)`, so with fixed `C` the ORU-per-RES cost floor weakly rises.
4. Public wording must say **maximum** supply 21M, not that 21M will necessarily be reached.

No consensus, wallet, miner, or RFC text was modified for this task.

---

## 2. PART 1 — Consensus implementation audit

### 2.1 Authority

| Symbol | Location |
|--------|----------|
| `ResRewardForEpoch` / `GetResMintReward` / `ResScheduleSum` | `core/src/consensus/rigor_consensus_lock.h` |
| ConnectBlock scheduled amount | `core/src/validation.cpp` (`res_reward_scheduled = GetResMintReward(pindex->nHeight)`) |
| Mint evaluation | `core/src/consensus/rigor_res.cpp` (`GetResMintReward(nBlockHeight)`) |
| Mint shape / spend height | `core/src/consensus/tx_verify.cpp` |
| Template assembly | `core/src/node/auto_res_mint.cpp` |

### 2.2 Reward inputs (fail-closed checklist)

| Influence | Affects `W(h)`? | Evidence |
|-----------|-----------------|----------|
| Block height | **Yes — only** | `GetResMintReward(height)` → `height / 210000` |
| UTXO / chain tip state | No | Pure `constexpr` / header height |
| Miner-chosen parameters | No | Amount must equal scheduled `W`; mismatch rejected |
| Mempool | No | Not in the function |
| Difficulty / ASERT | No | Not in the function |
| Demand / burn rate | No | Affects *whether* a mint occurs, not the amount |

### 2.3 Termination

```
k > RES_REWARD_LAST_EPOCH (= 250)  ⇒  W(k) = 0
```

- C++: `static_assert(ResRewardForEpoch(251) == 0)`
- Connect / mint eval: `res_reward <= 0` rejects unauthorized post-horizon mints
- Coq: `schedule_terminates`, `get_res_mint_reward_zero_after_horizon`, `reward_after_last_epoch_is_zero`

**Verdict:** PASS.

---

## 3. PART 2 — Monetary invariants

### Invariant 1 — Cap

```
210000 × ResScheduleSum() ≤ RES_MAX_SUPPLY
ResScheduleSum() = 99450437
max issuance = 20_884_591.770000 RES  <  21_000_000
```

| Layer | Evidence |
|-------|----------|
| C++ | `static_assert(ResScheduleSum() == 99450437)` and cap `static_assert` |
| Coq | `schedule_sum_value`, `max_issuance_within_cap` (`Print Assumptions` → Closed) |
| Python | `RES_SCHEDULE_SUM` assert in `rigor_consensus_spec.py` |

### Invariant 2 — Cost-floor monotonicity

For constant mint cost `C > 0` and paying epochs:

```
k < 250  ⇒  W(k+1) ≤ W(k)  ⇒  C/W(k+1) ≥ C/W(k)   (when W(k+1) > 0)
```

Coq: `reward_nonincreasing`, `cost_floor_rises_when_reward_falls`,  
`cost_floor_nondecreasing_along_schedule` (Closed under the global context).

### Invariant 3 — Determinism

Same height ⇒ same `W`. Independent of node state, UTXO, miner, and fork tip  
(height is taken from the block being connected / evaluated).

**Verdict:** PASS.

---

## 4. PART 3 — Differential testing

Gate: `formal_verification/tests/test_rfc017_schedule_parity.py`

| Check | Result |
|-------|--------|
| C++ `ResRewardForEpoch(k)` vs Python `res_reward_for_epoch(k)` for `k=0..251` | **0 mismatches** |
| Shared sum `99450437` | **Match** |
| Coq `reward_pin_table` pins `{0,1,10,100,250,251}` | **Match** |
| Reward nonincreasing on `k=0..249` | **Match** |

Sample pins:

| `k` | `W(k)` (atoms) |
|-----|----------------|
| 0 | 40000000 |
| 1 | 14142196 |
| 10 | 1096405 |
| 100 | 39407 |
| 250 | 10058 |
| 251 | 0 |

**Verdict:** PASS — no STOP condition.

---

## 5. PART 4 — Golden vectors

See [RES_MONETARY_GOLDEN_VECTOR_UPDATE_REPORT.md](./RES_MONETARY_GOLDEN_VECTOR_UPDATE_REPORT.md).

Existing NegativeCore rows already cover wrong-epoch amount (`NEG-007`) and  
post-schedule mint (`NEG-012`). Full `k=0..251` parity is enforced by the  
dedicated schedule parity test (avoids regenerating the entire Phase-3 / Phase-4F  
corpus solely for numeric pins).

---

## 6. PART 5 — Formal coverage

See [RES_MONETARY_FORMAL_VERIFICATION_GAP_ANALYSIS.md](./RES_MONETARY_FORMAL_VERIFICATION_GAP_ANALYSIS.md).

`equivalence_claimed` remains **false**. This audit claims **formally verified  
monetary invariants** in the Coq model plus differential alignment — not  
C++↔Coq refinement.

---

## 7. Documentation / public alignment

| Surface | Action in this task |
|---------|---------------------|
| Whitepaper | Clarify max supply vs realized issuance; RFC-017 schedule summary |
| Website | Add Documentation → Monetary Policy page; keep `equivalence_claimed = false` |
| RFC-017 | **Unchanged** (task forbids RFC modification) |

Correct public wording:

> RES has a **maximum** supply of 21,000,000.  
> Actual circulating supply depends on Convergence participation.

Incorrect wording (forbidden):

> 21M supply will necessarily be reached.

---

## 8. Findings

| ID | Severity | Status | Notes |
|----|----------|--------|-------|
| — | — | None new | No C++/Coq/Python schedule mismatch |
| M-live (prior) | High (liveness) | Open / deferred | Winner-must-mint after schedule end; not inflation; not patched here |

---

## 9. Deliverables index

1. This report  
2. `RES_MONETARY_FORMAL_VERIFICATION_GAP_ANALYSIS.md`  
3. Whitepaper updates under `docs/whitepaper/`  
4. Website page `web/rigorchain-site` Monetary Policy  
5. `RES_MONETARY_GOLDEN_VECTOR_UPDATE_REPORT.md`  
6. Formal alignment: `ResSchedule.v` pins + monotonicity; `test_rfc017_schedule_parity.py`
