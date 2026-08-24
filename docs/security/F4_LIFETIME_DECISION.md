# F-4 Lifetime Decision — RES Issuance Horizon

> **Historical RFC — not an active consensus parameter.** The “210,000 draws of
> 100 RES” figure below is RFC-015-era. It is **not an active consensus parameter**.
> The F-4 decision (no mint-rate retargeting of `T`) is unchanged.

| Field | Value |
|-------|-------|
| **Task** | `TASK-MAINNET-CLOSURE-001` (closes open item from TASK-SECURITY-001-F4) |
| **Date** | 2026-08-05 |
| **Consensus parameters changed** | **false** |
| **Decision** | **100–1000 year continuous RES issuance is an ASPIRATION, not a protocol requirement** |

---

## Decision

The protocol **does not** and **will not** (without a Level-5 hard fork) add negative-feedback retargeting of `RES_PROBABILITY_TARGET` based on realized mint rate.

Therefore:

1. RES lifetime (time to exhaust 210,000 draws of 100 RES) is a **market outcome** of aggregate ORU burn into convergence.
2. Multi-century operation of the **chain** is supported by ORU tail emission (125 ORU/block forever) and UTXO validation — independent of RES mint rate.
3. After `RES_MAX_SUPPLY` is reached, RES continues as a transferable scarce asset; **no further mint**.

This resolves F-4 for product launch without redesigning RES.

---

## Rationale (summary)

- Retargeting `T` introduces new consensus inputs and grinding surface (see F4 analysis).
- Redesign is an explicit Non-Goal of SECURITY-001-A8 and of TASK-MAINNET-CLOSURE-001.
- Honest documentation beats an unverifiable “1000-year guarantee.”

---

## Public wording (approved)

> RES has a fixed maximum supply of 21,000,000. New RES enters only through the deterministic convergence draw. How quickly that supply is issued depends on network participation (ORU converged). The protocol does not schedule a calendar emission curve for RES.

---

## Related

- [TASK-SECURITY-001-F4-ISSUANCE-RATE-FEEDBACK.md](../security/TASK-SECURITY-001-F4-ISSUANCE-RATE-FEEDBACK.md)
- [RES_ECONOMIC_VALIDATION_REPORT.md](./RES_ECONOMIC_VALIDATION_REPORT.md)
