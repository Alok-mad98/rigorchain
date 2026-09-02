# RIGOR Node Operations Baseline (PHASE C)

**Task:** `TASK-RIGOR-NODE-OPERATIONS-BASELINE-PHASE-C`  
**Date:** 2026-08-31  
**Nature:** Production operations baseline for a RIGOR mainnet node. This is
engineering fail-closed policy for deploy, start, run, monitor, upgrade, and
recover. It is not a consensus proof and not a second state manager.

This TASK does not modify Consensus Kernel, protocol parameters, issuance,
RES, ORU, PoW, or ASERT. It does not introduce wallet logic, a second UTXO
store, or a copy of consensus formulas. Mainnet is not live: there is no
historical compatibility layer and no automatic chainstate migration.

```
operator config / datadir / RPC / health snapshot
        ↓
NodeOperationsBaseline::Evaluate*     (fail-closed operations firewall)
        ↓
PASS  → node may continue (consensus path unchanged)
FAIL_CLOSED → process exits or RPC is refused
```

```
NODE_CONFIGURATION_SECURITY = PASS
DATA_DIRECTORY_SAFETY = PASS
RPC_SECURITY = PASS
LOGGING_AUDITABILITY = PASS
HEALTH_MONITORING = PASS
UPGRADE_RECOVERY_BASELINE = PASS
COQ_ADMITTED = 0
COQ_AXIOMS = 0
CONSENSUS_KERNEL_MODIFIED = NO
PROTOCOL_PARAMETERS_MODIFIED = NO

NODE_OPERATIONS_BASELINE_PHASE_C = CLOSED
```

Reproduce: `python3 tools/check-node-operations-baseline.py`

Out of scope (other TASKs):

| Topic | Owner |
|-------|--------|
| P2P protocol security | PHASE A |
| Wallet security | PHASE B |
| Economic model | consensus / issuance docs |
| Performance | not this baseline |
| Consensus rules | Consensus Kernel |

---

## 1. Node configuration management

Directory: `core/src/node/operations/`

| Type | Role |
|------|------|
| `NodeConfigSnapshot` | Pure config view (network, datadir, logdir, RPC, P2P, debug flags). Not `ArgsManager`. |
| `EvaluateConfig` | Fail-closed load/validate. Illegal configuration refuses startup. |
| `FillNodeConfigSnapshot` | Production adapter in `node_operations_hooks.cpp`. |

Network isolation:

* `main` / `test` / `regtest` / `devtest` are distinct (`ParseNetwork`).
* More than one of `-testnet`, `-regtest`, `-chain` is `FAIL_CLOSED`.
* Unknown `-chain` is `FAIL_CLOSED`.
* Datadir markers (`ops-network`) must match the selected network.

Data / log directories:

* `-datadir` must be non-empty.
* Logs default under the net-specific datadir (`debug.log`).
* Operations markers: `ops-network`, `ops-data-version` (integer `1`).

RPC configuration security:

* Daemon still enables local JSON-RPC (`-server` soft-default) so miners and
  `rigor-cli` can operate. That is **authenticated localhost**, not an open RPC
  port.
* Default bind remains loopback (`127.0.0.1` / `::1`) unless both `-rpcbind`
  and `-rpcallowip` are set.
* Authentication is mandatory when `-server` is on: cookie, `-rpcauth`, or
  `-rpcuser`/`-rpcpassword`. Missing auth → `FAIL_CLOSED`.
* Mainnet rejects well-known secrets (`user`/`pass`, `password`, …).
* Mainnet rejects `rpcallowip=0.0.0.0/0` (and `::/0`).

P2P configuration checks:

* `-maxconnections` must be non-negative.
* Listen/bind consistency remains the existing init checks.

Default security parameters (mainnet):

| Parameter | Default / policy |
|-----------|------------------|
| RPC bind | localhost |
| RPC auth | cookie if no password |
| REST | off; `-rest` on mainnet is `FAIL_CLOSED` |
| `-mocktime` | forbidden |
| `-acceptnonstdtxn` | forbidden |
| `-assumevalid` override | forbidden |
| `-minimumchainwork` override | forbidden |
| `-blockversion` | forbidden |
| `-nocheckpoints` | forbidden |

Configuration must not override consensus parameters. Fail-closed strings are
literal in `EvaluateConfig`; they do not rewrite Kernel values.

---

## 2. Data directory and state safety

`InspectDataDirectory` looks at the filesystem only (LevelDB `CURRENT` files).
It does not open a second coins view and does not take the datadir lock.

| Check | Fail-closed reason |
|-------|--------------------|
| Missing / unwritable datadir | data directory does not exist / not writable |
| `.lock` held by another process | data directory lock conflict (init `LockDirectory` + snapshot flag) |
| `chainstate/` present without `CURRENT` | chainstate data directory is corrupt |
| `blocks/index/` present without `CURRENT` | block index is corrupt |
| `ops-network` ≠ selected network | network type mismatch |
| `ops-data-version` ≠ `NODE_OPS_DATA_VERSION` | data version mismatch; no automatic migration |

Fresh nodes (no chainstate, no markers) pass. After a successful start the
node writes `ops-network` and `ops-data-version`. It never rewrites chainstate
to “fix” a mismatch.

Production hook: `RunNodeOperationsDataDirectoryChecks` in `AppInitMain`
after logging starts and after the datadir lock is held.

---

## 3. Logs and auditability

Required operational events (category in `FormatOpsLog`):

| Event | When |
|-------|------|
| `startup` | Config check passed; datadir markers verified |
| `network` | Connection / listen path (existing `net` logs) |
| `sync` | Block / header progress (existing validation logs) |
| `peer` | Peer connect / disconnect / misbehavior (existing `net` logs) |
| `consensus_reject` | Existing `state.ToString()` reject reasons (auditable codes, not signatures) |
| `fatal` | Init / shutdown errors |

Redaction (`SanitizeLogLine`):

* `rpcpassword=`, `rpcuser=`, `rpcauth=`
* `privkey=`, `private_key=`, `wif=`, `seed=`
* WIF / `xprv` material
* ≥128-char hex blobs (complete signature / key material)
* `scriptSig=` / `signature=` / `witness=` values

Production operations logs go through `FormatOpsLog`. Wallet private-key
dumping remains forbidden by the PHASE B wallet lint.

---

## 4. RPC operations security

Gate: `CRPCTable::execute` calls `EvaluateRpcMethod(network, method)`.

| Class | Examples | Mainnet |
|-------|----------|---------|
| `SAFE_READ` | `getblockchaininfo`, `getnodehealth` | allowed |
| `SAFE_WRITE` | `sendrawtransaction` | allowed (still enters validation) |
| `MINING_SUBMIT` | `submitblock`, `getblocktemplate` | allowed (still enters `ProcessNewBlock`) |
| `CONSENSUS_MUTATION` | `invalidateblock`, `reconsiderblock`, `preciousblock` | **closed** |
| `BLOCK_INJECT` | `generate`, `generatetoaddress`, `generateblock` | **closed** |
| `DANGEROUS_DEBUG` | `setmocktime`, `mockscheduler` | **closed** |

RPC cannot:

* modify consensus state on mainnet (`invalidateblock` family)
* bypass validation (`submitblock` still uses the existing admission path)
* inject generated blocks on mainnet

Permission / bind / auth checks are in `EvaluateConfig`. localhost is the
default restriction. Authentication is forced when the RPC server is on.

Regtest / testnet keep the debug RPCs for functional tests.

---

## 5. Node health

RPC: `getnodehealth`

| Field | Meaning |
|-------|---------|
| `height` | Best fully-validated height, or `-1` |
| `best_block_hash` | Tip hash |
| `peer_count` | Connected peers |
| `syncing` | Initial block download |
| `database_status` | `ok` / `missing` / `corrupt` / `locked` / `version_mismatch` / `network_mismatch` |
| `mempool_size` / `mempool_bytes` | Mempool occupancy |
| `consensus_kernel_status` | `available` if `ChainstateManager` is present (presence only, not a Kernel decision) |
| `network` | Isolated chain name |

`FormatHealth` is the standalone JSON used by tests. Production RPC maps the
same snapshot fields. Health is observational: it does not call issuance,
RES, draw, or `VerifyRigorPredicate`.

---

## 6. Upgrade and recovery

### Safe upgrade

`EvaluateUpgrade`:

* Missing `ops-data-version` → pass; markers are written. Not a migration.
* Matching version → pass.
* Any other version → **stop**. No automatic chainstate rewrite.
* A snapshot that would auto-modify chainstate → **stop**.

There is no old-version compatibility layer (mainnet has not launched).

### Backup / restore baseline

`BackupPathHints` (copy, do not mutate in place):

1. Configuration: `rigor.conf`, `rpc-auth.conf`.
2. Wallet directory is **isolated**: `wallets/` must be backed up separately
   from chainstate. This baseline does not open wallet files.
3. Chainstate: `chainstate/` plus `blocks/` (and `blocks/index/`).
4. Operations markers: `ops-network`, `ops-data-version`.

Fast restore:

1. Stop the node (datadir lock must be free).
2. Restore config, then `chainstate/` + `blocks/`, then `wallets/` if used.
3. Confirm `ops-network` matches the binary’s `-chain` / default mainnet.
4. Confirm `ops-data-version` equals `NODE_OPS_DATA_VERSION` (currently `1`).
5. Start. If markers or LevelDB `CURRENT` files disagree, the node exits
   fail-closed. Do not hand-edit chainstate to “make it boot”.

---

## 7. Tests and tool

| Artifact | Role |
|----------|------|
| `core/src/test/test_node_operations_baseline.cpp` | Illegal config, network isolation, datadir errors, RPC security, log redaction, health output, upgrade/restore hints |
| `tools/check-node-operations-baseline.py` | Static rules + standalone `g++` run of the test |

Standalone compile (also invoked by the checker):

```
g++ -O2 -std=c++17 -DNODE_OPERATIONS_STANDALONE -Icore/src \
  -o formal_verification/extracted/test_node_operations_baseline \
  core/src/test/test_node_operations_baseline.cpp \
  core/src/node/operations/node_operations_baseline.cpp
```

---

## Proof scope

### PASS

| Claim | Meaning |
|-------|---------|
| configuration security | Illegal / mixed-network / consensus-override / open-RPC / weak-RPC configs evaluate `FAIL_CLOSED`. Defaults do not expose RPC to `0.0.0.0/0`. |
| data directory safety | Corrupt chainstate, network mismatch, version mismatch, and lock conflict refuse startup. No second UTXO manager. |
| RPC security | Mainnet closes consensus-mutation, block-inject, and debug RPCs. Auth required. localhost default. |
| logging auditability | Secrets redacted. Consensus reject reasons remain in the log as reason codes. |
| health monitoring | `getnodehealth` / `FormatHealth` expose height, tip, peers, sync, db, mempool, kernel presence. |
| upgrade / recovery | Version mismatch stops the node. Restore is copy-in; chainstate is not auto-modified. |

### NOT CLAIM

| Out of scope | Why |
|--------------|-----|
| Coq refinement of init / RPC | No `.v` files in this TASK. `COQ_ADMITTED = 0`, `COQ_AXIOMS = 0`. |
| P2P eclipse / wire security | PHASE A |
| Wallet key security | PHASE B |
| Consensus correctness | Unchanged Kernel |

---

## Integration

* `AppInitParameterInteraction` → `RunNodeOperationsStartupChecks`
* `AppInitMain` (after `StartLogging`) → `RunNodeOperationsDataDirectoryChecks`
* `CRPCTable::execute` → `EvaluateRpcMethod`
* `RegisterNodeRPCCommands` → `getnodehealth`

Files added under `core/src/node/operations/` are operations-only. Consensus
Kernel sources and `kernel/chainparams.cpp` protocol constants are not
modified by this TASK.
