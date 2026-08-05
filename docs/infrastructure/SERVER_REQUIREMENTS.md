# RIGOR Production Server Requirements

| Field | Value |
|-------|-------|
| **Document ID** | `RIGOR-INFRA-SERVER-REQUIREMENTS` |
| **Task** | `TASK-RELEASE-008-INFRASTRUCTURE-PREDEPLOYMENT-001` |
| **Release** | **RELEASE-008** |
| **Status** | **SPEC FROZEN (pre-purchase)** |
| **`deployment_performed`** | **false** |
| **`consensus_changed`** | **false** |

Hardware sizing for the topology in [PRODUCTION_TOPOLOGY.md](./PRODUCTION_TOPOLOGY.md). Specs are **minimum production baselines**, not marketing peaks.

---

## 1. Summary matrix

| Role | CPU | RAM | Storage | Network |
|------|-----|-----|---------|---------|
| **Node Server** (full node ×3) | 8 cores | 32 GB | 2 TB NVMe | 1 Gbps |
| **Explorer Server** | 4–8 cores | 16–32 GB | 1 TB SSD | 1 Gbps |
| **Pool Server** | 4–8 cores | 16 GB | 500 GB–1 TB SSD | 1 Gbps |
| **Monitoring Server** | 4 cores | 8–16 GB | 200–500 GB SSD | 1 Gbps |

All figures assume Linux (Ubuntu LTS or equivalent) and exclude separate cold-custody / HSM appliances.

---

## 2. Node Server (Public Full Node)

### Minimum

| Resource | Spec | Rationale |
|----------|------|-----------|
| **CPU** | 8 cores | IBD, script validation, peer message processing under load |
| **RAM** | 32 GB | UTXO / chainstate cache, mempool, OS page cache headroom |
| **Storage** | 2 TB NVMe | Chain growth, indexes (`txindex` if enabled), prune-off archival preference for public seeds |
| **Network** | 1 Gbps | Concurrent P2P peers; block relay bursts |

### Recommended production extras

- Separate OS volume vs chain data volume
- Daily snapshot capability (provider snapshot or ZFS/btrfs send)
- No production hot wallet on public seed-facing hosts
- Dedicated private NIC or WireGuard for ops/RPC if multi-homed

### Why nodes need more hardware

Full nodes are the **consensus boundary**. They store and re-validate the chain under the script-is-asset model. Disk and RAM dominate because UTXO set + block store grow continuously; CPU spikes during IBD, reorgs, and peak mempool validation. Under-sizing nodes creates sync lag and peer churn that harms network health more than under-sizing a website.

---

## 3. Explorer Server

### Minimum / target band

| Resource | Spec | Rationale |
|----------|------|-----------|
| **CPU** | 4–8 cores | Indexer workers + API request handling |
| **RAM** | 16–32 GB | Indexer caches, DB buffer pool, API |
| **Storage** | 1 TB SSD | Derived indexes (address, tx history views); not consensus truth |
| **Network** | 1 Gbps | HTTPS traffic; private RPC to backend node |

### Notes

- Explorer may share a **dedicated** backend full node (not one of the three public seed faces) or use a private co-located node with RPC bound privately.
- Public surface is HTTPS only; DB never Internet-exposed.
- Scale RAM toward 32 GB if address-index and high QPS are required.

### Why explorer differs from nodes

Explorer workload is **read-heavy derived data**, not consensus validation for the network. It can be rebuilt from a trusted node. Latency and query concurrency matter more than multi-TB archival chain storage on the same box (chain data can live on the attached node host).

---

## 4. Pool Server

### Minimum / target band

| Resource | Spec | Rationale |
|----------|------|-----------|
| **CPU** | 4–8 cores | Stratum connections, share validation, job fan-out |
| **RAM** | 16 GB | Connection state, share pipeline, DB cache |
| **Storage** | 500 GB–1 TB SSD | Pool DB, logs, payout records |
| **Network** | 1 Gbps | Many miner TCP/TLS sessions; low latency preferred |

### Notes

- Stratum ports are **separated** from node P2P and never share a public RPC listener.
- Pool talks to a private node for GBT / submitblock paths (see M-08 / pool HA docs).
- Prefer colocating pool DB backups with tested restore drills (see disaster recovery plan).

### Why pool differs

Pool load is **connection-oriented and transactional** (shares, auth, payouts). It needs moderate CPU/RAM and reliable SSD for DB durability, but does not need a 2 TB archival chain on the stratum host itself. Network quality (latency, connection limits) often matters more than raw core count.

---

## 5. Monitoring Server

### Minimum / target band

| Resource | Spec | Rationale |
|----------|------|-----------|
| **CPU** | 4 cores | Prometheus scrape + Grafana + Alertmanager |
| **RAM** | 8–16 GB | Time-series TSDB + dashboards |
| **Storage** | 200–500 GB SSD | Retention window for metrics |
| **Network** | 1 Gbps or less OK | Internal scrape / remote-write |

### Why monitoring is smaller

Monitoring is **observability only** — it never participates in consensus. Retention and cardinality drive disk/RAM; 4 cores suffice for initial fleet size (3 nodes + explorer + pool). Scale retention storage before CPU.

---

## 6. Optional / deferred hosts

| Role | Guidance |
|------|----------|
| **Bastion / jump host** | Small VPS (2 vCPU / 2–4 GB); SSH gateway only |
| **Cold custody workstation** | Offline; never listed as production cloud SKU in this doc |
| **Backup target** | Object storage or second-region volume; size ≥ largest node snapshot |

---

## 7. Procurement notes (pre-purchase)

1. Prefer **independent regions and preferably ≥2 providers** across Node-01/02/03.
2. Confirm **egress bandwidth pricing** — full nodes transfer continuously.
3. Confirm **snapshot / backup** APIs before signing annual commits.
4. Do **not** downsize node disk below 2 TB NVMe for first public archival seeds without an explicit prune policy approved by ops.

---

## 8. Explicit non-claims

- Specs do not authorize purchase or deployment.
- No change to consensus parameters, block weight, or economic issuance.
- Hardware choices must not introduce admin keys or upgradeable consensus proxies.
