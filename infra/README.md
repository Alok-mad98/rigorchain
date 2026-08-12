# RIGOR Infrastructure Templates (`infra/`)

| Field | Value |
|-------|-------|
| **Purpose** | Deployment automation **skeletons** + closure templates |
| **`deployment_performed`** | **false** |
| **Closure pack** | `docs/infrastructure/PRODUCTION_OPS_CLOSURE.md` |

This tree holds **examples and placeholders** for production deployment.  
**Do not** treat presence of templates as a live mainnet deploy.

## Layout

```
infra/
├── README.md
├── ansible/inventory.example
├── conf/rigord-seed.conf.example
├── docker/                   (reserved)
├── systemd/rigord.service.example
├── nginx/explorer.conf.example
├── monitoring/
│   ├── README.md
│   ├── prometheus.yml.example
│   └── rigord-alerts.yml.example
└── scripts/
    ├── server-init.sh.example
    ├── firewall.ufw.example
    └── backup-rigord.sh.example
```

## Usage

1. Copy `*.example` files; remove `.example` suffix.
2. Fill placeholders; **never** commit real secrets.
3. Apply only after human purchase + ops sign-off (`docs/release/MAINNET_GO_NO_GO_CHECKLIST.md`).
4. Keep RPC private; P2P public as designed.

## Hard constraints

- No consensus / freeze modifications via infra scripts.
- No public JSON-RPC.
- No wallet keys in inventory or unit files.
- Monitoring is operational, not consensus truth.

## Related docs

- `docs/infrastructure/PRODUCTION_OPS_CLOSURE.md`
- `docs/infrastructure/PRODUCTION_TOPOLOGY.md`
- `docs/infrastructure/NETWORK_SECURITY_DESIGN.md`
- `docs/infrastructure/MONITORING_PLAN.md`
- `docs/infrastructure/DISASTER_RECOVERY_PLAN.md`
- `docs/release/RELEASE-008-INFRASTRUCTURE_CHECKLIST.md`
