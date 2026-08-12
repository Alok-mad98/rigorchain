# RIGOR Wallet Risk Register

**Task:** `TASK-WALLET-PROGRAM-004`  
**Version:** 1.0  
**Date:** 2026-06-23  
**Type:** Read-only risk register — **no implementation**

| Field | Value |
|-------|--------|
| Architecture Impact | **NONE** |
| Consensus Impact | **NONE** |
| Protocol Impact | **NONE** |
| **`equivalence_claimed`** | **false** |

Identifies risks for the RIGOR wallet build program and mitigations aligned with inheritance strategy (extend BC v25, do not redesign).

---

## 1. Risk summary

| Category | High | Medium | Low |
|----------|------|--------|-----|
| Technical | 2 | 5 | 3 |
| UX | 1 | 3 | 2 |
| Packaging | 1 | 2 | 2 |
| Synchronization | 2 | 2 | 1 |
| Explorer dependency | 2 | 3 | 1 |

---

## 2. Technical risks

| ID | Risk | Likelihood | Impact | Mitigation | Phase |
|----|------|------------|--------|------------|-------|
| T-01 | **SendCoinsDialog rewrite temptation** — team builds separate ORU/RES send stacks | Medium | **High** | Enforce single dialog + `comboSendAsset`; code review gate | A–B |
| T-02 | **`createconvergence` testnet failures** — RPC errors block convergence UX | Medium | **High** | Phase D dedicated E2E; operator runbook; sim regression (`sim/wallet_sim`) | D |
| T-03 | **Asset mix in coin selection** — wrong UTXO type selected for send | Low | **High** | KEEP `CCoinControl.m_asset_type`; wallet tests; no UI bypass | A |
| T-04 | **WalletModel balance stale under load** | Medium | Medium | KEEP existing poll + `tryGetBalances`; bounded stale signaling already in model | A |
| T-05 | **ConvergencePage direct RPC vs embedded API drift** | Low | Medium | Use `interfaces::Node::getConvergenceBlockResult` only; RPC Console for debug | D |
| T-06 | **ExplorerClient scope creep** — local verification logic | Medium | **High** | Constitution: display-only; no winner recompute; EXPLORERCLIENT_ARCHITECTURE review | E |
| T-07 | **Qt build breakage on new files** | Low | Medium | Add to `Makefile.qt.include` in single PR; user runs full make | E–F |
| T-08 | **Descriptor / BDB wallet edge cases** | Low | Medium | KEEP Core migration paths; no custom wallet DB | A |
| T-09 | **PSBT / bumpfee regression** | Low | Low | KEEP untouched; spot-check in Phase A | A |
| T-10 | **Observer + rigor-qt RPC contract drift** | Medium | Medium | Shared doc: RIGOR_RPC_REQUIREMENTS; versioned observer API | Parallel |

---

## 3. UX risks

| ID | Risk | Likelihood | Impact | Mitigation | Phase |
|----|------|------------|--------|------------|-------|
| U-01 | **Users confuse ORU and RES balances** — expect combined total | Medium | **High** | Dual cards, no combined total (by design); constitution audit | A–B |
| U-02 | **Alt+ shortcut churn** when adding Explorer/Diagnostics tabs | Medium | Low | Preserve Alt+1–5 through Phase D; document changes in release notes | E–F |
| U-03 | **Convergence page complexity** — operators don't understand draw vs send | Medium | Medium | Plain-language copy on ConvergencePage; link runbook | D |
| U-04 | **RES "immature" vs "pending" labeling** | Medium | Medium | Follow WALLET_RES_DISPLAY_RULES; QA glossary | C |
| U-05 | **Indexed supply vs wallet balance confusion** | High | Medium | Label explorer figures "(indexed)"; rigord wins for spendable | E |
| U-06 | **Forbidden terminology leak** (`rigor_target`, `T_conv`) | Low | **High** | GUI-18 grep gate; periodic CI string audit | A–G |

---

## 4. Packaging risks

| ID | Risk | Likelihood | Impact | Mitigation | Phase |
|----|------|------------|--------|------------|-------|
| P-01 | **Legacy Bitcoin branding in NSIS/installer** | **High** | **High** | Phase G dedicated rebrand; grep `bitcoincore.org` before release | G |
| P-02 | **Missing Qt runtime deps on Linux** | Medium | Medium | Document AppImage or static deps; test clean VM | G |
| P-03 | **macOS notarization delay** | Medium | Medium | Start early; macOS can lag Windows/Linux for RC | G |
| P-04 | **Code signing key access** | Medium | Medium | Ops-owned; unsigned builds OK for internal testnet | G |
| P-05 | **Observer deploy separate from rigor-qt** | Medium | Low | Combined download page; clear product labels (signing vs read-only) | G |

---

## 5. Synchronization risks

| ID | Risk | Likelihood | Impact | Mitigation | Phase |
|----|------|------------|--------|------------|-------|
| S-01 | **Wallet out of sync during send/convergence** | Medium | **High** | KEEP modal overlay + disable send when IBD; existing Core behavior | A |
| S-02 | **RES immature balance display lag after mint** | Medium | Medium | WalletModel refresh on tip; immature label on Overview | C |
| S-03 | **Multi-wallet tab wrong asset context** | Low | Medium | Per-wallet `WalletModel`; context resets on tab switch | B |
| S-04 | **Convergence result stale at tip** | Medium | Medium | Refresh ConvergencePage on `ClientModel::numBlocksChanged` | D |
| S-05 | **Block chain reorg affects recent tx list** | Low | Medium | KEEP TransactionTableModel reorg handling | A |

---

## 6. Explorer dependency risks

| ID | Risk | Likelihood | Impact | Mitigation | Phase |
|----|------|------------|--------|------------|-------|
| E-01 | **Explorer required for signing** (architecture violation) | Low | **Critical** | Signing paths use embedded wallet only; explorer optional | E |
| E-02 | **Indexer lag shows wrong supply** | **High** | Medium | Lag banner; rigord UTXO fallback; label "(indexed)" | E |
| E-03 | **`critical_root_mismatch` not surfaced** | Medium | **High** | `/healthz` + `/api/status` in ExplorerClient; dashboard banner | E |
| E-04 | **Explorer down — blank dashboard** | Medium | Medium | Graceful degradation; hide supply cards; wallet still works | E |
| E-05 | **Schema drift in `/api/res/mints`** | Medium | Medium | Schema-tolerant parsing per EXPLORERCLIENT_ARCHITECTURE | E |
| E-06 | **Operator trusts explorer over node for convergence truth** | Medium | **High** | Footer disclaimer; rigord wins policy; no equivalence claim | E |
| E-07 | **CORS / localhost binding in Observer** | Medium | Low | Document `.env` for testnet; systemd unit in observer MVP | Parallel |

---

## 7. Program / schedule risks

| ID | Risk | Likelihood | Impact | Mitigation |
|----|------|------------|--------|------------|
| R-01 | **Scope expands to full nav rewrite** | Medium | High | Stick to IMPLEMENTATION_MAP; defer ORU/RES top-level tabs |
| R-02 | **PROOF-034 / mainnet NO_GO blocks testnet wallet** | Low | Low | Testnet RC independent of mainnet gate (SIM-FINAL-001) |
| R-03 | **Parallel Observer + rigor-qt feature drift** | Medium | Medium | Shared RPC requirements doc; weekly sync |
| R-04 | **Insufficient QA on Windows** | Medium | Medium | Phase G clean VM matrix |

---

## 8. Risk heat map (impact × likelihood)

```
Impact →
         Low      Medium    High
L  Low   T-09     T-07      —
M  Med   U-02     T-04      T-01
       P-05     U-03      T-02
H  High  —        E-02      E-01
                 U-05      P-01
                           S-01
                           E-03
```

---

## 9. Top 5 risks to address before public testnet

| Rank | ID | Risk | Owner action |
|------|-----|------|--------------|
| 1 | P-01 | Installer branding | NSIS/icon audit in Phase G |
| 2 | T-02 | Convergence RPC reliability | E2E + runbook in Phase D |
| 3 | U-01 | ORU/RES confusion | UX copy + no combined balance |
| 4 | E-02 | Explorer lag misinformation | Lag banner + labels in Phase E |
| 5 | S-01 | Send while syncing | Verify overlay gates in Phase A |

---

## 10. Risk acceptance (explicit)

| Risk | Accepted? | Rationale |
|------|-----------|-----------|
| Explorer optional at testnet RC | **Yes** | Signing must not depend on indexer |
| macOS lagging Windows/Linux | **Yes** | Testnet can ship Linux/Windows first |
| No Inspector/Storyboard in rigor-qt v1 | **Yes** | Optional P2; prototype exists |
| `getresmintstatus` not added | **Yes** | Explorer + `listconvergencehistory` sufficient |
| Combined ORU+RES portfolio | **Never** | Constitution forbidden |

---

## 11. Output table

| Field | Status |
|-------|--------|
| Risk Register | **COMPLETE** |
| Architecture Impact | **NONE** |
| Consensus Impact | **NONE** |
| Protocol Impact | **NONE** |
| Equivalence Claimed | **false** |

---

*End of RIGOR_WALLET_RISK_REGISTER — TASK-WALLET-PROGRAM-004*
