# POOL_RES_REMOVAL_IMPLEMENTATION_REPORT

**Task:** `TASK-POOL-RES-BOUNDARY-IMPLEMENT-003` · Phase 1–2  
**Date:** 2026-08-12  
**ADR:** `docs/pool/POOL_RES_BOUNDARY_ARCHITECTURE_DECISION.md` — **Option A**  
**Consensus:** Unchanged (W / T / RES issuance / PoW / tx format / wallet Convergence untouched)

---

## 1. Objective

Detach all RES mint construction from the RIGOR mining pool production critical path so the pool is an **ORU-only** coordination layer.

---

## 2. Removed execution paths

| Former path | Status |
|-------------|--------|
| `StratumOrchestrator::try_mint` | **Removed** |
| `mint_engine.mint_for_authorized_winner` from orchestrator | **Removed** (crate detached) |
| `winner.authorize_block_found` on block find | **Removed** from production path |
| `mint_engine.revoke_for_jobs` on tip change | **Removed** |
| Body-mint merkle placeholder reservation (`reserved=true`) | **Disabled** — always `reserved=false` |
| Fake `placeholder_txid` merkle leaf | **Disabled** — empty; not inserted into merkle |
| Pilot `MintEngine` / ORU funding env (`RIGOR_POOL_ORU_FUNDING_*`) | **Removed** from security config |
| `stratum-server` dependency on `mint-engine` | **Removed** |
| `pilot` dependency on `mint-engine` | **Removed** |
| `stratum-server/src/res_mint.rs` | **Deleted** |

---

## 3. Target block flow (implemented)

```text
GBT (opaque txs, may include node automint)
  → shares
  → ORU coinbase / PPLNS
  → assemble_solved_block (coinbase ‖ GBT txs)
  → submitblock
```

RES-related GBT fields / txs are treated as **opaque**. The pool does not parse or modify RES mint fields for construction.

---

## 4. Remaining legacy code (audit only)

| Component | Location | Role |
|-----------|----------|------|
| `pool/mint-engine/` | full crate | Historical builder + unit/integration tests; **not** wired into stratum/pilot |
| `winner-engine` mint authorize APIs | `authorize.rs`, `service.rs` | Available for offline/replay; **not** called to build mint on block find |
| `BodyMintPlaceholder` schema fields | `block-assembler` types | Kept for JobSnapshot compatibility; always unreserved |

---

## 5. Node remains RES mint authority

`rigord` `BlockAssembler::CreateNewBlock` + `core/src/node/auto_res_mint.cpp` remain the sole production source of RES body mint bytes in templates. See `docs/pool/POOL_NODE_AUTOMINT_RUNBOOK.md`.

---

## 6. Acceptance checklist

| Criterion | Result |
|-----------|--------|
| Pool no longer contains active RES mint path | **PASS** |
| `try_mint` production path removed | **PASS** |
| mint-engine detached from pool runtime | **PASS** |
| Pool produces ORU-only mining flow | **PASS** |
| Node remains RES mint authority | **PASS** (unchanged) |
| Consensus / W / T / RES rules unchanged | **PASS** |
