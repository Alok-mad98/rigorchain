# RIGOR Wallet Error Handling Specification

**Task:** `TASK-WALLET-PROGRAM-008`  
**Version:** 1.0  
**Date:** 2026-06-23  
**Type:** User-facing error specification — **no implementation**

| Field | Value |
|-------|--------|
| Architecture Impact | **NONE** |
| Consensus Impact | **NONE** |
| Protocol Impact | **NONE** |
| **`equivalence_claimed`** | **false** |

Defines **all user-facing error states**: detection, presentation, recovery, and copy. Failures must be **deterministic and transparent** — show what failed, why (plain language), and what the user can do.

**Related:** [RIGOR_WALLET_GUI_SPEC.md](./RIGOR_WALLET_GUI_SPEC.md) · [RIGOR_WALLET_WIDGET_LIBRARY.md](./RIGOR_WALLET_WIDGET_LIBRARY.md)

---

## 1. Error presentation tiers

| Tier | UI pattern | When |
|------|------------|------|
| **T0 Inline** | Field-level red text | Validation before submit |
| **T1 Toast** | Bottom-right 4 s | Benign confirm failures (copy fail) |
| **T2 Banner** | `WarningBanner` | Persistent environmental (offline, lag) |
| **T3 Modal** | Blocking dialog | Action failed (broadcast, RPC) |
| **T4 Critical modal** | Ack required | Root mismatch, security |

**Rule:** Never silent failure on signing, broadcast, or backup.

---

## 2. Error catalog

### 2.1 Node offline

| Attribute | Value |
|-----------|--------|
| **ID** | `ERR_NODE_OFFLINE` |
| **Detection** | No RPC connection; `ClientModel` disconnected |
| **Tier** | T2 Banner (critical) |
| **User message** | "Not connected to RIGOR node. Check that rigord is running." |
| **Disabled actions** | Send, Receive (new addr optional), Convergence, broadcast |
| **Recovery** | Diagnostics → node panel; restart rigord; check datadir |
| **Technical detail** | Expandable: RPC URL, last error string |

---

### 2.2 Synchronization in progress (IBD)

| Attribute | Value |
|-----------|--------|
| **ID** | `ERR_SYNC_IBD` |
| **Detection** | `initialblockdownload: true` or verification progress < 9999/10000 |
| **Tier** | T2 Banner (warning) + `ModalOverlay` on click |
| **User message** | "Syncing blocks… X% (height H of estimated tip)." |
| **Disabled actions** | Send, Convergence (spend actions) |
| **Allowed** | Receive address display, read-only views |
| **Recovery** | Wait; ensure peers connected |

---

### 2.3 Synchronization lag (headers behind)

| Attribute | Value |
|-----------|--------|
| **ID** | `ERR_SYNC_HEADERS_LAG` |
| **Detection** | `headers` >> `blocks` sustained |
| **Tier** | T2 Banner (warning) |
| **User message** | "Block headers ahead of downloaded blocks. Sync continuing." |

---

### 2.4 Explorer unavailable

| Attribute | Value |
|-----------|--------|
| **ID** | `ERR_EXPLORER_DOWN` |
| **Detection** | ExplorerClient HTTP fail / timeout |
| **Tier** | T2 Banner (info) |
| **User message** | "Block explorer unavailable. Balances and sends still use your node." |
| **Affected UI** | Supply panel, mint history enrich, Explorer tab, ExplorerLink disabled |
| **Recovery** | Settings → explorer URL; start indexer; work offline |
| **Authority note** | Wallet signing **not** blocked |

---

### 2.5 Explorer lag

| Attribute | Value |
|-----------|--------|
| **ID** | `ERR_EXPLORER_LAG` |
| **Detection** | `indexer_height + threshold < node_height` |
| **Tier** | T2 Banner (warning) |
| **User message** | "Explorer index is N blocks behind your node. Indexed data may be stale." |
| **Label** | Show `(indexed)` on stale fields |

---

### 2.6 Critical root mismatch

| Attribute | Value |
|-----------|--------|
| **ID** | `ERR_ROOT_MISMATCH` |
| **Detection** | `/healthz` or `/api/status` `critical_root_mismatch`; or RPC root committed ≠ computed |
| **Tier** | T4 Critical modal + T2 Banner |
| **User message** | "Convergence root mismatch detected between node and index. Do not trust indexed supply figures." |
| **Recovery** | Diagnostics; operator runbook; node authoritative for spends |
| **Dismiss** | Acknowledge once per session — banner persists |

---

### 2.7 Wallet locked

| Attribute | Value |
|-----------|--------|
| **ID** | `ERR_WALLET_LOCKED` |
| **Detection** | `WalletModel::encryptionStatus == Locked` on protected action |
| **Tier** | T3 Modal (`AskPassphraseDialog`) |
| **User message** | "Wallet is locked. Enter your password to continue." |
| **Recovery** | Unlock or cancel |
| **Not an error** | Locked icon in status bar is normal state |

---

### 2.8 Insufficient ORU balance

| Attribute | Value |
|-----------|--------|
| **ID** | `ERR_INSUFFICIENT_ORU` |
| **Detection** | `prepareTransaction` / amount > spendable ORU |
| **Tier** | T0 Inline |
| **User message** | "Insufficient ORU (including fee)." |
| **Context** | Send ORU, Convergence send |

---

### 2.9 Insufficient ORU for fee (RES send)

| Attribute | Value |
|-----------|--------|
| **ID** | `ERR_INSUFFICIENT_ORU_FEE` |
| **Detection** | RES send path; ORU fee UTXOs insufficient |
| **Tier** | T0 Inline |
| **User message** | "Insufficient ORU to pay network fee for this RES send." |

---

### 2.10 Invalid address

| Attribute | Value |
|-----------|--------|
| **ID** | `ERR_INVALID_ADDRESS` |
| **Detection** | `validateAddress` fail or asset/address type mismatch |
| **Tier** | T0 Inline |
| **User message ORU** | "Not a valid ORU (witness v0) address." |
| **User message RES** | "Not a valid RES (witness v1) address." |

---

### 2.11 Mixed asset types

| Attribute | Value |
|-----------|--------|
| **ID** | `ERR_MIXED_ASSET` |
| **Detection** | `WalletModel::MixedAssetTypes` |
| **Tier** | T0 Inline |
| **User message** | "Cannot mix ORU and RES in one transaction." |

---

### 2.12 Absurd fee

| Attribute | Value |
|-----------|--------|
| **ID** | `ERR_ABSURD_FEE` |
| **Detection** | `WalletModel::AbsurdFee` |
| **Tier** | T3 Modal |
| **User message** | "Fee unusually high. Check amount and fee rate before confirming." |

---

### 2.13 Transaction broadcast failed

| Attribute | Value |
|-----------|--------|
| **ID** | `ERR_BROADCAST_FAILED` |
| **Detection** | RPC error post-sign |
| **Tier** | T3 Modal |
| **User message** | "Transaction failed to broadcast: {reason}" |
| **Recovery** | Copy tx hex (advanced); Diagnostics; retry when synced |

---

### 2.14 Convergence RPC failed

| Attribute | Value |
|-----------|--------|
| **ID** | `ERR_CONVERGENCE_RPC` |
| **Detection** | `createconvergence` error |
| **Tier** | T3 Modal |
| **User message** | "Convergence transaction could not be created: {reason}" |
| **Recovery** | Link convergence runbook; check testnet known issues |
| **Audit note** | Known testnet failures documented in readiness audit |

---

### 2.15 Convergence pending (not error)

| Attribute | Value |
|-----------|--------|
| **ID** | `STATE_CONVERGENCE_PENDING` |
| **Detection** | Tx in mempool, 0 conf |
| **Tier** | Status chip (warning color) |
| **User message** | "Convergence transaction pending confirmation." |
| **Not** | Error dialog |

---

### 2.16 RES mint not confirmed

| Attribute | Value |
|-----------|--------|
| **ID** | `STATE_MINT_UNCONFIRMED` |
| **Detection** | Mint tx confirmations = 0 |
| **Tier** | Row status chip |
| **User message** | "Mint unconfirmed" |
| **Balance** | RES immature/pending line — not merged with available |

---

### 2.17 RES mint not found (indexer)

| Attribute | Value |
|-----------|--------|
| **ID** | `ERR_MINT_NOT_INDEXED` |
| **Detection** | Wallet shows tx; explorer mint row missing |
| **Tier** | T2 Banner (info) on RES mint page |
| **User message** | "Mint not yet indexed. Node wallet data may still show RES." |
| **Authority** | Node balance wins for spendable; indexer for linkage table |

---

### 2.18 Backup failed

| Attribute | Value |
|-----------|--------|
| **ID** | `ERR_BACKUP_FAILED` |
| **Detection** | `backupWallet()` false; IO error |
| **Tier** | T3 Modal |
| **User message** | "Backup could not be saved. Check disk permissions and path." |

---

### 2.19 Restore / mnemonic invalid

| Attribute | Value |
|-----------|--------|
| **ID** | `ERR_INVALID_MNEMONIC` |
| **Detection** | Seed validation fail |
| **Tier** | T0 Inline on word grid |
| **User message** | "One or more words are invalid. Check your backup phrase." |

---

### 2.20 Rescan in progress

| Attribute | Value |
|-----------|--------|
| **ID** | `STATE_RESCAN` |
| **Detection** | Rescan RPC active |
| **Tier** | T2 Banner (info) |
| **User message** | "Rescanning blockchain for wallet transactions…" |
| **Effect** | History may be incomplete until done |

---

### 2.21 No peers

| Attribute | Value |
|-----------|--------|
| **ID** | `ERR_NO_PEERS` |
| **Detection** | `connections == 0` while expected online |
| **Tier** | T2 Banner (warning) |
| **User message** | "No network connections. Sync may stall." |
| **Recovery** | Diagnostics peers; firewall; addnode |

---

### 2.22 Watch-only action blocked

| Attribute | Value |
|-----------|--------|
| **ID** | `ERR_WATCH_ONLY` |
| **Detection** | Watch-only wallet flag |
| **Tier** | T1 Toast or inline disabled control |
| **User message** | "This wallet is watch-only and cannot sign transactions." |

---

### 2.23 Double-submit prevented

| Attribute | Value |
|-----------|--------|
| **ID** | `GUARD_DOUBLE_SUBMIT` |
| **Detection** | Confirm clicked twice |
| **Tier** | Disable button 2 s |
| **User message** | (none — prevention only) |

---

## 3. Error × journey matrix

| Error | A | B | C | D | E | F |
|-------|---|---|---|---|---|---|
| Node offline | ✓ | ✓ | ✓ | ✓ | ✓ | ✓ |
| IBD | ✓ | ✓ | ✓ | — | ✓ | — |
| Explorer down | — | — | — | ✓ | — | ✓ |
| Wallet locked | — | ✓ | ✓ | — | ✓ | — |
| Insufficient ORU | — | ✓ | ✓ | — | — | — |
| Convergence RPC | — | — | ✓ | — | — | — |
| Mint unconfirmed | — | — | — | ✓ | — | — |
| Backup failed | — | — | — | — | ✓ | — |
| Root mismatch | ✓ | ✓ | ✓ | ✓ | — | ✓ |

---

## 4. Copy rules

| Do | Don't |
|----|-------|
| Plain language first | Raw RPC `-25` only |
| "ORU" / "RES" explicit | "coins" / "tokens" |
| "Convergence transaction" | "burn" |
| Show txid in modals when available | Hide failure context |
| Link to Diagnostics | "Contact support" without path |
| State `(node)` vs `(indexed)` | Imply indexer authority |

---

## 5. Transparency requirements

1. **Environmental errors** (offline, sync) disable spend — never silent wrong balance.
2. **Action errors** always include whether tx was created but not broadcast.
3. **Indexed errors** never block node-authoritative sends.
4. **Critical mismatch** never auto-dismiss permanently.
5. **No equivalence reassurance** in error copy ("your funds are safe guaranteed") — factual only.

---

## 6. Qt mapping (implementation reference)

| Error | Current hook |
|-------|--------------|
| Locked | `AskPassphraseDialog`, `WalletModel::requireUnlock` |
| Send failures | `SendCoinsReturn.status` |
| Convergence | `WalletModel::sendConvergenceOru` |
| Sync | `ModalOverlay`, `ClientModel` signals |
| Explorer | Future `ExplorerClient` signals |

---

## 7. Output table

| Field | Status |
|-------|--------|
| Error Handling | **COMPLETE** |
| Architecture Impact | **NONE** |
| Consensus Impact | **NONE** |
| Protocol Impact | **NONE** |
| Equivalence Claimed | **false** |

---

*End of RIGOR_WALLET_ERROR_HANDLING_SPEC — TASK-WALLET-PROGRAM-008*
