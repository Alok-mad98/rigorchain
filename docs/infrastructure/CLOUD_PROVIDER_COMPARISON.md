# Cloud Provider Comparison (Pre-Purchase)

| Field | Value |
|-------|-------|
| **Document ID** | `RIGOR-INFRA-CLOUD-PROVIDER-COMPARISON` |
| **Task** | `TASK-RELEASE-008-INFRASTRUCTURE-PREDEPLOYMENT-001` |
| **Release** | **RELEASE-008** |
| **Status** | **EVALUATION ONLY — NO PROVIDER SELECTED** |
| **`provider_decision_required`** | **true** (human) |
| **`deployment_performed`** | **false** |
| **`consensus_changed`** | **false** |

Comparative notes for selecting hosts for [PRODUCTION_TOPOLOGY.md](./PRODUCTION_TOPOLOGY.md). Prices and SKUs change; treat figures as **order-of-magnitude guidance** and re-quote at purchase time.

**No provider decision is made by this document.**

---

## 1. Evaluation criteria

| Criterion | Why it matters for RIGOR |
|-----------|--------------------------|
| **Price** | Three full nodes + services; multi-year opex |
| **Bandwidth / egress** | Continuous P2P; unexpected egress fees dominate cloud TCO |
| **Storage** | 2 TB NVMe-class for nodes; snapshot speed |
| **Geographic availability** | Region A/B/C failure domains |
| **Reliability** | SLA, historical outages, support model |
| **Backup options** | Snapshots, object storage, cross-region copy |
| **Network reputation** | ASN quality for P2P; DDoS options |
| **Operational fit** | SSH-friendly VPS vs heavy managed control planes |

---

## 2. Provider matrix (qualitative)

| Provider | Price posture | Bandwidth | Storage | Geo coverage | Reliability posture | Backup options | Fit notes |
|----------|---------------|-----------|---------|--------------|---------------------|----------------|-----------|
| **Hetzner** | Typically low €/mo for dedicated & CX/CCX | Generous included traffic on many SKUs; check fair-use | Auction dedicated NVMe; cloud volumes | Strong EU; limited US vs hyperscalers | Good hobby→prod track record; EU-centric | Snapshots, Storage Boxes | Strong candidate for cost-efficient full nodes |
| **AWS** | Higher list; discounts via RI/Savings | Egress often expensive | EBS gp3/io2; flexible | Broad global regions | High enterprise SLA / ops maturity | AMI, EBS snap, S3, Backup | Best geo + tooling; watch egress for P2P |
| **DigitalOcean** | Mid-range Droplets | Transfer pools; overage fees | Droplet disk + Volumes + Spaces | Multiple regions (fewer than AWS) | Solid for mid-size fleets | Droplet backups, Snapshots, Spaces | Simple UX; good for explorer/pool |
| **Vultr** | Competitive bare metal & cloud | Often high included bandwidth | NVMe plans common | Many cities worldwide | Generally good; verify per-city | Snapshots, Block storage | Useful for geographic diversity |
| **Other suitable** | — | — | — | — | — | — | See §3 |

---

## 3. Other suitable providers (shortlist)

| Provider | When to consider |
|----------|------------------|
| **OVHcloud / OVH** | EU dedicated, anti-DDoS options, predictable bandwidth |
| **Linode (Akamai)** | Simple VPS + good docs; regional spread |
| **Scaleway** | EU-focused alternative to Hetzner |
| **Equinix Metal / colo** | When bare-metal control and cross-connect matter |
| **Self-colo / home ASN** | Only with professional power, upstream, and physical security — not default |

---

## 4. Workload-oriented guidance (non-binding)

| Workload | Prefer when… | Caution |
|----------|--------------|---------|
| **Public full nodes** | Predictable bandwidth + NVMe + multi-region | Hyperscaler egress-only pricing without committed discount |
| **Explorer** | Easy HTTPS LB + managed DB optional | Do not expose DB; managed DB ≠ consensus |
| **Pool / stratum** | Low latency to miners; DDoS mitigation | Single-region pool is a failure domain |
| **Monitoring** | Cheap continuous disk for TSDB | Can ride smallest region; keep off public nodes |
| **Website** | GitHub Pages (out of scope for VPS) | DNS still needs registrar decision |

---

## 5. Multi-provider strategy (recommended pattern)

To match failure domains in the topology:

1. Place **Node-01 / Node-02 / Node-03** across **at least two providers** when feasible.
2. Keep **Pool** and **Explorer** on separate hosts from public seed faces.
3. Store **encrypted backups** in a third location / object store.
4. Avoid locking all DNS + all compute + all backups to one corporate failure domain.

---

## 6. Re-quote checklist (at purchase time)

- [ ] Exact SKU CPU/RAM/disk matching [SERVER_REQUIREMENTS.md](./SERVER_REQUIREMENTS.md)
- [ ] Included vs billed egress for estimated P2P GB/month
- [ ] Snapshot RPO capability vs [DISASTER_RECOVERY_PLAN.md](./DISASTER_RECOVERY_PLAN.md)
- [ ] IPv4 availability and PTR / reverse DNS policy
- [ ] DDoS / network protection tiers
- [ ] Data residency / jurisdiction constraints (human/legal)

---

## 7. Explicit non-claims

- **No vendor is selected** by RELEASE-008 predeployment.
- Comparison does not modify consensus or authorize deployment.
- Marketing SLAs are not a substitute for multi-node geographic diversity.
