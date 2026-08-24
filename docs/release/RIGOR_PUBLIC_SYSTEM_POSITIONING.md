# RIGOR Public System Positioning

**Task:** `TASK-RELEASE-008` (System Positioning Document — Public Narrative)  
**Version:** 1.0  
**Date:** 2026-06-20  
**Type:** Public narrative — documentation only  
**Parent:** [UNIFIED_PUBLIC_NARRATIVE_SYSTEM.md](./UNIFIED_PUBLIC_NARRATIVE_SYSTEM.md)

| Field | Value |
|-------|-------|
| Consensus Impact | **NONE** |
| **`equivalence_claimed`** | **`false`** |

---

## 1. One-Sentence Positioning

> **RIGOR is a deterministic dual-asset cryptocurrency (ORU and RES) with live mining, pool, and validation infrastructure — verified through runtime archive evidence and deterministic replay, without claiming equivalence across proof, execution, and simulation domains.**

---

## 2. Elevator Narrative (30 seconds)

RIGOR implements a **frozen UTXO protocol** with two native assets. Miners find blocks via **OpuHash**. Validators apply **deterministic rules**: ORU subsidy, **convergence transactions** (ORU committed under consensus rules), and **RES minted** under a fixed cap through **deterministic consensus allocation**. The team is mining a **PROOF-034 archive** to height **2.1 million** to publish reproducible milestone evidence. **Simulation replays telemetry** to check structural consistency — it does **not** replace live execution. **Mainnet is not live today**; closed testnet is operational for operators.

---

## 3. Audience Matrix

| Audience | Lead message | Avoid |
|----------|--------------|-------|
| **Developers** | Frozen RFC spec · `rigord` · pool stack · replay tooling | “Drop-in Bitcoin fork” |
| **Researchers** | State machine · golden vectors · MATH layer summaries | “Formally verified production mainnet” |
| **Operators** | CT GO · soak PASS · PROOF-034 procedure | “Turn key mainnet” |
| **Public / press** | Dual-asset · verification phase · tier table | Investment framing |
| **Exchanges / markets** | **Not targeted** in current phase | Listing implications |

---

## 4. Competitive Framing (Permitted)

| Permitted | Prohibited |
|-----------|------------|
| “UTXO-anchored dual-asset design” | “Better than Bitcoin/Ethereum” |
| “Deterministic convergence selection” | “Same security as [other chain]” |
| “Runtime archive verification program” | “Fully audited by [implied third party]” |
| “`OP_CONVERGE` / convergence transaction mechanism” | “Burn/forge **equivalent** to [external system]” |

Product copy uses **convergence transaction** and **RES minted**. Legacy **burn/forge** wording appears only as historical narrative mapping — not as current product terminology.

---

## 5. Layer Positioning (Public)

```text
         ┌─────────────────────────────────────┐
         │  PUBLIC NARRATIVE (RELEASE-008)      │
         └──────────────────┬──────────────────┘
                            │
    Protocol ── Science ── Lab ── Runtime ── Simulation ── Release ── Cert
         │         │        │        │            │           │        │
       ORU/RES   MATH    PROOF-034  mining      SIM-011    gates    GO/NO-GO
    OP_CONVERGE stack    archive    pool                     │
       T E=246           vectors    explorer                 equivalence_claimed=false
```

---

## 6. Key Messages (Bullet)

1. **Dual native assets:** ORU + RES — not wrapped tokens.  
2. **Deterministic validation:** No RNG in consensus draw evaluation.  
3. **`OP_CONVERGE`:** Live Convergence classification (ORU destruction / draw-entry). Historical RFC-014 `CVG2` is retired design.  
4. **Live stack:** Mining and pool operate — not paper-only.  
5. **PROOF-034:** Archive @ 2,100,001 is mainnet precondition.  
6. **Simulation:** Structural replay checks — not equivalence.  
7. **Tiers:** Closed GO · Public conditional · Mainnet pending.  
8. **Invariant:** `equivalence_claimed = false` always.

---

## 7. Website & GitHub Tone

| Surface | Tone |
|---------|------|
| **www.rigorchain.com** | Scientific · precise · no hype |
| **GitHub README** | Engineering · reproducible · link to /status |
| **Release notes** | Factual · hash artifacts · tier verdict |
| **Social** | No price talk · no “moon” · link whitepaper |

Style guide: [RELEASE_LANGUAGE_STYLE_GUIDE.md](./RELEASE_LANGUAGE_STYLE_GUIDE.md).

---

## 8. Status Badge (Recommended)

```text
Verification Phase · PROOF-034 Pending · equivalence_claimed=false · Mainnet NO-GO
```

Update badge only after RELEASE-006 re-issue.

---

## 9. Post-PROOF-034 Positioning Shift

When archive completes **and** certification **GO**:

- Badge may read: `Archive Complete · Certification GO · Mainnet Preparation`
- Still **`equivalence_claimed = false`**
- Still require M-* PASS and operator sign-off before “Mainnet Live”

Never auto-upgrade to equivalence language.

---

*End of RIGOR Public System Positioning — TASK-RELEASE-008*
