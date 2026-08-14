# RIGOR_POOL_PRODUCT_POSITION

**Task:** `TASK-POOL-RES-BOUNDARY-DECISION-002` · Phase 4  
**Date:** 2026-08-12  
**Authority:** `docs/pool/POOL_RES_BOUNDARY_ARCHITECTURE_DECISION.md` (Option A)  
**Audience:** Product, ops, miners, integrators  
**Consensus:** Unchanged / frozen

---

## 1. One-line position

**The RIGOR Mining Pool is an ORU mining coordination and payout layer. It is not a RES issuer.**

---

## 2. Mining Pool **is**

| Role | Meaning |
|------|---------|
| **Infrastructure layer** | Off-consensus software between miners and `rigord` |
| **Mining coordination** | Stratum work distribution, share accounting, difficulty, job refresh from GBT |
| **ORU payout provider** | Splits coinbase ORU (and ORU fees reflected in `coinbasevalue`) to workers via pool policy (e.g. PPLNS) |
| **Block submitter** | Assembles miner solutions into blocks consistent with the template and submits to the node |

---

## 3. Mining Pool **is not**

| Anti-role | Meaning |
|-----------|---------|
| **RES issuer** | Does not create, fund-as-issuer, or allocate RES |
| **Convergence operator** | Does not start, run, or custody user Convergence / W=32 sessions |
| **Asset authority** | Does not redefine assets; classification and validity remain script + consensus |
| **RES market / treasury for miners** | Workers are not paid in RES by the pool |

---

## 4. Asset responsibility split

```text
ORU  ←── Proof of work (solo miner or pool coordination)
RES  ←── User-initiated Convergence in a wallet (eligibility)
         + consensus body mint when a draw winner exists
         (assembled by node template path — not by pool product)
```

| Actor | ORU | RES |
|-------|-----|-----|
| Miner / worker | Earns via PoW / pool payout | None from pool |
| Pool | Coordinates mining; pays ORU | No issuance role |
| User wallet | Holds/spends ORU; may burn via Convergence | Declares owner; receives mint when eligible under consensus |
| `rigord` | Validates PoW, ORU subsidy/fees | Validates Convergence + body mint rules |

---

## 5. User journey (canonical)

1. Mine (solo or pool) → receive **ORU**.  
2. Optionally, in **wallet**, start **Convergence** with ORU you control.  
3. Complete W=32 rules as defined by frozen consensus.  
4. If selected by the draw, **RES** is minted to your declared owner under consensus — **not** because the pool “granted” it.

---

## 6. Messaging constraints

**Allowed**

- “Mine ORU on the RIGOR pool.”  
- “Pool pays ORU for valid shares.”  
- “RES comes from Convergence in your wallet.”

**Forbidden**

- “Mine RES on the pool.”  
- “Pool mints / distributes RES.”  
- “Connect miner to earn Convergence rewards.”  
- Implying pool admin can freeze, redirect, or inflate RES.

---

## 7. Relationship to SECURITY-001-A5

A-5 correctly established:

- RES is not pool mining income.  
- RES must pay the winner’s declared owner.  
- Pool must not pocket RES.

This product position goes further for the **pool product**:

- Pool also must not **construct** RES mint transactions.  
- Issuance packaging for templates belongs to the **node** (or a future non-pool ADR), not Stratum.

A-5 custody rules remain in force. Pool mint construction is **retired as target architecture** (see ADR Option A).

---

## 8. Alpha vs public claims

| Claim | Alpha / closed testnet | Public pool |
|-------|------------------------|-------------|
| ORU Stratum coordination | Allowed when pilot gates pass | Requires public readiness (TLS, payouts, soak, …) |
| RES via pool | **Never** | **Never** |
| Solo mining | Always the safe public default until M-03-class gates pass | Same |

---

## 9. Implementation status

**`TASK-POOL-RES-BOUNDARY-IMPLEMENT-003` (Option A) is implemented** in the Rust pool stack:

- Stratum / pilot production path is ORU-only (no `mint-engine`, no `try_mint`).
- RES body mint bytes, when required, come only from `rigord` automint / GBT.
- `pool/mint-engine` remains as **legacy audit code** only.

See `docs/pool/POOL_RES_REMOVAL_IMPLEMENTATION_REPORT.md` and `docs/pool/POOL_NODE_AUTOMINT_RUNBOOK.md`.

---

## 10. Final statement

**RIGOR Pool is an ORU mining coordination layer.  
RES is never issued by the pool.  
RES is created only through user initiated Convergence.**

---

## 11. Phase 4 acceptance (product doc)

| Criterion | Result |
|-----------|--------|
| Product position defined | **PASS** |
| Pool is / is not tables complete | **PASS** |
| Final Option A statement present | **PASS** |
| Implementation aligned | **PASS** (`IMPLEMENT-003`) |
