# PHASE-4E ConnectBlock Mint — Formal Audit (RFC-017)

| Field | Value |
|-------|-------|
| **Module** | `formal_verification/coq/ReplayReorgCore/ConnectBlockMint.v` |
| **Schedule** | `formal_verification/coq/ResSchedule.v` |
| **RFC** | RFC-017 |
| **Audit date** | 2026-08-19 |
| **Method** | Definitions and lemmas from the `.v` corpus; no `Admitted` / `Axiom` |

---

## Theorem List

| Name | Kind | Claim (compressed) |
|------|------|--------------------|
| `cb_res_mint_amount` | Definition | `get_res_mint_reward (hdr_height (blk_header b))` |
| `cb_MAX_RES_SUPPLY` | Definition | `RES_MAX_SUPPLY` (21e6 × 10^6 atoms) |
| `cb_res_mint_amount_nonneg` | Lemma | Scheduled amount ≥ 0 |
| `cb_oru_shift` / `cb_expected_oru_subsidy` | Definitions | Halving schedule mirrored from consensus lock |
| `cb_oru_subsidy_ok` | Definition | Coinbase ORU ≤ expected subsidy at height |
| `cb_oru_shift_bounds` | Lemma | Shift ∈ [0, 3] |
| `cb_expected_oru_subsidy_pos` | Lemma | Expected subsidy > 0 at every height |
| `cb_expected_oru_subsidy_le_initial` | Lemma | Expected subsidy ≤ genesis-era subsidy |
| `cb_res_mint_delta` | Definition | Draw-won ∧ under-cap → scheduled amount, else 0 |
| `cb_oru_mint_delta` | Definition | Non-negative clamped coinbase ORU from mint spec |
| `protocol_mint_total_delta` | Definition | ORU delta + RES delta |
| `protocol_mint_total_delta_non_negative` | Lemma | Total authorized mint ≥ 0 |
| `cb_res_mint_delta_bounded` | Lemma | Delta is `0` or exactly `cb_res_mint_amount` |
| `cb_res_mint_delta_le_genesis` | Lemma | Delta ≤ `RES_REWARD_GENESIS` at every height |
| `apply_connectblock_supply_delta_*` | Lemmas | ORU/RES/total evolution and UTXO/history preservation |
| `supply_evolution_eq` | Definition | Tip scalars = prior + protocol mint |
| `default_mint_zero_delta` | Lemma | Default mint spec contributes 0 |

---

## Proof Status

| Item | Status |
|------|--------|
| RES amount = RFC-017 schedule at **header** height | Closed by definition + `ResSchedule` |
| Per-block RES upper bound = genesis reward | Closed (`cb_res_mint_delta_le_genesis`) |
| Cap gate on RES delta | Closed by definition of `cb_res_mint_delta` |
| ORU schedule bounds (G-2) | Closed |
| Axioms / Admits | None |

Pinned constants in this module (must stay aligned with
`rigor_consensus_lock.h` / `ResSchedule.v`):

- `cb_ORU_INITIAL_SUBSIDY = 100000000000` (1000 ORU sats)
- `cb_ORU_HALVING_INTERVAL = 210000`
- `cb_ORU_MAX_HALVING_SHIFTS = 3`
- `cb_MAX_RES_SUPPLY = RES_MAX_SUPPLY`

---

## Remaining Admits

None.

---

## Remaining Axioms

None.

---

## Required Output Fields

| Field | Source in model |
|-------|-----------------|
| Block height for RES | `hdr_height (blk_header b)` |
| RES mint amount | `cb_res_mint_amount b` |
| RES mint applied | `cb_res_mint_delta s b` |
| ORU mint applied | `cb_oru_mint_delta s b` |
| Draw-won flag | `cm_draw_won (blk_mint b)` |
| RES cap | `cb_MAX_RES_SUPPLY` |
| Supply evolution predicate | `supply_evolution_eq` |

Reviewers checking production parity should verify these fields against
`ConnectBlock` / `GetResMintReward` / `GetBlockSubsidy` call sites, not against
a second informal schedule spreadsheet.

---

## Review Gate

| Gate | Pass criterion |
|------|----------------|
| G1 Schedule source | `cb_res_mint_amount` calls `get_res_mint_reward`; no residual constant mint |
| G2 Reorg height | Height from block header, not tip `ReplayState` |
| G3 Cap atoms | `cb_MAX_RES_SUPPLY = RES_MAX_SUPPLY` (6-decimal atoms) |
| G4 Non-negative mint | `protocol_mint_total_delta_non_negative` |
| G5 Genesis bound | `cb_res_mint_delta_le_genesis` |
| G6 ORU schedule | `cb_oru_subsidy_ok` present for connect validation path |
| G7 No holes | No `Admitted` / `Axiom` in this module |

All gates above are **met** in the current corpus. Remaining external gate:
manual/CI confirmation that C++ uses the same schedule and header height
(already asserted in RFC-017 implementation notes; not re-proved as a Coq↔C++
refinement here).
