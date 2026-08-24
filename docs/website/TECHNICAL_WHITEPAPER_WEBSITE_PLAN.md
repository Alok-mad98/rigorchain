# Technical Whitepaper — Website Plan

**Task:** `TASK-ALPHAPLUS-TECHNICAL-WHITEPAPER-DESIGN-AUDIT-001`  
**Domain:** `https://www.rigorchain.com`  
**Implementation root (future):** `web/rigorchain-site/`  
**Status:** Plan only — **no routes added in this task**  
**Consensus impact:** **NONE**  
**`equivalence_claimed`:** `false`

This plan recommends a **new** documentation section for the engineering-level
technical whitepaper. It does not implement pages and does not delete existing
website work.

Structure: [`../whitepaper/RIGOR_TECHNICAL_WHITEPAPER_STRUCTURE.md`](../whitepaper/RIGOR_TECHNICAL_WHITEPAPER_STRUCTURE.md)  
Identity: [`../whitepaper/RIGOR_PROTOCOL_IDENTITY.md`](../whitepaper/RIGOR_PROTOCOL_IDENTITY.md)

Protocol-paper bind (already live): [`ALPHAPLUS_WHITEPAPER_WEBSITE_INTEGRATION_REPORT.md`](./ALPHAPLUS_WHITEPAPER_WEBSITE_INTEGRATION_REPORT.md)

---

## 1. Why a second route

| Route | Document | Reader question |
|-------|----------|-----------------|
| `/documentation/whitepaper` | Protocol whitepaper | Why two assets, and how Convergence allocates RES |
| `/documentation/technical-whitepaper` | Technical whitepaper | How the Bitcoin-derived UTXO node is built |

The current whitepaper is a **protocol introduction**.

The technical whitepaper is an **engineering specification**.

Merging them on one page would bury architecture under economics, or bury the
economic argument under interpreter and PoW detail. Keep both.

Do **not** replace, redirect-away, or unmount `/documentation/whitepaper`.

---

## 2. Recommended section

```
https://www.rigorchain.com/documentation/technical-whitepaper
```

Recommended nested IA (implement later; placeholders allowed):

```
/documentation/technical-whitepaper                 Hub + abstract
    ├── /documentation/technical-whitepaper/bitcoin
    ├── /documentation/technical-whitepaper/native-assets
    ├── /documentation/technical-whitepaper/scripts
    ├── /documentation/technical-whitepaper/opuhash
    ├── /documentation/technical-whitepaper/convergence
    ├── /documentation/technical-whitepaper/fairness
    ├── /documentation/technical-whitepaper/self-proving
    ├── /documentation/technical-whitepaper/formal-verification
    ├── /documentation/technical-whitepaper/mining
    └── /documentation/technical-whitepaper/security
```

A single long-form page (matching how the protocol paper is bound today) is
acceptable for v1. Nested chapter routes are optional polish.

### 2.1 Hub disclosure (required)

```
This document is an engineering specification.
It is not the protocol introduction.
It does not claim completed formal equivalence.
equivalence_claimed = false
```

CTA row:

- Protocol Whitepaper → `/documentation/whitepaper`
- Formal Verification freeze → `/documentation/formal-verification`
- Zero Trust → `/zero-trust-verification`
- Windows Mining → `/mining` (and `/mining/windows` when that guide is filled)

---

## 3. Preserve existing website (non-negotiable)

Do not delete, redirect-away, or unmount:

| Keep | Why |
|------|-----|
| `/documentation/whitepaper` | Live protocol introduction |
| `/documentation/formal-verification` | Live freeze / correspondence page |
| Zero Trust pages | Product identity |
| Explorer portal | Viewer surface |
| Mining overview + placeholders | Windows miner IA |
| Lab (`/protocol` `/science` `/lab` `/status` `/experiment`) | Scientific layer |
| Download / Wallet / About | Product IA |

The technical whitepaper is an **addition** to the Documentation library.

---

## 4. Current Documentation library (as of this audit)

Live:

| Path | Content |
|------|---------|
| `/documentation` | Hub |
| `/documentation/whitepaper` | Protocol paper |
| `/documentation/formal-verification` | FV freeze |

Placeholders (keep; do not steal their purpose):

| Path | Purpose |
|------|---------|
| `/documentation/architecture` | Portal / three-layer IA — **not** a substitute for the technical paper |
| `/documentation/rfc` | RFC index |
| `/documentation/security` | Security boundaries |
| `/documentation/developer-guide` | Build / RPC |
| `/documentation/user-guide` | End-user how-to |

`/documentation/architecture` should **link** to the technical whitepaper when
it exists. It should not become a second unofficial spec.

---

## 5. Information architecture mapping

| Technical chapter | Website section | Must not replace |
|-------------------|-----------------|------------------|
| Abstract | Hub | Protocol abstract |
| 1 Bitcoin foundation | `/bitcoin` | Lab `/protocol` |
| 2 Native UTXO assets | `/native-assets` | Protocol Ch. 2 (economics) |
| 3 Script minimization | `/scripts` | `docs/SCRIPT_STRUCTURE.md` dump |
| 4 OpuHash | `/opuhash` | Pool/GBT operator specs |
| 5 Convergence math | `/convergence` | Protocol Convergence teaching |
| 6 Fairness | `/fairness` | Personality / Knuth copy (forbidden) |
| 7 Self-proving | `/self-proving` | Live Zero Trust app |
| 8 FV boundary | `/formal-verification` (in-paper) + outbound | Freeze page (keep canonical) |
| 9 Mining | `/mining` (in-paper) | `/mining` product pages |
| 10 Security philosophy | `/security` (in-paper) | `/documentation/security` placeholder |

Chapter 8 on the site should **summarize** the three layers and link to
`/documentation/formal-verification` as the live freeze status. Do not fork
honesty flags.

---

## 6. Navigation recommendation (future implementation)

Navbar Documentation cluster:

```
Documentation
  ├── Whitepaper                         KEEP  /documentation/whitepaper
  ├── Technical Whitepaper               ADD   /documentation/technical-whitepaper
  ├── Formal Verification                KEEP  /documentation/formal-verification
  ├── …existing placeholders…
```

Hub card copy distinction:

| Card | One-line purpose |
|------|------------------|
| Whitepaper | Protocol introduction: dual assets, Convergence, fairness |
| Technical Whitepaper | Engineering specification: UTXO identity, scripts, OpuHash, verification boundary |
| Formal Verification | Coq model + correspondence freeze. `equivalence_claimed = false` |

Footer: add Technical Whitepaper next to Whitepaper. Do not drop existing links.

Visual system: reuse black / white / gray technical theme. No second design system.

---

## 7. Terminology on the site

Public pages for this section must use:

- Native UTXO Asset
- Dual Asset Architecture
- ORU / RES
- Convergence_Root
- draw_candidate_root
- Self-Proving Blockchain Commitment
- Zero Trust Verification
- Runtime Archive

Avoid on this section (except contrast sentences): token, contract coin, colored
coin, Rune, inscription, smart contract asset.

Required contrast sentence (Chapter 2 distill):

> ORU and RES are native UTXO assets validated by node consensus. They are not
> tokens, colored coins, inscriptions, Runes, or contract-issued assets.

---

## 8. Implementation notes (later task)

When a later website task binds this route:

1. Source markdown: `docs/whitepaper/RIGOR_TECHNICAL_WHITEPAPER.md` (not yet written).
2. Until the body exists, do **not** ship an empty marketing page. Hub can wait.
3. Match the protocol paper’s long-form layout (readable, dark, math blocks).
4. Banner: engineering specification; `equivalence_claimed = false`.
5. Update `web/rigorchain-site/src/ia/sitemap.ts` with `preservation: "KEEP"` only after the page is real; until then `PLACEHOLDER` is honest.
6. No consensus, RFC, wallet, or miner code changes.

This documentation task does **not** edit `App.tsx`, `sitemap.ts`, or any site file.

---

## 9. Relationship to older website plans

| Plan | Route intent | Status |
|------|--------------|--------|
| [`../whitepaper/RIGOR_PROTOCOL_IDENTITY.md`](../whitepaper/RIGOR_PROTOCOL_IDENTITY.md) | Public identity | Live (docs) |
| This plan | `/documentation/technical-whitepaper` | Technical paper — **not routed** |
| `ALPHAPLUS_OFFICIAL_WEBSITE_ARCHITECTURE_PLAN.md` | Documentation library | Add a child; do not rebuild the tree |

An older alias `/whitepaper` must not be reused for the technical paper.

---

```
equivalence_claimed = false
```
