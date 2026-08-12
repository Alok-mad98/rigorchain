# RIGOR Disclosure and Limitations Statement

**Task:** `TASK-RELEASE-008` (Governance & Disclosure Statement)  
**Version:** 1.0  
**Date:** 2026-06-20  
**Type:** Public disclosure — documentation only  
**Parent:** [RIGOR_MAINNET_LAUNCH_ANNOUNCEMENT.md](./RIGOR_MAINNET_LAUNCH_ANNOUNCEMENT.md)

| Field | Value |
|-------|--------|
| Consensus Impact | **NONE** |
| **`equivalence_claimed`** | **`false`** |

---

## 1. Purpose

This statement defines **mandatory public disclosures and limitations** for all RIGOR launch materials, website copy, GitHub README, and whitepaper derivatives.

---

## 2. System Under Continuous Verification

RIGOR is **under continuous verification**. Frozen protocol rules do not imply that all runtime evidence collection is complete.

| Phase | Status (baseline) |
|-------|-------------------|
| Formal proof track (PROOF-001~033) | Documentation/Coq layer **ESTABLISHED** |
| PROOF-034 runtime archive @ 2.1M+1 | **IN PROGRESS / NOT EXECUTED** |
| Simulation determinism (SIM-011) | **PASS** on fixtures |
| Mainnet certification | **NO_GO** |

Results published during verification are **preliminary** until PROOF-034 completes and RELEASE-006 re-issues certification.

---

## 3. Domain Separation (Non-Equivalence)

The system explicitly separates three domains:

| Domain | Description | Authority |
|--------|-------------|-----------|
| **Formal proof** | Mathematical models, catalogs, Coq track | Expectations |
| **Runtime execution** | Live/regtest chain, ConnectBlock, archive | **Consensus truth** |
| **Simulation** | Telemetry replay, orchestrator hashes | **Structural analysis only** |

**Mandatory statement:**

> The system explicitly separates formal proof, runtime execution, and simulation domains, and does **not** assert equivalence between them.

```
equivalence_claimed = false
```

### What this means in practice

| Allowed | Not allowed |
|---------|-------------|
| “Structural consistency score 0.87” | “Simulation proves runtime correct” |
| “Archive materialized at h=630k” | “Formal proof equals live chain” |
| “Deterministic replay hashes match” | “equivalence_claimed = true” |
| “CVG2 / convergence transaction is the current mechanism” | “RIGOR equivalent to Bitcoin/Ethereum” |

---

## 4. Simulation Limitations

- Simulation **does not** execute consensus on a public network.
- Simulation **does not** guarantee future runtime behavior beyond replayed scope.
- σ/root mismatches on partial fixtures are **expected** — not automatic consensus defects.
- GUI replay viewer is **observation** — not wallet authority.

---

## 5. Runtime & Archive Limitations

- Partial chain observation (e.g. h ≤ 501) **does not** satisfy PROOF-034.
- Explorer and indexer may lag or degrade — non-consensus surfaces.
- PROOF-034 archive is **regtest verification** — not automatic mainnet deployment.
- GV-DRAW-0003 harness **PARTIAL** is **expected** at FVR-M boundary — not FAIL.

---

## 6. Formal Proof Limitations

- Phase-5 formal completion **≠** runtime archive completion.
- CPP-01 closable: **false** in frozen model (PROOF-033).
- Draw closure (FVR-M): **permanently impossible** — by design.
- Formal proofs **do not** certify economic value, market price, or operator behavior.

---

## 7. Release & Certification Limitations

- RELEASE-005/006 scores are **decision criteria** — not legal guarantees.
- **GO** certification requires human operator sign-off — never automatic.
- Gate matrix **M-*** blockers (e.g. Guix G10) remain independent of archive progress.
- Public testnet **CONDITIONAL GO** is **not** mainnet permission.

---

## 8. No Financial or Legal Advice

This documentation:

- Does **not** constitute investment, legal, tax, or securities advice.
- Does **not** offer tokens for sale or solicit investment.
- Does **not** promise returns, liquidity, or listing.

Operators and readers must perform independent due diligence.

---

## 9. Prohibited Representations

The following representations are **prohibited** in official RIGOR materials:

1. `equivalence_claimed = true` or synonymous phrasing  
2. “Mainnet ready” without RELEASE-006 **GO** + M-* PASS  
3. “Fully verified” without completed PROOF-034 + certification  
4. “Production certified” without operator attestation  
5. Implying simulation validates financial outcomes  
6. Removing ORU/RES from system description  

---

## 10. Acknowledgment Block (for derivative materials)

Recommended footer for public pages:

```
RIGOR · deterministic dual-asset system (ORU/RES)
Verification phase · equivalence_claimed = false
Mainnet status: see /status · Not financial advice
```

---

## 11. Updates

This disclosure must be **re-published** when:

- PROOF-034 completes  
- RELEASE-006 certification status changes  
- Mainnet tier moves from NO-GO  

Each update must preserve **`equivalence_claimed = false`** unless baseline constitution changes (none planned).

---

*End of RIGOR Disclosure and Limitations Statement — TASK-RELEASE-008*
