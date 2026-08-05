# RIGOR Production Topology

| Field | Value |
|-------|-------|
| **Document ID** | `RIGOR-INFRA-PRODUCTION-TOPOLOGY` |
| **Task** | `TASK-RELEASE-008-INFRASTRUCTURE-PREDEPLOYMENT-001` |
| **Release** | **RELEASE-008** |
| **Baseline** | `mainnet-freeze-rfc015-2026-05-26` |
| **Status** | **DESIGN FROZEN (pre-purchase)** |
| **`deployment_performed`** | **false** |
| **`consensus_changed`** | **false** |

This document finalizes the **intended** production network topology. It does **not** purchase servers, modify DNS, deploy services, or authorize mainnet GO.

Related: [SERVER_REQUIREMENTS.md](./SERVER_REQUIREMENTS.md), [NETWORK_SECURITY_DESIGN.md](./NETWORK_SECURITY_DESIGN.md), `RELEASE-008-INFRASTRUCTURE_CHECKLIST.md`

---

## 1. Design goals

| Goal | Implementation |
|------|----------------|
| Geographic diversity | Three public full nodes in independent regions / failure domains |
| Trust minimization | Public surfaces never expose node RPC; consensus validity from independent IBD |
| UTXO supremacy | No account-balance service as asset truth source; explorer/indexer are derived views |
| Script-is-asset | Asset identity remains `scriptPubKey` parsing only; no parallel asset tags in storage |
| Failure isolation | Website / API / explorer / pool / monitoring fail independently of consensus peers |
| Scale without consensus change | Horizontal peer + service scaling only; no protocol parameter changes |

**Hard invariant:** Topology and ops design MUST NOT alter consensus, `gate_engine`, score logic, or PROOF-034 SSOT.

---

## 2. High-level architecture

```
                         Internet
                            |
                            |
                           DNS
                            |
              +-------------+-------------+
              |                           |
         Website                    API Gateway
      (GitHub Pages)               (optional edge)
              |                           |
              |                           |
              +-------------+-------------+
                            |
                      RIGOR Network
                            |
        +-------------------+-------------------+
        |                   |                   |
     Node-01             Node-02             Node-03
  Public Full Node   Public Full Node   Public Full Node
     Region A           Region B           Region C
        |                   |                   |
        +---------+---------+---------+---------+
                  |                   |
            Explorer Node        Pool Server
                  |                   |
                  +---------+---------+
                            |
                    Monitoring Server
```

---

## 3. Component responsibility

### 3.1 Edge / public presentation

| Component | Responsibility | Trust assumption |
|-----------|----------------|------------------|
| **DNS** | Resolve `rigorchain.com` and service subdomains to public endpoints | Registrar + DNS provider are operational trust; not consensus truth |
| **Website (GitHub Pages)** | Homepage, docs links, downloads, mining info | Static content only; no private keys; no consensus authority |
| **API Gateway (optional)** | Rate-limit / terminate TLS for explorer or status APIs | Must not proxy public JSON-RPC to nodes |

### 3.2 Node layer (consensus peers)

| Node | Role | Region | Notes |
|------|------|--------|-------|
| **Node-01** | Public full node | Region A | Inbound P2P; no public RPC; preferably no hot wallet |
| **Node-02** | Public full node | Region B | Independent provider / ASN preferred |
| **Node-03** | Public full node | Region C | Third failure domain; seed-capable when authorized |

**Node duties:** IBD, block/tx relay, independent script validation, UTXO set maintenance.

**Node non-duties:** Public explorer UI, stratum, website hosting, holding production treasury keys (separate custody hosts if any).

Mainnet P2P listen port (source of truth): **`8760`** (`core/src/kernel/chainparams.cpp` — `nDefaultPort`).

### 3.3 Service layer

| Service | Responsibility |
|---------|----------------|
| **Explorer Node** | Index derived chain views for UI/API; talk to a dedicated node over **private** RPC; HTTPS only publicly |
| **Pool Server** | Stratum + share validation + payout orchestration against a dedicated (non-public) node RPC |
| **Monitoring Server** | Metrics scrape, dashboards, alerting; no consensus authority |

---

## 4. Network flow

| Flow | Path | Public? |
|------|------|---------|
| Peer discovery / block relay | Internet → Node-0N:`8760`/tcp | **Yes** (P2P) |
| Explorer users | Internet → HTTPS → Explorer | **Yes** (HTTPS) |
| Miner submit | Internet → Stratum (TLS preferred) → Pool | **Yes** (Stratum only) |
| Website | Internet → GitHub Pages | **Yes** |
| Node JSON-RPC | Operator LAN / VPN / localhost only | **No** |
| Pool → node GBT/submit | Private network | **No** |
| Explorer indexer → node | Private network | **No** |
| Monitoring scrape | Private network / agent push | **No** (metrics may be internal-only) |
| Database (pool/explorer) | Localhost or private VLAN | **No** |

---

## 5. Trust boundaries

```
[ Untrusted Internet ]
        |
   TLS / P2P edge
        |
[ DMZ: nginx, stratum edge, public P2P ]
        |
   firewall / VPN
        |
[ Private ops: RPC, DB, monitoring, backups ]
        |
   air-gap / HSM policy (custody)
        |
[ Cold / offline signing — never on public nodes ]
```

| Boundary | Rule |
|----------|------|
| Internet → Node | P2P only; deny RPC, SSH-from-world unrestricted |
| Internet → Explorer | HTTPS only |
| Internet → Pool | Stratum (+ optional TLS wrapper) only |
| Service → Node RPC | Authenticated, private, non-routable from Internet |
| Ops humans → hosts | SSH key-only via bastion / allowlisted source |
| Consensus truth | Full node script validation only — never explorer DB or pool DB |

---

## 6. Failure domains

| Domain | Components | Single-domain failure impact |
|--------|------------|------------------------------|
| **FD-A** | Node-01 + Region A networking | Two remaining public peers; network continues |
| **FD-B** | Node-02 + Region B | Same |
| **FD-C** | Node-03 + Region C | Same |
| **FD-Explorer** | Explorer host + its indexer DB | Public chain view degraded; consensus unaffected |
| **FD-Pool** | Pool + stratum + pool DB | Mining UX degraded; solo mining / other pools unaffected |
| **FD-Monitor** | Monitoring stack | Blind ops briefly; consensus unaffected |
| **FD-DNS/Web** | DNS or GitHub Pages | Discovery / docs degraded; P2P by IP/seeds still possible |
| **FD-Provider** | Entire cloud vendor outage | Mitigated by multi-provider placement across Node-01/02/03 |

**Minimum production peer rule:** At least **2 of 3** geographic full nodes must remain reachable for public seed health; prefer never colocating all three on one vendor.

---

## 7. Scaling strategy

| Layer | Scale approach | Forbidden |
|-------|----------------|-----------|
| **Consensus peers** | Add more full nodes; publish additional seeds after human approval | Changing consensus params, ports as protocol identity, or economic rules |
| **Explorer** | Read replicas / CDN in front of HTTPS API; separate indexer workers | Treating indexer DB as asset truth |
| **Pool** | Horizontal stratum frontends; shared private backend; HA DB per existing M-08 patterns | Admin mint/freeze backdoors |
| **Website** | GitHub Pages + CDN (provider-native) | Embedding secrets in repo |
| **Monitoring** | Federated Prometheus / remote write | Making alerts consensus-critical |

Vertical scale (larger disks/CPU) is preferred for archival full nodes; horizontal scale for stratum and static web.

---

## 8. Explicit non-claims

- **`deployment_performed=false`** — topology is design-only.
- **`server_purchase_required=true`** — physical/cloud hosts not yet acquired under this task.
- **`mainnet_status` unchanged** — does not re-issue sovereign GO.
- **`consensus_changed=false`**
- Ownership slots in RELEASE-006 GO-05 remain human-signed separately.

---

## 9. Freeze statement

Until human approval to purchase and deploy:

1. This topology is the **default production layout**.
2. Region A/B/C provider identities may be filled later without changing component roles.
3. Any topology change after purchase must update this document and the RELEASE-008 checklist.
