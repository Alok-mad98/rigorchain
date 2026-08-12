# RIGOR Wallet Feature Matrix

**Task:** `TASK-WALLET-PROGRAM-001`  
**Version:** 1.0  
**Date:** 2026-06-23  
**Type:** Read-only product audit

| Field | Value |
|-------|--------|
| Consensus Impact | **NONE** |
| Protocol Impact | **NONE** |
| **`equivalence_claimed`** | **false** |

**Legend — Current Status:** IMPLEMENTED · PARTIAL · MISSING · NOT APPLICABLE (N/A) · NOT EXECUTED (validation)

**Legend — Priority:** P0 (release blocker) · P1 (testnet required) · P2 (mainnet candidate) · P3 (future)

---

## Core wallet features

| Feature | Current Status | Missing Work | Priority |
|---------|----------------|--------------|----------|
| ORU balance display | **IMPLEMENTED** (rigor-qt, observer `/oru`) | Observer: unconfirmed hardcoded `"0"` | P1 |
| RES balance display | **IMPLEMENTED** (rigor-qt, observer `/res`) | — | P1 |
| Dual-asset visual separation | **IMPLEMENTED** | Constitution compliance review ongoing | P1 |
| Combined ORU+RES total | **MISSING** (correct by design) | Maintain prohibition | — |
| Confirmed / unconfirmed ORU | **PARTIAL** | Observer unconfirmed not live | P1 |
| ORU convergence output history | **MISSING** | Spec requires on ORU page; implement as convergence outputs not "burn" | P1 |
| RES mint history | **IMPLEMENTED** (observer) | rigor-qt history integration | P2 |
| Convergence-linked RES display | **PARTIAL** | bind height, conv root, conv txid on all surfaces | P1 |
| `RES_PROBABILITY_TARGET` display | **PARTIAL** | Observer: yes; rigor-qt ConvergencePage: **MISSING** | P1 |
| Forbidden target terms suppressed | **IMPLEMENTED** | Periodic grep gate (GUI-18 PASS) | P1 |
| Transaction history | **IMPLEMENTED** (rigor-qt) | Observer: per-address only, no global history | P2 |
| Transaction viewer (txid lookup) | **IMPLEMENTED** (observer `/tx`) | rigor-qt integrated lookup | P2 |
| txid + wtxid display | **IMPLEMENTED** (observer) | — | P2 |
| Witness summary in tx viewer | **PARTIAL** | Depth of witness display varies | P3 |

---

## Convergence workflow

| Feature | Current Status | Missing Work | Priority |
|---------|----------------|--------------|----------|
| Convergence event list | **IMPLEMENTED** (observer, prototype) | Production Qt observer tab port (GUI-11) | P1 |
| Winner txid display | **IMPLEMENTED** | — | P1 |
| Eligible set size | **PARTIAL** | Live validation NOT EXECUTED | P1 |
| Convergence send (signing) | **IMPLEMENTED** (rigor-qt, RPC) | Testnet RPC reliability (`CONVERGENCE_RUNTIME_REPORT.md`) | P0 |
| `createconvergence` RPC | **IMPLEMENTED** | Runtime failure remediation / operator doc | P0 |
| ~~`createresclaim` RPC~~ | **REMOVED (SECURITY-001-A / A-7)** | None — the RES script path it drove was hijackable in the mempool and was deleted; `sendres` covers RES transfers | — |
| RES body mint via wallet | **N/A** | Miner/pool path only by design | — |
| Convergence operator guide | **MISSING** | Single wallet-facing runbook | P1 |
| Convergence inspector (prototype) | **IMPLEMENTED** (prototype only) | Merge to production (GUI-11) | P2 |

---

## Watch / observer mode

| Feature | Current Status | Missing Work | Priority |
|---------|----------------|--------------|----------|
| ORU address import | **PARTIAL** | Watch page stores localStorage; not wired to `/oru` | P0 |
| RES address import | **PARTIAL** | Same isolation issue | P0 |
| txid import | **PARTIAL** | Watch page only | P1 |
| Convergence txid import | **PARTIAL** | Watch page only | P1 |
| Watch persistence | **IMPLEMENTED** (localStorage) | Cross-page consumption | P0 |
| Read-only enforcement | **IMPLEMENTED** | Security boundary doc aligned | P1 |

---

## Dashboard / system status

| Feature | Current Status | Missing Work | Priority |
|---------|----------------|--------------|----------|
| Chain name / network | **IMPLEMENTED** | — | P1 |
| Block height + tip hash | **IMPLEMENTED** | — | P1 |
| Peer count | **IMPLEMENTED** | — | P2 |
| Explorer sync status | **IMPLEMENTED** | Live validation NOT EXECUTED | P1 |
| ORU / RES supply totals | **MISSING** | Required by UI minimum spec dashboard | P1 |
| Stale tip warning | **IMPLEMENTED** (code) | Runtime NOT EXECUTED | P1 |
| Root mismatch warning | **IMPLEMENTED** (code) | Runtime NOT EXECUTED | P1 |
| Explorer lag warning | **IMPLEMENTED** (code) | Runtime NOT EXECUTED | P1 |
| `equivalence_claimed` banner | **IMPLEMENTED** | Must remain false | P1 |
| Plain-language sync status | **MISSING** | Non-technical user onboarding copy | P1 |

---

## Signing wallet features (rigor-qt / rigor-cli)

| Feature | Current Status | Missing Work | Priority |
|---------|----------------|--------------|----------|
| Create wallet | **IMPLEMENTED** | First-run onboarding UX | P1 |
| Send ORU | **IMPLEMENTED** | — | P1 |
| Send RES | **IMPLEMENTED** (`sendres`) | — | P1 |
| Receive addresses | **IMPLEMENTED** | — | P1 |
| QR receive code | **PARTIAL** | libqrencode in build; UX polish | P2 |
| Wallet encryption | **IMPLEMENTED** (Core-derived) | User documentation | P2 |
| Seed phrase / mnemonic backup | **IMPLEMENTED** (Core-derived) | RIGOR-specific backup guide | P1 |
| Descriptor export | **IMPLEMENTED** | Operator documentation | P2 |
| Backup / restore | **IMPLEMENTED** | End-user guide | P1 |
| Multi-wallet | **IMPLEMENTED** | — | P3 |
| PSBT | **IMPLEMENTED** | RIGOR domain tests exist | P3 |
| Hardware wallet | **MISSING** | Out of current scope | P3 |
| Address book | **PARTIAL** | Core label system; no dedicated UX spec | P3 |

---

## Explorer integration

| Feature | Current Status | Missing Work | Priority |
|---------|----------------|--------------|----------|
| Explorer API proxy (observer) | **IMPLEMENTED** | Production hardening, TLS doc | P1 |
| ExplorerClient in rigor-qt | **MISSING** | GUI-10 NOT EXECUTED | P1 |
| Compliance endpoint display | **IMPLEMENTED** | — | P2 |
| Deep link watch → explorer | **MISSING** | — | P2 |
| Rate-limit error UX | **IMPLEMENTED** | — | P2 |
| Co-deploy explorer + wallet doc | **MISSING** | Single-stack operator guide | P1 |

---

## Release / quality gates

| Feature | Current Status | Missing Work | Priority |
|---------|----------------|--------------|----------|
| GUI-12 production wallet integration | **PASS** | — | — |
| GUI-13 minimum spec closure | **NOT EXECUTED** | Close spec gaps listed above | P0 |
| GUI-17 public testnet packaging | **NOT EXECUTED** | Observer release bundle | P0 |
| WALLET runtime validation | **NOT EXECUTED** | Unblock upstream services | P0 |
| WALLET-SIM deterministic harness | **IMPLEMENTED** | Does not imply production equiv. | — |
| Accessibility (GUI-14) | **NOT EXECUTED** | Keyboard / AT review | P2 |
| Localization (GUI-15) | **NOT EXECUTED** | `.ts` files for new strings | P2 |

---

## Summary counts

| Status | Count (of 52 features) |
|--------|------------------------|
| IMPLEMENTED | 28 |
| PARTIAL | 16 |
| MISSING | 7 |
| N/A | 1 |

**P0 items:** 7 — primarily watch-mode wiring, runtime validation, convergence RPC reliability, GUI-13/17 closure.

---

*End of WALLET_FEATURE_MATRIX — TASK-WALLET-PROGRAM-001*
