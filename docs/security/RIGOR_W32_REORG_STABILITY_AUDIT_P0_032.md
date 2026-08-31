# RIGOR W=32 Reorg Stability Audit

**Task:** TASK-RIGOR-W32-REORG-STABILITY-AUDIT-P0-032  
**Priority:** P0 — Consensus / reorg / window identity  
**Mode:** AUDIT + PROOF. Consensus behavior was not modified.  
**Date:** 2026-08-28  
**Mainnet status:** not launched.

```
W32_REORG_STABLE          = PASS
GROUP_BOUNDARY            = HEIGHT_ANCHORED
GROUP_CONTENT             = CANONICAL_CHAIN_DEPENDENT
REORG_SUPPORTED           = YES
DEEP_REORG_SUPPORTED      = YES
MATURITY_INDEPENDENT      = YES
STALE_GROUP_STATE         = NO
FORMAL_MODEL              = PROVED
CPP_REFINEMENT            = NOT PROVED
CONSENSUS_CODE_MODIFIED   = NO
```

Honesty lock:

```
model proved
refinement_proved = false
equivalence_claimed = false
```

The Coq theorems below are a mathematical model of the live window formula.
They are **not** a production refinement certificate of `validation.cpp`.

---

## 1. Executive Summary

RIGOR `W = 32` is **`CONVERGENCE_DRAW_WINDOW`**: a **height-anchored sliding window** used to assemble the convergence candidate set `F`.

It is **not** a disjoint partition of the height axis:

```
group_id(h) = floor(h / 32)          NOT the live protocol
group_start(g) = 32 * g              NOT the live protocol
group_end(g)   = 32 * g + 31         NOT the live protocol
```

The live rule, from `core/src/consensus/rigor_consensus_lock.h` and `TailFromParentDrawSet` / `BuildConvergenceWindowTail`:

```
source height s enumerates F over [max(0, s - 32 + 1), s]
draw paid at height h uses          [h - 32, h - 1]
```

For any fixed source height `s`, that interval is a function of `s` and `W` only. A reorg may replace **which blocks occupy those heights**. It does not relocate the interval, does not renumber the source, and does not turn the window into `tip-31 … tip` as a global group identity.

Caches that hold window / draw / witness / supply state are keyed by **block hash**, not by `group_id` or height. `DisconnectBlock` invalidates observational supply/witness state by hash. Coinbase maturity is `nCoinbaseMaturity = 100` and is not derived from `W`.

No consensus safety defect was found. Consensus code was not changed.

---

## 2. W=32 Definition

| Item | Live value |
|------|------------|
| Constant | `Consensus::CONVERGENCE_DRAW_WINDOW = 32` |
| Lock | `core/src/consensus/rigor_consensus_lock.h` |
| Coq mirror | `formal_verification/coq/legacy_account_model/Selection.v`, `formal_verification/rigor_v01/W32ReorgStability.v` |
| Shape | Sliding window of width 32, genesis-clamped |
| Identity of a draw | Source block height `s` (and that block's hash for contents) |
| Identity of a height interval | `window_of_source(s) = [max(0, s-31), s]` |

**Answer to the core question:**

> “32 个区块组成一个 group” is **not** a fixed `floor(h/32)` tile.  
> It is the **draw window of a source height**.  
> That window **slides by one height per connected block**.  
> For a **fixed** source height, the interval does **not** follow chain tip.

Coincidence that can mislead a reader: at `s = 32g + 31` (e.g. 1023), the sliding window **equals** the floor slice `[32g, 32g+31]`. At interior heights it does not. Example: source 1000 covers `[969, 1000]`, not `[992, 1023]`.

---

## 3. Live C++ Dependency Map

```
W = CONVERGENCE_DRAW_WINDOW = 32
  ↓
window_first(s) = max(0, s - W + 1)
window_last(s)  = s
  ↓
block height on a CBlockIndex (canonical or not)
  ↓
canonical chain (CChain / ActivateBestChain) selects which
block HASH occupies each height
  ↓
per-block contribution (candidates + paid winners)
  keyed by block hash
  ↓
ConvergenceDrawSet F / C for source s
  stored on CBlockIndex::m_draw_set (that hash)
  process caches g_window_contribution_cache, g_draw_set_cache
  also keyed by block hash
  ↓
witness / runtime supply (observational)
  RuntimeSupplyWitness, RuntimeSupplyMirror
  keyed by block hash; DisconnectBlock invalidates
  ↓
reorg = DisconnectBlock(old hashes) + ConnectBlock(new hashes)
  ↓
canonical state = UTXO + m_draw_set of ActiveChain() hashes
```

| Layer | Input | Output | Depends on tip? | Depends on hash? | Depends on height? | Cacheable? | Reorg invalidation |
|-------|--------|--------|-----------------|------------------|--------------------|------------|--------------------|
| `W` constant | none | 32 | no | no | no | n/a | n/a |
| `window_first/last` | source height `s` | `[s-31,s]` (clamped) | **no** | **no** | **yes** | pure | interval unchanged |
| Canonical occupancy | `CChain` | `hash(h)` | yes (which chain is tip) | yes | yes | block index | contents replaced |
| Per-block contribution | block body + undo | candidates, paid winners | no | **yes (key)** | recorded as `source_height` | `g_window_contribution_cache[hash]` | miss on new hash; old hash stays correct for that block |
| Draw set F | window contributions | `ConvergenceDrawSet` | no (uses parent index, not global tip) | **yes (key and σ seed)** | source height | `m_draw_set` + `g_draw_set_cache[hash]` | new block writes its own set |
| Network stats | current tip | GUI/RPC counts | **yes (observational)** | cache key = tip hash | window of tip | `g_convergence_network_stats` | `DisconnectTip` invalidates + refresh |
| Runtime supply mirror | connected `pindex` | observational ledgers | no | stored on that index | height field | `pindex->m_runtime_supply_mirror` | `InvalidateRuntimeSupplyMirror` on disconnect |
| Runtime witness | connected hash | observational witness | no | **yes (key)** | height field | `g_witness_cache[hash]` | `InvalidateRuntimeSupplyWitness` on disconnect |
| Coinbase maturity | coin height, spend height | spendable? | no | no | **yes** | UTXO `nHeight` + `fCoinBase` | orphaned coinbase leaves UTXO |

**Dangerous pattern `cache[group_id] = state`:** **not present** on the consensus path. The closest maps are `cache[block_hash]`.

---

## 4. Group Boundary Semantics

**Verdict: PASS (height-anchored sliding window).**

Production formula (same in three places):

```302:302:core/src/consensus/convergence_logic.cpp
    const int window_first = std::max(0, connect_height - Consensus::CONVERGENCE_DRAW_WINDOW + 1);
```

```2642:2642:core/src/validation.cpp
    const int window_first = std::max(0, tip.nHeight - Consensus::CONVERGENCE_DRAW_WINDOW + 1);
```

`BuildConvergenceWindowTail` walks `W-1` ancestors of the source's parent, oldest first. Near genesis the walk stops early; that is a **complete** clamp, not a truncated window.

Wrong semantics — **not found as consensus identity:**

| Wrong rule | Status |
|------------|--------|
| `group = tip-31 … tip` as a global group id | Not used. That interval is only the **current draw** when the source **is** the tip. |
| `group = last 32 canonical blocks` as a persistent identity | Not used. |
| Group boundary moves with tip for a **fixed** source height | Not used. |
| Group ID moves under reorg for a fixed height | Not used. There is no `group_id` integer. Source height is the id. |
| Position re-numbered from “current chain state” | Not used. |

RPC `getconvergencestats` / GUI “tip W-window” **do** report `[tip-W+1, tip]`. That is observational reporting of the **tip's own draw**, keyed by `tip_hash`. It does not rewrite historical `m_draw_set` objects.

Wallet UI (`wallet/convergence_window_ui.h`) uses `finalization = inclusion_height + W`. That is the **ticket participation lifetime** (a candidate stays in `F` for up to `W` draws). It is presentation, not a floor group, and not consensus.

---

## 5. Reorg Semantics

**Verdict: PASS.**

```
height determines position
canonical chain determines content
```

For any two branches that both contain height `h`:

```
window_of_source_A(h) = window_of_source_B(h) = [max(0,h-31), h]
```

Reorg **can** change: `BlockHash(h)`, UTXO at `h`, candidates in `F` for source `h`.  
Reorg **cannot** change: `window_first(h)`, `window_last(h)`.

Task scenario A (source 1023, replace 1010…1023):

- Interval remains `[992, 1023]`, not `[1010, 1041]`.
- `[1010, 1041]` is the window of **source 1041**, a different draw.

Task scenario B (991 / 992 / 1023 / 1024): membership function unchanged before and after reorg:

| Source | Window |
|--------|--------|
| 991 | [960, 991] |
| 992 | [961, 992] |
| 1023 | [992, 1023] |
| 1024 | [993, 1024] |

Task scenario C (cross-slice replacement 970…1055): every source keeps its interval; contents of affected heights are those of the new canonical hashes.

---

## 6. ConnectBlock / DisconnectBlock Analysis

**Verdict: PASS. Disconnect does not “roll back one group”.**

`ConnectBlock` (after mint/C checks pass):

- Writes `pindex->m_draw_set = own_draw_set` for **this block hash**.
- Writes `m_paid_winner_txid`.
- Writes observational ledgers, runtime supply mirror, runtime witness.

`DisconnectBlock`:

- Restores UTXO from undo (canonical money).
- `Observational().InvalidateLedgerCaches()`.
- `InvalidateRuntimeSupplyMirror(pindex)` (by index / hash).
- `InvalidateRuntimeSupplyWitness(pindex)` (by hash, never by height).

`DisconnectTip` additionally:

- `InvalidateConvergenceNetworkStatsCache()` then refresh for the new tip (tip-hash key).

`ActivateBestChain` disconnects the old fork then connects the new fork. Final canonical `m_draw_set` values are those written while connecting the **new** hashes. Orphan `CBlockIndex` objects may still hold **their** `m_draw_set`; that is the draw of that hash, not a height-keyed canonical slot.

Incomplete window ⇒ no draw (fail closed), never a silently shortened `F`.

Operational bound (not a consensus rule): `MIN_BLOCKS_TO_KEEP = 320 = MAX_SUPPORTED_REORG_DEPTH(288) + W(32)`. Deeper reorgs abort for missing blk/rev rather than inventing a short window.

---

## 7. Runtime Supply Analysis

**Verdict: PASS (observational; not a ConnectBlock predicate).**

`RuntimeSupplyState` is documented as strictly observational. ConnectBlock captures it **after** accept (`fJustCheck == false`). Fields include height, **block hash**, parent hash, cumulative ORU/RES ledgers.

Reorg path:

```
old branch mirror/witness
  → InvalidateRuntimeSupplyMirror / InvalidateRuntimeSupplyWitness (hash)
  → ConnectBlock on new hashes writes new mirror/witness
```

`W=32` is **not** an input to subsidy, mint amount `W(k)` (RFC-017, different `W`), ORU burn, or the RES cap. The draw window affects **which tickets are in F**, which can change **whether** a mint occurs on a given block after a reorg of contents. That is canonical-content change, not stale-group pollution.

No `cache[group_id]` of cumulative supply was found.

---

## 8. Witness / Closure Analysis

**Verdict: PASS.**

`RuntimeSupplyWitness` (FENCE-W7):

- Keyed by **block hash**, never height.
- Same-height forks store both entries.
- `Invalidate` drops the disconnected hash.
- `VerifyRuntimeWitnessChainStep` requires `child.height = parent.height + 1` **and** `child.parent_hash = parent.block_hash`.

`GetLatestRuntimeSupplyWitness` is observational (“last writer wins ties”). It must not be read as a consensus accept/reject input. ConnectBlock does not.

Formal Phase-5 witness/closure/equivalence modules remain observational catalogs (`refinement_proved = false`). This audit did not treat them as production certificates.

Same `group_id` / source height **may** correspond to different contents on different branches. Only one branch is `ActiveChain()`. Orphan witness is not retrieved as canonical after invalidate.

---

## 9. Cache / Persistence Analysis

**Verdict: PASS. No P0 REORG STATE SAFETY GAP.**

| Cache | Key | Reorg behavior |
|-------|-----|----------------|
| `g_window_contribution_cache` | block hash | Comment in `validation.cpp`: “a reorg can never hit a stale entry”. LRU 4·W. |
| `g_draw_set_cache` | block hash | Same. |
| `CBlockIndex::m_draw_set` | the index of that hash; serialized in block index | Survives disconnect of that hash (correct). New hash has its own index. |
| `g_witness_cache` | block hash | Explicit invalidate on disconnect. |
| `pindex->m_runtime_supply_mirror` | that index | Cleared on disconnect. |
| `g_convergence_network_stats` | tip hash | Invalidated on `DisconnectTip`, recomputed. |
| Observational W1 ledgers | on `CBlockIndex` | `InvalidateLedgerCaches` on disconnect. |

Orphan hash entries may remain in the process-local LRU maps until eviction. That is **not** `cache[group_id]`. Lookup of the new branch misses and rebuilds.

---

## 10. Coinbase Maturity Analysis

**Verdict: PASS. `W=32` is not a second maturity.**

| Rule | Value | Path |
|------|-------|------|
| `COINBASE_MATURITY` | 100 | `consensus/consensus.h` |
| `nCoinbaseMaturity` | 100 (main/test/regtest); 1 (slowtest) | `kernel/chainparams.cpp` |
| Spend check | `nSpendHeight - coin.nHeight < nCoinbaseMaturity` | `tx_verify.cpp` `AccumulateTxInputPools`, `coin.IsCoinBase()` only |
| RES mint depth lock | `RES_MINT_MATURITY = COINBASE_MATURITY` | same header; mint script lock is a **second** conjunct |

`RequiresMaturity()` is `IsCoinBase()` only. Neither predicate reads `CONVERGENCE_DRAW_WINDOW`.

Reorg tests:

1. Immature / mature coinbase follow coin height vs spend height, not window membership.
2. Reorg across the 100-block boundary changes **depth**, not group id.
3. Orphaned coinbase leaves the UTXO set (`HaveCoin` false after `InvalidateBlock`).
4. Recomputing draw sets does not resurrect an orphaned coinbase.

`group membership != coinbase maturity` holds.

---

## 11. Deep Reorg Analysis

**Verdict: PASS.**

Window functions have no `reorg_depth` input. Depths 1, 31, 32, 33, 64, and the operational max 288 leave `window_of_source(s)` unchanged.

Chain tests on `TestChain100Setup` reorged depths **1, 31, 32, 33, 64** (`> W`) and checked:

- height still maps to the same `window_first`;
- canonical `m_draw_set.source_height == nHeight` on the new hashes;
- new hashes differ from the orphaned ones.

No group-index overflow: there is no incrementing group counter. Source height is `nHeight`.

---

## 12. Test Matrix

New file: `core/src/test/w32_reorg_stability_tests.cpp`  
Registered in `core/src/Makefile.test.include`.  
Run: `./src/test/test_rigor --run_test=w32_reorg_stability_logic_tests,w32_reorg_stability_chain_tests`  
Result: **20/20 PASS** (2026-08-28).

| Case | Coverage |
|------|----------|
| Sliding ≠ `floor(h/32)` | source 1000 → [969,1000]; 1023 → [992,1023]; 1024 → [993,1024] |
| A internal reorg | 1023 stays [992,1023]; `TailFromParentDrawSet` drops height leaving the window |
| B boundaries | 991, 992, 1023, 1024 |
| C cross-slice | 960…1055 interval freeze |
| D deep | depths 1, 31, 32, 33, 64, 288 (formula); chain 1/31/32/33/64 |
| Overlap | height 1000 sits in 32 source windows (partition PROPERTY 4/5 N/A) |
| Genesis clamp | 0, 15, 31 → start 0; 32 → start 1 |
| Incomplete parent | not treated as empty complete window |
| Maturity | 100 ≠ 32; window membership does not mature |
| UI | inclusion-anchored, not floor group |
| Witness cache | hash key; same-height forks; invalidate |
| Orphan witness | not retrieved after invalidate |
| Witness chain | height skip / wrong parent rejected |
| Prune keep | 320 ≥ 288+32 |
| Chain reorg | contents change, `source_height` stable |
| Orphan coinbase | leaves UTXO; supply witness/mirror dropped |
| Connect/disconnect/connect | orphan `m_draw_set` is not ActiveChain tip |

Existing related tests (not modified): `a9c_tail_from_parent_draw_drops_candidates_leaving_the_window`, `a9c_reorg_branch_without_mint_does_not_inherit_already_paid`, `runtime_supply_witness_chain_tests` same-height forks + invalidate, `paid_winner_index_tests` persisted `m_draw_set`.

No new fuzz target: reorg fuzz infrastructure is generic (`process_message` / chain fuzz), not a W=32 state machine.

---

## 13. Coq Model

File: `formal_verification/rigor_v01/W32ReorgStability.v`  
Logical name: `RigorV01.W32ReorgStability`  
Build: `coqc -R formal_verification/rigor_v01 RigorV01 formal_verification/rigor_v01/W32ReorgStability.v`  
Coq 8.15.0: **QED**. **0 Admitted. 0 Axiom.**

Listed in `formal_verification/coq/_CoqProject` and `_CoqProject.protocol-core`.

| Theorem | Claim |
|---------|--------|
| `W32_group_membership_height_stable` | start/end depend only on `s` and `W` |
| `W32_reorg_preserves_group_boundary` | `branch_window a s = branch_window b s` |
| `W32_reorg_changes_contents_only` | interval equal; contents may differ |
| `W32_orphan_state_not_canonical` | `OrphanOwner` is not canonical |
| `W32_maturity_independent` | `+32` does not mature; `+100` does |
| `W32_maturity_not_derived_from_window` | membership ⇏ maturity |
| `W32_deep_reorg_stability` | depth is not an input |
| `W32_deep_reorg_depths_33_and_64` | depths 33 and 64 |
| `W32_not_floor_partition` | start(1000) ≠ 32·⌊1000/32⌋ |
| `W32_coincides_with_floor_slice_only_at_slice_end` | coincidence at `32g+31` |
| `W32_task_scenario_A_boundary` | 1023 → [992,1023] |
| `W32_task_scenario_A_reorg_does_not_slide_interval` | not [1010,1041] |
| `W32_task_scenario_B_boundaries` | 991/992/1023/1024 |
| `W32_windows_overlap` | 1000 ∈ window(1000) and window(1031) |
| `W32_genesis_clamp` | early heights |

Scope: height-space geometry + maturity independence + orphan/canonical distinction. **No Script VM. No Bitcoin opcode model.**

---

## 14. C++ ↔ Coq Refinement Status

```
refinement_proved = false
hash_kernel_shared_binary_with_cpp = false
```

The Coq file restates the C++ formula `max(0, s-W+1)` and proves consequences. There is **no** verified extraction, **no** `validation.cpp` simulation, **no** theorem that `TailFromParentDrawSet` implements `draw_window_start`.

C++ coverage of the same facts is **regression tests**, not refinement.

---

## 15. Findings

### P0 consensus defects

**None.**

### In-scope properties

| # | Property | Result |
|---|----------|--------|
| 1 | Fixed-height membership does not move under reorg | **PASS** (for each source window) |
| 2 | Reorg changes contents, not height-space boundaries | **PASS** |
| 3 | `group_id(h) = floor(h/W)` | **N/A — not the protocol.** Live: `window_of_source(s) = [max(0,s-W+1), s]` |
| 4 | One height belongs to one group | **N/A** for a partition. Live: one **source** window per source height; a height is a **member** of up to `W` overlapping windows |
| 5 | `[32g, 32g+31]` disjoint tiles | **N/A** (not the protocol) |
| 6 | Canonical group state from new chain | **PASS** |
| 7 | Orphan state not canonical | **PASS** |
| 8 | W does not change coinbase maturity | **PASS** |
| 9 | Maturity not derived from group position | **PASS** |
| 10 | Reorg depth > W does not break indexing | **PASS** |

### Non-blocking observational notes (not defects)

1. Process-local draw/contribution LRU may still hold **orphan hashes** until eviction. Safe: keyed by hash.
2. `GetLatestRuntimeSupplyWitness` is observational and tie-breaks by last writer. Not a ConnectBlock input.
3. GUI/RPC tip window is tip-relative **reporting**, not historical regrouping.

---

## 16. Required Fixes

**None.** No consensus patch. No cache-key change. Do **not** introduce `floor(h/32)` groups; that would be a consensus change and would contradict A-9-C (overlapping `W` draws per ticket).

---

## 17. Freeze Verdict

```
W=32 definition:              HEIGHT-ANCHORED sliding window
                              (NOT floor(h/32) tiles)
                              (NOT a single tip-relative global group)
Group boundary:               PASS
Reorg:                        PASS
Deep reorg:                   PASS
Runtime supply:               PASS
Witness:                      PASS
Cache invalidation:           PASS
Coinbase maturity:            PASS
Coq:                          QED (model)
C++ refinement:               NOT PROVED
Consensus code modified:      NO
Final verdict:                W32_REORG_STABLE
```

**Consensus behavior unchanged.**

Invariant (plain language):

- `W=32` is a fixed-width **height-space** window of the **source height**.
- Reorg may change group **contents**.
- Reorg may not change group **boundaries** of a fixed source height.
- Canonical chain chooses contents.
- Orphan branch state is not canonical.
- `W=32` does not define coinbase maturity.
- Reorg deeper than `W` does not break window identity.

---

## Appendix A — Files touched (this task)

| Path | Role |
|------|------|
| `docs/security/RIGOR_W32_REORG_STABILITY_AUDIT_P0_032.md` | This report |
| `core/src/test/w32_reorg_stability_tests.cpp` | Regression tests |
| `core/src/Makefile.test.include` | Register tests |
| `formal_verification/rigor_v01/W32ReorgStability.v` | Coq model |
| `formal_verification/coq/_CoqProject` | Register Coq module |
| `formal_verification/coq/_CoqProject.protocol-core` | Register Coq module |
| `formal_verification/rigor_v01/README.md` | Module table |

Consensus / validation / maturity / supply production code: **not modified**.
