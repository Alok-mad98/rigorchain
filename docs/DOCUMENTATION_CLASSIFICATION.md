# Documentation Classification Policy

RIGOR is a **public blockchain protocol** repository. GitHub holds protocol
truth, not operator infrastructure.

When uncertain, keep the document internal.

## Public (`docs/` and other tracked trees)

Allowed. Describe **what** the protocol does, **why** the design exists, and
**how** developers integrate.

| Class | Examples |
|-------|----------|
| Protocol | Whitepaper, consensus rules, OpuHash / RGRPOW01 |
| Integration | Mining guides, pool integration, RPC / API |
| Developer | Architecture, contributing, test vectors |
| Security principles | Threat model, “never expose RPC to the internet” |

Public text may mention loopback addresses (`127.0.0.1`), published P2P/RPC
**port numbers**, and placeholder credentials (`<GENERATE_AT_DEPLOY>`).

## Internal (never GitHub)

Forbidden in this repository. Keep under a local `ops-internal/` tree (or
equivalent outside git).

| Class | Examples |
|-------|----------|
| Servers | VPS provider, public IP, hostname, node topology, internal network |
| Access | SSH config, usernames, login methods, keys, certificates, tokens |
| Deployment | Production/release/backup host steps and inventories |
| Security ops | Live firewall rules, allowlists, internal ports, monitoring URLs |
| CI/CD secrets | Deploy tokens, signing keys, private credentials |

Root directories that must stay untracked: `ops-internal/`, `ops/`,
`internal/`, `private/`.

See [CONTRIBUTING_SECURITY.md](CONTRIBUTING_SECURITY.md).
