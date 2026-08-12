# RIGOR Wallet Screen Map

**Task:** `TASK-WALLET-PROGRAM-002`  
**Version:** 1.0  
**Date:** 2026-06-23  
**Type:** Read-only screen specification — **no implementation**

| Field | Value |
|-------|--------|
| Implementation Impact | **NONE** |
| **`equivalence_claimed`** | **false** |

**Products:** **W** = RIGOR Wallet (signing) · **O** = RIGOR Observer (read-only) · **B** = Both

---

## Primary navigation (target)

```text
┌──────────────────────────────────────────────────────────────┐
│  [Logo]  RIGOR Wallet          [Sync status]  [⚙ Settings]   │
├──────────┬───────────────────────────────────────────────────┤
│ Dashboard│  Main content area                                │
│ Wallet   │                                                   │
│ ORU      │                                                   │
│ RES      │                                                   │
│ Converge │                                                   │
│ History  │                                                   │
│ Explorer │                                                   │
│ Diagnost │                                                   │
│ About    │                                                   │
└──────────┴───────────────────────────────────────────────────┘
```

Observer uses top nav tabs (current MVP pattern) mapping to the same logical screens.

---

## Screen catalog

### 1. Dashboard

| Attribute | Value |
|-----------|--------|
| **Products** | B |
| **Purpose** | System health at a glance |
| **Entry** | App launch default |

**Sections:**

| Section | Content |
|---------|---------|
| Sync card | Block height, progress, "Up to date" / "Syncing…", est. time remaining |
| Network card | Chain name (testnet/mainnet), peer count, connection status |
| ORU summary card | Confirmed balance (link → ORU) — **not combined with RES** |
| RES summary card | Confirmed balance (link → RES) |
| Supply panel | Network ORU supply, RES supply (authority/explorer) |
| Explorer status | Indexer sync, lag blocks, last update time |
| Warning banner | Stale tip, root mismatch, lag, offline (priority stack) |
| Quick actions | Receive ORU, Send ORU, Open Convergence (W only) |

---

### 2. Wallet (overview)

| Attribute | Value |
|-----------|--------|
| **Products** | W |
| **Purpose** | Wallet identity and aggregate wallet operations |

**Sections:**

| Section | Content |
|---------|---------|
| Wallet name | User label |
| Encryption status | Locked / unlocked |
| Backup status | Last backup date / "Not backed up" warning |
| Default receive | Quick ORU address |
| Wallet type | Descriptor / legacy indicator |
| Actions | Backup now, Change password, Export descriptors |

---

### 3. ORU

| Attribute | Value |
|-----------|--------|
| **Products** | B |
| **Purpose** | Native mined asset workspace |

**Sub-screens:**

| Sub-screen | W | O | Content |
|------------|---|---|---------|
| **ORU Overview** | ✓ | ✓ | Confirmed, unconfirmed, spendable |
| **ORU Receive** | ✓ | ✓* | QR, address list, copy (*watch addresses in O) |
| **ORU Send** | ✓ | — | Amount, address, fee, confirm |
| **ORU Convergence outputs** | ✓ | ✓ | List of user's convergence transactions (terminology: not "burn") |
| **ORU History** | ✓ | ✓ | Filtered tx list for ORU |

**Display rules:**

- Unit label: ORU (internal Tharn display optional per branding lockdown)
- Never show RES on this screen's primary balance

---

### 4. RES

| Attribute | Value |
|-----------|--------|
| **Products** | B |
| **Purpose** | Convergence-minted asset workspace |

**Sub-screens:**

| Sub-screen | Content |
|------------|---------|
| **RES Overview** | Confirmed RES balance |
| **RES Receive** | RES program addresses (rg… when activated) |
| **RES Send** | W only — `sendres` flow |
| **RES Mint history** | Table: mint txid, bind txid, bind height, conv root, conv txid, amount |
| **RES Mint detail** | Full linkage drill-down |

**Mandatory fields per row:** per `WALLET_RES_DISPLAY_RULES.md`.

---

### 5. Convergence

| Attribute | Value |
|-----------|--------|
| **Products** | B |
| **Purpose** | Draw activity and convergence send |

**Sub-screens:**

| Sub-screen | W | O | Content |
|------------|---|---|---------|
| **Convergence Overview** | ✓ | ✓ | Explainer + `RES_PROBABILITY_TARGET` |
| **Convergence Activity** | ✓ | ✓ | Event feed table |
| **Convergence Send** | ✓ | — | Create convergence tx form |
| **Convergence Detail** | ✓ | ✓ | Single event: winner, eligible set, mint status |
| **Convergence Inspector** | ✓ | ✓ | Advanced round detail (from prototype) |

---

### 6. History

| Attribute | Value |
|-----------|--------|
| **Products** | W (full), O (watch-scoped) |
| **Purpose** | Unified transaction timeline |

**Columns:** date, type (send/receive/convergence/mint), asset, amount, confirmations, txid (link)

**Filters:** asset (ORU/RES), type, date range, watch addresses

---

### 7. Transaction Viewer

| Attribute | Value |
|-----------|--------|
| **Products** | B |
| **Purpose** | Single-tx deep inspection |
| **Entry** | History click, search, Watch import |

**Sections:**

| Section | Content |
|---------|---------|
| Header | txid (primary), wtxid (secondary) |
| Status | Confirmations, block height, time |
| Inputs / outputs | Asset-tagged rows |
| Witness summary | v0/v1 witness info |
| Convergence metadata | If convergence tx: root, round, eligibility context |
| Explorer link | Open in external explorer (optional) |
| Copy actions | txid, raw hex (advanced) |

---

### 8. Explorer

| Attribute | Value |
|-----------|--------|
| **Products** | B |
| **Purpose** | Indexer-Enriched blockchain views |

**Sub-screens:**

| Sub-screen | Content |
|------------|---------|
| **Chain explorer home** | Tip, recent blocks (embedded or iframe/link) |
| **Address lookup** | Search → address summary |
| **Block lookup** | Height/hash search |
| **RES mint explorer** | Filterable mint list |
| **Convergence explorer** | Event browser |

**Note:** May delegate to explorer-indexer web UI in iframe with chrome stripped, or native ExplorerClient tables.

---

### 9. Watch (Observer primary)

| Attribute | Value |
|-----------|--------|
| **Products** | O (required), W (watch-only wallet mode) |
| **Purpose** | Manage Watch Registry |

**Inputs:** ORU address, RES address, txid, convergence txid  
**Actions:** Add, remove, export JSON, import JSON  
**Downstream:** All O screens filter by registry

---

### 10. Settings

| Attribute | Value |
|-----------|--------|
| **Products** | W (full), O (connection only) |

**Sections:**

| Section | W | O |
|---------|---|---|
| Network | ✓ (node config) | ✓ (RPC/explorer URLs) |
| Connections | ✓ | ✓ |
| Display | ✓ | ✓ |
| Notifications | ✓ | ✓ |
| Advanced | ✓ (rescan, console) | — |
| Security | ✓ (encrypt, timeout) | — |

---

### 11. Diagnostics

| Attribute | Value |
|-----------|--------|
| **Products** | B |
| **Purpose** | Operator troubleshooting |

**Panels:** Node RPC health, Explorer health, Wallet rescan status, Peer list, Mempool summary, Log tail, Export debug bundle

---

### 12. About

| Attribute | Value |
|-----------|--------|
| **Products** | B |

**Content:** App name, version, build hash, network, compliance (`equivalence_claimed: false`), licenses, links to docs, check for updates

---

### 13. First-run wizard (W only)

| Step | Screen |
|------|--------|
| 1 | Welcome |
| 2 | Choose: Create / Restore / Watch-only |
| 3 | Seed generation or entry |
| 4 | Seed confirmation |
| 5 | Encryption password (optional) |
| 6 | Data directory + node start |
| 7 | Sync introduction → Dashboard |

---

### 14. Modal / overlay screens (transient)

| Modal | Trigger |
|-------|---------|
| Send confirm | Send ORU / RES / Convergence |
| Password unlock | Encrypted wallet action |
| Backup reminder | Periodic or first spend |
| Warning acknowledge | Critical root mismatch |
| QR fullscreen | Receive screens |
| Address book picker | Send form (future) |

---

## Screen × product matrix

| Screen | W | O |
|--------|---|---|
| Dashboard | ✓ | ✓ |
| Wallet overview | ✓ | — |
| ORU | ✓ | ✓ |
| RES | ✓ | ✓ |
| Convergence | ✓ | ✓ |
| History | ✓ | ✓ |
| Transaction Viewer | ✓ | ✓ |
| Explorer | ✓ | ✓ |
| Watch | opt | ✓ |
| Settings | ✓ | ✓ |
| Diagnostics | ✓ | ✓ |
| About | ✓ | ✓ |
| First-run wizard | ✓ | — |

---

## Output table

| Field | Status |
|-------|--------|
| Screen Map | **COMPLETE** |
| Implementation Impact | **NONE** |
| Equivalence Claimed | **false** |

---

*End of RIGOR_WALLET_SCREEN_MAP — TASK-WALLET-PROGRAM-002*
