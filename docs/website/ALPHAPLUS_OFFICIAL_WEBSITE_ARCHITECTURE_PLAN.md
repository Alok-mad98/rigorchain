# AlphaPlus Official Website — Architecture Plan

**Task:** `TASK-ALPHAPLUS-OFFICIAL-WEBSITE-ARCHITECTURE-AUDIT-MVP-001`  
**Domain:** https://www.rigorchain.com  
**Implementation root:** `web/rigorchain-site/`  
**Date:** 2026-08-17  
**equivalence_claimed:** `false`

Companion audit: [ALPHAPLUS_WEBSITE_CURRENT_STRUCTURE_AUDIT.md](./ALPHAPLUS_WEBSITE_CURRENT_STRUCTURE_AUDIT.md)

This plan defines the official portal information architecture. It does **not** change consensus, kernel, PoW, validation, wallet accounting, or RES mint logic.

---

## 1. Current status

The official site already exists as a React/Vite SPA deployed (or intended) at `www.rigorchain.com` via `public/CNAME`.

**What is already strong (KEEP):**

- Working **Zero-Trust Verification** portal (TXID, block hash, raw JSON, local WebCrypto).
- Read-only observer chrome (status bar, equivalence badge, no-send RPC client).
- Lab narrative (Protocol / Science / Lab / PROOF-034 status).
- Explorer as a separate viewer with outbound “Verify independently” links.
- Custom CSS design tokens suitable for visual refinement.

**What was incomplete before this task:**

- Home did not lead with “What is AlphaPlus?” (no premine, PoW, ORU, RES, Convergence, Zero Trust).
- No About page.
- Download / Wallet / Mining / Explorer / Docs were single pages without the target nested IA.
- Windows Miner ecosystem was not a first-class section.
- `ConvergencePanel` and `RESMintPanel` existed but were not mounted.
- No logos/favicon on the site.
- Specs (`web/public-site`) described `/axioms` and Tailwind; the live app did not.

**This task added:** nested placeholder routes, About, AlphaPlus home identity, Mining Windows ecosystem plan, Explorer→Verifier pipeline copy, wired unused wallet panels, copied existing logo into `public/assets/`, and bound Vite to `http://127.0.0.1:5173`.

No existing website files were deleted.

---

## 2. Recommended structure

Target tree for **www.rigorchain.com**:

```text
/
├── Home                         LandingPage — What is AlphaPlus?
│
├── Download
│   ├── Windows Wallet           PLACEHOLDER
│   ├── Linux Wallet             PLACEHOLDER
│   ├── CLI Tools                PLACEHOLDER
│   └── Miner                    PLACEHOLDER
│
├── Wallet
│   ├── Wallet Overview          KEEP
│   ├── ORU                      WIRED (ProtocolCard)
│   ├── Maturity                 PLACEHOLDER
│   ├── Convergence              WIRED (ConvergencePanel)
│   └── RES                      WIRED (RESMintPanel)
│
├── Mining
│   ├── Mining Overview          KEEP
│   ├── Windows Miner Guide      PLACEHOLDER
│   ├── Pool Dashboard           PLACEHOLDER → pool/web (later)
│   └── Mining Economics         PLACEHOLDER
│
├── Explorer
│   ├── Block Explorer           portal → explorer /blocks
│   ├── ORU Supply               portal → explorer /oru.html
│   ├── RES Statistics           portal → explorer /res.html
│   ├── Consensus Fingerprint    portal + verifier explanation
│   └── Network Status           portal → explorer /
│
├── Zero-Trust Verification  ★
│   ├── What is Zero Trust?      KEEP
│   ├── Verify Transaction       KEEP (live)
│   ├── Verify Block             KEEP (live)
│   └── Consensus Fingerprint    PLACEHOLDER
│
├── Documentation
│   ├── User Guide               PLACEHOLDER
│   ├── Developer Guide          PLACEHOLDER
│   ├── Architecture             PLACEHOLDER
│   ├── RFC Overview             PLACEHOLDER
│   └── Security                 PLACEHOLDER
│
└── About AlphaPlus              KEEP (new leaf)
```

**Secondary lab nav (KEEP, not deleted):**

```text
/protocol   /science   /lab   /status   /experiment
```

These remain the scientific publication layer. They are not the 30-second user path. Primary nav is product: Download, Wallet, Mining, Explorer, Zero-Trust, Documentation, About.

Canonical IA source in code: `web/rigorchain-site/src/ia/sitemap.ts`.

---

## 3. User journeys

### 3.1 New user path

```text
Visit website (/)
    → Understand AlphaPlus (/about and home identity cards)
    → Download Wallet (/download/windows-wallet or /download/linux-wallet)
    → Download Miner (/download/miner)
    → Start Mining (/mining/windows)
    → Observe ORU (/wallet/oru)
    → Understand Convergence (/wallet/convergence)
    → Verify independently (/zero-trust-verification)
```

Home “Start in 30 seconds” already encodes this sequence as links.

### 3.2 Developer path

```text
Documentation (/documentation)
    → Architecture (/documentation/architecture)
    → Consensus (lab /protocol + docs/rfc)
    → Zero Trust (/zero-trust-verification/what-is-zero-trust)
    → Verification (/zero-trust-verification)
```

### 3.3 Miner path

```text
Mining (/mining)
    → Windows Miner (/mining/windows)
    → Pool Dashboard (/mining/pool)
    → Wallet (/wallet)
    → Rewards (ORU observation + independent block verify)
```

---

## 4. Missing modules and fill plan

| Priority | Module | Current | Next implementation (UI only) |
|----------|--------|---------|--------------------------------|
| P0 | Home identity | Present | Copy polish only; no hype |
| P0 | Zero-Trust TX/Block | Live | Keep; add fingerprint local check later |
| P0 | Download artifacts | Placeholder | Bind checksummed release URLs when release publishes them |
| P1 | Windows Miner guide | Placeholder | Document `installer/windows/rigor-miner.cmd` + wallet address + pool |
| P1 | Explorer deep links | Placeholder | Prefill `VITE_EXPLORER_API_URL`; iframe or outbound links |
| P1 | Pool dashboard | Placeholder | Read-only link to deployed pool; do not merge Yii into SPA |
| P2 | Documentation HTML | Placeholder | Render selected `docs/` markdown at build time |
| P2 | `/axioms` | Missing | Optional alias to `/science` — do not delete Science |
| P2 | Wallet screenshots | Unused | Optional Download/Wallet gallery from `docs/wallet/evidence/` |
| P3 | Favicon / PNG set | SVG only | Derive PNG from existing Qt logo if hosts require it |

---

## 5. Core sections (Phase 5)

### 5.1 Home

**Purpose:** Explain “What is AlphaPlus?” within 30 seconds.

Must remain visible:

- No premine
- PoW
- ORU
- RES
- Convergence
- Zero-Trust Verification

Existing three-layer model and PROOF-034 language stay below the identity block (KEEP).

### 5.2 Mining

Prepare for the **Windows Miner** ecosystem:

| Slot | Route | Intent |
|------|-------|--------|
| Miner download | `/download/miner` | Artifact + checksum |
| Pool status | `/mining/pool` | Display-only |
| Mining tutorial | `/mining/windows` | Wallet → miner → pool |
| Wallet connection | `/wallet` | Receive ORU; not a consensus oracle |

Existing `getblockhash` verification copy on `/mining` is KEEP.

### 5.3 Zero-Trust Verification

Major AlphaPlus identity. Simple explanation (now on home, About, and `/what-is-zero-trust`):

> You do not need to trust a node. You can independently verify the result yourself.

| Function | Status |
|----------|--------|
| TXID verification | LIVE |
| Block verification | LIVE |
| Advanced raw block JSON | LIVE |
| Consensus fingerprint verification | PLANNED (explorer observational ID is not a verdict) |

### 5.4 Explorer integration

```text
Website (www.rigorchain.com)
    │
    ├── Explorer (viewer / indexer HTML)
    │
    └── Zero-Trust Verification (browser compute)
```

Rules:

- Portal never treats explorer `VALID` flags as consensus.
- Explorer continues to emit “Verify independently” URLs to the portal.
- Fingerprint on explorer pages is labeled observational (Draw Fingerprint).

---

## 6. Visual development environment

From the official tree:

```bash
cd ~/rigor-official-tree/web/rigorchain-site
npm install
npm run dev
```

Browser: **http://127.0.0.1:5173**

| Concern | Where to adjust |
|---------|-----------------|
| Colors | `:root` in `src/index.css` (`--bg`, `--surface`, `--text`, `--verified`, `--accent`, …) |
| Spacing | `--grid-gap`, `--section-gap`, `--nav-pad-y`, `--nav-pad-x` |
| Layout width | `--max-width` |
| Cards | `--card-radius`, `.card` |
| Navigation | `.top-nav`, `.nav-links`, `.subnav`, `.lab-nav` |
| Sections | `.section-hub`, `.identity-grid`, `.hero` |

`npm run build` then `npm run preview` → http://127.0.0.1:4173

Do not use this environment to edit `core/src/kernel/`, `core/src/consensus/`, `core/src/pow.cpp`, or `core/src/validation.cpp`.

---

## 7. UI design principles

### Trust, not marketing hype

- Verification, transparency, open tools.
- No investment language, no “guaranteed returns”, no slot-machine motion.
- Failures are visible (`—`, banners). No mock chain height.

### Simplicity

A new user should answer “How do I use it?” in 30 seconds: Understand → Download wallet → Download miner → Mine → Observe ORU → Convergence → Verify.

### Technical credibility

Developers must see RFC, consensus, fingerprint, security, and formal verification without mixing them into consumer CTAs. Lab nav and Documentation nested pages carry that load.

Voice: precise, experimental, `equivalence_claimed = false`.

---

## 8. Implementation roadmap

| Stage | Work | Consensus |
|-------|------|-----------|
| **MVP (this task)** | Audit, IA routes, placeholders, home identity, visual `npm run dev` | Unchanged |
| **UI refinement** | Spacing/color pass in `index.css`; card hierarchy; mobile nav wrap | Unchanged |
| **Release binding** | Real download URLs + SHA256; miner Windows guide from installer scripts | Unchanged |
| **Explorer bind** | Production `VITE_EXPLORER_API_URL`; deep-link cards open live viewer | Unchanged |
| **Docs bind** | Static render of selected RFC / architecture / security markdown | Unchanged |
| **Fingerprint** | Portal page that computes ConvergenceRoot locally and contrasts explorer short ID | Unchanged |

Out of scope forever for this SPA: signing, broadcasting, pool payout logic, consensus patches.

---

## 9. Restrictions (observed)

Not modified:

- `core/src/kernel/`
- `core/src/consensus/`
- `core/src/pow.cpp`
- `core/src/validation.cpp`
- wallet accounting
- RES consensus logic

Website architecture only.
