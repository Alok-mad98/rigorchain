# RIGOR monitoring templates

| Field | Value |
|-------|-------|
| **Status** | EXAMPLE ONLY — not a live deploy |
| **`deployment_performed`** | false |

## Files

| File | Role |
|------|------|
| `prometheus.yml.example` | Scrape node_exporter + blackbox (P2P) |
| `rigord-alerts.yml.example` | Tip stall / disk / peer alerts |

Wire metrics exporters on the **private** management network. Prefer probing P2P port and host metrics over exposing `rigord` RPC.

See `docs/infrastructure/MONITORING_PLAN.md` and `docs/infrastructure/PRODUCTION_OPS_CLOSURE.md`.
