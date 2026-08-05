# Annotated GUI Mapping — TASK-P2-002

Visual reference for intended **dynamic** data bindings on the TASK-P2-001 prototype screenshots.  
Prototype pixels are unchanged; annotations describe production sources per [`FUTURE_GUI_RPC_SPEC.md`](../FUTURE_GUI_RPC_SPEC.md).

Screenshots source: `rigor-qt-prototype/screenshots/`

---

## 01-dashboard.png

| UI element | Production binding | Poll |
|------------|-------------------|------|
| Chain height | `getblockchaininfo.blocks` | 30s / tip |
| ORU supply | Explorer `/api/status` → `economic_snapshot.oru_indexed_utxo_total_display`; fallback `gettxoutsetinfo` | 30s |
| RES supply | `getconvergencestatus.res_total` | 30s |
| Convergence events | Explorer event count **or** relabel to `cumulative_convergence` from `getblockchaininfo` | 30s |
| Last winner txid | `getconvergencestatus.last_winner_txid` | 30s |
| Last convergence root | `getconvergenceresult(tip).draw_candidate_root_committed` | 30s |
| Yellow banner | Live: connection + `equivalence_claimed: false` | — |

---

## 02-convergence.png

| UI element | Production binding | Poll |
|------------|-------------------|------|
| Candidate set size \|F\| | `getconvergenceresult.convergence_root_set_size` | tip + 30s |
| Convergence root | `draw_candidate_root_committed` | tip + 30s |
| Winner txid | `draw_winner_txid` | tip + 30s |
| Sigma digest | Winner `candidates[].h` (label: selection digest) | tip + 30s |
| Footer note | Replace with RPC authority disclaimer (constitution CP-06) | static |

**Warning overlay (dynamic):** if `draw_candidate_root_committed ≠ draw_candidate_root_computed` → root mismatch banner.

---

## 03-res-mint.png

| UI element | Production binding | Poll |
|------------|-------------------|------|
| Mint txid | `/api/res/mints[0].mint_txid` or `listconvergencehistory` | 60s |
| Mint output | `mint_vout_index` + `getrawtransaction` decode | 60s |
| Mint amount | `mint_amount` → RES formatter | 60s |
| Mint status | Presentation FSM §4.3 (not consensus) | tip |

---

## 04-network.png

| UI element | Production binding | Poll |
|------------|-------------------|------|
| Peer count | `getnetworkinfo.connections` | 30s |
| Sync status | IBD FSM §4.2 from `getblockchaininfo` | 5–30s |
| RPC endpoint | `OptionsModel` RPC URL | config |

---

## 05-logs.png

| UI element | Production binding | Poll |
|------------|-------------------|------|
| Log table | Local `RigorQtLogModel` + optional `debug.log` tail | 2s |
| Not from chain RPC | — | — |

---

## Required verdict

| Field | Status |
|-------|--------|
| Architecture Impact | **NONE** |
| Consensus Impact | **NONE** |
| Protocol Impact | **NONE** |
| Freeze Impact | **NONE** |
| Equivalence Claimed | **false** |
