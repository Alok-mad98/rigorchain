# RIGOR Production Network Security Design

| Field | Value |
|-------|-------|
| **Document ID** | `RIGOR-INFRA-NETWORK-SECURITY-DESIGN` |
| **Task** | `TASK-RELEASE-008-INFRASTRUCTURE-PREDEPLOYMENT-001` |
| **Release** | **RELEASE-008** |
| **Status** | **DESIGN FROZEN (pre-purchase)** |
| **`deployment_performed`** | **false** |
| **`consensus_changed`** | **false** |

Firewall and port policy for production hosts. Companion to [PRODUCTION_TOPOLOGY.md](./PRODUCTION_TOPOLOGY.md) and [PRODUCTION_SECURITY_MODEL.md](../security/PRODUCTION_SECURITY_MODEL.md).

---

## 1. Port inventory

### 1.1 P2P (public)

| Service | Port | Proto | Exposure |
|---------|------|-------|----------|
| **RIGOR mainnet P2P** | **8760** | TCP | **OPEN** to Internet on public full nodes |

Source of truth: `nDefaultPort = 8760` in `core/src/kernel/chainparams.cpp` (mainnet).  
(Bitcoin-historical **8333** is **not** the RIGOR mainnet listen port.)

Testnet / signet / regtest ports differ; production public seeds use **mainnet 8760** only unless explicitly operating another network.

### 1.2 RPC (private)

| Service | Typical port | Exposure |
|---------|--------------|----------|
| **rigord JSON-RPC** | `8761` (mainnet default RPC = P2P+1) or configured | **NOT public** — localhost, private VLAN, or VPN only |
| Cookie / user auth | n/a | Required; no anonymous RPC |

**Hard rule:** Never publish RPC on `0.0.0.0` without an authenticated private network control plane. Explorer and pool reach RPC only via private path.

### 1.3 Explorer

| Service | Port | Exposure |
|---------|------|----------|
| **HTTPS** | 443/tcp | **OPEN** (public) |
| HTTP → HTTPS redirect | 80/tcp | Optional redirect only |
| Indexer DB | vendor default | **DENY** public |
| Backend node RPC | private | **DENY** public |

### 1.4 Pool / Stratum

| Service | Port | Exposure |
|---------|------|----------|
| **Stratum (cleartext legacy)** | 3333/tcp (example) | Separated listener; prefer restrict or migrate to TLS |
| **Stratum TLS** | e.g. 3334/tcp or 443/tcp SNI | **OPEN** to miners as approved |
| Pool admin UI | if any | **DENY** public or VPN-only |
| Pool DB | private | **DENY** public |
| Node RPC used by pool | private | **DENY** public |

Stratum ports must be **separated** from P2P `8760` and from RPC.

### 1.5 Operations

| Service | Exposure |
|---------|----------|
| **SSH** | Allowlisted source IPs / bastion only — never world-open password SSH |
| **Monitoring** | Prometheus/Grafana internal or VPN; Alertmanager receivers as needed |
| **DNS management** | Registrar console MFA — not a host port |

---

## 2. Firewall policy (default deny)

### ALLOW

| Rule | Scope |
|------|-------|
| P2P `8760/tcp` | Public full nodes |
| HTTPS `443/tcp` | Explorer (and website if self-hosted; production website is GitHub Pages) |
| Stratum (approved ports) | Pool edge only |
| SSH from bastion / allowlist | All managed hosts |
| Established/related | Stateful firewalls |
| ICMP echo (optional, rate-limited) | Diagnostics |

### DENY

| Rule | Reason |
|------|--------|
| **Public RPC** | Prevent remote wallet/node control & info leak |
| **Database access from Internet** | Prevent data theft / payout tampering |
| **SSH unrestricted (`0.0.0.0/0` password)** | Credential stuffing / brute force |
| Admin panels on public interfaces | Reduce attack surface |
| Node P2P on non-node hosts | Avoid accidental exposure |

Default policy: **DROP** inbound except explicit ALLOW.

---

## 3. Host role firewall profiles

### Public full node

```
INBOUND ALLOW: 8760/tcp (world), SSH (allowlist)
INBOUND DENY:  everything else (incl. RPC)
OUTBOUND:      allow (P2P egress required)
```

### Explorer

```
INBOUND ALLOW: 80/tcp (redirect), 443/tcp, SSH (allowlist)
INBOUND DENY:  DB, node RPC, app admin
OUTBOUND:      private RPC to backend node; OS updates
```

### Pool

```
INBOUND ALLOW: stratum ports, SSH (allowlist)
INBOUND DENY:  RPC, DB, unrestricted SSH
OUTBOUND:      private RPC to mining node; payout broadcast path via node
```

### Monitoring

```
INBOUND ALLOW: SSH (allowlist); scrape targets may push or allow private scrape
INBOUND DENY:  world access to Grafana unless VPN
```

---

## 4. TLS and certificate policy

- Explorer and any public HTTP API: **TLS 1.2+** with automated renewal (e.g. ACME).
- Stratum: prefer TLS edge (see existing stratum TLS HA smoke patterns under `contrib/testing/stratum_tls_ha/` — templates only until deploy).
- Do not commit private keys or fullchain material to git.

---

## 5. Network segmentation sketch

```
Internet
   |
   +-- P2P VLAN / public NIC ---- Node-01/02/03 :8760
   |
   +-- HTTPS --------------------- Explorer :443
   |
   +-- Stratum ------------------- Pool edge
   |
   X-- (blocked) ---------------- RPC / DB / SSH-world

Private ops network (VPN)
   |
   +-- Node RPC
   +-- Pool DB
   +-- Explorer DB
   +-- Monitoring scrape
   +-- SSH bastion landing
```

---

## 6. Explicit non-claims

- No live firewall changes are performed by this task.
- Port numbers for stratum TLS final production values may be chosen at deploy time without consensus changes.
- Network policy does not grant mainnet GO or DNS cutover authority.
