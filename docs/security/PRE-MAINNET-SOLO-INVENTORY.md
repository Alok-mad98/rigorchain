# Pre-mainnet inventory: solo-mining path (no pool)

**Date:** 2026-08-18  
**Tree:** `~/rigor-official-tree`  
**Release scope:** Solo miner + `rigord` / wallet. **Pool / stratum / YiiMP / pool web are not shipped.**

## Release artifact lock

| Tree | Status for this mainnet theme |
|------|-------------------------------|
| `core/` (`rigord`, `rigor-cli`, wallet, GBT, `submitblock`) | **In scope** |
| `pool/`, `pool/stratum/`, `pool/stratum-server/`, `pool/web/` (YiiMP) | **Out of release scope** — do not audit or harden here; optional delete/exclude is a separate decision |
| Formal Coq / golden-vector campaigns | Frozen; not expanded |
| Window / reorg / RES monopoly suite | Closed: `feature_res_window_reorg_res_attacks.py` all PASS |

RPC policy flag `-solomining` allows GBT with zero peers / during IBD **on non-test chains**. It does not change consensus. Devtest / regtest already skip the peer/IBD GBT gates (`IsTestChain()`).

## Mempool / policy / `AcceptToMemoryPool`

| Hook | File |
|------|------|
| `AcceptToMemoryPool` | `core/src/validation.cpp` |
| Convergence min-relay fee (policy only) | same; log hint if `IsConvergenceTx` |
| `CheckTxInputs` / `CheckOruFundedResMintShape` | `core/src/consensus/tx_verify.cpp` — **consensus**, also used at mempool |
| ORU→RES without parseable PCORB | reject `mixed-asset-output` |
| Parseable proof whose inequality fails | reject `bad-rigor-res-proof` |
| Eviction score (not draw validity) | `core/src/policy/convergence_policy.h` (`CONVERGENCE_POLICY_V1_LOCK`) |

**Intentional policy ≠ consensus:** min-relay / mempool min fee may reject a tx that a miner could still include. Draw winner / `already_paid` / owner binding are **connect-only** (mempool cannot see the parent draw of a future block).

## RES mint connect

| Symbol | File |
|--------|------|
| `ConnectEvaluateResMintTx` | `core/src/consensus/rigor_res.cpp` |
| `CheckOruFundedResMintShape` | `core/src/consensus/tx_verify.cpp` |
| Winner / amount / cap | `core/src/validation.cpp` ConnectBlock (`bad-rigor-res-*`) |
| Automint assembly | `core/src/node/auto_res_mint.cpp`, `CreateNewBlock` in `core/src/node/miner.cpp` |
| Cap constant | `Consensus::RES_MAX_SUPPLY` in `rigor_consensus_lock.h` (21e6 × 100 atoms) |

Rejects of interest: `bad-rigor-res-without-winner`, `bad-rigor-res-owner-mismatch`, `bad-rigor-res-witness`, `bad-rigor-res-max-supply`, `bad-rigor-res-winner-mismatch`, `bad-rigor-multiple-res`.

## Solo mining RPC

| RPC | Role |
|-----|------|
| `getblocktemplate` | Template + `convergence_root` / `default_convergence_commitment` / parent draw |
| `submitblock` / `submitheader` | Solo submit |
| `generatetoaddress` | Devtest mine (safe with pending automint) |
| `generateblock` | Coinbase-only template + explicit tx list; **unsafe** if automint already inserted |
| `getautomintfundaddress` | P2WSH bootstrap for body mint |
| `-miningresfeeaddr` | Mockable-chain bootstrap sink only. **Rejected on mainnet-shaped params.** Prize still pays the winner. |
| `-solomining` | GBT with 0 peers on mainnet (RPC only) |

Existing functional hooks: `rigor_attack_blocks.py`, `mining_basic.py`, `feature_fault_injection_consensus.py`, `feature_rigor_reorg_winner_lock.py`, `feature_res_window_reorg_res_attacks.py`.

## `assert` / `AbortNode` on consensus paths

Invalid **peer blocks** use `state.Invalid` (`bad-convergence-*`, `bad-rigor-res-*`). Remaining `AbortNode` in `validation.cpp` is for **local** failures (corrupt block / hardware, failed window reconstruct, disk full, block-index write). Those take the node down by design; they are not a substitute for rejecting a malformed block.

`generateblock` previously used `CHECK_NONFATAL(vtx.size()==1)` when automint was present. Replaced with `JSONRPCError` (RPC only; **consensus impact: no**).

## Tests this inventory feeds

`core/test/functional/feature_res_solo_security.py` (V/S/A/G). Unit pins: `res_proof_math_tests.cpp` (mint eval), `runtime_supply_invariant_tests.cpp` (RES cap), `convergence_consensus_tests.cpp` (`already_paid`).

Runners: `core/test/functional/res_security/run_solo_suite.sh`, `run_pre_mainnet.sh` (window + solo + maturity). Narrative: `docs/security/PRE-MAINNET-SOLO-SECURITY.md`. Operator checklist: `docs/security/PRE-MAINNET-CHECKLIST.md`.
