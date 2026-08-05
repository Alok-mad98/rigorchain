# RIGOR Website Deployment Plan

| Field | Value |
|-------|-------|
| **Document ID** | `RIGOR-INFRA-WEBSITE-DEPLOYMENT-PLAN` |
| **Task** | `TASK-RELEASE-008-INFRASTRUCTURE-PREDEPLOYMENT-001` |
| **Release** | **RELEASE-008** |
| **Status** | **PLAN ONLY — NO DNS CUTOVER** |
| **Domain (intended)** | `rigorchain.com` |
| **Hosting** | GitHub Pages |
| **`deployment_performed`** | **false** |
| **`consensus_changed`** | **false** |

Public website plan for organization presentation. Does **not** configure live DNS or announce mainnet.

---

## 1. Domain

| Item | Value |
|------|-------|
| **Primary domain** | `rigorchain.com` |
| **Registrar** | To be selected by human (GO-04 / INFRA-06) |
| **DNS authority** | Human-approved only |

Optional future records (examples, not applied):

| Name | Type | Target |
|------|------|--------|
| `@` | A/AAAA or ALIAS | GitHub Pages per current GitHub docs |
| `www` | CNAME | org Pages host |
| `explorer` | A/AAAA/CNAME | Explorer HTTPS endpoint (post-purchase) |
| `pool` / `stratum` | A/AAAA | Pool edge (post-purchase) |
| Seed DNS | per seed policy | Only after seed authorization |

---

## 2. Hosting model

| Item | Value |
|------|-------|
| **Platform** | **GitHub Pages** |
| **Organization** | `rigorchain` (intended) |
| **Repository** | Dedicated **website** repository (e.g. `rigorchain/website`) |
| **Source** | Static site (HTML/MD generator); no server-side secrets |
| **HTTPS** | GitHub Pages managed certificates |

---

## 3. Site structure

| Path | Purpose |
|------|---------|
| `/` | Homepage — brand, one-line protocol summary, primary CTA |
| `/docs` | Protocol / architecture documentation entry (links to RFCs, ARCHITECTURE) |
| `/download` | Wallet downloads (checksums, signed manifests) |
| `/miner` | Mining overview — solo vs pool pointers, stratum endpoints when public |

Additional pages may be added without consensus changes. Avoid embedding private campaign evidence dumps.

---

## 4. Content rules

1. **No private keys, RPC passwords, or API secrets** in the website repo.
2. **No false claims** of mainnet GO, exchange listing, or Bitcoin equivalence.
3. Downloads must reference **reproducible checksums** when binaries are published.
4. Protocol wording must remain consistent with freeze baseline `mainnet-freeze-rfc015-2026-05-26`.
5. Explorer/pool deep links go live only after those services exist and GO-05 ownership is signed.

---

## 5. Custom domain configuration (procedure outline)

When human approves DNS:

1. Create `rigorchain/website` repository with Pages enabled.
2. Add custom domain `rigorchain.com` / `www` in Pages settings.
3. Publish DNS records exactly as GitHub requires at that date.
4. Enable HTTPS; wait for certificate provisioning.
5. Enforce HTTPS redirects.
6. Document TTL and change window in ops runbook.

**This task stops before step 1–6 execution.**

---

## 6. Relationship to API Gateway

Website remains static on GitHub Pages. Any future API Gateway sits on separate hosts (explorer/status) and must not be required for homepage availability.

---

## 7. Explicit non-claims

- Domain registration / DNS modification **not performed**.
- Public release announcement **not authorized**.
- Website plan does not change consensus, gate_engine, score logic, or PROOF-034.
