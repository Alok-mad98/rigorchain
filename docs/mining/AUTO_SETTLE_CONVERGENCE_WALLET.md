# Auto-settle Convergence when mining locally

**Product:** Wallet Settings → Wallet → **Auto-settle Convergence when mining locally** (default **ON**).

## What users should know

- **ORU** is what miners earn (coinbase + network fees). Network fees are **always ORU**.
- **RES** from Convergence is a draw outcome. The prize pays the **winner**, not the miner.
- When a draw has a winner, the next block must include the automatic RES mint or the chain rejects the block. This wallet option **arms** that settlement on the local node.
- The legacy node arg `-miningresfeeaddr` is only an arming signal (and test-chain bootstrap sink). It is **not** a fee-receive address and must not be described as “RES fees”.

## Operator / headless

See [POOL_NODE_AUTOMINT_RUNBOOK.md](../pool/POOL_NODE_AUTOMINT_RUNBOOK.md). Fund `getautomintfundaddress` with ORU when a winner is present.
