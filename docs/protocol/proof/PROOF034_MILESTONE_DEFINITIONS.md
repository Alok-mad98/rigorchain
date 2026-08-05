# PROOF-034 Milestone Definitions

**Task:** `TASK-PROOF-034-CAMPAIGN-001`  
**Updated:** 2026-06-24  
**Authority:** `core/src/proof/runtime_vector_harness.cpp`, `sim/proof_validator/catalog.py`  
**equivalence_claimed:** `false`

---

## GV-ORU-0003

| Field | Definition |
|-------|------------|
| **Height** | **630,000** |
| **Verification target** | ORU supply recurrence at post-halving subsidy floor with convergence burn in block |
| **Consensus property verified** | `GetBlockSubsidy()` halving schedule (125 ORU floor after 3 shifts); ORU supply accounting `S_ORU(t+1) = S_ORU(t) + R(t) - C(t)`; convergence burn accumulation |
| **Expected pass condition** | Harness `GVR_PASS`; all required fields MATCH |

**Required witness fields** (`catalog.py`):

| Field | Expected value |
|-------|----------------|
| `oru_supply_parent` | 500,000,000,000,000 |
| `oru_supply_cumulative` | 500,010,000,000,000 |
| `oru_subsidy_block` | 12,500,000,000 (125 ORU in atoms) |
| `oru_converge_burn_block` | 2,500,000,000 |
| `res_mint_block` | 0 |
| `res_mint_count_block` | 0 |

| **Expected failure condition** | Any supply field MISMATCH; `GVR_FAIL`; harness `NOT_EXECUTED` if chain height < 630,000 |
| **Evidence source** | C++: `runtime_vector_harness.cpp` L178–203; Python: `catalog.py` L28–42; Doc: `PROOF-001-GOLDEN-ORU-VECTORS.md` |

---

## GV-RES-0003

| Field | Definition |
|-------|------------|
| **Height** | **2,100,000** |
| **Verification target** | Terminal RES mint bringing cumulative supply to `RES_MAX_SUPPLY` |
| **Consensus property verified** | RES hard cap `RES_MAX_SUPPLY = 21_000_000 × RES_COIN` atoms; per-block mint `RES_REWARD_AMOUNT = 100 × RES_COIN` (10,000 atoms); at most one mint per block (`EnforceResMintUnique`) |
| **Expected pass condition** | Harness `GVR_PASS`; `res_supply_cumulative == 2_100_000_000` |

**Required witness fields** (`catalog.py`):

| Field | Expected value | Meaning |
|-------|----------------|---------|
| `res_supply_parent` | 2,099,990,000 | Supply before terminal mint |
| `res_supply_cumulative` | **2,100,000,000** | **21,000,000.00 RES** (cap reached) |
| `res_mint_block` | 10,000 | Final 100.00 RES mint |
| `res_mint_count_block` | 1 | Exactly one mint transaction |

| **Expected failure condition** | `res_supply_cumulative > RES_MAX_SUPPLY` (consensus reject); field MISMATCH; `NOT_EXECUTED` if height < 2,100,000 |
| **Evidence source** | C++: `runtime_vector_harness.cpp` L256–281; `rigor_consensus_lock.h` `RES_MAX_SUPPLY`; Python: `catalog.py` L43–55; Doc: `PROOF-002-GOLDEN-RES-VECTORS.md` |

---

## GV-DRAW-0003

| Field | Definition |
|-------|------------|
| **Height** | **2,100,001** |
| **Verification target** | Draw kernel still computable; block **rejected** when RES cap already reached (FVR-M boundary) |
| **Consensus property verified** | Sigma-CR-01 draw selection remains deterministic; no additional RES mint permitted at cap; `valid_block = false` for cap-violation block |
| **Expected pass condition** | Harness **`GVR_PARTIAL`** (not FAIL — partial is expected per PROOF-034 §3); `res_mint_block = 0`; draw kernel fields present |

**Required witness fields** (`catalog.py`):

| Field | Expected value |
|-------|----------------|
| `res_supply_parent` | 2,100,000,000 (at cap) |
| `res_mint_block` | 0 |
| `res_mint_count_block` | 0 |
| `draw_winner_txid` | `aa…aa` (fixture placeholder) |
| `convergence_root_hex` | 64 zero hex |
| `verification_status` | `GVR_PARTIAL` |

| **Expected failure condition** | `GVR_FAIL` on required field MISMATCH; unexpected additional RES mint; harness reports FAIL instead of PARTIAL |
| **Evidence source** | C++: `runtime_vector_harness.cpp` L334+ (`reject_expected=true`); Python: `catalog.py` L56–79; Doc: `PROOF-003-GOLDEN-DRAW-VECTORS.md` |

**PCORB structural check (SIM-006):** `checks.py` recomputes σ and winner via `rigor_consensus_spec.py` against `draw_kernel` fixture.

---

## Explicit Answers

### What is verified at height 2,100,000?

**GV-RES-0003** — the **economic / supply verification** that:

1. Cumulative RES supply equals **2,100,000,000 atoms** (21,000,000.00 RES).
2. The block contains exactly **one** RES mint of **10,000 atoms** (100.00 RES).
3. Parent supply was **2,099,990,000** atoms immediately before the terminal mint.
4. Runtime supply witness fields match the C++ harness spec and `RES_MAX_SUPPLY` lock in `rigor_consensus_lock.h`.

This is **runtime field verification** against golden vector expectations — **not** a Coq equivalence proof and **not** a formal theorem discharge.

### What is verified at height 2,100,001?

**GV-DRAW-0003** — the **draw-kernel / cap-boundary verification** that:

1. RES supply remains at cap (no new mint: `res_mint_block = 0`).
2. The convergence draw kernel (σ digest, winner selection) remains **computable** at the boundary.
3. The block at cap+1 is expected to be **invalid** for mint purposes (`valid_block=false` in catalog).
4. Harness returns **`GVR_PARTIAL`** — documenting FVR-M (draw closure impossible at cap), which is the **expected** outcome per PROOF-034 final archive (3 PARTIAL vectors allowed).

Height 2,100,001 is also the **campaign target height** (`PROOF034_TARGET_HEIGHT`) — the **final milestone** for PROOF-034 closure.

---

## Milestone Sequence Diagram

```
h ≤ 501     → 9/12 low vectors (integration only)
h = 630,000 → GV-ORU-0003  (ORU halving floor)
h = 2,100,000 → GV-RES-0003  (RES cap)     ← "2.1M"
h = 2,100,001 → GV-DRAW-0003  (cap reject)  ← campaign end
```

---

*End of milestone definitions.*
