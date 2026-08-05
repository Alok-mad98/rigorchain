# RIGOR Production Monitoring Plan

| Field | Value |
|-------|-------|
| **Document ID** | `RIGOR-INFRA-MONITORING-PLAN` |
| **Task** | `TASK-RELEASE-008-INFRASTRUCTURE-PREDEPLOYMENT-001` |
| **Release** | **RELEASE-008** |
| **Status** | **DESIGN ONLY — NOT DEPLOYED** |
| **`deployment_performed`** | **false** |
| **`consensus_changed`** | **false** |

Defines metrics and a future observability stack. Companion: [PRODUCTION_TOPOLOGY.md](./PRODUCTION_TOPOLOGY.md), M-18 observability campaign evidence (historical, not production).

---

## 1. Principles

1. Monitoring is **operational**, not consensus truth.
2. Alerts must not create admin freeze/mint powers.
3. Prefer scraping private endpoints; do not expose node RPC to the Internet for metrics.
4. Explorer/pool DB health ≠ chain validity — always cross-check node tip hashes across regions.

---

## 2. Metric catalog

### 2.1 Node (`rigord`)

| Metric / signal | Purpose | Suggested alert |
|-----------------|---------|-----------------|
| **Block height** | Tip progress | Stall vs peer median > N minutes |
| **Best block hash** | Cross-node agreement | Divergence across Node-01/02/03 |
| **Peer count** | Connectivity | Peers < threshold |
| **Sync status** | IBD / headers/blocks catch-up | Not synced in production unexpectedly |
| **Mempool size** | Load / fee pressure | Informational / capacity |
| **Disk free on chain volume** | Survival | < 15% free |
| **Mining status** (if node used for GBT) | Template freshness | GBT/RPC failures |

### 2.2 Wallet / RPC availability

| Metric / signal | Purpose | Suggested alert |
|-----------------|---------|-----------------|
| **RPC availability** | Ops & service dependency | Probe fail from private network |
| **Transaction confirmation** | Wallet UX / payout path | Stuck broadcast / non-increasing confirmations |
| **Wallet unlock state** (if hot path exists) | Safety | Unexpected unlock / missing wallet process |

Production public seeds should minimize hot wallets; probes may target dedicated service nodes.

### 2.3 Pool

| Metric / signal | Purpose | Suggested alert |
|-----------------|---------|-----------------|
| **Miner connections** | Stratum health | Sudden drop |
| **Shares** (accepted/rejected) | Pipeline integrity | Reject spike |
| **Job push latency** | Miner efficiency | Sustained high latency |
| **Payout queue depth** | Economic ops | Queue growth / failed payouts |
| **Backend node RPC errors** | Dependency | Error rate |

### 2.4 System

| Metric / signal | Purpose |
|-----------------|---------|
| **CPU** | Saturation |
| **RAM** | OOM risk |
| **Disk** | Full volume / inode exhaustion |
| **Network** | Bandwidth, errors, conntrack |
| **NTP / clock skew** | Stratum & log correlation |

---

## 3. Future stack (example — not deployed)

| Component | Role |
|-----------|------|
| **Prometheus** | Pull metrics; recording rules |
| **Grafana** | Dashboards for node / pool / explorer / system |
| **Alertmanager** | Route pages to on-call |
| **Node exporter** | Host metrics |
| **Blackbox exporter** | HTTPS / TCP probes (P2P port open, explorer 443) |
| **Optional Loki / journald** | Log aggregation (PII/secrets scrubbing required) |

Skeleton directory: `infra/monitoring/` (templates only).

---

## 4. Dashboard outline (future)

1. **Network consensus** — heights + tip hashes for Node-01/02/03  
2. **Peer health** — peer counts, inbound capacity  
3. **Explorer** — HTTPS up, indexer lag vs node tip  
4. **Pool** — connections, shares, payout queue  
5. **Hosts** — CPU/RAM/disk/net  

---

## 5. Alert routing (design)

| Severity | Example | Response |
|----------|---------|----------|
| **P1** | Multi-node tip divergence; all seeds down | Emergency access procedure |
| **P2** | Single node down; explorer HTTPS down; pool stratum down | On-call restore |
| **P3** | Disk warning; elevated reject rate | Ticket within business hours |

Human ownership of paging remains RELEASE-006 GO-05 / INFRA-07.

---

## 6. Explicit non-claims

- Stack is **not** installed by this task.
- Metrics do not replace independent full-node verification for users.
- No consensus / gate_engine / score / PROOF-034 changes.
