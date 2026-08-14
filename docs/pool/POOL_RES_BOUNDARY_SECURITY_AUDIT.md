# POOL_RES_BOUNDARY_SECURITY_AUDIT

**Task:** `TASK-POOL-ARCHITECTURE-AUDIT-001` · Phase 3  
**Date:** 2026-08-12  
**Scope:** Verify pool code against the required RES separation boundary. **Do not fix** — report only.  
**Consensus freeze:** No changes to W=32, RES mint rules, ORU issuance, PoW, block/tx format.

---

## 1. Required boundary (this task)

```text
User wallet
    → Active ORU ownership
    → User manually starts CONVERGENCE
    → 32 blocks completion
    → RES minted
```

**Pool MUST:**

- Coordinate mining  
- Pay **ORU** only  

**Pool MUST NOT:**

- Generate RES  
- Trigger Convergence  
- Allocate RES  
- Simulate RES  
- Manipulate wallet ownership of RES  

---

## 2. Verdict

| Check | Result |
|-------|--------|
| NO RES minting | **FAIL** — `mint-engine` builds 100 RES body-mint txs |
| NO Convergence triggering | **PASS** — pool does not start user W=32 Convergence / wallet CVG |
| NO RES allocation to miners | **PASS** — PPLNS is ORU-only; no miner RES balances |
| NO wallet ownership manipulation | **PASS with note** — A-5 pays declared owner; pool no longer redirects RES to itself |
| NO RES simulation | **FAIL** — body-mint merkle **placeholder** reserves a simulated mint leaf |

**Overall boundary compliance vs this task’s rule:** **FAIL**

**Important policy tension:** Prior governance (`docs/security/SECURITY_001_A5_POOL_REPOSITIONING.md`) intentionally kept **pool-built body mint** so that including a draw winner does not stall RES issuance under consensus (`bad-rigor-res-winner-mismatch`). That decision **conflicts** with this task’s stricter “pool must not generate RES” rule. This audit reports the conflict; it does not choose a side or change code.

---

## 3. Search coverage

Searched under `pool/` (Rust active stack) for: `RES`, `Convergence`, `W=32` / draw window constants, `mint`, `allocation`, `reward`, asset creation paths.

Legacy yiimp (`pool/web`, `pool/stratum` C++) is out of RIGOR ops path and was not treated as a live RES threat surface for mainnet RIGOR (still should not be pointed at `rigord`).

---

## 4. Findings (evidence)

### F-01 — Pool generates RES body mint (**VIOLATION**)

| Item | Detail |
|------|--------|
| Component | `pool/mint-engine` |
| Behavior | Constructs body mint with `vout[0] = 100 RES` (`RES_REWARD_ATOMS = 10_000`) |
| Evidence | `mint-engine/src/txbuild.rs` (`build_body_mint_tx`); `types.rs`; `engine.rs` (`mint_for_winner` / `mint_for_authorized_winner`) |
| Trigger | Stratum orchestrator on block find: `try_mint` → `mint_for_authorized_winner` (`stratum-server/src/orchestrator.rs`) |
| Destination | Winner convergence’s declared Witness-v1 program (`res_owner.rs`) — **not** pool treasury |

### F-02 — Pool simulates RES mint slot (**VIOLATION**)

| Item | Detail |
|------|--------|
| Component | `pool/block-assembler` |
| Behavior | `build_body_mint_placeholder` reserves merkle leaf before mint exists |
| Evidence | `block-assembler/src/placeholder.rs`; wired in `assembly.rs` |
| Impact | Miner work commits to a future mint leaf; without mint, block may be unsubmittable |

### F-03 — Pool authorizes / decides `mint_required` (**TRIGGER-adjacent**)

| Item | Detail |
|------|--------|
| Component | `pool/winner-engine` |
| Behavior | Draw replay sets `mint_required`; issues `MintAuthorizationToken` |
| Evidence | `winner-engine/src/draw.rs`, `authorize.rs`; `stratum-server/src/winner_service.rs` |
| Note | This is **not** user Convergence start; it is block-time mint authorization for an already-won draw |

### F-04 — ORU miner payout has no RES split (**COMPLIANT**)

| Item | Detail |
|------|--------|
| Component | `pool/stratum-server/src/payout.rs` |
| Behavior | Explicit “ORU-only PPLNS”; `MinerBalance.oru_pending` only |
| PG | `payout_rounds.coinbase_oru_atoms`, `payout_lines.oru_atoms` — no RES columns |

### F-05 — Pool does not start wallet Convergence / W=32 (**COMPLIANT**)

| Item | Detail |
|------|--------|
| Behavior | No RPC/wallet calls to begin user Convergence sessions |
| W=32 | Consensus window remains node SSOT; pool does not redefine it |
| Note | Pool **replays** draw / ConvergenceRoot fields from GBT for job assembly — observation, not user Convergence trigger |

### F-06 — Historical pool-self RES receipt mitigated (**COMPLIANT vs A-5**)

| Item | Detail |
|------|--------|
| Prior bug | Mint paid operator-configured `RIGOR_POOL_RES_PAYOUT_PROGRAM_HEX` |
| Fix | Removed; owner read from winning tx declaration (A-5) |
| Doc | `docs/security/SECURITY_001_A5_POOL_REPOSITIONING.md` |

### F-07 — Schema still models mint / RES paths (**SURFACE**)

| Tables | `winner_jobs`, `mint_authorization_tokens`, `mint_jobs`, `treasury_utxos`, `candidate_blocks.mint_required` / `body_mint_reserved` |
| Risk | Persistence assumes pool mint lifecycle; reinforces architectural coupling |

### F-08 — Share-validator mint gates (**DEFENSIVE, not minting**)

| Item | Detail |
|------|--------|
| Files | `share-validator/src/res_mint.rs`, `convergence.rs` |
| Role | Reject unauthorized / malformed mint commitments on share/block path |
| Assessment | Not a mint generator; supports current mint-coupled design |

---

## 5. Required confirmations (task checklist)

| Statement | Status |
|-----------|--------|
| Pool: NO RES minting | **FAIL** (F-01) |
| Pool: NO Convergence triggering | **PASS** (F-05) |
| Pool: NO RES allocation | **PASS** for miner PPLNS (F-04); **FAIL** for chain issuance construction (F-01) |
| Pool: NO wallet ownership manipulation | **PASS** (F-06) — pays declared owner only |

---

## 6. Consensus interaction note (not a fix)

Under current frozen consensus, a block that includes a draw winner without the required RES body mint is rejected (`bad-rigor-res-winner-mismatch`). Therefore:

- If the pool includes winners in templates and **does not** mint, those blocks fail validation.  
- If the pool excludes all convergence txs, it avoids minting but censors the draw (ordinary miner censorship power).

Any future “pool never touches RES” redesign must reconcile this **without** changing frozen consensus — e.g. external mint supplier, wallet-side mint insertion contract, or template policy that never carries winners (economic/censorship tradeoff). **Out of scope for this audit to implement.**

---

## 7. Acceptance (Phase 3)

| Criterion | Result |
|-----------|--------|
| RES boundary verified | **PASS** (verification complete) |
| Violations reported without fixing | **PASS** |
| No consensus changes | **PASS** |
