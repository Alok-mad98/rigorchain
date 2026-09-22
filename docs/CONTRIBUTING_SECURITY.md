# Public Repository Documentation Rules

This repository holds protocol truth. Operator infrastructure does not belong
in GitHub.

## Three-level gate

| Level | Where | When |
|-------|--------|------|
| 1 | Local `scripts/security_scan.py` (pre-commit) | Every commit |
| 2 | `.github/workflows/security-audit.yml` | Pull requests **into `main` only**, plus `workflow_dispatch` |
| 3 | Manual review | On demand |

Pushes to feature branches do **not** run the security workflow.

## Allowed

- protocol documentation
- technical specifications
- developer guides
- consensus rules
- algorithms
- APIs
- mining / pool integration guides

## Forbidden

- server information
- credentials
- SSH keys
- tokens
- private infrastructure
- VPS / hostname / topology
- production deployment runbooks
- wallet seeds / mnemonics

## Rule

When uncertain, keep internal.

## Where things live

| Kind | Location |
|------|----------|
| Public docs | `docs/` (tracked) |
| Operator runbooks | local `ops-internal/` only (gitignored) |
| Vulnerability reports | [SECURITY.md](../SECURITY.md), not a public issue |

Do not commit files under `ops-internal/`, `ops/`, `internal/`, or `private/`.
Do not commit `.env`, `*.pem`, `*.key`, `*.secret`, or `credentials.json`.

Public docs may state **principles** (key-only SSH, RPC on localhost, no
developer keys on a deployment host). They must not name a live host, IP,
username, key path, or fingerprint.

## Local enable

```sh
# option A — pre-commit framework
pip install pre-commit
pre-commit install

# option B — git hooksPath (does not change git config for you)
git config core.hooksPath scripts/githooks
```

Or run `python3 scripts/security_scan.py` on the staged diff before `git commit`.
CI equivalent: `python3 scripts/security_scan.py --all`.
