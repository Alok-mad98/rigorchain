# RIGOR Disaster Recovery Plan

| Field | Value |
|-------|-------|
| **Document ID** | `RIGOR-INFRA-DISASTER-RECOVERY-PLAN` |
| **Task** | `TASK-RELEASE-008-INFRASTRUCTURE-PREDEPLOYMENT-001` |
| **Release** | **RELEASE-008** |
| **Status** | **PLAN ONLY — NOT EXECUTED** |
| **`deployment_performed`** | **false** |
| **`consensus_changed`** | **false** |

Recovery playbooks for production failure modes. Does not authorize deployment or key custody changes.

---

## 1. Objectives

| Acronym | Definition | Production target (initial) |
|---------|------------|-----------------------------|
| **RTO** | Recovery Time Objective — max acceptable downtime | See per-scenario table |
| **RPO** | Recovery Point Objective — max acceptable data loss | See per-scenario table |

Global guidance:

- **Consensus continuity** depends on surviving peer set, not a single host backup.
- **Wallet keys** have RPO ≈ 0 for key material (offline copies); chain data RPO is hours via snapshots.
- Never “recover” by changing consensus rules.

---

## 2. Scenario playbooks

### 2.1 Server failure (single host)

| Item | Detail |
|------|--------|
| **Symptoms** | Host unreachable; provider instance dead |
| **Impact** | One of Node/Explorer/Pool/Monitor down |
| **RTO** | Node: 4–12 h · Explorer/Pool: 2–8 h · Monitor: 24 h |
| **RPO** | Node chain: last good snapshot (≤ 24 h) · Pool DB: ≤ 1 h if continuous WAL/backup |
| **Steps** | 1) Confirm failure domain 2) Launch replacement per SERVER_REQUIREMENTS 3) Restore config from config backup 4) IBD or snapshot restore for nodes 5) Re-attach monitoring 6) Update DNS only if IP changed (human-approved) |

### 2.2 Database corruption (explorer / pool)

| Item | Detail |
|------|--------|
| **Symptoms** | Indexer/pool DB checksum errors; inconsistent queries |
| **Impact** | Derived services wrong; **consensus unaffected** |
| **RTO** | 2–8 h |
| **RPO** | ≤ last verified logical backup |
| **Steps** | 1) Take host offline from public LB if serving bad data 2) Restore DB from backup to new volume 3) Rebuild index from node if needed 4) Verify tip hash vs Node-01/02/03 5) Re-enable traffic |

**Forbidden:** “Fixing” explorer mismatch by patching consensus or rewriting historical blocks.

### 2.3 Wallet loss

| Item | Detail |
|------|--------|
| **Symptoms** | Hot wallet datastore missing/corrupt; keys unavailable |
| **Impact** | Spendability for that wallet only |
| **RTO** | Depends on cold backup access (hours–days) |
| **RPO** | **Key material RPO = 0** if seed/backup exists; otherwise catastrophic for that wallet |
| **Steps** | 1) Halt automated payouts using that wallet 2) Restore from encrypted offline backup / HSM procedure 3) Verify addresses on watch-only node 4) Rotate any exposed credentials 5) Post-incident review |

**Hard rule:** Production seed backups never live only on the same failed server.

### 2.4 Node compromise

| Item | Detail |
|------|--------|
| **Symptoms** | Unauthorized SSH; unexpected processes; RPC abuse; binary tampering |
| **Impact** | Host untrusted; possible key theft if wallet present |
| **RTO** | Immediate isolation; replacement 4–24 h |
| **RPO** | Assume host keys/secrets burned |
| **Steps** | 1) Network quarantine (provider firewall) 2) Preserve volatile evidence if safe 3) **Do not** reuse disks for keys 4) Redeploy clean OS + verified `rigord` binaries 5) Rotate all credentials that touched the host 6) IBD from genesis or trusted snapshot 7) Security steward review before returning to seed list |

Compromise does **not** justify consensus parameter changes.

### 2.5 DNS failure

| Item | Detail |
|-------|--------|
| **Symptoms** | Domain resolve fails; wrong targets; registrar lock issues |
| **Impact** | Website / explorer hostname / seed DNS degraded; P2P by static peers may continue |
| **RTO** | 1–4 h for DNS; website via GitHub Pages may still work on `*.github.io` |
| **RPO** | n/a (config) |
| **Steps** | 1) Verify registrar login (MFA) 2) Check DNSSEC / records 3) Fail over to secondary DNS if configured 4) Communicate IP fallback for operators 5) Avoid hasty TTL=0 thrash |

### 2.6 Provider outage

| Item | Detail |
|------|--------|
| **Symptoms** | Entire region/vendor down |
| **Impact** | All hosts on that provider offline |
| **RTO** | Network remains up if ≥1 other-region peer survives; restore vendor hosts when provider recovers or migrate |
| **RPO** | Per backup policy |
| **Steps** | 1) Confirm multi-provider peer health 2) Announce degraded public services if needed 3) Do not rush single-vendor reconsolidation 4) After recovery, verify tip equality across nodes |

---

## 3. Backup classes

| Class | Contents | Cadence (initial) | Storage |
|-------|----------|-------------------|---------|
| **Node backup** | Chainstate snapshot / data dir snapshot; `rigor.conf` (no secrets in git) | Daily snapshot | Offline / second region |
| **Wallet backup** | Encrypted wallet / seed / descriptor backups | On create + after major change; verified restore quarterly | Air-gapped + sealed copy |
| **Configuration backup** | systemd units, nginx, firewall scripts, ansible inventory (no private keys) | On change + weekly | Private git / encrypted store |
| **Pool/Explorer DB** | Logical dumps + WAL | Hourly–daily | Encrypted object storage |

---

## 4. Recovery validation (post-purchase)

- [ ] Restore node snapshot to disposable host; compare tip after catch-up  
- [ ] Restore pool DB to staging; verify share/payout tables  
- [ ] Wallet restore drill on offline machine  
- [ ] DNS secondary failover table-top  

---

## 5. Explicit non-claims

- Plan execution awaits human approval and purchased hosts.
- DR does not modify `gate_engine`, score logic, or PROOF-034.
- Social recovery / admin keys are **out of consensus** and must not be invented as protocol backdoors.
