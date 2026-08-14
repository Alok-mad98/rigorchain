# CURRENT_POOL_RES_MINT_FLOW

**Task:** `TASK-POOL-RES-BOUNDARY-DECISION-002` · Phase 1  
**Date:** 2026-08-12  
**Scope:** Document the **as-implemented** RES body-mint path inside the Rust pool stack. Design only — no code changes.  
**Related:** `docs/pool/POOL_RES_BOUNDARY_SECURITY_AUDIT.md`

---

## 1. End-to-end flow (current)

```text
Miner (Stratum client)
    │  mining.subscribe / authorize
    │  mining.notify  ← job (may advertise mint_required / preview_winner)
    │  mining.submit
    ▼
stratum-server (orchestrator)
    │  share-validator (OpuHash)
    │  ORU PPLNS credit
    │  if network solution → handle_block_candidate
    ▼
block-assembler (already ran at job publish time)
    │  coinbase (ORU) + commitments
    │  body-mint PLACEHOLDER reserved in merkle tree
    ▼
winner-engine
    │  replay draw / preview_winner
    │  authorize_mint_for_block → MintAuthorizationToken
    ▼
mint-engine
    │  resolve RES owner from winning convergence declaration (A-5)
    │  build_body_mint_tx (vout[0] = 100 RES)
    │  assemble block with mint at vtx[k+1]
    ▼
submitblock → rigord
```

**Parallel node path (not pool, but relevant):**  
`rigord` `BlockAssembler::CreateNewBlock` can also insert a RES body mint via `TryCreateAutomaticResMintTx` (`core/src/node/auto_res_mint.cpp`) when the parent draw has a winner and automint is configured. Solo GBT miners may receive that mint already inside the template. The **pool Stratum path does not currently rely on that**; it rebuilds/inserts mint itself.

---

## 2. Where each RES-related step happens

| Step | Location | What happens |
|------|----------|--------------|
| **GBT ingest** | `pool/rigor-gbt/` | Poll `getblocktemplate`; carry `convergence_*` fields, txs, `coinbasevalue` (ORU) |
| **Merkle placeholder created** | `pool/block-assembler/src/placeholder.rs` → `assembly.rs` | `build_body_mint_placeholder` reserves a leaf (`reserved=true`, `body_mint_txid=None`) so miner work commits to a future mint slot |
| **Job published / template modified for miners** | `pool/block-assembler/src/publisher.rs` | Coinbase rebuilt for pool ORU payout; commitments refreshed; placeholder folded into merkle branches delivered via Stratum |
| **Winner authorization** | `pool/winner-engine/` + `stratum-server/src/winner*.rs` | Draw replay → `mint_required`; on block find → `MintAuthorizationToken` |
| **RES object created** | `pool/mint-engine/src/txbuild.rs` (`build_body_mint_tx`) | Constructs body mint tx: **100 RES** to declared Witness-v1 program |
| **Mint triggered** | `pool/stratum-server/src/orchestrator.rs` `try_mint` / `handle_block_candidate` | Calls `mint_for_authorized_winner` after share solves network target |
| **Block assembly with mint** | `pool/mint-engine/src/block.rs` | Inserts mint hex at body slot; produces `block_hex` |
| **Block submission** | `mint-engine` RPC and/or orchestrator `BlockSubmitter` | `submitblock` to `rigord` |
| **ORU miner payout** | `pool/stratum-server/src/payout.rs` | PPLNS **ORU only** — no RES balances |

---

## 3. Detailed stage notes

### 3.1 Job publish (before any share)

1. `rigor-gbt` caches a `TemplateRecord`.  
2. `JobPublisher::publish_from_template` runs assembler.  
3. Assembler always builds an ORU coinbase for the pool payout script.  
4. If mint path is active, assembler **simulates** a body-mint leaf via placeholder txid.  
5. `WinnerRuntime::register_published_job` + `preview_winner` may set `mint_required` / `preview_winner_txid` on the Stratum envelope.  
6. Miners hash headers against that merkle root (placeholder committed).

### 3.2 Share accept (non-block)

- Validate OpuHash vs share difficulty.  
- Credit ORU PPLNS weights.  
- **No mint.**

### 3.3 Block candidate

1. Confirm winner preview.  
2. `authorize_block_found` → token (or skip).  
3. `mint_for_authorized_winner`:  
   - Read owner from winning convergence raw tx (`res_owner.rs`)  
   - Build mint (`RES_REWARD_ATOMS`)  
   - Optionally `testmempoolaccept`  
   - Assemble full block including mint  
4. If mint fails and placeholder was reserved → block may be **unsubmittable**.  
5. On `submitblock` accept → finalize ORU PPLNS round.

---

## 4. What is *not* in this flow

| Non-action | Status |
|------------|--------|
| Pool starts user Convergence (W=32) | **Does not happen** |
| Pool allocates RES to Stratum workers | **Does not happen** (ORU-only ledger) |
| Pool keeps RES to operator address | **Removed by A-5** (was prior vulnerability) |
| Wallet Convergence | **Separate product path** — user burns ORU; creates draw eligibility |

---

## 5. Consensus compulsion (why this flow exists)

`ConnectBlock` rejects a block that has a draw winner without the required RES body mint (`bad-rigor-res-winner-mismatch`, `validation.cpp`).

Historical pool architecture (`docs/mining/RES_BODY_MINT_ARCHITECTURE.md`, `RIGOR_POOL_STACK_ARCHITECTURE.md`) therefore made the pool the **authoritative off-node mint builder**.

SECURITY-001-A5 kept **construction** in the pool while removing **custody**.

Current node `CreateNewBlock` can also assemble the mint (A-5 owner-correct) when automint funding + config are present — a viable alternative source of mint bytes for templates.

---

## 6. Artifact map

| Concern | Primary files |
|---------|---------------|
| Placeholder | `pool/block-assembler/src/placeholder.rs`, `assembly.rs` |
| Winner auth | `pool/winner-engine/src/{draw,authorize,service}.rs` |
| Mint build | `pool/mint-engine/src/{txbuild,engine,block,res_owner}.rs` |
| Trigger | `pool/stratum-server/src/orchestrator.rs` (`try_mint`) |
| Wiring | `pool/pilot/src/lib.rs` |
| Node alternative | `core/src/node/auto_res_mint.cpp`, `core/src/node/miner.cpp` |

---

## 7. Phase 1 acceptance

| Criterion | Result |
|-----------|--------|
| Current RES flow fully documented | **PASS** |
| No code changed | **PASS** |
