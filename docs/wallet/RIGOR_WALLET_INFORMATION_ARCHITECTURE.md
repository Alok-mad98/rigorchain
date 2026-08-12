# RIGOR Wallet Information Architecture

**Task:** `TASK-WALLET-PROGRAM-002`  
**Version:** 1.0  
**Date:** 2026-06-23  
**Type:** Read-only information architecture — **no implementation**

| Field | Value |
|-------|--------|
| Implementation Impact | **NONE** |
| **`equivalence_claimed`** | **false** |

---

## 1. Navigation hierarchy

### 1.1 RIGOR Wallet (signing)

```text
RIGOR Wallet
├── Dashboard
├── Wallet
│   ├── Overview
│   ├── Backup & Recovery
│   └── Security
├── ORU
│   ├── Overview
│   ├── Receive
│   ├── Send
│   ├── Convergence outputs
│   └── History
├── RES
│   ├── Overview
│   ├── Receive
│   ├── Send
│   ├── Mint history
│   └── Mint detail
├── Convergence
│   ├── Overview
│   ├── Activity
│   ├── Send (convergence tx)
│   ├── Event detail
│   └── Inspector (advanced)
├── History (global)
├── Explorer
│   ├── Search
│   ├── Address
│   ├── Block
│   └── RES / Convergence browsers
├── Settings
├── Diagnostics
└── About
```

### 1.2 RIGOR Observer (read-only)

```text
RIGOR Observer
├── Dashboard
├── Watch Registry
├── ORU (watch-scoped)
├── RES (watch-scoped)
├── Convergence
├── Transaction Viewer
├── Explorer (embedded views)
├── Settings (connections)
├── Diagnostics
└── About
```

---

## 2. Page hierarchy (URL / route map)

### Observer (Next.js target routes)

| Route | Screen | Parent |
|-------|--------|--------|
| `/` | Dashboard | — |
| `/watch` | Watch Registry | — |
| `/oru` | ORU Overview | ORU |
| `/oru/receive` | ORU Receive | ORU |
| `/oru/history` | ORU History | ORU |
| `/oru/convergence-outputs` | Convergence outputs | ORU |
| `/res` | RES Overview | RES |
| `/res/mints` | Mint history | RES |
| `/res/mints/[txid]` | Mint detail | RES |
| `/convergence` | Activity | Convergence |
| `/convergence/[id]` | Event detail | Convergence |
| `/tx` | Transaction search | — |
| `/tx/[txid]` | Transaction Viewer | — |
| `/explorer` | Explorer hub | — |
| `/settings` | Settings | — |
| `/diagnostics` | Diagnostics | — |
| `/about` | About | — |

### Wallet (rigor-qt stack index — target)

| Stack index | Screen |
|-------------|--------|
| 0 | Dashboard |
| 1 | Wallet overview |
| 2 | ORU |
| 3 | RES |
| 4 | Convergence |
| 5 | History |
| 6 | Explorer |
| 7 | Settings |
| 8 | Diagnostics |
| 9 | About |

---

## 3. Data hierarchy

### 3.1 Entity model (presentation layer)

```text
Network
├── ChainState (height, tip_hash, ibd, chain_name)
├── PeerSet
└── SupplyMetrics (oru_supply, res_supply) [authority]

Wallet
├── WalletIdentity (name, encrypted, descriptor_type)
├── KeyMaterial (local only — never sent to explorer)
└── BackupState

Assets
├── ORUBalance (confirmed, unconfirmed, spendable)
├── RESBalance (confirmed)
├── ORUTransaction[]
└── RESTransaction[] / RESMint[]

Convergence
├── RESProbabilityTarget [authority]
├── ConvergenceEvent[]
│   ├── convergence_txid
│   ├── winner_txid
│   ├── eligible_set_size
│   └── mint_status
└── UserConvergenceTx[]

ExplorerIndex (read-only cache)
├── IndexedAddress
├── IndexedTransaction
├── IndexedRESMint
└── ComplianceFlags (equivalence_claimed)

WatchRegistry (observer)
├── WatchEntry (type, value, label, added_at)
```

### 3.2 Data source priority

| Entity field | Primary | Fallback | Never |
|--------------|---------|----------|-------|
| ORU balance (signing) | Wallet RPC | — | Local UTXO guess |
| ORU balance (watch) | Explorer address | RPC validate | — |
| RES balance | Wallet RPC + mint list | Explorer mints | Combined with ORU |
| Convergence winner | Explorer event | `getconvergenceresult` | Local computation |
| RES probability target | Explorer/RPC compliance | — | bits-derived display |
| Sync status | Node RPC | — | Explorer-only for signing wallet |

### 3.3 Refresh policy

| Data class | Interval | Trigger |
|------------|----------|---------|
| Chain tip | 5–10 s during IBD; 30 s synced | Poll + block notify |
| Balances | 30 s | Poll + wallet tx notify |
| Convergence events | 30 s | Poll |
| Compliance | 5 min | Poll |
| Diagnostics | On demand | User open panel |

---

## 4. State hierarchy

### 4.1 Application state machine

```text
AppLifecycle
├── UNINITIALIZED
├── FIRST_RUN
├── STARTING_NODE
├── SYNCING
├── READY
├── DEGRADED (explorer down / lag warnings)
└── OFFLINE (node unreachable)
```

### 4.2 Wallet state

```text
WalletState
├── LOCKED
├── UNLOCKED
├── WATCH_ONLY
└── NO_WALLET (observer)
```

### 4.3 Transaction state (per tx)

```text
TxPresentationState
├── PENDING (0 conf)
├── CONFIRMING (1–5 conf)
├── CONFIRMED (6+ conf)
├── CONVERGENCE_PENDING (submitted convergence)
└── RES_MINT_LINKED (mint row attached)
```

### 4.4 Warning state (priority queue)

| Priority | Condition | Banner |
|----------|-----------|--------|
| P0 | `critical_root_mismatch` | Block trust in displayed data |
| P1 | Node offline | Cannot sign or refresh |
| P2 | Explorer stale / lag | Indexer behind |
| P3 | Backup not completed | Remind backup |
| P4 | `equivalence_claimed` info | Informational compliance |

Only highest priority banner shown in header; full list in Diagnostics.

---

## 5. Content hierarchy (labels)

### 5.1 Primary labels (user-facing)

| Concept | Label | Avoid |
|---------|-------|-------|
| Native mined asset | **ORU** | BTC, coin, token |
| Convergence-minted asset | **RES** | token, reward coin |
| PoW algorithm | **OpuHash** | SHA256, Bitcoin mining |
| ORU commit action | **Convergence** / convergence transaction | burn, destroy |
| RES creation | **Mint** | issue, generate, create |
| Probability display | **RES probability target** | rigor_target, T_conv |

### 5.2 Secondary labels (advanced)

| Field | Display name |
|-------|--------------|
| txid | Transaction ID |
| wtxid | Witness transaction ID |
| convergence_root | Convergence root |
| bind_height | Bind height |
| eligible_set_size | Eligible set size |

---

## 6. Cross-linking rules

| From | To | Link type |
|------|-----|-----------|
| History row | Transaction Viewer | txid click |
| RES mint row | Mint detail | mint txid |
| Mint detail | Convergence event | convergence txid |
| Convergence event | Transaction Viewer | winner txid |
| ORU convergence output | Transaction Viewer | convergence txid |
| Dashboard supply | Explorer supply view | optional drill-down |
| Any address | Explorer address | explorer icon |

---

## 7. Output table

| Field | Status |
|-------|--------|
| Information Architecture | **COMPLETE** |
| Implementation Impact | **NONE** |
| Equivalence Claimed | **false** |

---

*End of RIGOR_WALLET_INFORMATION_ARCHITECTURE — TASK-WALLET-PROGRAM-002*
