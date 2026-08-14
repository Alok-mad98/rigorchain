# POOL_RES_BOUNDARY_ARCHITECTURE_DECISION

**Task:** `TASK-POOL-RES-BOUNDARY-DECISION-002` · Phases 2–3  
**Date:** 2026-08-12  
**Status:** **ACCEPTED** (design decision only)  
**Supersedes (pool product scope):** the A-5 follow-up that “the pool still builds the RES mint transaction” as the *target* pool architecture  
**Does not supersede:** A-5 consensus rules R1/R2 (RES pays declared owner) — those remain frozen and correct  
**Consensus impact:** **NONE** — no W/T/RES issuance / PoW / block / tx format changes

---

## 0. Decision summary

| Field | Value |
|-------|-------|
| **Selected architecture** | **OPTION A — Pool ORU-only; no pool RES construction** |
| **RES issuance locus** | User-initiated Convergence (wallet) creates eligibility; **chain mint bytes** come from **`rigord` template assembly** (`auto_res_mint` / GBT), not from the pool |
| **Pool role** | Stratum, shares, GBT consume, ORU coinbase/PPLNS, `submitblock` of assembled **ORU mining** work |
| **Rejected as target** | Option B (keep pool mint builder), Option C (external mint provider as primary), Option D-as-mint-builder |
| **Migration complexity** | **High** (remove mint path from pool critical path; ops shift mint funding to node automint) |

---

## 1. Option analysis

### OPTION A — Remove RES generation from pool completely

**Pool keeps:** Stratum, shares, GBT, block assembly (ORU coinbase / extranonce / commitments as required for PoW jobs), ORU payout.

**Wallet / user:** Convergence → W=32 completion → eligibility; RES body mint appears only when consensus requires it and is supplied by **node template**, not pool `mint-engine`.

| | |
|--|--|
| **Pros** | Matches product rule; removes mint-engine attack surface; removes placeholder “simulate RES” coupling; clear ORU vs RES narrative; miners never paid RES by pool |
| **Cons** | Pool must **not invent** mint when GBT lacks one; operator must configure **node automint** (`-miningresfeeaddr`, fund `getautomintfundaddress`) if templates carry winners; if node mint unavailable, pool must mine **without** inventing RES (may mean templates with no winner / failed GBT until funded) |
| **Migration impact** | **High** — delete/disable `try_mint`, placeholder reservation, winner→mint authorize path from production pool; rewrite tests; update pilot/security_config; ops runbook for node automint |

**Compatibility with frozen consensus:**  
Preserved if pool submits blocks whose vtx set already satisfies ConnectBlock (mint present when winner present). Node `CreateNewBlock` already builds A-5-correct mints when configured (`auto_res_mint.cpp`).

---

### OPTION B — Keep pool mint construction; remove custody/control

This is **SECURITY-001-A5 status quo**.

| | |
|--|--|
| **Pros** | Already implemented; avoids “pool hashrate stalls RES” if node automint misconfigured; pool can insert mint even when rebuilding merkle |
| **Cons** | **Fails** product MUST-NOT (generate/trigger/simulate RES); keeps `mint-engine` + placeholder; pool remains economically adjacent to issuance; confuses “mining vs Convergence” narrative |
| **A-5 compatibility** | Fully compatible with A-5 *custody* rules; **incompatible** with this task’s product boundary |

**Verdict:** Acceptable as **historical interim**, **rejected as final architecture**.

---

### OPTION C — External RES service / provider

Pool requests mint bytes from a separate component before submit.

| | |
|--|--|
| **Security** | New trust boundary: who can withhold or delay mint? Key/UTXO custody moves but remains off-wallet |
| **Trust model** | Miners/users must trust pool **and** mint provider (or pool-operated sidecar — still “pool”) |
| **Complexity** | High (RPC, availability, versioning, HA) for little product clarity vs Option A |

**Verdict:** **Not selected** as primary. May reappear later as an *optional* sidecar only if node automint proves insufficient for multi-node pool topologies — would require a separate ADR.

---

### OPTION D — Wallet-assisted RES generation

**Question:** Can a wallet independently create Convergence after receiving ORU?

| Answer | Detail |
|--------|--------|
| **Yes — Convergence** | After receiving ORU (solo coinbase or pool payout), the user wallet can start Convergence; W=32 is a wallet/consensus path. Pool must not participate. |
| **No — as block body mint builder** | The RES **body mint transaction** is a **block-assembly** artifact required when a draw winner is present. Ordinary wallets do not insert txs into other people’s blocks. Making every winner’s wallet supply mint bytes to every pool is an availability/DoS nightmare. |

**Verdict:** **Adopt the Convergence half of D** (user-only Convergence). **Reject wallet-as-pool-mint-supplier** as the block-mint mechanism.

---

## 2. Selected architecture (normative)

```text
                    ┌─────────────────────────────┐
                    │ User wallet                 │
                    │  hold ORU → start CONVERGE  │
                    │  W=32 → draw eligibility    │
                    │  declare RES owner          │
                    └──────────────┬──────────────┘
                                   │ convergence txs (mempool)
                                   ▼
┌──────────────┐   GBT (may include     ┌──────────────────┐
│ Pool         │◄── node auto_res_mint)──│ rigord           │
│ Stratum      │                        │ CreateNewBlock   │
│ shares       │   submitblock          │ ConnectBlock     │
│ ORU coinbase │───────────────────────►│ (consensus)      │
│ ORU PPLNS    │                        └──────────────────┘
└──────────────┘
        │
        ▼
   Miner workers (hash only; ORU income via pool)
```

### Hard rules (pool)

1. **MUST NOT** construct RES body mint transactions.  
2. **MUST NOT** reserve merkle placeholders for future RES mints.  
3. **MUST NOT** run winner→mint authorization as a mint *builder* gate.  
4. **MUST NOT** allocate RES to workers or hold a RES treasury for issuance.  
5. **MUST** treat GBT non-coinbase transactions as opaque inputs for mint presence (include as provided; do not synthesize RES).  
6. **MUST** pay miners in **ORU only**.  
7. **MAY** read convergence-related GBT fields only as needed to build a **valid ORU mining job** (e.g. preserve commitments / roots required by consensus when adjusting coinbase/extranonce) — **observation, not issuance**.  
8. **MUST NOT** start, stop, or custody user Convergence.

### Hard rules (RES path)

1. User Convergence remains the **only** product action that creates RES *eligibility*.  
2. Body mint, when required, is produced by **node template assembly** (or future non-pool mechanism under a new ADR) paying the **declared owner** (A-5).  
3. Pool hashrate must not become a silent RES redirector (already forbidden by A-5).

---

## 3. Mandatory answers

### 1. Should pool ever construct RES transactions?

**No.** (Target architecture.)

### 2. Should pool know RES state?

**Minimally, read-only, and only as GBT/template necessities.**  
Know: opaque txs / commitments needed to keep jobs consensus-valid.  
Must not: maintain mint job state machines, RES balances, or mint authorization tokens for construction.

### 3. Should miner receive RES information?

**No for payouts.** Stratum must not imply RES income.  
Optional telemetry about chain draw winners may exist off the mining critical path but must not drive miner accounting. Remove `mint_required` / winner fields from miner-facing notify as product surface.

### 4. Should block template contain RES-related data?

**GBT from `rigord` may contain a body mint and convergence fields** — that is node/consensus packaging.  
**Pool-built job snapshots must not add RES mint leaves or placeholders.** Preserve node-provided mint txs if present; do not invent them.

### 5. How does Convergence interact with mined ORU?

```text
PoW / pool → ORU (electricity income)
     ↓
User receives ORU (solo or pool payout)
     ↓
User optionally starts Convergence in wallet (burns ORU)
     ↓
Draw eligibility over W=32 rules (frozen)
     ↓
If winner: node template carries body mint → RES to declared owner
```

Mining does not mint RES. Convergence does not replace mining. They meet only through ORU as the feedstock users choose to burn.

### 6. How does this preserve decentralization?

- No pool admin key over RES owners (A-5).  
- No pool RES treasury.  
- Any full node with automint funding can produce valid winner-paying templates.  
- Users retain exclusive Convergence initiation.  
- Removing pool mint-engine reduces a privileged off-chain issuer role that large pools would otherwise centralize.

---

## 4. Migration outline (implementation deferred)

| Phase | Work | Consensus? |
|-------|------|------------|
| M1 | Feature-flag / remove `try_mint` + `mint_engine` from `stratum-server` orchestrator | No |
| M2 | Stop body-mint placeholder reservation in `block-assembler` | No |
| M3 | Demote `winner-engine` mint authorize API from production path (optional read-only later) | No |
| M4 | Pilot/security_config: drop pool ORU mint-funding requirements; document node automint ops | No |
| M5 | Tests: assert pool never emits RES mint; E2E with node-provided mint in GBT | No |
| M6 | Docs: mark `RES_BODY_MINT_ARCHITECTURE` pool-builder sections historical | No |

**Rollback:** Option B code remains in git history / feature flag until soak proves Option A.

---

## 5. Follow-up implementation TASK

**`TASK-POOL-RES-BOUNDARY-IMPLEMENT-003`**

Implement Option A in the Rust pool stack per this ADR: remove RES mint construction/trigger/placeholder from the production mining path; consume node GBT mint when present; ORU-only Stratum product surface; negative tests; operator runbook for `rigord` automint. **No consensus changes.**

---

## 6. Acceptance (Phases 2–3)

| Criterion | Result |
|-----------|--------|
| Options compared | **PASS** |
| One final architecture selected | **PASS** (Option A) |
| No code changed | **PASS** |
| No consensus changed | **PASS** |
| Follow-up TASK defined | **PASS** (`TASK-POOL-RES-BOUNDARY-IMPLEMENT-003`) |
