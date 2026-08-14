# mint-engine (LEGACY — detached from pool runtime)

**Status:** Historical / audit-only crate.  
**Authority:** `docs/pool/POOL_RES_BOUNDARY_ARCHITECTURE_DECISION.md` Option A  
**Task:** `TASK-POOL-RES-BOUNDARY-IMPLEMENT-003`

This crate previously built RES body mint transactions for the Stratum orchestrator.
That production path has been **removed**. The live pool is ORU-only:

- `stratum-server` no longer depends on `mint-engine`
- `try_mint` / `mint_for_authorized_winner` are not called from the mining critical path
- RES body mint, when required, is assembled by `rigord` (`auto_res_mint` / GBT)

The source remains in-tree for audit and historical tests only. Do **not** rewire it
into pool pilot / stratum without a new ADR.

## Former responsibilities (historical)

- Consume `MintAuthorizationToken` from Winner Engine
- Build RES body mint transaction
- Insert mint at `vtx[k+1]` in candidate block assembly
- Optional `submitblock` RPC to `rigord`

## Architecture impact

| Field | Value |
|-------|-------|
| Consensus | NONE |
| Pool production path | **DETACHED** |
| equivalence_claimed | false |
