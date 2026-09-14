# SECURITY_TEST_MATRIX (fragment) — window / reorg / RES monopoly


> **Shipping issuance (RFC-020).** Continuous `destroy_cost(h)`, pole predicate
> `R · (2²⁵+h)² ≤ B << 40`, cap `2²²` RES, flat 128 ORU. Numeric SSOT:
> `tools/res_continuous_price_schedule.py`. Formal status:
> `audit/formal_bridge/FORMAL_STATUS_SOURCE_OF_TRUTH_v1.md`
> (`REFINEMENT_PROVED=false`).


Subset of pre-mainnet tests for miner exclusivity, `already_paid`, and reorg undo.
Full narrative: [PRE-MAINNET-WINDOW-REORG-RES-ATTACKS.md](./PRE-MAINNET-WINDOW-REORG-RES-ATTACKS.md).  
Harness inventory: `core/test/functional/res_security/README.md`.

| ID | Class | Harness | Asserts | Consensus change |
|----|-------|---------|---------|------------------|
| M-1 | Monopoly / exclusivity | `feature_res_window_reorg_res_attacks.py` | Buried foreign tx stays in F under mempool censorship; miner 1 ORU tickets do not empty F | none |
| M-2 | Monopoly / window | same | W-1 exclusive-inclusion blocks still carry buried competitors | none |
| M-3 | Candidate-set packing | same | N min-burns enter F; cost is N ORU | none |
| M-4 | No-double-win | same | Paid txid leaves F; paid-walk unique | none |
| M-5 | Weight borders | same | 0.99 ORU / non-convergence ∉ F; 1.00 ORU ∈ F | none |
| R-1 | Reorg undo | same | After `invalidateblock` + longer fork without W1: `mint_count`/`res_balance` match fork point | none |
| R-2 | Reorg re-include | same | Same txid paid ≤ 1 on surviving chain | none |
| R-3 | Deep reorg ≥ W | same | Window self-check; no double-pay; no extra mint | none |
| R-4 | Restore branch | same | invalidate+reconsider: identical σ, winner, supply | none |
| R-5 | Empty / singleton F | same | C = 0 when \|F\|<2; no mint without winner | none |
| B-1 | σ binding (A-8) | same | New coinbase ⇒ new block hash ⇒ new σ | none |
| B-2 | Censorship cost | same | Fees forgone; exclusivity does not grant the draw | none |
| B-3 | Multi-window capture | same | 100k ORU buried ticket vs 1 ORU censor tickets; miner wins = 0 | none |

Related (already in the default runner, not duplicated as new IDs):

| Existing | Role |
|----------|------|
| `feature_res_core_convergence_window.py` | Sliding W, buried candidate, no-double-pay walk |
| `feature_rigor_reorg_winner_lock.py` | A-8 lock vs deliberate re-roll |
| `rigor_attack_blocks.py` | `submitblock` reject strings |
| `src/test/convergence_consensus_tests.cpp` `a9c_already_paid_*` | Logic-level exclusion / branch-local paid set |

---

# SECURITY_TEST_MATRIX (fragment) — solo-mining path (no pool)

Subset of pre-mainnet tests for mempool↔consensus, RES mint authorization, robustness, and GBT.  
Full narrative: [PRE-MAINNET-SOLO-SECURITY.md](./PRE-MAINNET-SOLO-SECURITY.md).  
Inventory: [PRE-MAINNET-SOLO-INVENTORY.md](./PRE-MAINNET-SOLO-INVENTORY.md).  
Pool / stratum / web are **not shipped**.

| ID | Class | Harness | Asserts | Consensus change |
|----|-------|---------|---------|------------------|
| V-1 | Mempool vs connect | `feature_res_solo_security.py` | Forged mint mempool-allowed; connect `bad-rigor-res-proof-height`; supply unchanged | none |
| V-2 | Policy ≠ consensus | same | Zero-fee policy reject; `generateblock` may include | none |
| V-3 | Floor / form | same | 0.99 ORU reject; 1.00 ORU `is_convergence` | none |
| V-4 | RBF | same | Replaced ticket leaves mempool and F | none |
| V-5 | Mempool reorg | same | Re-accept after disconnect; no extra mint | none |
| S-1 | Forged mint | same | No winner binding; `mint_count=0` | none |
| S-2 | Mint predicate | `res_issuance_tests` | Live mint `CheckResMintArithmetic`; genesis destroy cost 1024 ORU/RES | none |
| S-3 | RES cap | same + `runtime_supply_invariant_tests` | Snapshot supply; `bad-rigor-res-max-supply` pin | none |
| S-4 | Duplicate pay | same | Saturating win; `distinct_paid=1` | none |
| S-5 | Automint vs generateblock | same | JSONRPCError when automint already in template | RPC only (`mining.cpp`) |
| A-1 | Abort vs Invalid | same | Peer blocks `state.Invalid`; remaining AbortNode is local | none |
| A-2 | Malformed block | same | Bad mint / bad CVGE cumulative; node stays up | none |
| A-3 | Deep reorg | inherit window R-3 | No abort; supply consistent | none |
| A-4 | Restart mid-window | same | Tip + `mint_count` recover | none |
| G-1 | GBT consistency | same | Template lists ticket; connect matches window | none |
| G-2 | Censor foreign tickets | same | Block accepted per consensus; no free monopoly | none |
| G-3 | Stale GBT | same | Wrong prev / commitment rejected | none |
| G-4 | Solo restart | same | No stuck automint; no duplicate mint | none |

---

# SECURITY_TEST_MATRIX (fragment) — RES / ORU maturity

Consensus spendability of coinbase ORU and body RES mint UTXOs.  
Narrative: [RES-ORU-MATURITY.md](./RES-ORU-MATURITY.md). Inventory: [RES-ORU-MATURITY-INVENTORY.md](./RES-ORU-MATURITY-INVENTORY.md).

| ID | Class | Harness | Asserts | Consensus change |
|----|-------|---------|---------|------------------|
| T-RES-1 | Immature mint | `feature_res_maturity.py` | Raw spend of mint at depth 1 → `bad-txns-premature-spend-of-res-mint` | yes (`COIN_FLAG_RES_MINT`) |
| T-RES-2 | Depth MATURITY-1 | same | Still reject | yes |
| T-RES-3 | Depth MATURITY | same | Mempool + block accept if otherwise valid | yes |
| T-RES-4 | Miner block | same | `generateblock` / connect rejects immature spend | yes |
| T-RES-5 | Reorg | same | Disconnect mint ⇒ spend invalid; restore keeps flag | yes |
| T-RES-6 | Bypass wallet | same as T-RES-1 | Node reject, not wallet UI | yes |
| T-ORU-1 | Coinbase | same | Immature ORU coinbase still `bad-txns-premature-spend-of-coinbase` | none (regression) |
| T-ISSUE-1 | Issuance sanity | same | Coinbase RES=0; mint 100.00; `mint_count` | none |
