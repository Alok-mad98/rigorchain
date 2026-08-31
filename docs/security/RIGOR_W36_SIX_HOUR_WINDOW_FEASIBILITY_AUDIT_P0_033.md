# RIGOR W=36 Six-Hour Window Feasibility Audit

**Task:** TASK-RIGOR-W36-SIX-HOUR-WINDOW-FEASIBILITY-AUDIT-P0-033  
**Priority:** P0 — Consensus / Formal / Reorg Boundary Audit  
**Mode:** AUDIT ONLY. Consensus code was not modified. `W=32` was not changed. No target, difficulty, maturity, ScriptPubKey, Ed25519, RES/ORU, sighash, serialization, network, wallet, Coq, or existing test was edited to make this audit pass.  
**Date:** 2026-08-28  
**Mainnet status:** not launched. Freeze text already treats `W` and `T` as permanently locked.  
**Companion:** [P0-032 W=32 reorg stability](./RIGOR_W32_REORG_STABILITY_AUDIT_P0_032.md)

```
W36 FEASIBILITY VERDICT:     W36-SAFE-WITH-REPROOF
CURRENT_W:                   32
PROPOSED_W:                  36
BLOCK_TIME (mainnet nominal): 10 minutes
CURRENT_WINDOW:              320 minutes / 5h20m   (mainnet 10-minute spacing only)
PROPOSED_WINDOW:             360 minutes / 6h      (mainnet 10-minute spacing only)

W_IS:                        PROTOCOL PARAMETER (height-anchored sliding window)
W_IS_NOT:                    floor(h/32) group tile
T_COUPLING:                  MANDATORY  — T := floor(unwindowed / W)
POWER_OF_TWO:                ENCODING LOCK, not a required theorem of H < T·f
FROZEN_T_HEX_WOULD_CHANGE:   YES
MATURITY_COUPLED:            NO
SUPPLY_FORMULA_COUPLED:      NO (issuance neutrality requires T retune)
REORG_MODEL_COUPLED:         NO (P0-032 geometry is W-generic)
CONSENSUS_CODE_MODIFIED:     NO
```

Honesty lock:

```
model of live window:     same as P0-032 (height-anchored sliding window)
refinement_proved:        false
equivalence_claimed:      false
this file is not a C++ refinement certificate of validation.cpp
```

---

## 1. Executive Summary

RIGOR `W = 32` is **`Consensus::CONVERGENCE_DRAW_WINDOW`**: the width, in blocks, of the **height-anchored sliding window** that assembles the convergence candidate set `F`.

It is **not** a wall-clock constant. “32 × 10 minutes = 5h20m” and “36 × 10 minutes = 6h” hold **only** on networks whose `nPowTargetSpacing` is 600 seconds (mainnet, regtest). Testnet / devtest use 60-second spacing; slowtest uses 30-second spacing. On those networks W=36 is 36 minutes or 18 minutes, not 6 hours. `W` is a **block count**. The six-hour slogan is a mainnet documentation convenience, not a consensus object.

The live window of a source height `s` is:

```
window_of_source(s) = [max(0, s − W + 1), s]
draw paid at height h uses [h − W, h − 1]
```

That formula does not require `W` to be a power of two. P0-032 already proved (model, not C++ refinement) that reorg changes **contents** at a height, not the **interval** of a source height. That statement is `W`-generic.

What **does** require a coordinated change is the A-9C issuance-neutrality identity:

```
RES_PROBABILITY_TARGET := floor(2^246 / MIN_CONVERGENCE_AMOUNT / W)
p_draw(n)              = n / (C · W)          C = 1024 ORU
expected wins per tx   = W · p_draw(n) = n / C
```

Today that division is **encoded** as a right shift:

```
CONVERGENCE_DRAW_WINDOW      = 32
CONVERGENCE_DRAW_WINDOW_LOG2 = 5
static_assert(W == 1 << LOG2)
T = UNWINDOWED >> 5     ≡  floor(UNWINDOWED / 32)
pinned hex WINDOWED     = 000000000055e63b88c230e77e7ee106959b5d3e1e9ea69ebac0b0b4be383479
```

Answers to the three mathematical questions:

| Q | Answer |
|---|--------|
| **A.** Is `>> 5` merely `/32` while `W=32` is an independent window parameter? | They are **the same parameter, encoded twice**. `>> 5` is not an independent PoW/difficulty shift. It is `T / W` with `W = 2^5`. Protocol coupling is real: changing `W` without retuning `T` multiplies expected RES issuance by `36/32 = 1.125`. |
| **B.** If `>> 5` is the encoding of `W=32`, does W=36 need a new formula? | The **intended** formula is already integer division: `T := floor(unwindowed / W)`. Coq `Selection.v` already writes that. W=36 does **not** need a new inequality. It needs: drop the power-of-two `static_assert`, compute `T` by division (or a new pinned hex), and regenerate golden vectors. |
| **C.** If coupled, can W=36 keep safety, monotonicity, bounds, and convergence? | **Yes, after T retune.** Non-vacuity `T · MIN < 2^256` still holds. Floor remainder is 31 instead of 27; relative error remains ~2.7e-65. `α^36` at `α=0.5` is **stricter** exclusivity than `α^32`. Expected wins stay `n/C`. Per-draw saturation moves `C·W`: 32768 → 36864 ORU. That is an intended A-9C parameter, not a broken theorem. |

**Why not `W36-SAFE`:** frozen consensus **would** change. `docs/MAINNET_PROTOCOL_FREEZE_DECLARATION.md` permanently freezes both `CONVERGENCE_DRAW_WINDOW := 32` and the windowed `T` hex. A one-line `32 → 36` without retuning `T` is an issuance bug, not a safe swap.

**Why not `W36-REQUIRES-CONSENSUS-REDESIGN`:** 36 not being a power of two does **not** break `H_i < T · f_i`, σ layout, C construction, sliding-window geometry, reorg identity, maturity, or supply caps. The power-of-two lock is an encoding convenience chosen so `T/W` is a shift. Coq already divides. The protocol math is `W ∈ ℕ_{>1}` with `T := floor(unwindowed / W)`.

**Why not `W36-NOT-RECOMMENDED` as the technical class:** W=36 is admissible. The six-hour slogan is **not** a sufficient reason to spend a Level-5 freeze amendment. That product judgment is recorded in §18–§21: **do not replace 32 with 36 as the next stage.** Next stage, if any, is to lift the formal definition from `W = 32` to `W : Z, W > 1` and prove the W-independent theorems.

**Landing W=36 is a freeze amendment of the pair `(W, T)`, plus reproof of every lemma that pins `T` hex, `W=32`, `2^5`, or `C·W = 32768`. It is not a silent constant tweak.**

---

## 2. Current W=32 Definition

| Item | Live value | Kind |
|------|------------|------|
| Constant | `Consensus::CONVERGENCE_DRAW_WINDOW = 32` | **1. consensus constant** |
| Log encoding | `CONVERGENCE_DRAW_WINDOW_LOG2 = 5` | **1. consensus constant** (encoding of W) |
| Power-of-two lock | `static_assert(W == 1 << LOG2)` | **1. consensus constant** |
| File | `core/src/consensus/rigor_consensus_lock.h:138–145` | SSOT |
| Runtime T | `Params::RES_PROBABILITY_TARGET` assigned `RES_PROBABILITY_TARGET_CONST` | **1. consensus constant** (hex, not a live shift) |
| Unwindowed T | `RES_PROBABILITY_TARGET_UNWINDOWED` | **2. consensus calculation** (preimage of `/ W`) |
| Window formula | `window_first = max(0, s − W + 1)` | **2. consensus calculation** |
| Coq width | `Selection.v` `CONVERGENCE_DRAW_WINDOW : Z := 32` | formal pin |
| Coq T | `T := UNWINDOWED / WINDOW` (integer division) | already parameterized |
| Coq power-of-two | `window_is_a_power_of_two : W = 2^5` | **C — W-specific** |
| Cache bound | `MAX_ENTRIES = 4 * W` | **4. cache/window size** |
| Prune keep | `MIN_BLOCKS_TO_KEEP = 320 = 288 + 32` | operational, tied to W by `static_assert` |
| Wallet UI | `finalization = inclusion + W` | presentation, not consensus |
| RFC-017 `W(k)` | `ResRewardForEpoch` / `GetResMintReward` | **different symbol** — mint reward, not the draw window |

**Classification of this `W` (task taxonomy):**

| # | Class | Applies to live `CONVERGENCE_DRAW_WINDOW`? |
|---|--------|--------------------------------------------|
| 1 | consensus constant | **YES** — locked to 32 |
| 2 | consensus calculation | **YES** — window interval and `T = floor(unwindowed/W)` |
| 3 | runtime parameter | **NO** — not a CLI/RPC/chainparam override; identical on all networks |
| 4 | cache/window size | **YES, derived** — LRU caps `4·W`; prune keep `288+W` |
| 5 | reorg grouping | **NO** — there is no `group_id`; see P0-032 |
| 6 | supply calculation | **NO** as an input to subsidy/`W(k)`/cap; **indirect** via which tickets sit in `F` |
| 7 | witness grouping | **NO** — witness is keyed by block hash; cache cap uses `4·W` |
| 8 | convergence calculation | **YES** — `F` membership and `T` |
| 9 | test-only constant | **NO** — production lock. Tests also pin 32. |
| 10 | documentation-only | **NO** — live consensus |

**Security derivation (why 32, not 36):** `rigor_consensus_lock.h` states the binding exclusivity constraint is `α^W` with design `α = 0.5` and target `< 1e-9`, which gives `W ≥ 30`. **32 is the next power of two**, so `T/W` is a shift and `log2(W)` is an integer. W=36 also satisfies `W ≥ 30` (`0.5^36 ≈ 1.46e-11`). The power-of-two step is convenience, not the security bound.

**Naming collision (must not confuse):**

| Symbol | Meaning | W=36 impact |
|--------|---------|-------------|
| Draw window `W` | `CONVERGENCE_DRAW_WINDOW` | this audit |
| RFC-017 `W(k)` | scheduled RES mint reward | **none** — `ResRewardForEpoch` uses `<< 32` as **2^16 × 2^16 fixed point**, not draw W |
| Wallet UI `W` | presentation alias of draw window | would follow the lock |

---

## 3. Complete Dependency Inventory

### 3.1 Direct dependencies (`CONVERGENCE_DRAW_WINDOW` / `CONVERGENCE_DRAW_WINDOW_LOG2`)

Production / consensus:

| File | Use | Class |
|------|-----|-------|
| `core/src/consensus/rigor_consensus_lock.h` | `W=32`, `LOG2=5`, power-of-two `static_assert`, A-9C rationale | 1 |
| `core/src/consensus/params.h` | `T := unwindowed >> LOG2`; pinned hex; `p_draw = n/(C·W)` | 1, 2 |
| `core/src/consensus/convergence_logic.cpp` | `window_first = max(0, connect_height − W + 1)` | 2 |
| `core/src/consensus/convergence_logic.h` | documents `[s−W+1, s]` | 2 |
| `core/src/consensus/convergenceres.h` | comment: `T = floor(2^246/MIN)/W` | 10 |
| `core/src/consensus/selection.h` | comment: same T formula | 10 |
| `core/src/consensus/rigor_math.h` | comment: T formula | 10 |
| `core/src/kernel/chainparams.cpp` | all nets assign the **same** `RES_PROBABILITY_TARGET_CONST` | 1 |
| `core/src/validation.cpp` | ancestor walk `W−1`; prune `static_assert(keep ≥ 288+W)`; cache `4·W`; tip window stats | 2, 4 |
| `core/src/validation.h` | `MIN_BLOCKS_TO_KEEP = 320`; documents `D+W` | 4 (operational) |
| `core/src/rpc/blockchain.cpp` | `window_size`, `window_first` reported from `W` | RPC observational |
| `core/src/rpc/mining.cpp` | GBT F spans `W` | RPC / template |
| `core/src/wallet/wallet.cpp` | UI countdown uses `W` | 3/presentation |
| `core/src/wallet/convergence_window_ui.h` | `finalization = inclusion + W` | 10/presentation |
| `core/src/wallet/convergencestate.h` | comment: 32 subsequent draws | 10 |
| `core/src/qt/rigorwallet/...` | GUI labels / progress range `0..W` | presentation |
| `core/src/node/supply_monitor/runtime_supply_witness_chain.cpp` | LRU `4·W` | 4 only |

Tests / spec / formal (must update if W changes; **not modified in this task**):

| File | Pin |
|------|-----|
| `core/src/test/w32_reorg_stability_tests.cpp` | `W==32`, `MIN_BLOCKS_TO_KEEP==320`, floor-partition counterexamples |
| `core/src/test/res_proof_math_tests.cpp` | `T == unwindowed >> LOG2`, `W == 1<<LOG2`, saturation 32768 ORU |
| `core/src/test/convergence_consensus_tests.cpp` | buried-candidate heights `h−W`, saturating amount `65536·W·MIN` |
| `core/src/test/convergence_window_ui_tests.cpp` | `W==32`, `finalization = 132` from inclusion 100 |
| `core/src/test/res_orum_tests.cpp` | saturating amount uses `W` |
| `core/src/test/canonical_layer_tests.cpp` | T formula comment |
| `core/test/functional/test_framework/blocktools.py` | `CONVERGENCE_DRAW_WINDOW = 32` |
| `core/test/functional/res_security/harness.py` | same |
| `formal_verification/spec/rigor_consensus_spec.py` | `W=32`, `LOG2=5`, `T = unwindowed >> 5`, saturation 32768, residue mod 32 == 27 |
| `formal_verification/coq/legacy_account_model/Selection.v` | `W:=32`, `T:=UNWINDOWED/W`, `window_is_a_power_of_two`, saturation 32768 |
| `formal_verification/rigor_v01/W32ReorgStability.v` | `W:=32`, `W32_window_width_locked`, numeric scenarios at 32-boundaries |
| `docs/MAINNET_PROTOCOL_FREEZE_DECLARATION.md` | `W:=32`, `LOG2:=5`, `T := UNWINDOWED >> 5`, pinned hex |

### 3.2 Indirect dependencies (evidence chain: “is this 32 semantically W?”)

| Pattern | Location | Semantically W? | Evidence |
|---------|----------|-----------------|----------|
| `>> 5` / `LOG2 = 5` | lock header, `params.h` formula, `res_proof_math_tests`, Python spec | **YES** | comment: “RES_PROBABILITY_TARGET is T >> this”; `static_assert(W == 1<<LOG2)` |
| `T` hex `...be383479` | `params.h` `RES_PROBABILITY_TARGET_CONST` | **YES** | equals `unwindowed >> 5`; freeze pins it as windowed T |
| `32768` ORU saturation | `params.h`, tests, `Selection.v` `CONVERGENCE_SATURATION_WEIGHT` | **YES** | `C·W = 1024·32` |
| `MIN_BLOCKS_TO_KEEP = 320` | `validation.h` | **YES (operational)** | `288 + 32`; `static_assert ≥ MAX_SUPPORTED_REORG_DEPTH + W` |
| `window_first = h − 32 + 1` | only via the named constant | **YES** | no raw `h-31` consensus path found; formula uses the symbol |
| `floor(h/32)` group | **not live** | **NO** | P0-032; tests assert `DrawWindowFirst(1000) != 32*(1000/32)` |
| `group_id` / `window_id` on chain | **absent** | **NO** | no consensus field |
| RFC-017 `<< 32` in `ResIntegerSqrt` | `rigor_consensus_lock.h:208` | **NO** | `isqrt(x << 32) == floor(sqrt(x)·2^16)` fixed point |
| `ORU_MIN_BLOCK_SUBSIDY = INITIAL >> 3` | same header | **NO** | subsidy halvings, not draw W |
| `nBits 0x207fffff` | genesis / tests | **NO** | compact target |
| `MAX_BLOCKFILE_SIZE = 0x2000000` | 32 MiB files | **NO** | storage |
| witness v1 / 32-byte program | classifier / P0-025 | **NO** | Ed25519 pk size |
| txid / `uint256` 32 bytes | C construction | **NO** | hash width |
| `SCRIPT_VERIFY_SIGPUSHONLY = 1<<5` | `predicate_verify.h` | **NO** | script flag bit 5 |
| `CAP_RES_RELAY = 1<<5` | `rigor_capability.h` | **NO** | P2P capability bit |
| SHA-256 `w32` / `shiftr` 32 | `RigorSha256.v` | **NO** | word size |
| `nMinerConfirmationWindow = 288` | chainparams | **NO** | BIP9 / ASERT-adjacent; equals `MAX_SUPPORTED_REORG_DEPTH`, not W |
| `MAINNET_POW_ASERT_HALF_LIFE = 43200` | 12 hours | **NO** | 72 blocks at 10 min, not 32 or 36 |
| `TARGET_BLOCK_TIME = 600` | lock header | **NO** | docs cadence; not an input to W |
| Bech32 `(pos >> 5)` | `descriptor.cpp` | **NO** | charset grouping |
| wallet `GROUP_SIZE = 10` | `group_outputs_tests.cpp` | **NO** | coin selection |

No consensus-path `h % 32`, `h / 32`, `& 31`, or `& 0x1f` implementing the draw window was found. Window arithmetic uses the named `int` constant.

### 3.3 W32_ENCODING_DEPENDENCY

These encodings **are** W=32 even though they do not spell `CONVERGENCE_DRAW_WINDOW` at the use site of `T`:

| Encoding | Meaning | W=36 action |
|----------|---------|-------------|
| `CONVERGENCE_DRAW_WINDOW_LOG2 = 5` | `log2(32)` | **delete or stop using**; 36 is not `2^k` |
| `static_assert(W == (1 << LOG2))` | power-of-two lock | **must be removed** or replaced by `W > 1` |
| `T := UNWINDOWED >> 5` (`params.h` prose + freeze) | `/32` as shift | replace with `T := UNWINDOWED / W` (floor) |
| `RES_PROBABILITY_TARGET_CONST` hex `...383479` | `unwindowed // 32` | **recompute** → `00000000004c5adf9601f294e237e477a16da83737a977a9898ed5f5fe6ad94f` |
| Python `T = UNWINDOWED >> LOG2` | same | use `// W` |
| Coq `window_is_a_power_of_two` | `W = 2^5` | drop or replace with `W > 1` |
| Coq / tests `32768 * MIN` saturation | `C·32` | `C·36 = 36864` ORU |
| `MIN_BLOCKS_TO_KEEP = 320` | `288+32` | `324` if the `D+W` inequality is kept |
| Freeze `>> 5` and residue `mod 32 == 27` | encoding pins | residue `mod 36 == 31` |

False-positive encodings **not** W:

| Encoding | Actual meaning |
|----------|----------------|
| `>> 3` subsidy | `ORU_SUBSIDY_MAX_SHIFTS` |
| `<< 32` in `ResIntegerSqrt` | RFC-017 fixed-point `2^16` |
| `1 << 5` script/capability flags | unrelated bit 5 |
| `0x20` ASCII space / `0x2000000` 32 MiB | unrelated |

---

## 4. Convergence / Target Mathematical Audit

### 4.1 What T is

Shipping (RFC-017, `params.h`):

```
E = 246
UNWINDOWED = floor(2^E / MIN_CONVERGENCE_AMOUNT)
           = floor(2^246 / 1e8)
           = 0x000000000abcc77118461cefcfdc20d2b36ba7c3d3d4d3d758161697c7068f3b
C = 2^(256−E) = 1024 ORU per mint
T_windowed = floor(UNWINDOWED / W)
```

Computed values (audit calculation, 2026-08-28):

| | W=32 | W=36 |
|--|------|------|
| `T` hex | `000000000055e63b88c230e77e7ee106959b5d3e1e9ea69ebac0b0b4be383479` | `00000000004c5adf9601f294e237e477a16da83737a977a9898ed5f5fe6ad94f` |
| `T_32 == T_36` | — | **false** |
| `T_36 / T_32` | — | exactly `8/9` in the real (floor residue aside) |
| `UNWINDOWED mod W` | 27 | 31 |
| relative floor error | ~2.39e-65 | ~2.74e-65 |
| `p_draw(1 ORU)` | `1/32768` | `1/36864` |
| expected wins / tx | `1/1024` | `1/1024` **unchanged** |
| per-draw saturation | 32768 ORU | 36864 ORU |
| `T·MIN < 2^256` | yes | yes |
| `0.5^W` | 2.33e-10 | 1.46e-11 (stricter) |

Runtime does **not** shift `T` on every block. ConnectBlock uses the **pinned hex**. The shift exists in:

1. the compile-time story (`LOG2`, `static_assert`);
2. tests that check `T == unwindowed >> LOG2`;
3. freeze / spec prose.

PoW difficulty / ASERT / `nBits` / chain work **do not** use `W` or `>> 5` as the draw window. Historical RFC-014 `2^240` is a different (superseded) exponent, not `W`.

### 4.2 If W changes and T does not

A-9C’s point is: each ticket sits in `W` draws, so `T` must be divided by `W` or issuance scales with `W`. W=36 with the **current** T hex would raise expected wins per ticket from `n/C` to `(36/32)·n/C = 1.125 n/C`. That is a monetary consensus change and a freeze violation. **Forbidden as a “just change W” patch.**

### 4.3 If W and T change together as `T := floor(unwindowed/W)`

This is the **designed** A-9C pair. Monotonicity of `H < T·f` is unchanged (smaller T is stricter per draw). Aggregate neutrality is preserved up to the same floor error class. No new overflow: 256-bit T still fits; `T·f` still uses the existing 512-bit path.

**36 not being `2^k` only forbids the shift encoding.** Integer division is already the Coq definition and already the freeze’s equivalent line `floor(UNWINDOWED / 32)`.

### 4.4 Difficulty / work / MIN / threshold

| Object | Uses draw W? |
|--------|----------------|
| ASERT target | no |
| `nBits` / header work | no |
| `MIN_CONVERGENCE_AMOUNT` | no (1 ORU); T divides it |
| `H_i < T·f_i` | T depends on W |
| chain weight | no |

---

## 5. Group Boundary Audit

**Live rule (not a floor tile):**

```
group_id(h) = floor(h / W)     NOT the protocol
```

P0-032: coincidence with a floor slice happens only at `s = W·g + (W−1)`. For W=32 that is `32g+31` (e.g. 1023 → [992,1023]). For W=36 that would be `36g+35` (e.g. 35 → [0,35], 71 → [36,71], 107 → [72,107]). Interior heights still overlap neighbouring sources.

| Question | Finding |
|----------|---------|
| Is group boundary consensus-visible? | The **interval** of source `s` is consensus (it determines F). There is no integer `group_id` in the header, coinbase, or tx. |
| Is group ID written on-chain? | **No.** `ConvergenceDrawCandidate::source_height` is bookkeeping; comment says it is **not a σ input and not committed**. It **is** serialized on `CBlockIndex::m_draw_set` (block index), not in the block hash. |
| Does group boundary enter hash? | **Not as W or group_id.** C = Hash256(sorted txids of F). Changing W changes **which txids** are in F, hence C. W is not concatenated into C or σ. |
| Witness? | Observational witness is per **block hash**, not group. |
| Supply? | No `floor(h/W)` supply bucket. |
| Convergence? | Yes — F membership. |
| Cache/index? | Hash-keyed; cap `4·W`. |
| Reorg recomputes? | Yes, from canonical hashes (P0-032). |
| Can reorg change historical group identity? | Interval of a **fixed** `s` cannot. Occupancy `hash(s)` can. |
| Cross-boundary reorg bug from floor tiles? | **N/A** — no floor tiles. Same as P0-032. |

W=36 does not introduce a `group_id`. It only widens the sliding interval.

---

## 6. Reorg Audit (P0-032 relationship)

P0-032 verdict `W32_REORG_STABLE` rests on:

1. `window_of_source(s)` is a function of `s` and `W` only;
2. caches keyed by **block hash**;
3. Disconnect invalidates observational state by hash;
4. incomplete window fail-closed;
5. maturity independent of W.

None of (1)–(5) requires `W=32` specifically. Replacing 32 with 36 **does not make reorg pollution easier** in the P0-032 sense (stale `cache[group_id]`). There is still no `group_id`.

**Content exposure:** a ticket included at height `h` participates in sources `[h, h+W−1]`. Larger W ⇒ a given replaced height sits in **more** overlapping draws. That is more F-content churn under a same-depth reorg, not a new stale-group bug. W=36 is slightly **more** content-coupled to reorgs and **less** exclusive (`α^W` smaller). Stability of **interval identity** is identical.

### 6.1 W=36 logic boundaries (formula only; production W not changed)

`window(s) = [max(0, s−35), s]`

| s | window | notes |
|---|--------|--------|
| 0 | [0, 0] | genesis clamp |
| 1 | [0, 1] | clamp |
| 34 (W−2) | [0, 34] | still clamped |
| 35 (W−1) | [0, 35] | last fully clamped full-width window |
| 36 (W) | [1, 36] | first unclamped start>0 |
| 37 (W+1) | [2, 37] | |
| 38 (W+2) | [3, 38] | |
| 70 (2W−2) | [35, 70] | |
| 71 (2W−1) | [36, 71] | floor-slice coincidence `[36,71]` |
| 72 (2W) | [37, 72] | |
| 73 (2W+1) | [38, 73] | |
| 74 (2W+2) | [39, 74] | |
| 100 | [65, 100] | maturity height; **not** a window boundary |
| 107 (3W−1) | [72, 107] | floor-slice coincidence |
| 108 (3W) | [73, 108] | |
| 109 (3W+1) | [74, 109] | |

Reorg pairs `35→36`, `36→35`, `71→72`, `72→71`, `107→108`, `108→107`: each **source height keeps its interval**. Only which block hash occupies those heights changes. Same theorem as P0-032 `W32_reorg_preserves_group_boundary`, with `W` uninterpreted.

Operational prune: `MIN_BLOCKS_TO_KEEP` must stay `≥ 288 + W`. Today 320 = 288+32. W=36 requires **324** or the `static_assert` in `validation.cpp` fails. That is not consensus F, but a node that pruned too aggressively would fail-closed on ConnectBlock (incomplete tail), not silently shorten F.

---

## 7. Maturity Audit

| Rule | Value | Uses draw W? |
|------|-------|----------------|
| `COINBASE_MATURITY` | 100 | **no** (`consensus.h`) |
| `nCoinbaseMaturity` | 100 public nets; 1 slowtest | **no** |
| `RES_MINT_MATURITY` | `= COINBASE_MATURITY` | **no** |
| Spend check | `nSpendHeight − coin.nHeight < nCoinbaseMaturity` | **no** |
| RES mint script lock height | `block_height + nCoinbaseMaturity` | **no** |
| ORU coinbase | same 100 | **no** |
| Witness / supply “maturity” | not defined as N×W | **no** |

`100/32 ≈ 3.125` and `100/36 ≈ 2.778` are **commentary ratios only**. No code implements `maturity = N × W`. P0-032 tests assert `COINBASE_MATURITY != W` and that sitting in the window does not mature a coinbase.

W=36 **does not move** any maturity boundary.

---

## 8. Supply / RES / ORU Audit

| Quantity | Uses draw W? |
|----------|----------------|
| ORU subsidy `GetBlockSubsidy` | no |
| RFC-017 `W(k)` / `GetResMintReward` | **no** (different W) |
| `RES_MAX_SUPPLY` / schedule sum | no |
| ORU RES-transfer fee burn 1/2 | no |
| `valid_mint` / one mint per block | no |
| ORU burn (convergence nValue) | no |
| **Whether** a given block mints | **yes, indirectly** — F and T determine eligibility/winner |

Total supply **formulas** (caps, subsidy, `W(k)`) do not contain draw `W`. Changing W with T retuned leaves expected RES atoms per ORU burned at `n/C` (aggregate). Changing W **without** T retune **does** change expected issuance. New “supply state boundaries” of type `floor(h/W)` were **not** found.

Runtime supply witness / mirror: observational, hash-keyed (P0-032 §7). Cache size `4·W` is capacity, not a supply epoch.

---

## 9. Witness / Convergence Audit

| Surface | W=36 effect | Class |
|---------|-------------|-------|
| `F` width | 36 blocks instead of 32 | **A — expected parameter change** (and a consensus change) |
| Ticket lifetime in F | up to 36 draws | A |
| `C` = Hash256(sorted F txids) | different set ⇒ different C | A (not W in the preimage) |
| σ preimage | still `txid ‖ C ‖ B_{h−1} ‖ LE32(h)` | **no layout change** |
| Winner inequality | `H < T·f` with new T | A if T retuned; **B forbidden** if T frozen |
| Canonical candidate / equivalence | observational modules do not take W as a consensus predicate | cache cap only |
| Witness **length** (σ 100 bytes, C 32 bytes) | unchanged | — |
| Witness **validity** | unchanged format; F membership may differ | A |
| `runtime_supply_witness_chain` | LRU `4·W`; still hash-keyed | capacity A |
| `runtime_supply_*_equivalence` / draw / phase5 | observational catalogs; not ConnectBlock | no W-as-group |

**B — disallowed consensus semantic change** would be: keep T hex, change W; or encode `group_id` into C/σ; or make maturity `N·W`. None of those is required for a correct W=36 retune.

---

## 10. Hash / Commitment Audit

| Commitment | W in preimage? | W=36 |
|------------|----------------|------|
| tx hash | no | no |
| block hash | no | no (header unchanged) |
| sighash / Ed25519 domain | no | no |
| ConvergenceRoot C | **not W**; F membership depends on W | C values change because F changes |
| σ | not W | C slot may change |
| coinbase ConvergenceRoot commitment | commits C, not W | follows C |
| `source_height` on draw-set disk | serialized on **block index**, not block hash | bookkeeping |
| RPC `window_size` | reports W | display |
| `state_hash` / observational ledgers | not a W field | — |

No hidden `W` / `group_id` / `window_id` in hash preimages was found.

---

## 11. Serialization Audit

| Object | Contains W? |
|--------|-------------|
| Block / tx serialization | no |
| `ConvergenceDrawCandidate` | `source_height` varint; not W |
| `CBlockIndex::m_draw_set` | candidates + source height; not W |
| P2P messages | no W field |
| GBT `convergence_txids` | F contents depend on W |

W=36 does not change serialization **layout**. It can change **which** txids appear in F and therefore in C and GBT.

---

## 12. Runtime / Cache Audit

| Cache | Key | Size vs W | Semantic? |
|-------|-----|-----------|-----------|
| `g_window_contribution_cache` | block hash | `4·W` | capacity; reorg-safe (P0-032) |
| `g_draw_set_cache` | block hash | `4·W` | same |
| `g_witness_cache` | block hash | `4·W` | same |
| `m_draw_set` on index | that hash | unbounded per index | per-block, not a 32-entry ring of groups |
| GUI progress `setRange(0, W)` | n/a | W | presentation |

W=32 is **not** a 32-entry consensus ring buffer. Capacity scales linearly. W=36 ⇒ ~12.5% larger LRU. Not a semantic fork by itself.

---

## 13. Integer / Overflow / Types

| Operation | Live use | W=36 |
|-----------|----------|------|
| `height / W` | **not used** for groups | still unused |
| `height % W` | **not used** | still unused |
| `& 31` as W mask | **not used** for the window | must not be introduced |
| `h − W + 1` | `int` | 36 is fine; genesis clamp via `max(0,·)` |
| `W * N` | `4·W` size_t; `288+W` unsigned | 144, 324 — no overflow |
| `1 << LOG2` | compile-time | **cannot** represent 36 |
| division `UNWINDOWED / 36` | 256-bit floor | well-defined; residue 31 |
| signed W | `constexpr int` 36 | fine |

No division-by-zero: W is a positive lock. Off-by-one: the live formula is already `s−W+1` inclusive; tests pin edges. W=36 needs the same formula, not `s−36`.

---

## 14. Formal Verification Impact

### 14.1 Classification key

- **A — W-independent:** proof does not mention draw W / 32 / 2^5 / 32768-as-C·W.  
- **B — parameterize:** replace `W:=32` with `W : Z` and `W > 1`; proof structure survives.  
- **C — W-specific:** pins 32, 2^5, T hex, or 32768; must recompute.  
- **D — structure breaks:** none found for W=36 with T retune.

### 14.2 `formal_verification/rigor_v01/`

| File | Class | Notes |
|------|-------|-------|
| `W32ReorgStability.v` | **B + C** | Geometry theorems are B (`window = [s−W+1,s]`). `W32_window_width_locked`, genesis clamp numbers, `32*g+31` coincidence, scenarios 991/1023, `h+32` in maturity lemmas are **C**. Rename/generalize recommended. |
| `AssetClassification.v` | **A** | `32` = pubkey / PUSHBYTES_32 |
| `Ed25519Ownership.v` | **A** | pk 32 |
| `SignatureDomainSeparation.v` | **A** | hash 32 |
| `ScriptAssetBoundary.v` | **A** | |
| `NoInterpreterConsensus.v` | **A** | |
| `ResMintCorrectness.v` | **A** (draw W) | mint amount is RFC-017 `W(k)`, not draw W |
| `AssetStateTransition.v` | **A** | |
| `Network*.v` / `RigorUpgrade*.v` / `RigorOpcode*.v` / `RigorByteNamespace.v` / `RigorPredicateRegistry.v` / `RigorWalletOwnership.v` | **A** | |

### 14.3 `formal_verification/coq/` (legacy + kernel)

| File / theorem | Class |
|----------------|-------|
| `legacy_account_model/Selection.v` `CONVERGENCE_DRAW_WINDOW := 32` | C |
| `RES_PROBABILITY_TARGET := UNWINDOWED / WINDOW` | **B** (already) |
| `window_is_a_power_of_two` | **C** — **false** at W=36 |
| `window_widens_the_candidate_set` (`W > 1`) | **B** |
| `target_is_unwindowed_divided_by_window` | **B** — re-`vm_compute` at new T |
| `res_probability_target_not_vacuous` | **C** compute |
| `saturation_weight_is_32768_oru` / `CONVERGENCE_SATURATION_WEIGHT := 32768 * MIN` | **C** → 36864 |
| `mint_cost_is_1024_oru` | **A** (uses UNWINDOWED, not W) |
| `WindowExclusion.v` | **B** — exclusion is txid-based; comment mentions W=32 |
| `Kernel/KernelEligibility.v` `KernelClosure.v` `Correctness.v` | **B/C** — use `RES_PROBABILITY_TARGET`; recompute if T hex changes |
| `Kernel/GoldenHarness.v` chk_107–110 | **C** — literals mix T with `3276800000000` (`32768 * 1e8`) |
| `DrawCandidateRoot.v` / `Execution.v` / `Proofs/FairnessIdealized.v` | **C** if they close over shipping T |
| `Phase4FConsensus/ConsensusGoldenVectorsData.v` | **C** — eligibility/winner vectors at shipping T |
| `ReplayReorgCore/ConnectBlockMint.v` `Z.shiftr` | **A** — ORU subsidy shifts |
| `RigorSha256.v` 32-bit words | **A** |
| `spec/rigor_consensus_spec.py` | **C** — W, LOG2, T, residue 27, saturation 32768 |
| Golden vector generators | **C** — regenerate after T hex change |

No **D** theorem (“W must be 2^k or the inequality is ill-typed”) exists. The only power-of-two **lemma** is a pin of the current encoding.

---

## 15. Parameterization Feasibility

| Design | Assessment |
|--------|------------|
| `W = 32` frozen | current shipping; freeze Level-5 |
| `W = 36` frozen | admissible pair with `T := floor(unwindowed/36)`; **not** motivated by math |
| `W : ℕ_{>1}` (ideal) | **matches the real theorems**: exclusivity `W ≥ 30` (policy), neutrality `T = floor(unwindowed/W)`, geometry `[s−W+1,s]`, fail-closed incomplete tail, hash-keyed caches |

**Preferred formal work (not this task):** change Coq from `W := 32` to a parameter with hypotheses `W > 1` (and optionally `W >= 30` as a policy lemma, not a kernel axiom). Prove:

- window start/end depend only on `s` and `W`;
- reorg does not move the interval;
- `T * W ≤ UNWINDOWED < (T+1)*W`;
- windowing preserves non-vacuity;
- `maturity ≠ W`;
- overlap: `h` belongs to `W` consecutive source windows.

Keep **shipping** W=32 until a freeze amendment. Do not treat 36 as the unique “correct” parameter.

---

## 16. W32 vs W36 Boundary Matrix

| Height | W=32 window | W=36 window | Consensus-visible difference if W changed |
|--------|-------------|-------------|-------------------------------------------|
| 0 | [0,0] | [0,0] | none at genesis singleton |
| 1 | [0,1] | [0,1] | none |
| 34 | [3,34] | [0,34] | **yes** — 36 still clamped |
| 35 | [4,35] | [0,35] | **yes** |
| 36 | [5,36] | [1,36] | **yes** |
| 37 | [6,37] | [2,37] | **yes** |
| 38 | [7,38] | [3,38] | **yes** |
| 70 | [39,70] | [35,70] | **yes** |
| 71 | [40,71] | [36,71] | **yes**; W=36 floor coincidence |
| 72 | [41,72] | [37,72] | **yes** |
| 73 | [42,73] | [38,73] | **yes** |
| 74 | [43,74] | [39,74] | **yes** |
| 100 | [69,100] | [65,100] | **yes**; maturity **unchanged** |
| 107 | [76,107] | [72,107] | **yes**; W=36 slice end |
| 108 | [77,108] | [73,108] | **yes** |
| 109 | [78,109] | [74,109] | **yes** |

Under **current** W=32 these W=36 columns are counterfactual. They are the intervals ConnectBlock **would** use if the lock were 36.

---

## 17. W36 Benefits

| Claim | Verdict |
|-------|---------|
| 6-hour semantic window | **Mainnet-only slogan.** True iff `nPowTargetSpacing = 600`. False on 60s/30s nets. Consensus `W` is blocks, not hours. |
| Easier ops / monitoring / docs | Weak. “32 blocks ≈ 5h20m” is already documentable. ASERT τ is already 12 hours (72 blocks), not 6. |
| Alignment with other windows | **Poor.** 288-block prune/BIP9 window, 100-block maturity, 72-block ASERT half-life, 210000-block epoch are not multiples of 36. 36 does not align them. |
| Exclusivity `α^W` | Slightly **better** than 32 (already `< 1e-9` at 32). Not a reason to move. |
| Parameter honesty | If the project wants “W is a parameter”, **prove it in Coq**; do not pick 36 for the clock. |

---

## 18. W36 Risks

| Risk | Severity |
|------|----------|
| Freeze: T hex + W permanently frozen | **Level-5.** Cannot land as a silent patch. |
| Changing W without T | **issuance × 1.125** — consensus bug |
| Golden vectors / Coq `vm_compute` pins | all T-closed lemmas and eligibility vectors |
| `window_is_a_power_of_two` becomes false | must not keep that lemma |
| `MIN_BLOCKS_TO_KEEP` 320 → 324 | prune / functional tests (`feature_pruning.py`) |
| Wallet UI tests pin `+32` finalization | presentation tests |
| Slightly more overlapping F under reorg | content churn, not stale group_id |
| Cache `4·W` memory | small |
| Confusing RFC-017 `W(k)` with draw W | documentation hazard (already present) |
| Six-hour docs drifting on testnet | high if W is sold as time |

---

## 19. Impact Matrix

| Area | W=32 dependency | W=36 impact | Consensus impact | Reproof required | Status |
|------|-----------------|-------------|------------------|------------------|--------|
| Window | `CONVERGENCE_DRAW_WINDOW=32`; `[s−31,s]` | `[s−35,s]` | **YES** (F) | geometry B; numeric C | coupled parameter |
| Group boundary | none (`floor(h/32)` not live) | still none | no new `group_id` | P0-032 generalize | OK |
| Reorg | hash-keyed; interval = f(s,W) | same model; more overlap | contents, not identity | generalize W32*.v | OK |
| Convergence | F width + T/W | both change | **YES** | Selection + vectors | coupled |
| Target/Difficulty | PoW **no**; draw T **yes** (`>>5`) | new T hex; no ASERT change | **YES** for draw T | T lemmas | encoding + pin |
| Supply | not in subsidy/`W(k)`/cap | expected RES iff T not retuned | **YES if T frozen** | none if pair retuned | pair only |
| RES | mint amount `W(k)` independent; eligibility uses T,F | eligibility odds per draw | **YES** (per-draw) | saturation lemma | retune T |
| ORU | subsidy/burn independent | no | no | no | OK |
| Coinbase maturity | 100, not N×W | none | no | `maturity ≠ W` still | OK |
| Witness | hash-keyed; LRU 4·W | capacity; F→C | C contents | no layout | OK |
| Hash/Commitment | W not in preimage | C/σ change via F | **YES** (values) | golden σ/C | no layout fork |
| Serialization | no W field | no layout change | no | no | OK |
| Runtime cache | `4·W`, prune `288+W` | 144 entries; keep 324 | operational | tests | not F |
| Formal Coq | W:=32, 2^5, T hex, 32768 | parameterize + recompute | n/a | **YES** | see §14 |
| Tests | many pins of 32 / T / 320 / 32768 | update after lock change | n/a | n/a | not this task |

---

## 20. Expected Change Set

**Not applied.** Prediction only, if a future freeze amendment adopted `W=36` **and** `T := floor(unwindowed/36)`.

```
EXPECTED_CHANGESET

Production code:
- core/src/consensus/rigor_consensus_lock.h
    W=36; drop or replace LOG2 / power-of-two static_assert; bump RIGOR_CONSENSUS_LOCK_VERSION
- core/src/consensus/params.h
    new RES_PROBABILITY_TARGET_CONST hex; rewrite >> LOG2 as / W; saturation comments 36864
- core/src/kernel/chainparams.cpp
    comments only (assignment already uses CONST)
- core/src/validation.h
    MIN_BLOCKS_TO_KEEP = 324 (or 288+W derived)
- core/src/validation.cpp
    static_assert still holds if keep is updated; no formula change if it uses the symbol
- comments in convergence_logic.h, convergenceres.h, selection.h, rigor_math.h, rpc/*

Tests:
- core/src/test/w32_reorg_stability_tests.cpp (rename/generalize; numeric 32 pins)
- core/src/test/res_proof_math_tests.cpp (shift identity → division; 32768 → 36864)
- core/src/test/convergence_window_ui_tests.cpp (finalization +32)
- core/src/test/convergence_consensus_tests.cpp / res_orum_tests.cpp / canonical_layer_tests.cpp
- core/test/functional/test_framework/blocktools.py, res_security/harness.py
- core/test/functional/feature_pruning.py / feature_pruned_reorg_depth.py (320 keep)
- contrib/testing/local_mainnet_simulation/run_simulation.py

Formal verification:
- formal_verification/spec/rigor_consensus_spec.py
- formal_verification/coq/legacy_account_model/Selection.v
    (drop window_is_a_power_of_two; saturation; vm_compute T)
- formal_verification/rigor_v01/W32ReorgStability.v (parameterize; new numeric scenarios)
- GoldenHarness.v / ConsensusGoldenVectorsData.v / generate_phase3_vectors.py
- regenerate golden vectors (eligibility, draw, T)

Documentation:
- docs/MAINNET_PROTOCOL_FREEZE_DECLARATION.md  (Level-5 amendment)
- core/doc/convergence-consensus-v1.md, mining-pool-gbt.md, rigor_consensus_pipeline.md
- docs/archive/rfc/RFC-014-*.md, RFC-017 comments, wallet UI specs

Consensus freeze impact:
- MAJOR
  (W and T hex are named permanent locks; A-9C pair must move together)
```

A patch that only edits `CONVERGENCE_DRAW_WINDOW = 36` **will not compile** (`W == 1<<5`) and **must not ship** even if the assert were deleted, because T would be wrong.

---

## 21. Final Verdict

```
W36 FEASIBILITY VERDICT: W36-SAFE-WITH-REPROOF
```

**Meaning:** The protocol **design** already treats `W` as the width of a sliding height window and `T` as `floor(unwindowed / W)`. W=36 is a legal value of that pair. It does not require redesigning σ, C, ASERT, maturity, RES `W(k)`, or reorg identity. It **does** require a freeze amendment, a new T hex, dropping the power-of-two encoding, operational `MIN_BLOCKS_TO_KEEP`, and reproof/recompute of every W- or T-pinned lemma and golden vector.

**It is not `W36-SAFE`:** frozen semantics of F and T **would** change.

**It is not `W36-REQUIRES-CONSENSUS-REDESIGN`:** 36 not being `2^k` is an encoding issue, not a broken `H < T·f`.

**Product recommendation (separate from the four-way class):** **Do not make the next stage “replace 32 with 36.”** The six-hour clock is not a consensus object. If work continues, make `W` a Coq parameter and keep shipping 32 until a Level-5 process has a reason stronger than wall-clock roundness.

### Impact lines

```
CONSENSUS_IMPACT:     MAJOR if landed (F width + T hex). NONE in this audit (code unchanged).
REORG_IMPACT:         NONE on interval identity (P0-032 generalizes). Slightly more F overlap.
CONVERGENCE_IMPACT:   MAJOR if landed (F, T, C values). Neutrality preserved only with T retune.
SUPPLY_IMPACT:        NONE on subsidy/W(k)/cap formulas. Issuance rate changes unless T retuned.
MATURITY_IMPACT:      NONE (100 blocks remains the only maturity).
WITNESS_IMPACT:       NONE on layout. Observational LRU scales with W. C follows F.
HASH_IMPACT:          W not in preimage. C/σ values follow F.
FORMAL_IMPACT:        REPROOF — Selection.v pins, power-of-two lemma, saturation, golden T, W32ReorgStability numerics.
```

### Should the next stage replace W=32 with W=36?

**No.** Next stage should **not** swap 32→36. Prefer: parameterize `W` in the formal model (`W ∈ ℕ_{>1}`), keep the shipping lock at 32, and treat any future width change as an explicit `(W, T)` freeze amendment with regenerated vectors.

---

## 22. Verification (this task)

```
git status   — only this audit document (and no production consensus edits) is in scope
static scan  — CONVERGENCE_DRAW_WINDOW, LOG2, >>5, 32768, MIN_BLOCKS_TO_KEEP, Coq W pins
math         — T_36 hex and neutrality identity computed; not written into params.h
tests        — existing P0-032 / res_proof_math tests were not modified; test_rigor binary not present in tree to re-run here
coqc         — not required to modify .v files; dependency classification from source
```

Production constants remain:

```
CONVERGENCE_DRAW_WINDOW      = 32
CONVERGENCE_DRAW_WINDOW_LOG2 = 5
RES_PROBABILITY_TARGET       = 000000000055e63b88c230e77e7ee106959b5d3e1e9ea69ebac0b0b4be383479
COINBASE_MATURITY            = 100
MIN_BLOCKS_TO_KEEP           = 320
```
