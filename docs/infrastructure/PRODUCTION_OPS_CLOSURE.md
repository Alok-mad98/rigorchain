# Production Infrastructure Closure Pack

| Field | Value |
|-------|-------|
| **Task** | `TASK-MAINNET-CLOSURE-001` |
| **Date** | 2026-08-05 |
| **`deployment_performed`** | **false** — artifacts ready; humans apply on purchased hosts |
| **Consensus impact** | **NONE** |

Completes the **documentation and template** gaps for seed nodes, DNS, monitoring, backup, release binaries, and installer. Live deployment remains an ops/sovereign act (RELEASE-006 GO-02…GO-04).

---

## 1. Seed nodes

**Template:** `infra/ansible/inventory.example` + `infra/systemd/rigord.service.example`

Recommended roles (from `docs/infrastructure/PRODUCTION_TOPOLOGY.md`):

| Role | Count (min) | Notes |
|------|-------------|-------|
| Public seed | ≥2 regions | P2P only; no public RPC |
| Validator / archive | ≥1 | Optional explorer backend |
| GBT / pool backend | optional | Private RPC |

**Config skeleton** — `infra/conf/rigord-seed.conf.example` (this pack):

- `listen=1`, `port=8760` (or project P2P port)
- `rpcallowip=127.0.0.1` + `rpcbind=127.0.0.1` only
- `txindex=1` only if required by explorer (conflicts with prune)
- No wallet on public seeds

**Exit for GO:** named operators + published seed IPs/DNS in chainparams seeds or docs.

---

## 2. DNS

| Record | Purpose |
|--------|---------|
| `seed.example.org` | DNS seed / SRV or A/AAAA for seed discovery |
| `explorer.example.org` | HTTPS explorer |
| `download.example.org` | Release binaries / installer |

**Checklist:** GO-04 domain approval; TLS certs; no RPC on public A records.

See also: `docs/infrastructure/NETWORK_SECURITY_DESIGN.md`.

---

## 3. Monitoring

**Templates added:**

- `infra/monitoring/prometheus.yml.example`
- `infra/monitoring/rigord-alerts.yml.example`
- `infra/monitoring/README.md`

Metric catalog remains in `docs/infrastructure/MONITORING_PLAN.md`.  
**Exit for GO:** scrape live; tip-stall and peer-count alerts firing in a drill.

---

## 4. Backup

**Script:** `infra/scripts/backup-rigord.sh.example`

Covers: stop or use consistent copy of datadir blocks/chainstate (operator choice), age retention, offsite sync.  
**Exit for GO:** restore drill documented with date and operator name.

Companion: `docs/infrastructure/DISASTER_RECOVERY_PLAN.md`.

---

## 5. Release binaries

| Step | Artifact |
|------|----------|
| Build | Guix or documented reproducible build |
| Sign | Detached signatures + `SHA256SUMS` |
| Publish | `download.` host or GitHub Releases |
| Verify | User docs in root README / install guide |

Installer: `installer/windows/rigor_installer.nsi` + `scripts/build/build_nsis_installer.sh`.  
**Exit for GO:** signed hashes published; Guix gate or sovereign waiver recorded on checklist.

---

## 6. Hard constraints (unchanged)

- No public JSON-RPC  
- No wallet keys in ansible inventory  
- No consensus / freeze changes via infra scripts  
- Monitoring is operational, not consensus truth  

---

## Related

- [MAINNET_GO_NO_GO_CHECKLIST.md](../release/MAINNET_GO_NO_GO_CHECKLIST.md)
- [RELEASE-008-INFRASTRUCTURE_CHECKLIST.md](../release/RELEASE-008-INFRASTRUCTURE_CHECKLIST.md)
- [infra/README.md](../../infra/README.md)
