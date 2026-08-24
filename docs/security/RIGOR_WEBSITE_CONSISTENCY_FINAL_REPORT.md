# RIGOR Website Consistency Final Report

**Task:** `TASK-RIGOR-PROTOCOL-CONSISTENCY-FINAL-AUDIT-P0-001`  
**Date:** 2026-08-20  
**Scope:** Public documentation routes under `web/rigorchain-site/`.  
**Consensus / wallet / miner / RFC impact:** **NONE**  
**Honesty:** `equivalence_claimed = false`

Companion: [RIGOR_PROTOCOL_CONSISTENCY_FINAL_AUDIT.md](./RIGOR_PROTOCOL_CONSISTENCY_FINAL_AUDIT.md), [RIGOR_CLAIM_WORDING_AUDIT.md](./RIGOR_CLAIM_WORDING_AUDIT.md).

Reviewed routes:

| Route | Component | Status |
|-------|-----------|--------|
| `/documentation` | `DocumentationPage.tsx` | Live library |
| `/documentation/whitepaper` | `WhitepaperPage.tsx` | Live |
| `/documentation/security` | `SecurityArchitecturePage.tsx` | Live |
| `/documentation/security/minimal-script-architecture` | `MinimalScriptArchitecturePage.tsx` | Live |
| `/documentation/monetary-policy` | `MonetaryPolicyPage.tsx` | Live |
| `/documentation/formal-verification` | `FormalVerificationPage.tsx` | Live |
| `/` | `LandingPage.tsx` | Live (see residual) |
| `/protocol` | `ProtocolPage.tsx` | Live lab page |

---

## 1. Terminology

| Topic | Website | Bound protocol paper / RFC / code | Match? |
|-------|---------|-------------------------------------|--------|
| Dual native assets ORU / RES | Whitepaper, Minimal Script, Monetary Policy, About, ProtocolCard | Yes | **Yes** (roles: MoE vs scarce settlement) |
| Not colored coins / inscriptions / Runes / wrapped / contract tokens | Whitepaper Ch.3 snippet; Minimal Script §4 | Yes | **Yes** |
| Constrained scripts; no GP VM | Security + Minimal Script + whitepaper snippet | Yes | **Yes** |
| Required minimize-scripts sentence | Security, Minimal Script, whitepaper snippet | Yes | **Yes** |
| Witness v0 = spendable ORU; v1 = RES key-path + CVG2 | Minimal Script table; whitepaper snippet | Registry | **Yes** |
| Witness v1 ≠ Taproot | Same | Registry | **Yes** |
| RFC-017 `W(k)`, epoch 210000, cutoff 250, 21M cap, sum pin | Monetary Policy page | RFC-017 / `rigor_consensus_lock.h` / Coq / Python | **Yes** |
| FV allowed claim sentence | Documentation card + Formal Verification page | Ch.8 | **Yes** |
| `equivalence_claimed = false` | Footer, AppShell, all five doc pages | Global | **Yes** |
| Shipping draw cost `E = 246`, `C = 1024` | ProtocolCard FORGE; FV/whitepaper abstracts | RFC-017 / `Selection.v` | **Partial** — landing + whitepaper §5 still lead with 65536 |

Landing and condensed whitepaper §5 still present `1/65536` as if it were the live aggregate identity. The whitepaper **abstract** on the same page correctly says that identity is historical and shipping `T` uses `E = 246`.

---

## 2. Links

### In-app (React Router)

| From | To | Result |
|------|----|--------|
| Documentation library | Whitepaper, Monetary Policy, Formal Verification, Security, Minimal Script | Routed in `App.tsx` |
| Security hub | Minimal Script, Formal Verification, Zero Trust, Whitepaper | Live |
| Minimal Script | Whitepaper, Formal Verification, Zero Trust | Live |
| Formal Verification | Status, PROOF-034, Zero Trust, Minimal Script | Live |
| Monetary Policy | Formal Verification | Live |
| Whitepaper | Minimal Script, Mining, Wallet Convergence, Explorer | Live |

Placeholder documentation routes (not this audit’s five pages): `/documentation/architecture`, `/documentation/rfc`, `/documentation/developer-guide`, `/documentation/user-guide` remain `PlaceholderPage`.

### GitHub (`https://github.com/rigorchain/rigorchain/blob/main/…`)

Formal Verification, Security, and Minimal Script pages point at `verification/README.md`, `HOW_TO_VERIFY_RIGOR.md`, `RIGOR_FORMAL_CLAIM_TRACEABILITY.md`, `FORMAL-VERIFICATION-STATUS.md`, `FORMAL-VERIFICATION-VERDICT.md`, `FORMAL_VERIFICATION_ASSUMPTION_BOUNDARY.md`, `GOLDEN-VECTOR-DISCREPANCIES.md`, `docs/security/minimal-script-architecture.md`, script registry, boundary, and prior script audit.

**In-tree targets exist.** Remote `main` may lag this working tree until a later push (known residual from the script-architecture audit).

### Verification architecture

Site Formal Verification page cites the public map `verification/README.md` and the reproduce guide. Paths exist locally.

---

## 3. Claims

| Claim on site | Allowed? |
|---------------|----------|
| Formally verified protocol invariants with explicit verification boundaries | **Yes** |
| Formally verified monetary invariants — not a refinement proof | **Yes** (scoped) |
| Strongly verified Coq model; 422 / 215 living lock | **Yes** (matches `golden_lock.json`) |
| Complete C++ equivalence listed as **outside** boundary | **Yes** |
| `equivalence_claimed = false` displayed; AppShell faults on `true` | **Yes** |
| “Fully proven blockchain” / “zero risk” / “bug free” as a positive claim | **Absent** |
| “RIGOR has zero scripts” | **Absent** |
| Landing `P(success) = 1/65536` unlabeled | **Misreadable** vs RFC-017 |

`EQUIVALENCE_CLAIMED` in `version.ts` is `false as const`. Status page prints that constant.

---

## 4. Navigation

`PRIMARY_NAV` includes Documentation. `DocumentationPage` library includes all five reviewed pages.

**Gap:** `web/rigorchain-site/src/ia/sitemap.ts` `documentation` children list Whitepaper, Formal Verification, Security, Minimal Script, and several placeholders, but **omit Monetary Policy**. Consequences:

- `SubNav` on `/documentation/*` does not list Monetary Policy.
- `sectionByPath('/documentation/monetary-policy')` still attaches the Documentation subnav (prefix match), so the page is not orphaned, but it is missing from the section child list.

Library card + `App.tsx` route **do** exist (`/documentation/monetary-policy`).

Footer: `equivalence_claimed = false · Not an investment offering · Native assets: ORU and RES` — consistent.

Landing CTA includes Formal Verification. It does not deep-link Monetary Policy or Minimal Script (reachable via Documentation).

---

## 5. Page-by-page

### Whitepaper (`/documentation/whitepaper`)

Dual assets, RFC-017 amount `W(h)`, 21M cap, constrained scripts, CVG2 as Witness v1 ORU-domain, FV three-layer split, living 422/215 with historical 324/117 note. Abstract correctly labels 65536 as teaching-only. §5 math block still uses 65536 identities without repeating `E = 246`.

### Security Architecture

Priority list, required minimize-scripts sentence, no GP VM, execution-surface inventory, links to Minimal Script / FV / Zero Trust / whitepaper. Honesty banner.

### Minimal Script Architecture

Native dual-asset system; overlay rejection list; Witness v0 / P2WSH / v1 RES / CVG2 table; Witness v1 ≠ Taproot; `equivalence_claimed = false`. Matches security note and Ch.3.

### Monetary Policy

`W(k)` formula, `RES_COIN = 1_000_000`, cutoff `k > 250`, pins `k ∈ {0,1,10,100,250,251+}` matching Coq `reward_pin_table`, finite sum `210000 × 99450437` atoms, `max_issuance_within_cap`, honesty flag. Does not claim complete monetary correctness.

### Formal Verification

Public architecture framing, allowed claim, 422/215, coverage vs outside-boundary lists, reproduce commands, GitHub links to `verification/` and claim table. Honesty triple displayed.

---

## Verdict

**Documentation cluster PASS** for dual assets, minimal scripts, RFC-017 numbers, and FV claim boundary.

**Residual (docs/site only, not consensus):**

1. Sitemap omits Monetary Policy from Documentation children.  
2. Landing “65536 principle” unlabeled vs shipping `E = 246`.  
3. Condensed whitepaper §5 still leads with 65536 despite a correct abstract.  
4. GitHub `blob/main` URLs may trail local docs.

No website files were modified by this audit.
