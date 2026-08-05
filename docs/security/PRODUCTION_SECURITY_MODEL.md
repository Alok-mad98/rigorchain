# RIGOR Production Security Model

| Field | Value |
|-------|-------|
| **Document ID** | `RIGOR-SEC-PRODUCTION-SECURITY-MODEL` |
| **Task** | `TASK-RELEASE-008-INFRASTRUCTURE-PREDEPLOYMENT-001` |
| **Release** | **RELEASE-008** |
| **Status** | **POLICY FROZEN (pre-purchase)** |
| **`deployment_performed`** | **false** |
| **`consensus_changed`** | **false** |

Operational security policy for production hosts. Aligns with architecture constitution: no admin consensus backdoors; script-is-asset; UTXO supremacy; independent full-node verification.

---

## 1. SSH access

| Control | Requirement |
|---------|-------------|
| **Authentication** | **Public-key only** |
| **Password login** | **Disabled** |
| **Root login** | **Disabled** (`PermitRootLogin no`) |
| **Privilege** | Named sudo users; least privilege |
| **Source** | Bastion or IP allowlist; fail2ban/equivalent optional |
| **Keys** | Hardware-backed keys preferred for operators; revoke on offboarding |

Example hardening directives (apply at deploy time, not in this task):

```
PasswordAuthentication no
KbdInteractiveAuthentication no
PermitRootLogin no
PubkeyAuthentication yes
AllowUsers rigor-op
```

---

## 2. Secrets — never store in git or public disks unencrypted

**Never commit or leave plaintext in website repos, `infra/` templates, tickets, or chat:**

| Secret class | Examples |
|--------------|----------|
| **Wallet keys** | `wallet.dat`, seed phrases, descriptors with private keys |
| **HSM keys** | HSM PINs, wrapping keys, PKCS#11 credentials |
| **API secrets** | Explorer admin tokens, provider API keys, Alertmanager webhooks with credentials |
| **Private keys** | TLS private keys, SSH private keys, stratum TLS key material |

Allowed in repo: **example** configs with placeholders (`CHANGEME`, `{{ vault_rpc_password }}`).

Runtime secret storage (post-purchase): OS user permissions, tmpfs where appropriate, secret manager / ansible-vault / age — human-operated.

---

## 3. Backup security

| Backup type | Controls |
|-------------|----------|
| **Node backup** | Encrypted snapshots; access limited to ops; verify restore |
| **Wallet backup** | Encrypted at rest; offline copies; split knowledge optional; never on public node alone |
| **Configuration backup** | No embedded secrets; use secret refs; integrity hashed |

See [DISASTER_RECOVERY_PLAN.md](../infrastructure/DISASTER_RECOVERY_PLAN.md).

---

## 4. Access roles

| Role | Capabilities | Must not |
|------|--------------|----------|
| **Operator** | SSH to assigned hosts; restart services; firewall; non-custodial node ops | Unilateral treasury spend; consensus parameter edits |
| **Developer** | Repo access; propose infra templates; staging builds | Production SSH by default; production wallet access |
| **Monitoring** | Read metrics/dashboards; receive alerts | Host shell; wallet; RPC write |
| **Emergency access** | Break-glass SSH / provider console for P1 incidents | Standing broad access; protocol “admin override” |

Emergency access must be **named humans** (RELEASE-006 GO-06), dual-control preferred, logged, and revoked after incident.

---

## 5. Host and binary integrity

1. Install only **reproducible / checksum-verified** `rigord` and related binaries.
2. Prefer dedicated non-root service users (`rigor` system user).
3. Separate public P2P nodes from custody wallets.
4. Patch OS on a defined cadence; reboot windows documented.
5. No consensus “kill switch” binary flags that violate freeze declarations.

---

## 6. Network controls

Follow [NETWORK_SECURITY_DESIGN.md](../infrastructure/NETWORK_SECURITY_DESIGN.md):

- Public: P2P `8760`, HTTPS, Stratum  
- Private: RPC, DB, SSH unrestricted denial  

---

## 7. Incident severity (security)

| Level | Example | Action |
|-------|---------|--------|
| L1 | Single service HTTPS down | Operator restore |
| L2 | Suspected SSH brute force blocked | Review allowlists |
| L3 | Confirmed host compromise | Quarantine + DR node compromise playbook |
| L4 | Key material exposure | Emergency access + wallet rotation + public disclosure process (human) |

---

## 8. Explicit non-claims

- Policy publication ≠ production hardening applied.
- No HSM purchase or key ceremony performed by this task.
- Security model forbids consensus-layer admin mint/freeze/upgrade backdoors.
