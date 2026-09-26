# Local Mainnet Simulation (TASK-LOCAL-MAINNET-SIMULATION-001)

Production-like **local** multi-node rehearsal of the frozen RIGOR product journey.
Does **not** change consensus parameters (ORU emission, RES cap, serialization, scripts).

## Topology

| Node | Role | P2P | RPC |
|------|------|-----|
| Node-0 | Miner + Wallet | 27801 | 27811 |
| Node-1 | Validator | 27802 | 27812 |
| Node-2 | Observer | 27803 | 27813 |

Runtime: `.runtime/local-mainnet-simulation/`  
Artifacts: `contrib/testing/local_mainnet_simulation/out/`  
Reports: `docs/release/local_simulation/`

## Prerequisites

```bash
cd ~/ring/core && make -j$(nproc)
# optional consensus regression (operator):
./src/test/test_bitcoin --run_test=res_proof_math_tests
./src/test/test_bitcoin --run_test=convergence_consensus_tests
```

## Scripts

```bash
./start_network.sh [--wipe]
./stop_network.sh
./mine_oru.sh [N]
./create_convergence.sh [AMOUNT]
./verify_res_mint.sh
./run_full_acceptance.sh
```

## Security invariants (enforced by harness)

- No fake balance injection
- No coinbase maturity bypass
- No manual RES creation / no `-reswalletbootstrap`
- RES only via RFC-020 body mint (`CheckResMintArithmetic` / `destroy_cost`)

## Product fixes exercised by this harness

1. **Wallet:** `createconvergence` requires an OP_RETURN RESO owner declaration.
   `WalletStrictSendDestinationOk` admits that companion script on ORU-domain sends
   (`ASSET_NONE` classification).
2. **Miner:** `CreateNewBlock` can assemble the automatic RES body mint when
   `-miningresfeeaddr` is set. That does not change ORU emission, RES cap,
   serialization, or script consensus rules.
