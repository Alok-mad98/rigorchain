# RIGOR Product Alpha — Local Distribution

**Task:** `TASK-MAINNET-PRODUCT-ALPHA-001`  
**Scope:** Packaging + local stack only. No consensus / RFC / RES model changes.

## Components

| Product name | Binary source |
|--------------|---------------|
| `rigor-wallet` | `core/src/qt/rigor-qt` |
| `rigord` | `core/src/rigord` |
| `rigor-miner` | `cpuminer/minerd` |
| Pool (local) | `pool/pilot` (`rigor-pool-pilot`) |
| Explorer (local) | `explorer-indexer-mvp` |

## Build packages

```bash
# Prerequisites: configured+built core (Qt) and cpuminer
bash scripts/alpha/build_alpha_linux.sh
bash scripts/alpha/build_alpha_windows_nsis.sh   # needs PE staging on Windows
```

Outputs:

- `dist/alpha/linux/installers/` — `.deb`, AppImage/portable, tarball
- `dist/alpha/windows/installers/` — NSIS `.exe` when PE binaries staged

## Local stack (Node + Wallet + Miner + Pool + Explorer)

```bash
contrib/alpha/start_local_stack.sh
# stop:
contrib/alpha/stop_local_stack.sh
```

Validation:

```bash
contrib/alpha/validate_miner_local.sh
contrib/alpha/validate_pool_local.sh
contrib/alpha/run_uat_checklist.sh
```

## User journey (acceptance)

1. Install / extract alpha package  
2. Create wallet  
3. Receive ORU (mine or send)  
4. Send ORU  
5. Start mining (`rigor-miner` or stack script)  
6. Create CONVERGENCE  
7. Wait W=32  
8. Observe **RES minted** at `GetResMintReward(h)` (genesis epoch: 40.000000 RES) or verified no-mint  
9. Verify result in wallet / RPC  

Harness evidence also covered by `contrib/testing/local_mainnet_simulation/run_full_acceptance.sh`.
