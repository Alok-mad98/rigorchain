# Protocol Terminology Alignment

**Task:** `TASK-DOC-README-ARCHITECTURE-UPDATE`  
**Date:** 2026-08-05  
**Scope:** Documentation terminology only — no consensus, parameter, or economic-model changes.

This document is the normative mapping from pre–SECURITY-001-A public phrasing to the
current protocol philosophy. Active specifications and the public README follow this
table. Historical audit records retain original wording unless a clarification note was
required.

---

## 1. Core principle

| | Old term | New term |
|---|----------|----------|
| Principle | **the script is the asset** / **script-as-asset** | **the script is the compliance layer** / **script-as-compliance** |
| Meaning (old) | The `scriptPubKey` itself encodes asset identity and is treated as a complete definition of the asset. | — |
| Meaning (new) | The script defines the **validity conditions** under which an asset output may exist, transfer, and remain spendable. Asset *class* is still inferred from `scriptPubKey`; asset *legitimacy* requires UTXO commitments, script rules, economic proofs, and deterministic consensus verification. | |

**What did not change**

- No parallel asset field in `CTxOut`.
- Classification still uses Witness v0 / v1 script patterns.
- No account model, no oracle, no administrative registry.

**What the old phrase overstated**

- That parsing a script alone completely defines asset legitimacy (especially for RES mint paths).

---

## 2. ORU

| Old phrasing | New phrasing |
|--------------|--------------|
| “ORU is the proof-of-work asset.” | “ORU is the proof-of-work **security and economic coordination** asset.” |
| Implied: ORU ≈ mined coin only | Explicit roles: chain security, miner incentives, fee economy, RES participation cost, coordination layer |

ORU **is** still issued by PoW subsidy. The update expands the role description; it does
not change issuance rules.

---

## 3. RES

| Old / forbidden active phrasing | Required active phrasing |
|---------------------------------|--------------------------|
| RES mining / RES mining reward / RES block subsidy | **Forbidden** in active docs |
| RES as a second mining reward | RES is an **independent scarce resource** |
| PoW creates RES | **PoW does not create RES** |

Preferred sentence (canonical):

> PoW does not directly create RES. PoW provides the irreversible random ordering that
> allows RES allocation to remain fair and decentralized.

---

## 4. Convergence draw

| Old / informal | Current |
|----------------|---------|
| Lottery (as protocol description) | **Deterministic consensus allocation mechanism** |
| Single-block candidate set (as current design) | **32-block sliding window**, deferred by one block |
| Mining reward draw | Burn ORU → candidates → eligibility → winner → maturity / mint predicates |

**Note on historical security docs:** design reviews (e.g. A-8) use “lottery” as an
economic-analysis metaphor for weighted selection under adversarial miner incentives.
Those occurrences are **preserved** as historical analysis language, not as the public
protocol label.

---

## 5. Fairness model (public narrative)

```
Bitcoin:   PoW → block ordering + subsidy

RIGOR:     PoW → secure ordering
                 ↓
          independent RES scarcity allocation
```

RES is not another mining reward.

---

## 6. Related storage / classification terms

| Term | Status |
|------|--------|
| “no asset field” / “no parallel asset field” | **Keep** — still correct |
| “assets are parsed, not persisted” | Prefer **“classified by script, enforced by consensus”** |
| “asset identity from script alone” (CR-023 old) | Prefer **“asset class from script; legitimacy via consensus”** |
| “asset balance” as consensus truth | **Forbidden** |
| “asset field replacement” | **Forbidden** as a design direction |

---

## 7. Spec identifier renames (documentation IDs)

| Old label in specs | New label |
|--------------------|-----------|
| SI-011 “Script-as-asset” | SI-011 “Script-as-compliance” |
| CR-023 “Asset Type Derivable” / script-as-asset | CR-023 “Asset Class Derivable” / script-as-compliance |

Rule identifiers (SI-011, CR-023) are unchanged; only the human-readable titles and
purpose text were aligned.

---

## 8. Authority

When narrative docs disagree with this table, this table and `README.md` govern for
**public-facing** language. Normative byte layouts and arithmetic remain in `docs/rfc/`
and `core/src/consensus/`.
