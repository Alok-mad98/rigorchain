# RIGOR Wallet Implementation Map

**Task:** `TASK-WALLET-PROGRAM-004`  
**Version:** 1.0  
**Date:** 2026-06-23  
**Type:** Read-only implementation mapping — **no code, GUI, protocol, or consensus changes**

| Field | Value |
|-------|--------|
| Architecture Impact | **NONE** |
| Consensus Impact | **NONE** |
| Protocol Impact | **NONE** |
| **`equivalence_claimed`** | **false** |

Maps every wallet component from **current `core/src/qt/` state** to **RIGOR blueprint target**, with file-level change lists and inheritance actions.

**Upstream:** [BITCOIN_V25_WALLET_INHERITANCE_ANALYSIS.md](./BITCOIN_V25_WALLET_INHERITANCE_ANALYSIS.md) · [RIGOR_WALLET_PRODUCT_ARCHITECTURE.md](./RIGOR_WALLET_PRODUCT_ARCHITECTURE.md)

---

## 1. Executive summary

| Question | Answer |
|----------|--------|
| If implementation starts tomorrow, what changes first? | **Phase A:** `utilitydialog.cpp` (About), `intro.cpp`, string rebrand in `bitcoingui.cpp` / `.ts`; validate `overviewpage.cpp`, `sendcoinsdialog.cpp`, `convergencepage.cpp` |
| What % remains untouched? | **~68%** of Qt wallet LOC (encryption, backup, address book, coin control, wallet controller, PSBT, RPC console core) |
| What unblocks public testnet? | Phases **A–C** + partial **D** + **G** (packaging); Explorer/Diagnostics are **P1** not P0 for first testnet RC |
| What is optional? | Dedicated ORU/RES top-level tabs, Inspector/Storyboard panels, diagnostic bundle export, macOS notarization polish |

---

## 2. Master component map

| Current Component | Future Component | Action | Primary Files | Notes |
|-------------------|------------------|--------|---------------|-------|
| `RigorGUI` / `BitcoinGUI` | Main shell + nav | **MODIFY** | `bitcoingui.cpp/h`, `guiconstants.h` | Add Explorer/Diagnostics actions (Phase E/F); rebrand |
| `WalletFrame` | Multi-wallet container | **KEEP** | `walletframe.cpp/h` | No structural change |
| `WalletView` | Page stack router | **EXTEND** | `walletview.cpp/h` | Register ExplorerPage, DiagnosticsPage, WalletOverviewPage |
| `OverviewPage` | Dashboard | **MODIFY** | `overviewpage.cpp/h`, `forms/overviewpage.ui` | Supply cards, explorer status, warning stack |
| `SendCoinsDialog` | ORU Send + RES Send (context) | **MODIFY** | `sendcoinsdialog.cpp/h`, `forms/sendcoinsdialog.ui` | Asset combo exists; add nav context default |
| `SendCoinsEntry` | Send row widget | **KEEP** | `sendcoinsentry.cpp/h` | Asset detection signals exist |
| `ReceiveCoinsDialog` | ORU/RES Receive (context) | **MODIFY** | `receivecoinsdialog.cpp/h` | Label/copy only; same widget |
| `TransactionView` | History | **EXTEND** | `transactionview.cpp/h` | ORU/RES/Convergence filters |
| `TransactionTableModel` | History model | **EXTEND** | `transactiontablemodel.cpp/h` | `AssetDomainRole` exists |
| `TransactionRecord` | Tx row data | **KEEP** | `transactionrecord.cpp/h` | `amount_asset` populated |
| `TransactionFilterProxy` | Column/filter proxy | **EXTEND** | `transactionfilterproxy.cpp/h` | Asset filter predicates |
| `TransactionDesc` / `TransactionDescDialog` | Tx detail | **EXTEND** | `transactiondesc.cpp`, `transactiondescdialog.cpp` | Convergence metadata badges |
| `ConvergencePage` | Convergence | **EXTEND** | `convergencepage.cpp/h`, `forms/convergencepage.ui` | RES probability, root mismatch UX |
| *(none)* | ExplorerPage | **ADD** | `explorerpage.cpp/h` *(new)* | Thin view over ExplorerClient |
| *(none)* | DiagnosticsPage | **ADD** | `diagnosticspage.cpp/h` *(new)* | Wallet + node health |
| *(none)* | WalletOverviewPage | **ADD** | `walletoverviewpage.cpp/h` *(new)* | Descriptor/backup summary |
| *(none)* | ResMintPanel | **ADD** | `resmintpanel.cpp/h` *(new)* | Port from prototype spec |
| `AddressBookPage` | Address book | **KEEP** | `addressbookpage.cpp/h` | Modal from Send/Receive |
| `AddressTableModel` | Address model | **KEEP** | `addresstablemodel.cpp/h` | |
| `CoinControlDialog` | Coin control | **KEEP** | `coincontroldialog.cpp/h` | `CCoinControl.m_asset_type` wired |
| `CoinControlTreeWidget` | UTXO tree | **KEEP** | `coincontroltreewidget.cpp/h` | |
| `AskPassphraseDialog` | Encryption UX | **KEEP** | `askpassphrasedialog.cpp/h` | |
| `CreateWalletDialog` | Wallet creation | **KEEP** | `createwalletdialog.cpp/h` | |
| `WalletController` | Load/create/unload | **KEEP** | `walletcontroller.cpp/h` | |
| `Intro` | First-run wizard | **MODIFY** | `intro.cpp/h` | RIGOR copy, backup reminder |
| `OptionsDialog` / `OptionsModel` | Settings | **EXTEND** | `optionsdialog.cpp/h`, `optionsmodel.cpp/h` | Explorer URL, refresh interval |
| `RPCConsole` | Console + peers + network | **KEEP** + **EXTEND** | `rpcconsole.cpp/h` | Diagnostics may fork tab content |
| `ClientModel` | Node state | **KEEP** | `clientmodel.cpp/h` | Height, sync, peers |
| `WalletModel` | Wallet ↔ Qt bridge | **MODIFY** | `walletmodel.cpp/h` | Dual balances, convergence send |
| `RecentRequestsTableModel` | Payment requests | **KEEP** | `recentrequeststablemodel.cpp/h` | |
| `BitcoinUnits` | Unit formatting | **KEEP** | `bitcoinunits.cpp/h` | ORU/mORU/uORU/Tharn done |
| `PaymentServer` | URI handling | **MODIFY** | `paymentserver.cpp/h` | `rigor:` scheme |
| `PSBTOperationsDialog` | PSBT | **KEEP** | `psbtoperationsdialog.cpp/h` | |
| `SignVerifyMessageDialog` | Message signing | **KEEP** | `signverifymessagedialog.cpp/h` | |
| `UtilityDialog` (About) | About / protocol | **EXTEND** | `utilitydialog.cpp` | RFC links, equivalence disclaimer |
| `ModalOverlay` | Sync overlay | **KEEP** | `modaloverlay.cpp/h` | |
| `SplashScreen` | Splash | **MODIFY** | `splashscreen.cpp/h` | Branding |
| `GUIUtil` | Shared helpers | **EXTEND** | `guiutil.cpp/h` | Asset formatting helpers exist |
| *(none)* | `ExplorerClient` | **ADD** | `explorerclient.cpp/h` *(new)* | Per EXPLORERCLIENT_ARCHITECTURE |
| *(none)* | `ObserverDataModel` | **ADD** *(optional)* | `observerdatamodel.cpp/h` | Facade over ClientModel + ExplorerClient |
| `interfaces::Wallet` | C++ wallet API | **KEEP** | `interfaces/wallet.h` | `WalletBalances` dual-asset |
| `interfaces::Node` | C++ node API | **KEEP** | `interfaces/node.h` | `getConvergenceBlockResult` |
| `wallet/` backend | Signing, UTXO, descriptors | **KEEP** | `core/src/wallet/` | No Qt changes required for core send paths |

---

## 3. Model layer map

| Model | Action | Change Required | Data Source |
|-------|--------|-----------------|-------------|
| `WalletModel` | **MODIFY** | Asset-scoped `prepareTransaction`, `sendConvergenceOru`, balance cache refresh | `interfaces::Wallet::getBalances()` |
| `TransactionTableModel` | **EXTEND** | Convergence tx type role; filter by `AssetDomainRole` | Wallet tx index |
| `TransactionFilterProxy` | **EXTEND** | ORU / RES / All / Convergence presets | Proxy on table model |
| `AddressTableModel` | **KEEP** | — | Wallet address book |
| `RecentRequestsTableModel` | **KEEP** | — | Wallet payment requests |
| `ClientModel` | **KEEP** | Optional signal for explorer lag hook | `interfaces::Node` |
| `OptionsModel` | **EXTEND** | `explorerUrl`, `explorerRefreshSec`, `defaultSendAsset` | QSettings |
| `PeerTableModel` | **KEEP** | — | Node P2P |
| `BanTableModel` | **KEEP** | — | Node |
| `ExplorerClient` *(new)* | **ADD** | HTTP cache, lag detection, typed responses | Explorer REST |
| `ObserverDataModel` *(new, optional)* | **ADD** | Merge rigord + explorer for dashboard | ClientModel + ExplorerClient |

**Must not change:** `CTxOut` storage, UTXO set layout, descriptor DB schema, consensus paths in `validation.cpp`.

---

## 4. RPC integration map (Qt ↔ node)

| UI Surface | RPC / API Path | Integration Style |
|------------|----------------|-------------------|
| Overview balances | `interfaces::Wallet` (embedded) | Direct C++ — no JSON-RPC in hot path |
| Send ORU | `sendtoaddress` via wallet API | Direct C++ |
| Send RES | `sendres` via wallet API | Direct C++ |
| Convergence send | `createconvergence` via `WalletModel::sendConvergenceOru` | Direct C++ |
| Convergence display | `Node::getConvergenceBlockResult` | Direct C++ (wraps `getconvergenceresult`) |
| Dashboard supply | Explorer `/api/status` | **ADD** ExplorerClient (Phase E) |
| RES mint panel | Explorer `/api/res/mints` + optional `listconvergencehistory` | Explorer primary; RPC fallback |
| RPC Console | All wallet + node RPC | **KEEP** existing console |
| Diagnostics | `getblockchaininfo`, `getnetworkinfo`, `getwalletinfo` | Console + new panel |

Production `rigor-qt` uses **embedded `interfaces::` calls**, not HTTP self-RPC, for signing paths. Explorer HTTP is **display-only**.

---

## 5. Untouched component list (explicit KEEP)

These files require **no functional changes** for RIGOR wallet RC unless branding strings appear:

```
askpassphrasedialog.cpp/h
addressbookpage.cpp/h
addresstablemodel.cpp/h
coincontroldialog.cpp/h
coincontroltreewidget.cpp/h
createwalletdialog.cpp/h
walletcontroller.cpp/h
walletframe.cpp/h
psbtoperationsdialog.cpp/h
signverifymessagedialog.cpp/h
recentrequeststablemodel.cpp/h
receiverequestdialog.cpp/h
editaddressdialog.cpp/h
openuridialog.cpp/h
peertablemodel.cpp/h
peertablesortproxy.cpp/h
bantablemodel.cpp/h
trafficgraphwidget.cpp/h
csvmodelwriter.cpp/h
qvalidatedlineedit.cpp/h
qvaluecombobox.cpp/h
qrimagewidget.cpp/h
bitcoinaddressvalidator.cpp/h
bitcoinamountfield.cpp/h
initexecutor.cpp/h
winshutdownmonitor.cpp/h
macdockiconhandler.cpp/h
macnotificationhandler.cpp/h
notificator.cpp/h
```

**Conditional KEEP:** `rpcconsole.cpp` — core tabs unchanged; Diagnostics may reuse peer/network widgets.

---

## 6. File change sequence (if starting tomorrow)

| Order | Week | Files | Purpose |
|-------|------|-------|---------|
| 1 | 1 | `utilitydialog.cpp`, `intro.cpp`, `bitcoingui.cpp`, `bitcoinstrings.cpp`, `src/qt/locale/*.ts` | Stabilization + rebrand + About disclaimer |
| 2 | 1–2 | `overviewpage.cpp`, `forms/overviewpage.ui` | Dashboard cards (sync/network; defer explorer supply) |
| 3 | 2 | `transactionview.cpp`, `transactionfilterproxy.cpp` | ORU/RES history filters |
| 4 | 2–3 | `sendcoinsdialog.cpp`, `receivecoinsdialog.cpp`, `optionsmodel.cpp` | ORU/RES context defaults |
| 5 | 3 | `transactiondesc.cpp` | Asset + convergence badges |
| 6 | 3–4 | `resmintpanel.cpp` *(new)*, `walletview.cpp`, `bitcoingui.cpp` | RES mint history surface |
| 7 | 4–5 | `convergencepage.cpp` | RES probability, root mismatch |
| 8 | 5–7 | `explorerclient.cpp` *(new)*, `explorerpage.cpp` *(new)*, `optionsdialog.cpp` | Explorer integration |
| 9 | 6–7 | `diagnosticspage.cpp` *(new)*, `rpcconsole.cpp` | Diagnostics |
| 10 | 7–10 | `share/setup.nsi`, `contrib/macdeploy`, icons, `rigor.png` | Packaging (Phase G) |

---

## 7. Build system touchpoints

| Area | File | Action |
|------|------|--------|
| Qt module list | `core/src/qt/bitcoin.qrc`, `Makefile.qt.include` | ADD new `.cpp` to build |
| Forms | `core/src/qt/forms/*.ui` | MODIFY overview, convergence; ADD explorer, diagnostics |
| MOC | Auto-generated | Follows new headers |
| i18n | `src/qt/locale/bitcoin_*.ts` | MODIFY strings |
| NSIS | `share/setup.nsi` | MODIFY branding (Phase G) |
| macOS deploy | `contrib/macdeploy/` | MODIFY (Phase G) |

User executes full `make` locally per `.cursorrules`; this map does not authorize automated builds.

---

## 8. Observer product (parallel track)

| Component | Location | Action |
|-----------|----------|--------|
| Watch Registry | `wallet-observer-mvp/app/watch/` | **MODIFY** |
| ORU / RES pages | `wallet-observer-mvp/app/oru/`, `app/res/` | **MODIFY** |
| Convergence read-only | `wallet-observer-mvp/app/convergence/` | **EXTEND** |
| API routes | `wallet-observer-mvp/app/api/observer/` | **KEEP** contract |
| Explorer compliance | `components/ExplorerCompliance.tsx` | **KEEP** |

Observer shares **RPC + explorer contracts** with rigor-qt; does not share Qt widgets.

---

## 9. Output table

| Field | Status |
|-------|--------|
| Implementation Map | **COMPLETE** |
| Architecture Impact | **NONE** |
| Consensus Impact | **NONE** |
| Protocol Impact | **NONE** |
| Equivalence Claimed | **false** |

---

*End of RIGOR_WALLET_IMPLEMENTATION_MAP — TASK-WALLET-PROGRAM-004*
