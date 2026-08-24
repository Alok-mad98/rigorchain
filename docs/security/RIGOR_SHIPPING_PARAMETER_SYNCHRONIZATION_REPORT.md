# RIGOR Shipping Parameter Synchronization Report

**Task:** `TASK-RIGOR-SHIPPING-PARAMETER-SYNCHRONIZATION-P0-001`  
**Date:** 2026-08-20  
**Scope:** Category D documentation and tooling-copy drift only.  
**Consensus / wallet behavior / miner behavior / RFC / monetary policy:** **NONE**  
**Honesty:** `equivalence_claimed = false`

Closes live-parameter residuals inventoried by
[RIGOR_DOCUMENTATION_FULL_PARAMETER_CONSISTENCY_AUDIT.md](./RIGOR_DOCUMENTATION_FULL_PARAMETER_CONSISTENCY_AUDIT.md).

---

## Shipping economics (canonical)

| Item | Value |
|------|-------|
| Draw exponent | `E = 246` |
| Mint cost | `C = 1024` ORU per RES mint |
| Windowed `T` | `floor(2^246 / MIN_CONVERGENCE_AMOUNT) >> 5` |
| Windowed hex | `000000000055e63b88c230e77e7ee106959b5d3e1e9ea69ebac0b0b4be383479` |
| Unwindowed hex | `000000000abcc77118461cefcfdc20d2b36ba7c3d3d4d3d758161697c7068f3b` |
| Expected wins | `n / 1024` |
| `RES_REWARD_GENESIS` / `W(0)` | `40.000000 RES` |
| Epoch length | `210000` blocks |
| Cutoff | `250` epochs |
| Cap | `21,000,000 RES` (`21_000_000_000_000` atoms) |
| `RES_COIN` | `1_000_000` |

Code SSOT: `core/src/consensus/params.h`, `rigor_consensus_lock.h`, Coq `Selection.v` / `ResSchedule.v`, Python `RES_PROBABILITY_TARGET_EXPONENT = 246`.

**Historical RFC-014 parameter:** `E = 240` ⇒ `2^240 / 2^256 = 1/65536` ⇒ `C = 65536` ORU.  
**Historical RFC-015 parameter:** constant `100 RES` per mint.

---

## Part 1 — Freeze declaration

`docs/MAINNET_PROTOCOL_FREEZE_DECLARATION.md` §3.1 / §3.3 now state shipping `E = 246`, `C = 1024`, and `W(k)`. Pinned hex matches `params.h`. RFC-014 `2^240` / `n/65536` / RFC-015 `100 RES` remain labeled historical only.

---

## Part 2 — SPEC mint / freeze tables

Updated live mint rows and recurrences:

| Document | Live mint |
|----------|-----------|
| SPEC-000 | RFC-017 `W(k)` / `GetResMintReward(h)`; genesis 40 RES; epoch 210000; cutoff 250; cap 21M |
| SPEC-001 | Same |
| SPEC-002 | Schedule table; `RES_COIN = 1e6`; cap atoms `21e12` |
| SPEC-003 | CR-044 / CR-052 use `GetResMintReward(h)` |
| SPEC-005 | CI-022 / CI-040 |
| SPEC-006 | RES recurrence mapping |

Constant `100 RES` remains only as **Historical RFC-015**.

---

## Part 3 — Architecture maps

Live diagrams no longer present unlabeled `T = 2^240`:

- `docs/architecture/RIGOR_DETERMINISM_MAP.md`
- `docs/architecture/RIGOR_EXECUTION_MODEL.md`
- `docs/architecture/RIGOR_GLOBAL_ARCHITECTURE_REPORT.md`

Remaining `2^240` strings are labeled **Historical RFC-014**. Mint copy uses `GetResMintReward(h)`.

---

## Part 4 — Explorer / wallet / Qt verification docs

No explorer **code** compared `res_probability_target` to `2^240` hex (`00010000…`). Drift was documentation-only.

| File | Change |
|------|--------|
| `docs/explorer/RES_INDEXING_RULES.md` | Verify shipping windowed hex; mint `W(k)`; `RES_COIN = 1e6` |
| `docs/explorer/EXPLORER_RPC_COMPATIBILITY_MATRIX.md` | Same hex; Historical RFC-014 `00010000…` must not match |
| `docs/explorer/WITNESS_V1_DISPLAY_RULES.md` | `E = 246` |
| `docs/wallet/RIGOR_RPC_REQUIREMENTS.md` | Pass criterion = shipping hex |
| `docs/rigor-qt/FUTURE_GUI_RPC_SPEC.md` | Same |
| `docs/wallet/CONVERGENCE_NETWORK_STATS_SPEC.md` | `RES_COIN = 1e6` |

Wallet **behavior** (sign/spend/RPC implementation) was not changed.

---

## Part 5 — RPC help and developer comments

| Surface | Change |
|---------|--------|
| `core/src/rpc/blockchain.cpp` RPCResult / comments | `floor(2^246 / MIN) / CONVERGENCE_DRAW_WINDOW`; `C = 1024` ORU |
| `getconvergencestats` help | `RES_COIN = 1e6` (was `100.00 RES = 10000`) |
| `core/src/script/interpreter.h` | Same formula |
| Consensus header comments (`selection.h`, `convergenceres.h`, `rigor_math.h`, `res_proof.h`, `convergence_logic.*`, `chainparams.cpp`) | Comment-only; no rule change |
| Test comments (`convergence_consensus_tests.cpp`, `canonical_layer_tests.cpp`) | Shipping formula |

No consensus arithmetic, no `params.h` constant, no wallet C++ behavior.

---

## Additional Category D copy (same task)

Release / operator surfaces that still advertised live `T = 2^240` or `100 RES`:

- `docs/release/MAINNET_GO_NO_GO_CHECKLIST.md` C-01
- `docs/release/RIGOR_PUBLIC_SYSTEM_POSITIONING.md`
- `docs/release/RIGOR_MAINNET_LAUNCH_ANNOUNCEMENT.md`
- `docs/release/RIGOR_MAINNET_RELEASE_PACKAGE.md`
- `docs/release/RIGOR_WHITEPAPER_V1_FINAL.md` abstract
- `docs/mining/RES_BODY_MINT_OPERATIONAL_SPEC.md`
- `docs/mining/RIGOR_POOL_STACK_ARCHITECTURE.md` (docs only)
- `docs/security/RES-ORU-MATURITY-INVENTORY.md`
- `docs/security/SECURITY_TEST_MATRIX.md` S-2
- `contrib/alpha/README.md`
- `docs/whitepaper/RIGOR_PROTOCOL_WHITEPAPER_V1_STRUCTURE.md` mint row
- Python spec module docstring (`rigor_consensus_spec.py`)

---

## Part 6 — Search classification

Repository search after this pass: `2^240`, `65536`, `100 RES`.

### Allowed (keep)

| Class | Examples |
|-------|----------|
| Historical RFC-014 / RFC-015 corpus | `docs/rfc/RFC-014-*.md`, RFC-001/008/012/015, RFC-017 comparison `Was 2^240` |
| Labeled historical in live docs | Freeze §3.1, SPEC T rows, architecture maps, explorer hex footnotes, website WhitepaperPage |
| Audit / security records | `SECURITY-001-A-FINAL-REPORT.md`, `SECURITY_001_A8_REPORT.md`, prior consistency audits |
| Dated freeze-alignment evidence | `docs/audit/CONSENSUS_FREEZE_ALIGNMENT.md` (2026-08-05 E=240 snapshot; freeze §3.1 is now SSOT) |
| Archive / LMS / PROOF fixtures | `docs/archive/**`, local_simulation reports, golden-vector regenerators |
| Teaching chapters with banner | `docs/whitepaper/chapters/05-mathematical-design.md` (**Historical RFC-014** banner; shipping `E=246` in the bound paper) |
| Historical tests / A-6 tools | `formal_verification/tools/regenerate_golden_vectors_a6.py`; `params.h` A-6 narrative |
| Unrelated numeric `65536` | Explorer tests selecting a `65536.00000000` **ORU** payment (not probability `T`) |

### Forbidden as unlabeled current protocol — resolved

Operator freeze, SPEC mint tables, architecture maps, explorer/wallet/Qt verify-hex, GO/NO-GO C-01, RPC help `floor(2^240/MIN)`, release positioning/announcements.

### Remaining out of scope (not Category D docs; not changed)

| Item | Why left |
|------|----------|
| `pool/winner-engine/src/constants.rs` `RES_PROBABILITY_TARGET` bytes | Still Historical RFC-014 **unwindowed E=240** hex. **Miner/pool behavior** — this task forbids changing it. |
| `pool/share-validator/**` 100.00 RES mint checks | Same: miner/pool validator behavior. Docs in those crates still describe 100.00 RES. |
| RFC files | No RFC redesign. |
| Wallet C++ / explorer TS verification code | No `2^240` hex compare found; no behavior change. |
| Dated wallet validation reports | Category C (observed then). |

---

## Validation

| Check | Result |
|-------|--------|
| Consensus rule / `params.h` / `GetResMintReward` | Unchanged |
| RFC corpus | Unchanged |
| Wallet sign/spend/RPC implementation | Unchanged |
| Miner/pool runtime constants | Unchanged |
| Website build | `npm run build` in `web/rigorchain-site` |
| Documentation link check | Sitemap `/documentation/monetary-policy` registered in `App.tsx`; documentation children in `ia/sitemap.ts` have matching routes |

---

## Website / documentation links

| Path | `App.tsx` route | `ia/sitemap.ts` |
|------|-----------------|-----------------|
| `/documentation` | yes | yes |
| `/documentation/whitepaper` | yes | yes |
| `/documentation/monetary-policy` | yes | yes |
| `/documentation/formal-verification` | yes | yes |
| `/documentation/user-guide` | yes (placeholder) | yes |
| `/documentation/developer-guide` | yes (placeholder) | yes |
| `/documentation/architecture` | yes (placeholder) | yes |
| `/documentation/rfc` | yes (placeholder) | yes |
| `/documentation/security` | yes | yes |
| `/documentation/security/minimal-script-architecture` | yes | yes |

---

## Verdict

Category D live-parameter documentation for freeze, SPEC, architecture, explorer/wallet RPC verify copy, and RPC help now matches shipping RFC-017 (`E = 246`, `C = 1024`, `W(k)`). Remaining `2^240` / `65536` / `100 RES` strings are historical RFC, audits, archives, or labeled teaching copy — except pool/miner runtime constants, which this task must not change.
