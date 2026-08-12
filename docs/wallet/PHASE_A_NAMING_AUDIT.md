# Phase A Naming Audit (Pre-Modification)

**Task:** `TASK-WALLET-PROGRAM-010` · **BUILD-02**  
**Date:** 2026-06-23  
**Type:** Read-only audit captured **before** naming cleanup

| Field | Value |
|-------|--------|
| Architecture Impact | **NONE** |
| Consensus Impact | **NONE** |
| Protocol Impact | **NONE** |
| **`equivalence_claimed`** | **false** |

---

## 1. Scope

Audit of user-visible and IPC-visible legacy terms **Bitcoin**, **Arda**, and **Ring** under `core/src/qt/` and related wallet install paths. Internal C++ identifiers inherited from Bitcoin Core (e.g. `BitcoinUnits`, `BitcoinAmountField`) are **out of scope** for Phase A — renaming would break API compatibility without user benefit.

---

## 2. Findings

| ID | Location | Term | Context | Phase A action |
|----|----------|------|---------|----------------|
| N-01 | `paymentserver.cpp:49` | Bitcoin | IPC server name `"BitcoinQt"` | **CHANGE** → `RigorQt` |
| N-02 | `guiconstants.h:51–54` | Rigor | QAPP names use `Rigor-Qt` | **KEEP** (already RIGOR-branded; WalletIdentity centralizes display name) |
| N-03 | `bitcoingui.cpp:1476` | Rigor Core | Window title via `PACKAGE_NAME` | **CHANGE** → `WalletIdentity::brand().productName` (`RIGOR Wallet`) |
| N-04 | `utilitydialog.cpp` | — | About uses generic license only | **CHANGE** → RIGOR Protocol block (BUILD-01) |
| N-05 | `rigorwallet/WALLET_DESIGN_LOCK_V1.h:1` | Ring | Copyright "RingCore" | **CHANGE** → Rigor Core |
| N-06 | `share/setup.nsi` | Arda | InstallDir, URL handler, startup link | **DEFER** (Phase G installer; not wallet layer) |
| N-07 | `forms/*.ui` | Bitcoin | Widget class `BitcoinAmountField` | **KEEP** (internal class name) |
| N-08 | `forms/optionsdialog.ui` | Bitcoin | `openBitcoinConfButton` object name | **KEEP** (internal; tooltip already generic) |
| N-09 | `res/icons/bitcoin.*` | bitcoin | Icon resource filenames | **DEFER** (Phase G brand assets) |
| N-10 | `locale/bitcoin_*.ts` | Bitcoin | Translation context names | **DEFER** (locale sweep Phase A exit) |
| N-11 | Internal headers | Bitcoin | `BitcoinUnit`, `BitcoinApplication`, etc. | **KEEP** (Core inheritance; not user-visible) |

---

## 3. User-visible strings already correct

- Send/receive tooltips reference "Rigor address"
- URI scheme: `rigor:`
- ORU/RES asset labels in UI
- `QAPP_ORG_NAME` = `Rigor`
- Tray startup links target `Rigor.lnk`

---

## 4. Planned modifications (BUILD-02)

1. `paymentserver.cpp` — IPC name `RigorQt`
2. `bitcoingui.cpp` — window title from `WalletIdentity`
3. `utilitydialog.cpp` — About modernization (BUILD-01)
4. `rigorwallet/WALLET_DESIGN_LOCK_V1.h` — copyright line
5. `guiconstants.h` — align QAPP display with `WalletIdentity::brand().applicationName`

**Not modified in Phase A:** NSIS installer, locale `.ts` files, internal class renames, icon filenames.

---

*End of PHASE_A_NAMING_AUDIT — pre-modification snapshot*
