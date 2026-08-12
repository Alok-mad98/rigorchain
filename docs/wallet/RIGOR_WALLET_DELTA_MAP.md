# RIGOR Wallet Delta Map

**Task:** `TASK-WALLET-PROGRAM-003`  
**Version:** 1.0  
**Date:** 2026-06-23  
**Type:** Read-only delta specification — **no implementation**

| Field | Value |
|-------|--------|
| Architecture Impact | **NONE** |
| Consensus Impact | **NONE** |
| Protocol Impact | **NONE** |
| **`equivalence_claimed`** | **false** |

This document defines **all differences** between Bitcoin Core v25 Qt wallet and the RIGOR wallet product. Each delta is classified by layer and inheritance action.

**Reference baseline:** Bitcoin Core v25.0 Qt wallet  
**Reference target:** [RIGOR_WALLET_PRODUCT_ARCHITECTURE.md](./RIGOR_WALLET_PRODUCT_ARCHITECTURE.md)

---

## 1. Delta taxonomy

| Layer | Description | Inheritance default |
|-------|-------------|---------------------|
| **L0 — Shell** | Main window, menus, tray, status bar | KEEP + rebrand |
| **L1 — Wallet model** | Balances, tx list, coin control, signing | KEEP + asset roles |
| **L2 — Asset presentation** | ORU / RES labels, units, filters | MODIFY |
| **L3 — Program surfaces** | Convergence, RES mint, supply | ADD / EXTEND |
| **L4 — Read-only integration** | Explorer, diagnostics, protocol visibility | ADD |
| **L5 — Observer product** | Separate read-only client | ADD (parallel app) |

---

## 2. ORU asset layer

| ID | Delta | Bitcoin v25 | RIGOR | Status in tree | Action |
|----|-------|-------------|-------|----------------|--------|
| ORU-01 | Native unit | BTC / sat | ORU / mORU / uORU / Tharn | **Done** — `bitcoinunits.cpp` | KEEP |
| ORU-02 | Balance display | Single balance | ORU balance card on Overview | **Done** — `overviewpage.cpp` | KEEP |
| ORU-03 | Send asset lock | N/A | Send defaults to ORU; fee in ORU | **Done** — `sendcoinsdialog.cpp` | KEEP |
| ORU-04 | Receive addresses | P2WPKH default | Witness v0 ORU program addresses | **Done** — wallet layer | KEEP |
| ORU-05 | Tx history filter | All txs | ORU-only filter on History | **Partial** — domain role exists | EXTEND |
| ORU-06 | Coin control scope | All UTXOs | ORU UTXOs only when sending ORU | **Done** — `CCoinControl` | KEEP |
| ORU-07 | URI scheme | `bitcoin:` | `rigor:` (payment server) | **Done** — adapted | MODIFY labels |
| ORU-08 | Supply visibility | N/A | Dashboard ORU supply from explorer | **Not in Qt** | ADD (Phase D) |

**Minimum delta:** Presentation + filter roles on existing widgets. No new ORU wallet engine.

---

## 3. RES asset layer

| ID | Delta | Bitcoin v25 | RIGOR | Status in tree | Action |
|----|-------|-------------|-------|----------------|--------|
| RES-01 | Second native asset | N/A | RES balance on Overview | **Done** — dual panels | KEEP |
| RES-02 | Send RES | N/A | Asset combo → RES path | **Done** — `sendcoinsdialog.cpp` | KEEP |
| RES-03 | Receive RES | N/A | RES (Witness v1) addresses | **Done** — descriptor paths | KEEP |
| RES-04 | RES mint history | N/A | Filtered tx list + mint metadata | **Not in Qt** | ADD (Phase B) |
| RES-05 | RES probability display | N/A | Read-only target from node | **Not in Qt** | ADD (Phase B) |
| RES-06 | RES lag warning | N/A | Explorer vs wallet RES mismatch | **Not in Qt** | ADD (Phase D) |
| RES-07 | Coin control RES | N/A | RES-scoped UTXO tree | **Done** | KEEP |
| RES-08 | No account model | N/A | RES balance from UTXO sum only | **Done** — wallet model | KEEP |

**Constitution note:** RES must never appear as a token balance table; all RES deltas are UTXO-derived or explorer-read.

---

## 4. Convergence module

| ID | Delta | Bitcoin v25 | RIGOR | Status in tree | Action |
|----|-------|-------------|-------|----------------|--------|
| CONV-01 | Convergence UI | Does not exist | `ConvergencePage` tab | **Done** — `convergencepage.cpp` | KEEP + EXTEND |
| CONV-02 | RPC path | N/A | `createconvergence` | **Done** — RPC layer | KEEP |
| CONV-03 | Cluster inputs | N/A | Multi-UTXO selection UI | **Done** | EXTEND validation UX |
| CONV-04 | RES probability target | N/A | Display authority constant | **Missing** | ADD (Phase C) |
| CONV-05 | Convergence tx in history | N/A | Type tag + detail metadata | **Partial** | EXTEND |
| CONV-06 | Operator runbook link | N/A | Help → convergence guide | **Not in Qt** | ADD (docs link) |
| CONV-07 | Testnet path validation | N/A | Runtime matrix PASS | **Pending** — GUI-13 | VALIDATE |

**Minimum delta:** One page already added; remaining work is metadata display and testnet validation — not a second convergence UI.

---

## 5. Explorer integration

| ID | Delta | Bitcoin v25 | RIGOR | Status in tree | Action |
|----|-------|-------------|-------|----------------|--------|
| EXP-01 | Block explorer link | External URL in tx detail | RIGOR explorer base URL | **Partial** — config | EXTEND |
| EXP-02 | ExplorerClient in Qt | N/A | HTTP client for supply / RES | **Not implemented** — GUI-10 | ADD (Phase D) |
| EXP-03 | Explorer nav tab | N/A | Read-only chain browser embed or webview | **Blueprint only** | ADD (Phase D) |
| EXP-04 | Supply totals | N/A | ORU/RES circulating from indexer | **Not in Qt** | ADD |
| EXP-05 | Lag detection | N/A | Wallet height vs explorer height | **Not in Qt** | ADD |
| EXP-06 | Offline mode | N/A | Graceful degradation when explorer down | **Not in Qt** | ADD |

**Architecture constraint:** Explorer is **non-consensus** read path only. Wallet signing must work with explorer disabled.

---

## 6. Diagnostics layer

| ID | Delta | Bitcoin v25 | RIGOR | Status in tree | Action |
|----|-------|-------------|-------|----------------|--------|
| DIAG-01 | Debug window | RPC console + peers + network | Same | **Done** | KEEP |
| DIAG-02 | Wallet diagnostics tab | N/A | Descriptor health, sync state, asset split | **Blueprint** | ADD (Phase D) |
| DIAG-03 | Convergence last-run log | N/A | Surface from `.runtime` or RPC | **Not in Qt** | ADD |
| DIAG-04 | Constitution banner | N/A | `equivalence_claimed = false` | **Partial** — About | EXTEND |
| DIAG-05 | Export diagnostic bundle | N/A | Redacted logs for support | **Not in Qt** | ADD (optional RC) |

**Minimum delta:** Extend existing `RPCConsole` with a **Diagnostics** sub-tab rather than new application.

---

## 7. Protocol visibility layer

| ID | Delta | Bitcoin v25 | RIGOR | Status in tree | Action |
|----|-------|-------------|-------|----------------|--------|
| PROTO-01 | Equivalence disclaimer | N/A | Persistent non-equivalence statement | **Docs only** | ADD to About |
| PROTO-02 | Witness program labels | Generic segwit | ORU (v0) / RES (v1) | **Partial** | EXTEND |
| PROTO-03 | Supply evolution link | N/A | RFC-002 reference in About | **Not in Qt** | ADD |
| PROTO-04 | Consensus constitution link | N/A | RFC-000 in About | **Not in Qt** | ADD |
| PROTO-05 | Testnet vs mainnet badge | Testnet checkbox | RIGOR network identity | **Done** — chain params | KEEP |

---

## 8. Branding and packaging deltas

| ID | Delta | Bitcoin v25 | RIGOR | Action |
|----|-------|-------------|-------|--------|
| BRAND-01 | Application name | Bitcoin Core | RIGOR / rigor-qt | **Partial** — `RigorGUI` | MODIFY |
| BRAND-02 | Icons / splash | Bitcoin | RIGOR asset set | **Pending** | MODIFY |
| BRAND-03 | Installer | bitcoin.org refs | RIGOR manifest | **Pending** — NSIS | MODIFY |
| BRAND-04 | Help URLs | bitcoin.org | docs.rigor.* | MODIFY |

Not wallet logic; required for public testnet candidate (Phase E).

---

## 9. Observer product (parallel delta)

| ID | Delta | Bitcoin v25 | RIGOR Observer | Action |
|----|-------|-------------|----------------|--------|
| OBS-01 | Signing | Full wallet | **None** — watch-only | Separate app |
| OBS-02 | Watch registry | Import address in Qt | Dedicated Watch Registry UI | **MVP exists** — `wallet-observer-mvp/` |
| OBS-03 | ORU/RES read views | N/A | Dashboard + asset pages | **Partial** |
| OBS-04 | Convergence read-only | N/A | Convergence status panel | **Blueprint** |
| OBS-05 | Explorer embed | N/A | Primary data source | **MVP** |

Observer deltas are **not** inherited from Core Qt; they share **API contracts** with full wallet (RPC + explorer).

---

## 10. Wallet backend deltas (non-UI, inherited)

These exist in `core/src/wallet/` and are **already forked** — UI inherits behavior without redesign:

| Area | Bitcoin v25 | RIGOR delta |
|------|-------------|-------------|
| `AssetType` / ORU-RES separation | N/A | Enforced in coin selection |
| `sendres` RPC | N/A | RES transfer path |
| `createconvergence` RPC | N/A | Convergence construction |
| Descriptor wallet | Core standard | Same + RES/Witness v1 paths |
| Script verification | Core | Unchanged at consensus |

**UI delta map does not duplicate wallet engine work** — assume backend stable per WALLET DESIGN LOCK V1.

---

## 11. Delta priority matrix

| Priority | Delta IDs | Phase |
|----------|-----------|-------|
| P0 — Already shipped in tree | ORU-01–07, RES-01–03, RES-07–08, CONV-01–03 | — |
| P1 — Blueprint gap, low risk | ORU-05, RES-04–05, CONV-04–05, PROTO-01–02 | B, C |
| P2 — Integration | EXP-01–06, RES-06, ORU-08 | D |
| P3 — Operator polish | DIAG-02–05, BRAND-01–04, OBS-03–05 | D, E |

---

## 12. Minimum implementation delta (summary)

To reach **public testnet wallet candidate** without redesign:

1. **Do not rewrite:** Send, Receive, coin control, encryption, backup, address book, wallet controller, RPC console core.
2. **Finish MODIFY surfaces:** History filters, Overview dashboard cards, About/protocol panel.
3. **ADD net-new:** ExplorerClient, RES mint view, RES probability on Convergence, Diagnostics tab, branding pass.
4. **Ship Observer separately** with shared explorer/RPC contracts.

**Estimated net-new Qt code:** ~3–5k LOC (Explorer client, dashboard widgets, diagnostics tab) vs ~50k+ LOC inherited Core wallet Qt.

---

## 13. Output table

| Field | Status |
|-------|--------|
| Delta Map | **COMPLETE** |
| Architecture Impact | **NONE** |
| Consensus Impact | **NONE** |
| Protocol Impact | **NONE** |
| Equivalence Claimed | **false** |

---

*End of RIGOR_WALLET_DELTA_MAP — TASK-WALLET-PROGRAM-003*
