# RIGOR Wallet Release Roadmap

**Task:** `TASK-WALLET-PROGRAM-001`  
**Version:** 1.0  
**Date:** 2026-06-23  
**Type:** Read-only product planning document

| Field | Value |
|-------|--------|
| Consensus Impact | **NONE** |
| Protocol Impact | **NONE** |
| **`equivalence_claimed`** | **false** |

This roadmap defines product phases from current audit state to mainnet wallet candidate. It does **not** authorize protocol or consensus changes.

**Current baseline:** Full-node wallet engineering-ready; observer MVP code-complete; product packaging, validation, and branding **not ready**.

---

## Phase 1 — Product Completion

**Goal:** Close spec gaps and validate observer wallet against live stack.

**Duration estimate:** 4–8 weeks (engineering + ops)

| Work item | Owner | Exit criterion |
|-----------|-------|----------------|
| Execute `WALLET_RUNTIME_VALIDATION.md` matrix | Ops + QA | All checks PASS with screenshots |
| Wire watch mode → ORU/RES/tx pages | Frontend | Import address → balance visible |
| Dashboard supply totals + ORU convergence outputs | Frontend | `WALLET_UI_MINIMUM_SPEC.md` dashboard + ORU closed |
| Add `RES_PROBABILITY_TARGET` to rigor-qt ConvergencePage | Qt | GUI-13 partial closure |
| Resolve / document testnet `createconvergence` path | Wallet + Ops | `CONVERGENCE_RUNTIME_REPORT.md` → PASS or documented workaround |
| Single operator convergence runbook | Docs | One wallet-facing guide |
| Constitution terminology audit (observer + Qt strings) | QA | SEC-17 / PT-X4 gates PASS |

**Phase 1 gate:** GUI-13 checklist ≥ 90% PASS; runtime validation EXECUTED.

**Does not include:** Public download, signed installers, mainnet.

---

## Phase 2 — Cross-Platform Release

**Goal:** Ship installable full-node wallet; package observer for operators.

**Duration estimate:** 6–10 weeks

| Work item | Platform | Exit criterion |
|-----------|----------|----------------|
| Source app icon + rebrand NSIS | Windows | No Arda/bitcoincore.org references |
| Replace legacy pixmaps | All | `rigor*.xpm` in tree |
| Linux `.deb` or AppImage | Linux | One-command install documented |
| macOS DMG + codesign pipeline | macOS | CI artifact or manual GA process |
| Observer Docker Compose stack | All | node + explorer + wallet single `docker compose up` |
| Guix GA activation (core wallet) | All | `RELEASE_DEPLOYMENT_CHECKLIST.md` §1 green |
| GUI-17 public testnet packaging | All | Observer + rigor-qt bundle spec published |
| End-user install guide | Docs | "Download → install → sync" for non-developers |

**Phase 2 gate:** GUI-17 PASS; at least Linux + Windows install paths documented for testnet.

---

## Phase 3 — Public Testnet Wallet

**Goal:** Non-technical users can install, sync, observe ORU/RES, and follow convergence activity.

**Duration estimate:** 4–6 weeks after Phase 2

| Work item | Exit criterion |
|-----------|----------------|
| Public testnet download page | Signed tarball/installer links |
| First-run onboarding wizard (rigor-qt) | Network selection, sync explanation, ORU vs RES |
| Observer public deployment template | TLS, nginx, systemd, health checks |
| Watch-only mode for public users | No seed required for observer path |
| PT-W1 / PT-W2 gate sign-off | Wallet binary tag parity + W2 convergence signed |
| Accessibility baseline (GUI-14) | Keyboard navigation on primary flows |
| Localization scaffold (GUI-15) | English complete; `.ts` infrastructure |

**User journey success criteria:**

- [ ] Install without reading source code
- [ ] See sync progress in plain language
- [ ] View ORU balance for watched address
- [ ] See convergence events and `RES_PROBABILITY_TARGET`
- [ ] See RES mint linkage (mint txid, convergence root)
- [ ] Understand warnings (stale tip, lag, root mismatch)

**Phase 3 gate:** Closed-testnet → public-testnet wallet pilot PASS per `FINAL_RELEASE_GATE_MATRIX.md` PT-G8.

---

## Phase 4 — Mainnet Wallet Candidate

**Goal:** Wallet product meets mainnet release governance alongside PROOF-034 and SIM-FINAL-GATE.

**Prerequisites (hard):**

| Prerequisite | Source |
|--------------|--------|
| PROOF-034 archive @ h ≥ 2,100,001 | `PROOF-034-FULL-RUNTIME-ARCHIVE-VERIFICATION.md` |
| SIM-FINAL-GATE → GO | `.runtime/final-gate/` |
| RELEASE-006 operator sign-off | Release governance |
| `equivalence_claimed = false` | Global invariant |

| Work item | Exit criterion |
|-----------|----------------|
| GUI-10 ExplorerClient in production Qt | Observer features in rigor-qt |
| GUI-11 observer tab port complete | Prototype merge done |
| GUI-16 encrypted RPC / embedded node | Security review PASS |
| Full backup/restore user documentation | Seed + descriptor recovery tested |
| Upgrade path documented | Version N → N+1 wallet migration |
| Mainnet installer GA (Guix-signed) | All platforms |
| RES claim + convergence operator certification | PT-W2 + runtime archive alignment |
| Public support / known-issues page | Product ops ready |

**Phase 4 gate:** `mainnet_status: GO` from release governance **and** SIM-FINAL-GATE **and** PROOF-034 complete — not wallet team alone.

---

## Roadmap timeline (indicative)

```text
Now ────── Phase 1 ────── Phase 2 ────── Phase 3 ────── Phase 4
              │               │               │               │
         Spec + runtime   Installers     Public testnet   Mainnet
         validation       + branding     wallet pilot     candidate
```

**Critical path:** Phase 1 runtime validation → Phase 2 packaging → PROOF-034 (parallel) → Phase 4 mainnet gate.

---

## Dependencies outside wallet team

| Dependency | Blocks |
|------------|--------|
| Explorer-indexer stable on testnet | Observer all phases |
| rigord public testnet nodes | Sync + RPC |
| PROOF-034 completion | Phase 4 |
| Release governance RELEASE-006 | Mainnet declaration |
| Mining/faucet for ORU acquisition | User journey "obtain ORU" |

---

## Explicit non-goals (all phases)

- Protocol or consensus changes
- Simulation layer modifications
- Claiming `equivalence_claimed = true`
- Mobile wallet (future program)
- Hardware wallet (future program)

---

*End of WALLET_RELEASE_ROADMAP — TASK-WALLET-PROGRAM-001*
