# Pool ORU-only — layered validation plan

Product acceptance is **not** tied to bare `devtest` alone. Use layers so we exercise a mainnet-*shaped* product path without putting an unrepaired pool on real MAINNET.

## Layers

| Layer | Environment | Purpose | Gate to next |
|-------|-------------|---------|--------------|
| **1** | Solo `regtest` (LMS product knobs: `bech32m` `-miningresfeeaddr`, automint fund, Convergence) | Pool connect → GBT → shares → `submitblock` → ORU → Convergence → node RES | Merkle/`submitblock` PASS + ORU-only boundary held |
| **2** | Read-only real mainnet observation | Confirm GBT/RPC shape & ops assumptions; **no** pool submit of real funds | Ops checklist only |
| **3** | Real mainnet pool | Live ORU coordination | Only after Layer 1 submitblock green |

**Out of scope for Layer 1–2:** changing consensus, W=32, T, RES amount, or pool mint construction.

## How to run

```bash
export CARGO_TARGET_DIR=/tmp/cursor-sandbox-cache/05199d473af6aca404e5d6709bc4f355/cargo-target
# Confirm ORU-only banner (must NOT mention ORU_FUNDING_TXID):
$CARGO_TARGET_DIR/release/rigor-pool-pilot 2>&1 | head -1

# Layer 1 (default)
bash contrib/testing/pool_oru_only_e2e/run_validation.sh

# Legacy Phase-1 binary smoke on devtest (optional)
POOL_ORU_E2E_LAYER=legacy_devtest bash contrib/testing/pool_oru_only_e2e/run_validation.sh

# Continue phases from LATEST runtime (no hardcoded stamp)
python3 contrib/testing/pool_oru_only_e2e/continue_validation.py
# or: POOL_ORU_E2E_RUNTIME=/path/to/runtime python3 .../continue_validation.py
```

## Known Layer-1 blocker

ORU-only `assemble_solved_block` still yields `bad-txnmrklroot` until merkle assembly is fixed. Layer 1 remains the correct place to re-validate after that fix — **not** real mainnet.
