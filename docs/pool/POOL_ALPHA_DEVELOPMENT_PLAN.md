# POOL_ALPHA_DEVELOPMENT_PLAN

**Task:** `TASK-POOL-ARCHITECTURE-AUDIT-001` · Phase 6  
**Date:** 2026-08-12  
**Inputs:**  
`POOL_EXISTING_CODE_INVENTORY.md`, `POOL_MINER_NODE_FLOW_AUDIT.md`, `POOL_RES_BOUNDARY_SECURITY_AUDIT.md`, `POOL_EXISTING_TEST_AUDIT.md`  
**Constraint:** Consensus frozen. No W=32 / RES mint rule / ORU issuance / PoW / block / tx format changes in follow-on work without a separate governance task.

---

## 1. Should existing code be extended or rewritten?

**Recommendation: Extend the Rust greenfield stack; do not revive yiimp; do not rewrite Stratum/GBT/share pipelines from scratch.**

| Component | Action |
|-----------|--------|
| `rigor-gbt`, `share-validator`, Stratum protocol, VarDiff, ORU PPLNS | **Extend / harden** — production-critical foundations |
| `rigor-miner-agent` | **Extend** — primary pool miner path |
| `rigor-pool-pg` | **Extend** — make durable storage the default for Alpha+ |
| `mint-engine` + mint placeholder + winner mint auth | **Redesign decision required first** (see §6) — not a blind delete |
| Legacy `pool/stratum` + `pool/web` | **Do not extend** — quarantine / eventual removal from ops docs |

**Rationale:** The Rust stack already implements multi-worker Stratum, OpuHash validation, GBT packaging, and ORU accounting. A full rewrite would discard ~200 passing tests and closed-testnet pilot value. The primary architectural debt is **RES coupling**, not the mining coordination core.

---

## 2. What is production-critical?

Must be correct before any public hashrate:

1. **GBT correctness** vs frozen node fields (commitments, bits, tip sync)  
2. **OpuHash share / block validation** (no false accepts)  
3. **submitblock path** with credit-only-after-accept (C-5)  
4. **ORU PPLNS integrity** (no double-pay; reorg clawback)  
5. **Worker auth** (credential lockout; no open anonymous abuse)  
6. **ORU coinbase / treasury custody** (hot wallet ops)  
7. **RES boundary enforcement** once policy is finalized (see §6)  
8. **Persistence** (shares/jobs survive restart)  
9. **Observability** (block found / submit fail / tip change metrics)

---

## 3. What can remain Alpha?

Acceptable for closed Alpha / invite-only:

- In-memory fallback when `DATABASE_URL` unset (devtest only)  
- Single-process `rigor-pool-pilot` (no HA)  
- Cleartext Stratum on localhost / VPN  
- Manual ORU payout settlement from ledger balances  
- Limited worker count / no public website  
- Devtest funding placeholders **only** under `RIGOR_POOL_PROFILE=devtest`  
- Telemetry JSONL files instead of full SRE stack  

---

## 4. What is required before a public mining pool?

Gate checklist (product, not consensus):

| Gate | Status today | Required |
|------|--------------|----------|
| M-03 public pool readiness | **FAIL** (prior audits) | PASS |
| TLS Stratum + DDoS basics | Spec only | Implemented + tested |
| Multi-miner PG soak | Partial | ≥ soak criteria |
| Automated ORU payouts | Partial | Hot/cold procedure + tested |
| RES boundary policy resolved | **Conflict** | Written decision + code match |
| Stock/compatible miner docs | Partial | Published miner matrix |
| Incident / reorg SOP | Partial docs | Operator-runbook complete |
| No yiimp confusion | Doc debt | RIGOR `pool/README` |

Until these pass: **advertise solo mining only**.

---

## 5. How does the pool interact with wallet?

**Target model (ORU mining coordination):**

```text
Pool operator wallet (ORU)
    ← coinbase payout script (pool income / fee address)
    ← optional funding UTXOs for fees (if any non-RES costs remain)
    → batch payouts to miner ORU addresses (from PPLNS ledger)

Miner wallets (ORU)
    ← receive pool ORU payouts
    → independently hold ORU
    → optionally start CONVERGENCE in their own wallet (W=32)
    → RES minted to their declared owner only via consensus rules
```

**Current Alpha reality:**

- Coinbase script via env (`RIGOR_POOL_PAYOUT_SCRIPT_HEX`)  
- PPLNS ledger tracks `oru_pending`  
- Automated `sendtoaddress` / batch payout robot **incomplete**  
- Mint path still spends ORU funding UTXO when building RES body mint (today)

**Pool must never:**

- Hold user Convergence keys  
- Start Convergence for users  
- Reassign RES ownership away from declared winners  

---

## 6. How does the pool guarantee RES separation?

### 6.1 Target guarantee (this task’s rule)

```text
Pool path:  hashpower + ORU payout only
RES path:   wallet Convergence only (user-initiated, W=32)
```

### 6.2 Current guarantee (A-5)

```text
Pool does not keep RES
Pool still constructs RES body mint to declared owner when mining winners
```

### 6.3 Required next design choice (blocking)

Pick **one** strategy **without** consensus changes:

| Option | Idea | Pros | Cons |
|--------|------|------|------|
| **A. External mint supplier** | Pool mines; a separate non-pool service inserts body mint before submit | Keeps mining ORU-only in pool binary | New component; latency/custody |
| **B. Winner-exclusion template policy** | Pool never includes convergence winners → never needs mint | Strict separation | Censors draws when pool has hashrate |
| **C. Keep A-5 mint builder** | Document exception: “build mint ≠ claim RES” | Matches today’s consensus compulsion | **Fails** this task’s MUST-NOT rule |
| **D. Wallet-assisted assemble** | Winning wallet supplies mint bytes; pool only verifies + inserts | Aligns with user ownership narrative | Protocol/UX complexity; still pool “triggers” insert |

**This audit does not select an option.** The next task must.

### 6.4 Interim Alpha controls (no code in this task)

Until redesign lands, operators should treat:

- `mint-engine` as a **known boundary exception**  
- Miner PPLNS as **ORU-only** (already true)  
- `RIGOR_POOL_PROFILE=devtest` as non-production  

---

## 7. Suggested roadmap (documentation order only)

1. **TASK-POOL-RES-BOUNDARY-DECISION-002** — Choose A/B/C/D; write ADR; freeze pool RES interface.  
2. **TASK-POOL-RES-BOUNDARY-IMPLEMENT-003** — Implement chosen option; add negative tests; update READMEs.  
3. **TASK-POOL-ORU-PAYOUT-HARDENING-004** — PG default, payout robot, reorg finality.  
4. **TASK-POOL-STRATUM-TLS-HA-005** — Execute TASK-MINING-023A.  
5. **TASK-POOL-PUBLIC-READINESS-006** — Soak + M-03 checklist closeout.

---

## 8. Answers required by Phase 6 (short form)

1. **Extend vs rewrite?** Extend Rust stack; quarantine yiimp; redesign only RES mint coupling.  
2. **Production-critical?** GBT, OpuHash validation, submitblock+credit, ORU PPLNS, auth, custody, persistence, RES policy.  
3. **Remain Alpha?** Single-process pilot, cleartext Stratum, manual payouts, memory mode, invite-only.  
4. **Before public?** TLS/HA, PG soak, automated ORU payouts, RES decision implemented, miner matrix, ops runbooks.  
5. **Wallet interaction?** ORU coinbase + miner ORU payouts; Convergence/RES only in user wallets.  
6. **RES separation?** Not guaranteed today vs task rule; requires decision task then enforcement tests.

---

## 9. Acceptance (Phase 6 + task rollup)

| Criterion | Result |
|-----------|--------|
| Development roadmap created | **PASS** |
| No implementation started | **PASS** |
| No consensus changes | **PASS** |
