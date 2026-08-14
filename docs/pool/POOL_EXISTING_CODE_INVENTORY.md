# POOL_EXISTING_CODE_INVENTORY

**Task:** `TASK-POOL-ARCHITECTURE-AUDIT-001` · Phase 1 (+ Phase 4 capability matrix)  
**Date:** 2026-08-12  
**Scope:** Read-only discovery of mining-pool related code. No code or consensus changes.  
**Consensus freeze:** W=32 / RES mint rules / ORU issuance / PoW / block & tx format — **untouched**.

---

## 1. Executive inventory

RIGOR has **two** pool code lineages under `pool/`:

| Lineage | Path | Role for RIGOR | Status |
|---------|------|----------------|--------|
| **A. Greenfield Rust stack** | `pool/{rigor-gbt,block-assembler,share-validator,stratum-server,winner-engine,rigor-pool-pg,rigor-convergence-root,rigor-miner-agent,pilot,shared}` (+ `mint-engine` legacy only) | Authoritative RIGOR OpuHash / GBT / Stratum / PPLNS path | **Active Alpha / closed-testnet (ORU-only)** |
| **B. Legacy yiimp** | `pool/stratum` (C++), `pool/web` (PHP), `pool/sql`, `pool/bin`, `pool/blocknotify*` | Upstream multi-algo altcoin pool | **Abandoned for RIGOR** (vendor tree retained; see `ABANDONED.md`) |

Related **outside** `pool/`:

| Path | Role |
|------|------|
| `cpuminer/` | Solo GBT CPU miner (`minerd`); not Stratum-pool primary |
| `core/src/rpc/mining.cpp`, `core/src/node/miner.cpp` | Node GBT / block assembly (consensus SSOT) |
| `core/doc/mining-pool-gbt.md` | Node GBT contract for pools |
| `contrib/testing/pool_pilot/` | Closed-testnet pilot scripts |
| `contrib/alpha/validate_pool_local.sh` | Local alpha pool validation |
| `docs/mining/*` | Prior architecture / mint / readiness docs |

---

## 2. Active Rust modules (Lineage A)

### 2.1 Dependency / wiring overview

```text
rigord (JSON-RPC)
    ↑↓
rigor-gbt ──► block-assembler ──► stratum-server ──► miners (Stratum v1)
                    │                   │
                    │                   ├── share-validator
                    │                   ├── winner-engine
                    │                   ├── mint-engine  ──► submitblock
                    │                   └── payout (ORU PPLNS)
                    └── rigor-convergence-root (C root helpers)
rigor-pool-pg (optional DATABASE_URL persistence)
pilot = in-process composition binary (rigor-pool-pilot)
```

### 2.2 Module table

| Module | Path | Key files | Purpose | Status |
|--------|------|-----------|---------|--------|
| **GBT adapter** | `pool/rigor-gbt/` | `src/rpc.rs`, `gbt.rs`, `cache.rs`, `validate.rs`, `record.rs` | Poll `getblocktemplate` / tip; normalize + cache templates | **Completed** (closed-testnet) |
| **Block assembler** | `pool/block-assembler/` | `src/assembly.rs`, `coinbase.rs`, `publisher.rs`, `placeholder.rs`, `payout_script.rs` | Coinbase, commitments, merkle, job publish; **RES mint slot placeholder** | **Completed** |
| **Share validator** | `pool/share-validator/` | `src/validator.rs`, `opuhash.rs`, `header.rs`, `replay.rs`, `res_mint.rs`, `convergence.rs` | OpuHash share/block gates; replay; mint-slot integrity | **Completed** |
| **Stratum server** | `pool/stratum-server/` | `src/server.rs`, `protocol.rs`, `session.rs`, `auth.rs`, `vardiff.rs`, `orchestrator.rs`, `payout.rs`, `winner*.rs`, `res_mint.rs` | Stratum v1, workers, vardiff, PPLNS, block→mint→submit | **Completed** (pilot scope) |
| **Winner engine** | `pool/winner-engine/` | `src/draw.rs`, `authorize.rs`, `service.rs`, `candidate.rs`, `constants.rs` | Convergence draw preview + mint authorization tokens | **Completed** |
| **Mint engine** | `pool/mint-engine/` | `src/engine.rs`, `txbuild.rs`, `block.rs`, `res_owner.rs`, `submit.rs`, `rpc.rs` | **Builds RES body mint**, inserts at `vtx[k+1]`, optional `submitblock` | **Completed** (policy conflict — see RES audit) |
| **Convergence root** | `pool/rigor-convergence-root/` | `src/root.rs`, `encoding.rs` | Off-node ConvergenceRoot helpers / golden replay | **Completed** |
| **Pool PG** | `pool/rigor-pool-pg/` | `migrations/001_schema.sql`, `src/stores/*`, `backend.rs` | PostgreSQL schema + stores | **Partial** (schema ready; live soak incomplete) |
| **Pilot binary** | `pool/pilot/` | `src/main.rs`, `lib.rs`, `security_config.rs` | Wires full stack for closed testnet | **Completed** (devtest) |
| **Miner agent** | `pool/rigor-miner-agent/` | `src/agent.rs`, `stratum/*`, `pow/*` | Stratum CPU agent with OpuHash | **Partial → usable** |
| **Shared metrics** | `pool/shared/metrics/` | `src/schema.rs` | JSONL telemetry schemas | **Completed** |

### 2.3 Stratum / miner / RPC / GBT / shares / difficulty / workers / payout

| Concern | Implementation location | Status |
|---------|-------------------------|--------|
| Stratum v1 | `pool/stratum-server/` (`subscribe`, `authorize`, `notify`, `submit`, `set_difficulty`) | **Completed** |
| Miner connection | TCP accept in `server.rs`; session + extranonce in `session.rs` | **Completed** |
| RPC to node | `rigor-gbt/src/rpc.rs` (`getbestblockhash`, `getblocktemplate`); `mint-engine/src/rpc.rs` (`submitblock`, optional `testmempoolaccept`) | **Completed** |
| GBT handling | `rigor-gbt` adapter + cache + V-01.. validation | **Completed** |
| Share validation | `share-validator` 8-step pipeline (OpuHash) | **Completed** |
| Difficulty / VarDiff | `stratum-server/src/vardiff.rs`, `difficulty.rs` | **Completed** (in-memory) |
| Worker management | `auth.rs` + `workers` table (PG); in-memory credentials via env | **Partial** (pilot auth OK; production identity/HA incomplete) |
| Payout logic | `stratum-server/src/payout.rs` — **ORU-only PPLNS**; PG `payout_rounds` / `payout_lines` | **Partial** (ledger exists; hot-wallet send / public settlement incomplete) |

---

## 3. Legacy yiimp (Lineage B) — not RIGOR production path

| Area | Paths | Status for RIGOR |
|------|-------|------------------|
| C++ Stratum | `pool/stratum/*.cpp`, `algos/`, `sha3/` | **Do not use** — SHA256d/multi-algo; incompatible with OpuHash + RIGOR GBT |
| PHP web UI | `pool/web/yaamp/` | **Do not use** — MySQL yiimp economics |
| SQL dumps | `pool/sql/*.sql` | Legacy MySQL; superseded by `rigor-pool-pg` |
| blocknotify | `pool/blocknotify/`, `pool/blocknotify-dcr/` | Legacy coin daemon hooks |
| Root README | `pool/README.md` | Still yiimp install docs (**doc debt**) |

Architecture docs (`docs/mining/RIGOR_POOL_STACK_ARCHITECTURE.md`, `STRATUM_GAP_ANALYSIS.md`) explicitly exclude yiimp from the RIGOR stack.

---

## 4. Completed vs incomplete (Alpha lens)

### Completed (closed-testnet / unit-test proven)

- GBT ingest + template cache  
- Job assembly (coinbase, commitments, merkle branches)  
- Stratum v1 edge + `opushash` user-agent gate  
- OpuHash share validation + replay protection  
- VarDiff  
- Winner draw preview + mint authorization  
- RES body mint construction + block insert (see boundary FAIL)  
- ORU PPLNS in-memory accounting + reorg clawback hooks  
- Pilot binary (`rigor-pool-pilot`)  
- Large unit/integration test corpus (~223 `#[test]` attrs across crates)

### Incomplete / not production-ready

- Public Stratum TLS / HA (spec only: `TASK-MINING-023A`)  
- Production treasury / ORU hot-wallet automation  
- Durable PG soak as default runtime (often in-memory)  
- Stock `cpuminer` Stratum compatibility (`opushash` agent gate)  
- Public payout settlement UX / website (yiimp web excluded)  
- Operator RIGOR-specific `pool/README`  
- **Policy alignment:** pool MUST NOT mint RES (task rule) vs current mint-engine (conflict)

---

## 5. Phase 4 — Capability assessment

| Capability | Status | Evidence |
|------------|--------|----------|
| Solo mining only | **PASS** (via `cpuminer` + `rigord` GBT; outside pool stack) | `cpuminer/`, node GBT |
| Single miner pool | **PASS** | Pilot + Stratum authorize |
| Multi miner support | **PARTIAL** | Sessions/workers implemented; public multi-tenant ops unproven |
| Stratum protocol | **PASS** | `pool/stratum-server` Stratum v1 |
| Share accounting | **PASS** | Credits + PG `shares` schema |
| Difficulty adjustment | **PASS** | VarDiff |
| Worker accounts | **PARTIAL** | Auth + workers table; no full product IAM |
| Payment accounting | **PARTIAL** | ORU PPLNS ledger; no production `sendmany`/wallet settle |
| Block confirmation tracking | **PARTIAL** | Tip change / reorg hooks; confirmation-depth product incomplete |
| Database / storage | **PARTIAL** | PG schema present; pilot defaults to memory |

**Maturity label:** **Closed-testnet Alpha** — not public mining-pool ready.

---

## 6. Prior documentation (do not treat as this audit)

| Doc | Note |
|-----|------|
| `docs/mining/RIGOR_POOL_STACK_ARCHITECTURE.md` | Spec that **requires** pool-side RES body mint |
| `docs/mining/RIGOR_POOL_ARCHITECTURE_AUDIT.md` | TASK-MINING-003 design audit |
| `docs/mining/TASK-MINING-011-PRODUCTION-READINESS-AUDIT.md` | 2026-06 readiness |
| `docs/security/SECURITY_001_A5_POOL_REPOSITIONING.md` | Pool no longer *receives* RES; still *builds* mint |
| `docs/audit/POOL_PRODUCT_VALIDATION_REPORT.md` | Public mainnet pool = FAIL |
| `docs/release/POOL_LOCAL_VALIDATION_REPORT.md` | Local pilot PASS |

---

## 7. Acceptance (Phase 1)

| Criterion | Result |
|-----------|--------|
| Existing pool code fully identified | **PASS** |
| No code changes | **PASS** |
| No consensus changes | **PASS** |
