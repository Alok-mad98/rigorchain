# RIGOR Wallet Release Candidate Checklist

**Task:** `TASK-WALLET-PROGRAM-002`  
**Version:** 1.0  
**Date:** 2026-06-23  
**Type:** Read-only release checklist — **no implementation**

| Field | Value |
|-------|--------|
| Architecture Impact | **NONE** |
| Consensus Impact | **NONE** |
| Protocol Impact | **NONE** |
| Implementation Impact | **NONE** |
| **`equivalence_claimed`** | **false** |

Use this checklist at each release candidate gate. All items must be **PASS** unless marked optional (OPT).

**Related:** [WALLET_RELEASE_ROADMAP.md](./WALLET_RELEASE_ROADMAP.md), [WALLET_PRODUCT_READINESS_AUDIT.md](./WALLET_PRODUCT_READINESS_AUDIT.md)

---

## RC gate definitions

| Gate | Meaning |
|------|---------|
| **PASS** | Verified with evidence artifact |
| **FAIL** | Blocker — RC rejected |
| **OPT** | Optional for testnet; required for mainnet |
| **N/A** | Not applicable to this platform/gate |

---

## 1. Windows — RIGOR Wallet

| # | Item | Testnet RC | Mainnet RC |
|---|------|------------|------------|
| W1 | NSIS installer builds from CI tag | PASS required | PASS required |
| W2 | Installer branded (no Arda/bitcoincore.org) | PASS required | PASS required |
| W3 | `rigor-qt.exe` launches on clean Windows 10/11 | PASS required | PASS required |
| W4 | Embedded or documented rigord startup | PASS required | PASS required |
| W5 | First-run wizard completes (Journey A) | PASS required | PASS required |
| W6 | Code-signed installer (Authenticode) | OPT | PASS required |
| W7 | Uninstall removes shortcuts cleanly | PASS required | PASS required |
| W8 | `%APPDATA%\.rigor` data dir documented | PASS required | PASS required |
| W9 | Testnet shortcut `-testnet` works | PASS required | N/A |
| W10 | Windows Defender false-positive check | OPT | PASS required |

---

## 2. Linux — RIGOR Wallet

| # | Item | Testnet RC | Mainnet RC |
|---|------|------------|------------|
| L1 | `.deb` or AppImage builds | PASS required | PASS required |
| L2 | `.desktop` file + icon installed | PASS required | PASS required |
| L3 | Launch on Ubuntu 22.04 / 24.04 | PASS required | PASS required |
| L4 | Qt runtime deps documented or bundled | PASS required | PASS required |
| L5 | First-run wizard (Journey A) | PASS required | PASS required |
| L6 | systemd user unit template (OPT) | OPT | OPT |
| L7 | AppStream metadata | OPT | PASS required |
| L8 | Guix-signed tarball available | OPT | PASS required |

---

## 3. macOS — RIGOR Wallet

| # | Item | Testnet RC | Mainnet RC |
|---|------|------------|------------|
| M1 | DMG or `.app` bundle builds | PASS required | PASS required |
| M2 | Launch on macOS 13+ (Apple Silicon + Intel) | PASS required | PASS required |
| M3 | Gatekeeper / notarization | OPT | PASS required |
| M4 | First-run wizard (Journey A) | PASS required | PASS required |
| M5 | `~/Library/Application Support/Rigor` data path | PASS required | PASS required |
| M6 | Dock icon matches brand spec | PASS required | PASS required |

---

## 4. RIGOR Observer (all platforms)

| # | Item | Testnet RC | Mainnet RC |
|---|------|------------|------------|
| O1 | Docker Compose stack starts (node+explorer+observer) | PASS required | PASS required |
| O2 | Watch Registry wired to ORU/RES pages | PASS required | PASS required |
| O3 | All 6 minimum spec pages functional | PASS required | PASS required |
| O4 | `WALLET_RUNTIME_VALIDATION.md` matrix EXECUTED | PASS required | PASS required |
| O5 | TLS deployment guide for public hosting | OPT | PASS required |
| O6 | npm production build (`next build`) reproducible | PASS required | PASS required |
| O7 | No secrets in client bundle | PASS required | PASS required |
| O8 | Product name "RIGOR Observer" (no MVP suffix) | PASS required | PASS required |

---

## 5. Feature completeness (both products)

| # | Feature | Reference | Testnet RC |
|---|---------|-----------|------------|
| F1 | ORU balance display separated from RES | UX spec | PASS |
| F2 | RES mint history with all mandatory fields | RES display rules | PASS |
| F3 | `RES_PROBABILITY_TARGET` visible | UI minimum spec | PASS |
| F4 | Convergence activity feed | UX Journey E | PASS |
| F5 | Convergence send (Wallet only) | UX Journey C | PASS |
| F6 | Transaction viewer with convergence metadata | Screen map | PASS |
| F7 | Dashboard supply totals | Screen map | PASS |
| F8 | ORU convergence outputs list | Screen map | PASS |
| F9 | Warning banners (stale, lag, root mismatch) | Architecture | PASS |
| F10 | `equivalence_claimed: false` in About/Compliance | Branding | PASS |
| F11 | Forbidden terms absent (grep gate) | GUI-18 | PASS |
| F12 | ExplorerClient integrated in rigor-qt | GUI-10 | OPT testnet / PASS mainnet |

---

## 6. User journey validation

| Journey | Description | Evidence required |
|---------|-------------|-------------------|
| **A** | Install → Sync → Receive ORU | Screen recording + log |
| **B** | Send ORU | Testnet txid |
| **C** | Convergence → Receive RES | Convergence txid + mint row |
| **D** | Restore wallet | Rescan completes; balance match |
| **E** | Observe convergence history | Observer screenshot |
| **F** | Inspect protocol state | Diagnostics export |

All six journeys: **PASS required** for public testnet RC.

---

## 7. Branding & packaging

| # | Item | Testnet RC | Mainnet RC |
|---|------|------------|------------|
| B1 | Source app icon in repo | PASS required | PASS required |
| B2 | No legacy bitcoin*.xpm in release | PASS required | PASS required |
| B3 | Splash screen RIGOR-branded | PASS required | PASS required |
| B4 | Color tokens ORU/RES distinct | PASS required | PASS required |
| B5 | Typography spec applied | OPT | PASS required |
| B6 | Public download page live | PASS required | PASS required |
| B7 | First-run / README for non-developers | PASS required | PASS required |

---

## 8. Public testnet gate (aggregate)

| # | Gate | Requirement |
|---|------|-------------|
| PT1 | GUI-13 wallet minimum spec closure | PASS |
| PT2 | GUI-17 public testnet packaging | PASS |
| PT3 | PT-W1 wallet binary tag parity | PASS |
| PT4 | PT-W2 convergence + RES reclaim signed on closed net | PASS |
| PT5 | SIM-FINAL-GATE evaluated (sim layer) | PASS (may be NO_GO — does not block testnet wallet RC alone) |
| PT6 | Runtime validation EXECUTED | PASS |
| PT7 | Known-issues page published | PASS |
| PT8 | Support channel documented | OPT |

**Public testnet RC authorized when:** PT1–PT4, PT6–PT7 PASS + at least one platform installer PASS (W1 or L1 or M1) + Observer O1–O4 PASS.

---

## 9. Mainnet gate (aggregate)

| # | Gate | Requirement |
|---|------|-------------|
| MN1 | PROOF-034 archive @ h ≥ 2,100,001 | PASS |
| MN2 | SIM-FINAL-GATE `mainnet_status: GO` | PASS |
| MN3 | RELEASE-006 governance sign-off | PASS |
| MN4 | Guix GA signed binaries all platforms | PASS |
| MN5 | GUI-10 ExplorerClient in production Qt | PASS |
| MN6 | GUI-16 encrypted RPC security review | PASS |
| MN7 | Accessibility baseline (GUI-14) | PASS |
| MN8 | Upgrade path documented and tested | PASS |
| MN9 | `equivalence_claimed` remains false on all surfaces | PASS |
| MN10 | Mainnet installer notarized (macOS) + signed (Windows) | PASS |

**Mainnet RC authorized when:** All MN items PASS + all platform mainnet columns PASS + all journey validations PASS.

---

## 10. Evidence bundle (per RC)

Each RC submission must attach:

```
release-evidence/wallet-rc-{version}/
├── checklist-signed.json          # this checklist with PASS/FAIL
├── journey-recordings/            # A–F
├── installer-hashes.sha256
├── runtime-validation-report.md
├── grep-forbidden-terms.log
├── compliance-screenshot.png
└── known-issues.md
```

---

## 11. Sign-off block

| Role | Name | Date | Testnet RC | Mainnet RC |
|------|------|------|------------|------------|
| Wallet product | | | ☐ | ☐ |
| QA / runtime validation | | | ☐ | ☐ |
| Release governance | | | ☐ | ☐ |
| Security review | | | ☐ | ☐ |

---

## 12. Output table

| Field | Status |
|-------|--------|
| Release Candidate Checklist | **COMPLETE** |
| Architecture Impact | **NONE** |
| Consensus Impact | **NONE** |
| Protocol Impact | **NONE** |
| Implementation Impact | **NONE** |
| Equivalence Claimed | **false** |

---

*End of RIGOR_WALLET_RELEASE_CANDIDATE_CHECKLIST — TASK-WALLET-PROGRAM-002*
