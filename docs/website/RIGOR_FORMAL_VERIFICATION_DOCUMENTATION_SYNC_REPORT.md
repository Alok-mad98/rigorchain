# RIGOR Formal Verification Documentation Sync Report

**Task:** `TASK-RIGOR-FORMAL-VERIFICATION-DOCUMENTATION-SYNC-001`  
**Date:** 2026-08-18  
**Layer:** Public documentation / official website  
**Consensus impact:** **NONE**

Source of truth: `docs/verification/FORMAL-VERIFICATION-STATUS.md` (freeze).

Honesty flags (unchanged):

```
equivalence_claimed = false
hash_kernel_shared_binary_with_cpp = false
refinement_proved = false
```

---

## Files changed

### Website (`web/rigorchain-site/`)

| File | Change |
|------|--------|
| `src/pages/FormalVerificationPage.tsx` | **New** freeze page |
| `src/App.tsx` | Route `/documentation/formal-verification` |
| `src/ia/sitemap.ts` | Documentation child KEEP |
| `src/pages/DocumentationPage.tsx` | Hub card |
| `src/components/Navbar.tsx` | Cluster link |
| `src/components/Footer.tsx` | Link + independent-verification wording |
| `src/components/VerificationNotice.tsx` | Freeze / correspondence wording |
| `src/components/ThreeLayerModel.tsx` | Layer 3 = differential verification, not refinement |
| `src/pages/LandingPage.tsx` | Remove “live formal verification” hype; CTA |
| `src/pages/WhitepaperPage.tsx` | Freeze numbers; honesty; card |
| `src/pages/WhatIsZeroTrustPage.tsx` | Deterministic rules / public inputs / independent validation |
| `src/pages/ZeroTrustVerificationPage.tsx` | Not a refinement proof; FV link |
| `src/pages/SciencePage.tsx` | Freeze link |
| `src/pages/ProtocolPage.tsx` | Freeze link |
| `src/pages/StatusPage.tsx` | Correspondence freeze vs PROOF-034 |
| `src/pages/ExperimentPage.tsx` | PROOF-034 = empirical archive |
| `src/pages/LabPage.tsx` | `COQ-MODEL` chip; freeze link |

### Whitepaper / GitHub docs

| File | Change |
|------|--------|
| `docs/whitepaper/RIGOR_PROTOCOL_WHITEPAPER.md` | New **Formal Verification Status** in Ch. 7; Layer 3 differential; live routes |
| `docs/whitepaper/README.md` | Freeze links; whitepaper route live |
| `docs/README.md` | Status / Freeze / Verdict / golden-vector links |
| `formal_verification/README.md` | Public document table |
| `docs/website/RIGOR_FORMAL_VERIFICATION_DOCUMENTATION_SYNC_REPORT.md` | This report |

---

## Wording changes (summary)

Replaced or avoided:

- “fully formally verified”
- “complete mathematical proof”
- “protocol equivalence proven”
- “live formal verification” as a product claim
- “FORMAL-PROOF” chip
- “Mathematical trust is established…” in the footer

Used:

- strongly verified Coq model
- formal correspondence boundary
- differential verification
- golden-vector verification
- runtime verification boundary
- `equivalence_claimed = false`

Zero Trust now states verification is based on deterministic rules, public identifiers as inputs, reproducible local computation, and independent validation. “Mathematically proven impossible” is reserved for specific Coq-model theorems and is not used as a portal claim.

---

## Routes

**Added (KEEP, not a placeholder):**

- `/documentation/formal-verification`

**Unchanged:**

- `/documentation/whitepaper`
- `/documentation`
- `/explorer`
- `/zero-trust-verification`
- `/mining`
- `/wallet`

Validation: `npm run build` in `web/rigorchain-site` succeeded. Preview GET:

- `/documentation/formal-verification` → **HTTP 200**
- `/documentation/whitepaper` → HTTP 200
- `/documentation` → HTTP 200
- `/explorer` → HTTP 200
- `/zero-trust-verification` → HTTP 200
- `/mining` → HTTP 200
- `/wallet` → HTTP 200

---

## Consensus / core code

**Unchanged.** This task did not modify:

- `core/src/`
- consensus / validation / PoW
- wallet or miner logic
- RFC protocol definitions
- Coq proofs (`.v` files)

---

## Git

Local commit only. No push, tag, or merge from this task.
