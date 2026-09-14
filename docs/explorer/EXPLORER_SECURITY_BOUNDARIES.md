# Explorer Security Boundaries

**Document status:** Production documentation  
**Principle:** Explorer is **non-authoritative** for consensus and replay.

---

## 1. Explicit prohibitions

The explorer/indexer stack MUST NOT:

| ID | Forbidden behavior |
|----|-------------------|
| **F-01** | Derive consensus validity from explorer DB cache (“indexed therefore valid”) |
| **F-02** | Use **wtxid** as primary identity for convergence, convergence lineage, or URLs |
| **F-03** | Apply hidden indexing heuristics (probability inference, ML eligibility, fee guessing) |
| **F-04** | Auto-generate replay assumptions not present in RPC/witness (invented `bind_hash`, wrong `target_prev` as T) |
| **F-05** | Reimplement `RunSelection` / inequality as source of truth vs `getconvergenceresult` |
| **F-06** | Parse `rigor_target`, `res_draw_target`, or `bits` as RES inequality |
| **F-07** | Call `invalidateblock` / `reconsiderblock` from API |
| **F-08** | Hold spendable wallet keys on indexer host |
| **F-09** | Expose `rigord` RPC to `0.0.0.0/0` |
| **F-10** | Merge ORU+RES into account balances |
| **F-11** | Treat mempool as finalized RES mint |
| **F-12** | Use Taproot/BIP341 verification or labeling |
| **F-13** | Interpret pushes past PCORB index 5 as proof fields or extensions (A-7 removed the reserved slots; a trailing push is funding-input material such as the P2WSH redeem script) |
| **F-14** | Serve stale convergence winners after reorg without truncate |

---

## 2. Trust model

```
Consensus truth:  rigord validation + chainstate (NODE-C)
Explorer truth:   copy of RPC outputs at import time
User trust:       MUST verify critical claims via own node RPC
```

**Closed testnet:** Operators may trust NODE-C; **public** phase requires client-side verify for high-value audits.

---

## 3. Cache semantics

| Cache type | Allowed | Invalidation |
|------------|---------|--------------|
| Block body hex | Yes | Reorg truncate |
| `getconvergenceresult` JSON | Yes | Reorg + re-fetch |
| Derived lineage edges | Yes | Reorg truncate |
| “Eligibility score” computed locally | **No** as authority | — |

---

## 4. API hardening (closed phase)

| Control | Setting |
|---------|---------|
| Authentication | VPN/mTLS — no anonymous public write |
| Rate limit | 60 req/min/IP on API |
| CORS | Deny `*` in closed phase |
| Admin routes | None on explorer |

---

## 5. Data exposure

| Data | Exposure |
|------|----------|
| Testnet addresses | Public within VPN |
| RPC credentials | Never in UI bundle |
| `debug.log` | Ops only |

---

## 6. Incident response

| Event | Action |
|-------|--------|
| Root mismatch | Disable API; keep node running; file SEV-1 |
| Suspected indexer bug | Compare `getconvergenceresult` CLI vs DB |
| Chain split | Follow `CLOSED_TESTNET_MONITORING_PLAN` — do not pick fork in indexer |

---

## 7. Document metadata

| Field | Value |
|-------|--------|
| Risk classification | **HIGH** if F-01–F-04 violated |
| Explorer classification | **SECURITY-BOUNDARY** |
| Consensus impact | **NONE** (if boundaries held) |
| Replay impact | **INDIRECT** — violations break audit trust |
| Determinism impact | **INDIRECT** |
| Indexing risk | **CRITICAL** on heuristic indexing |
| Minimal safe recommendation | RPC-authoritative; txid keys; no hidden logic |
