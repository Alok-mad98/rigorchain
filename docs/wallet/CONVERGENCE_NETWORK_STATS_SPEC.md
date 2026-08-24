# Convergence Network Statistics Spec

| Field | Value |
|-------|-------|
| **Task** | `TASK-WALLET-CONVERGENCE-NETWORK-STATS-001` |
| **Date** | 2026-08-11 |
| **Consensus impact** | **NONE** — node cache + RPC + GUI presentation only |
| **W** | **32** (`Consensus::CONVERGENCE_DRAW_WINDOW`) — unchanged |
| **T** | unchanged |

---

## Why this exists

The Convergence page should communicate three ideas:

1. **My Convergence** — I participate  
2. **Network Statistics** — others participate  
3. **RES minted** — the system is functioning  

Network Statistics show that Convergence is an active chain-wide mechanism where ORU participants commit ORU toward deterministic RES minting.

- **ORU** provides PoW security and economic coordination.  
- **RES** is minted through deterministic Convergence — not as a mining reward.

---

## User surface

**Wallet → Convergence**

| Section | Content |
|---------|---------|
| My Convergence | Personal W=32 lifecycle (existing) |
| **CONVERGENCE NETWORK** | Chain-wide stats (this task) |
| Start Convergence / History | Existing spend + history |

### Displayed fields

| Label | RPC field | Meaning |
|-------|-----------|---------|
| Active Convergence | `active_convergences` | Convergence candidates in the tip W-window (paid + unpaid) |
| ORU committed | `active_oru` | Sum of those candidates’ ORU (display: ORU with 8 decimals) |
| Pending W=32 windows | `pending_windows` | Unpaid candidates still in tip draw set F |
| Total RES minted | `total_res_minted` | Cumulative RES on active chain (display: RES with 6 decimals; `RES_COIN = 1e6`) |
| Last updated | `best_height` | Active tip height |

### Not shown to normal users

candidate list · digest · selection target · normalized score · tx binding · consensus hashes  

Those remain under **Settings → Advanced → System Diagnostics → Convergence Details**.

---

## RPC: `getconvergencestats`

```json
{
  "active_convergences": 125,
  "active_oru": 35642000000000,
  "pending_windows": 98,
  "total_res_minted": 1240000,
  "best_height": 125432
}
```

### Units

| Field | Unit |
|-------|------|
| `active_oru` | ORU sat-style atoms (`COIN` = 1e8) |
| `total_res_minted` | RES atoms (`RES_COIN` = 1_000_000; 1.000000 RES = 1000000) |

### Registration

- Command: `getconvergencestats`  
- Category: `blockchain`  
- Implementation: `core/src/rpc/blockchain.cpp`

---

## Data source (full node only)

| Allowed | Forbidden |
|---------|-----------|
| Tip W-window via `GetBlockWindowContribution` / draw set | External API |
| Cumulative RES from block-index mint sums | Explorer / indexer |
| Active chain tip height | Centralized DB / oracle |

Every full node independently derives the same tip stats from the same chain.

---

## Cache / performance

| Requirement | Implementation |
|-------------|----------------|
| No full-chain scan per GUI refresh | Tip-hash process cache |
| Update on connect | `RefreshConvergenceNetworkStatsCache` in `ConnectTip` |
| Update on disconnect / reorg | Invalidate + refresh in `DisconnectTip` |
| Restart safe | Cold miss rebuilds from last W blocks (O(W)) |
| Reorg safe | Cache keyed by tip hash |

API: `GetConvergenceNetworkStats` / `RefreshConvergenceNetworkStatsCache` / `InvalidateConvergenceNetworkStatsCache` in `validation.h`.

---

## Implementation map

| Layer | Path |
|-------|------|
| Stats compute + cache | `core/src/validation.cpp` / `validation.h` |
| RPC | `getconvergencestats` in `rpc/blockchain.cpp` |
| GUI panel | `qt/rigorwallet/widgets/convergence_networkpanel.*` |
| Page wiring | `qt/convergencepage.cpp` |
| Functional test | `test/functional/feature_rigor_convergence_network_stats.py` |

---

## Acceptance checklist

- [x] Full node calculates global Convergence statistics  
- [x] RPC available (`getconvergencestats`)  
- [x] Wallet displays Network Statistics  
- [x] No consensus / W / T / mint-rule / tx-format changes  
- [x] No external service dependency  
- [x] Reorg safe (tip-keyed cache + DisconnectTip refresh)  
- [x] Restart safe (O(W) rebuild)  
- [x] Documentation completed  

### Local verification

Out-of-tree build (`build-native`):

```bash
# Daemon already built; functional scripts live under the source tree.
cd ./core/build-native
./test/functional/test_runner.py feature_rigor_convergence_network_stats.py --descriptors
```

Note: the test framework writes both `bitcoin.conf` and `rigor.conf` (rigord reads `rigor.conf`).
