# PHASE-4D Supply Conservation — Formal Audit (RFC-017 consumer)

| Field | Value |
|-------|-------|
| **Module** | `formal_verification/coq/ReplayReorgCore/SupplyConservation.v` |
| **Mint source** | `ConnectBlockMint.v` → `ResSchedule.get_res_mint_reward` |
| **RFC** | RFC-017 (scheduled RES reward; finite-sum cap) |
| **Audit date** | 2026-08-19 |
| **Method** | Theorem inventory from the `.v` corpus; no `Admitted` / `Axiom` in this module |

This audit covers the **replay-model** supply theorems that consume the RFC-017
schedule. It does not claim full-node Coq↔C++ equivalence.

---

## Theorem List

| Name | Claim (compressed) |
|------|--------------------|
| `apply_block_supply_correct` | Successful `apply_block` changes total supply only by `protocol_mint_total_delta` |
| `supply_deterministic` | Same start state + block → same tip total supply |
| `valid_chain_supply_deterministic` | Same valid sequence → same tip total supply |
| `supply_monotonic` | Authorized mint ≥ 0 ⇒ total supply never decreases on connect |
| `valid_sequence_supply_accumulation` | Tip supply = start + folded protocol mint along the executed list |
| `rollback_preserves_supply` | Rollback depth 0 is identity; depth > 0 restores snapshot supply |
| `rollback_snapshot_supply_unique` | Same rollback witness ⇒ same snapshot supply |
| `reorg_replay_supply_equal` | Rollback-then-replay tip supply equals direct candidate replay tip supply |
| `reorg_replay_preserves_winning_supply` | Reorg to candidate preserves candidate tip supply and suffix monotonicity |
| `genesis_supply_zero` | `total_supply init_replay_state = 0` |
| `oru_supply_bounded_by_block_count` | n blocks add ≤ n × genesis-era ORU subsidy (G-2 repair) |
| `res_mint_respects_cap` | Runtime RES cap check: pre-cap ⇒ post-mint ≤ `cb_MAX_RES_SUPPLY` |

Supporting lemmas used by the above (same module / ConnectBlockMint):
`apply_block_body_supply_evolution`, `protocol_mint_total_delta_non_negative`,
`cb_res_mint_delta_bounded`, `cb_res_mint_delta_le_genesis`,
`apply_block_oru_bounded`.

---

## Proof Status

| Item | Status | Notes |
|------|--------|-------|
| Connect-only mint channel | **Closed** | `apply_block_supply_correct` |
| Monotonicity / accumulation | **Closed** | Depends on mint delta ≥ 0 |
| Reorg supply agreement | **Closed** | Commutativity with PHASE-4C rollback-replay |
| ORU linear bound (no absolute ORU cap) | **Closed** | Deliberate: subsidy floors at 125 ORU forever |
| RES runtime cap predicate | **Closed** | `res_mint_respects_cap` |
| RES **finite-sum** cap identity | **Closed upstream** | `ResSchedule.max_issuance_within_cap`; makes the runtime check redundant under one-mint-per-block |
| Axioms / Admits in this module | **None** | Corpus grep: no `Admitted`, no `Axiom` |

RFC-017 change to this layer: `cb_res_mint_amount` is no longer a constant; it is
`get_res_mint_reward(hdr_height)`. Height is taken from the **block header**, so
the same block mints the same amount on any fork tip (reorg-safe).

---

## Remaining Admits

None in `SupplyConservation.v`.

---

## Remaining Axioms

None in `SupplyConservation.v`.

---

## Counterexamples

None for the theorems listed. Documented non-goals / design facts:

1. **`total_oru_supply ≤ ORU_MAX_SUPPLY` is false by design** (tail emission). The
   proved substitute is `oru_supply_bounded_by_block_count`.
2. Pre-G-2 model defect (unbounded `cm_oru_subsidy` without `cb_oru_subsidy_ok`) is
   closed in the model; production already rejected via `bad-cb-amount`.

---

## Supply Risk Assessment

| Risk | Formal status | Residual |
|------|---------------|----------|
| Unauthorized RES inflation past hard cap | Ruled out as finite sum under one mint/block (`max_issuance_within_cap`); runtime check retained | Assumes model mint path matches production call sites |
| Reorg changes mint amount for the same block | Ruled out by header-height scheduling | Production must use the same height source |
| Attacker-controlled ORU issuance | Ruled out as linear in block count once subsidy schedule is enforced | Producing blocks still costs PoW |
| Dual-asset total (`ORU+RES`) “cap” | Not a protocol object; only RES is capped | Do not market a joint hard cap |
| Coq model ≠ C++ ConnectBlock | Not closed here | Phase-5 / review of call sites |

Overall: for RFC-017 monetary claims that live in this model (scheduled RES,
finite-sum cap, reorg-stable height), the Coq obligations listed above are
**closed**. Persuasiveness for external review should cite `Print Assumptions`
on the tip theorems plus the C++/Coq/Python schedule parity dump, not a larger
case-split proof packaging of the same arithmetic.
