# RIGOR Formal Claim Traceability

**Public claim table** for external reviewers.  
**Honesty:** `equivalence_claimed = false`  
**Architecture map:** [`../../verification/README.md`](../../verification/README.md)  
**Related:** [`FORMAL_CLAIM_TRACEABILITY_MATRIX.md`](./FORMAL_CLAIM_TRACEABILITY_MATRIX.md) (whitepaper-line audit).

Status: **VERIFIED** (Coq model Closed) · **VECTOR** (finite golden / parity) · **PARTIAL** · **NOT**

| Claim | Specification | Coq theorem (examples) | Golden vector / check | Status |
|-------|---------------|------------------------|-----------------------|--------|
| RES supply cap | RFC-017; `rigor_consensus_lock.h` / Python schedule | `max_issuance_within_cap`, `schedule_sum_value` | NEG / SUP families; schedule parity | VERIFIED + VECTOR |
| RES monetary schedule `W(k)` | RFC-017; `ResRewardForEpoch` | `reward_pin_table`, `reward_nonincreasing`, `schedule_terminates` | `test_rfc017_schedule_parity.py` | VERIFIED + VECTOR |
| Cost floor rises as `W` falls | RFC-017 / Selection `E=246` | `cost_floor_nondecreasing_along_schedule`, `mint_cost_is_1024_oru` | Selection / EdgeCross schedule rows | VERIFIED |
| Convergence root order-independent | Draw RFCs; Python `convergence_root` | `convergence_root_order_independent` | CR / DCR / ICVG | VERIFIED + VECTOR |
| Selection determinism | Draw RFCs | `select_winner_order_independent` | WIN / INQ / A9C | VERIFIED + VECTOR |
| A-9-C no double-win (model) | Window RFCs | WindowExclusion family (`txid_wins_at_most_once`) | WIN / A9C / XFAM | VERIFIED + VECTOR |
| Dual-pool / asset separation | Dual-asset RFCs | `dual_pool_conservation`, `apply_block_res_net_bounded_by_scheduled_mints` | NEG / SMW / XFAM | VERIFIED + VECTOR |
| RES-transfer ORU fee burn (RFC-018) | RFC-018; `OruResTransferFeeBurnShare` | `fee_burn_determinism`, `supply_conservation`, `res_isolation`, `reorg_safety`, `replay_safety`, `precision_consistency`, `fee_burn_cannot_enter_circulating_supply` | `test_rfc018_fee_burn_parity.py`; `test_supply_layer_semantics.py` | VERIFIED + VECTOR (model; not ConnectBlock refinement) |
| Circulating vs ledger vs CVG2 (model) | Supply accounting model | `cvg2_locked_outputs_excluded_from_circulating`, `wallet_balance_matches_spendable_utxo_model` | `golden-vectors/supply_layer_semantics.json` | VERIFIED (model only; `equivalence_claimed` false) |
| Script shape restrictions | Minimal script docs | `apply_block_succeeds_implies_script_shape` | SMW / ICVG / XFAM | VERIFIED + VECTOR |
| Winner-must-mint (H-1) under abstract flag | Mint RFCs | `winner_must_mint` | NEG H-1 rows | PARTIAL (`cm_draw_won` abstract) |
| ORU subsidy bound | ORU schedule | `oru_supply_bounded_by_block_count` | (model; C++ GetBlockSubsidy tests) | PARTIAL (no absolute ORU cap) |
| σ 100-byte layout | SECURITY-001-A8 | Sigma / CanonicalDrawEncoder lemmas | SIG / ENC + σ CLI | VERIFIED + VECTOR |
| SHA-256d functional model | Hash kernel | `RigorSha256` / FIPS locks | HASH / FIPS + tripartite CLI | PARTIAL (not crypto security proof) |
| Complete C++ equivalence | — | — | — | NOT (`equivalence_claimed = false`) |
| Mining / difficulty / wallet / P2P | — | — | — | NOT |

## Citation rule

Allowed: *Formally verified protocol invariants with explicit verification boundaries.*  
Forbidden: *fully proven blockchain* / *bug free* / *zero risk*.
