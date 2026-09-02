# RIGOR P2P Security Boundary (P0-076)

**Task:** `TASK-RIGOR-P2P-SECURITY-BOUNDARY-AUDIT-P0-076`  
**Date:** 2026-08-31  
**Nature:** Mainnet P2P network security baseline. Isolation model + fail-closed
wire firewall. This is not a formal proof of the whole network.

This TASK does not modify consensus rules, Consensus Kernel, Transaction
Kernel, or Genesis parameters. It does not add a second classifier, a
second signature verifier, or a P2P consensus authority.

```
P2P message input
        ↓
network validation boundary     P2PMessageBoundary::Evaluate
        ↓
block / tx admission boundary   CheckBlock / CheckTransaction / ATMP
        ↓
ConsensusKernelInput
        ↓
Consensus decision
```

```
P2P_SECURITY_STATUS          = CLOSED
BLOCK_ADMISSION_BOUNDARY     = CLOSED
TX_RELAY_BOUNDARY            = CLOSED
PEER_SECURITY                = CLOSED
FUZZ_STATUS                  = PASS
CONSENSUS_BYPASS_STATUS      = NONE

P2P_SECURITY_BOUNDARY        = CLOSED
P2P_CONSENSUS_BYPASS         = NONE
CRASH_FOUND                  = NO
CONSENSUS_MODIFIED           = NO

FINAL_VERDICT:
P2P_SECURITY_BOUNDARY_CLOSED
```

Reproduce: `bash tools/run_p2p_security_gate.sh`

---

## Proof scope

### PASS

| Claim | Meaning |
|-------|---------|
| malformed input rejection | Structurally illegal `block` / `tx` / `version` payloads evaluate to `REJECT` or `BAN_PEER`. Evaluate does not throw, does not crash, and does not write chainstate. |
| network boundary isolation | `core/src/net/security/` cannot call issuance, RES reward, draw, classify, supply, or `VerifyRigorPredicate`. It cannot mutate UTXO / chainstate. Lint: `tools/lint-p2p-security-boundary.py`. |
| consensus entry protection | P2P output is only `ACCEPT_TO_VALIDATION`, `REJECT`, `BAN_PEER`. `ACCEPT_TO_VALIDATION` is admission (`CheckBlock` / `CheckTransaction`), not `ConsensusKernelInput` and not a Kernel decision. |
| deterministic admission | The same well-formed block set, delivered in different network orders, with invalid peers mixed in, admits the same block-identity set. `PeerMisbehaviorScore` is not part of that set. |

### NOT CLAIM

| Out of scope | Why |
|--------------|-----|
| internet anonymity | No onion / mixnet / anti-correlation proof. |
| eclipse impossibility | Topology tests are a baseline (invalid peers, reconnect, duplicates, slow peers, ordering). They do not prove eclipse is impossible. |
| OS network stack security | TCP/IP, kernel sockets, NIC firmware are assumed. |
| cryptographic primitive proof | Ed25519, SHA-256, and PoW remain the Kernel / Transaction Kernel / PoW surface. P2P does not re-prove them. |

This note is an isolation + rejection baseline, not a Coq refinement of `net_processing.cpp`.

---

## 1. P2P security boundary

Directory: `core/src/net/security/`

| Type | Role |
|------|------|
| `P2PSecurityInput` | Command + payload + peer counters + network-layer seen-sets. Not `ConsensusKernelInput`. |
| `P2PMessageBoundary` | Fail-closed evaluator. Never throws. Never calls Kernel formulas. |
| `P2PValidationResult` | Exclusive output: `ACCEPT_TO_VALIDATION` / `REJECT` / `BAN_PEER`. |
| `PeerMisbehaviorScore` | Network penalty. Threshold 100 (same number as `DISCOURAGEMENT_THRESHOLD`). Isolated from consensus. |

Forbidden at this layer:

- P2P directly modifying chainstate
- P2P directly calling reward / subsidy / draw / supply
- P2P directly bypassing Consensus Kernel
- A fourth P2P decision (relay-accept-as-consensus, etc.)

Production hook: `PeerManagerImpl::ProcessMessage` runs `P2PMessageBoundary::Evaluate` on `version`, `block`, and `tx` before deserialize-and-admit. `REJECT` drops the message. `BAN_PEER` calls existing `Misbehaving()` (peer punishment, not Kernel). `ACCEPT_TO_VALIDATION` continues into the existing handlers.

`libbitcoinkernel` does not contain `net/security`. P2P is not a consensus library.

---

## 2. Block message adversarial audit

Header (fail-closed → `REJECT`):

| Case | Reason token |
|------|----------------|
| truncated / not 80 bytes | `malformed-header` |
| claimed size > 4_000_000 | `invalid-size` |
| `nTime == 0` | `invalid-timestamp` |
| `nBits == 0` | `invalid-nbits` |
| truncated prev (header short) | `malformed-header` |
| header already in P2P seen-set | `duplicate-block` |
| `nVersion <= 0` | `unknown-version` |

Payload:

| Case | Reason token |
|------|----------------|
| header only / tx count 0 | `empty-block` |
| serialized size > 1_000_000 | `oversized-block` |
| CompactSize / leftover bytes | `malformed-transaction-vector` |
| identical tx payloads in one block | `duplicated-transaction` |
| merkle root all-zero with txs | `invalid-merkle-root` |

A well-formed but *wrong* merkle root (non-zero, txs present) is `ACCEPT_TO_VALIDATION`. Full merkle equality remains `CheckBlock` (`BlockMerkleRoot`). P2P does not implement a second merkle function.

Unknown parent hashes of legal length are `ACCEPT_TO_VALIDATION`. Looking up `hashPrevBlock` in chainstate from P2P would be a chainstate read; that is forbidden here.

Illegal inputs do not crash, do not have undefined parser behavior, and do not partially mutate chainstate (the boundary has no chainstate).

---

## 3. Transaction relay adversarial audit

P2P relay is a propagation boundary. Final spend validity MUST enter `VerifyRigorPredicate`.

| Case | P2P decision |
|------|----------------|
| truncated / leftover bytes | `REJECT` `malformed-transaction` |
| empty vin | `REJECT` `empty-input` |
| empty vout | `REJECT` `empty-output` |
| witness flag with all-null stacks, or element > 520 | `REJECT` `invalid-witness` |
| script bytes > 10_000 | `REJECT` `oversized-script-data` |
| payload already in P2P seen-set | `REJECT` `duplicate-txid` / replay at relay cache |
| garbage signature, unknown asset script | `ACCEPT_TO_VALIDATION` with `spend_must_enter_verify_rigor_predicate = true` |

P2P does **not** add:

- a script parser (opcode VM)
- a signature verifier
- an asset classifier

`ClassifyRigorTxout` and `VerifyRigorPredicate` remain the unique production bodies. An accepted tx message is admission-only.

---

## 4. Peer behavior

| Case | Decision |
|------|----------|
| truncated `version` | `REJECT` `handshake-malformed` |
| `VERSION.nVersion` outside `[1, 1]`, including Bitcoin `70016` | `BAN_PEER` `version-negotiation-abuse` |
| `messages_in_window > 512` | `BAN_PEER` `excessive-messages` |
| `invalid_blocks_from_peer >= 8` | `BAN_PEER` `repeated-invalid-blocks` |
| `invalid_tx_from_peer >= 16` | `BAN_PEER` `repeated-invalid-tx` |

`PeerMisbehaviorScore` may ban a peer. It must not appear in `ConsensusKernelInput` and must not change a consensus result. Tests pin: different scores, same admitted block set.

---

## 5. Eclipse / topology baseline

Covered in `test_p2p_security_boundary.cpp`:

- many invalid peers
- disconnect / reconnect (score reset, admitted set persists)
- duplicate peers
- slow peers (delay is not an Evaluate input)
- message ordering variation (`ABC` / `CBA` / `A-junk-C-B`)

Node state for this baseline is the **admitted block-identity set**. Same blocks, different network order → same set. This is deterministic admission, not an eclipse-impossibility proof.

---

## 6. Fuzz harness

`fuzz/p2p_security/`

| Entry | Target |
|-------|--------|
| block message parser | `EvaluateBlock` |
| tx message parser | `EvaluateTx` |
| peer message handler | `Evaluate` / `EvaluatePeer` |

Random bytes: no crash, no memory corruption in the firewall, no consensus bypass (the harness does not link Kernel objects).

Smoke: `bash fuzz/p2p_security/run_smoke.sh`

---

## 7. Tests and gates

| Artifact | Role |
|----------|------|
| `core/src/test/test_p2p_security_boundary.cpp` | invalid block / tx, peer abuse, ordering, fuzz seeds |
| `tools/lint-p2p-security-boundary.py` | isolation + exclusive decision set |
| `tools/run_p2p_security_gate.sh` | lint + compile + tests + fuzz smoke |

Existing gates that must still pass (unchanged consensus):

- `formal_verification/scripts/run_differential_gate.sh`
- authority lint (`tools/lint-consensus-authority.py`, `lint-consensus-authority-security.py`)
- consensus kernel freeze (`tools/lint-consensus-kernel-freeze.py`)
- refinement gate (`formal_verification/run_consensus_kernel_refinement_gate.sh`)

The P076 gate re-runs the authority and freeze lints so `CONSENSUS_MODIFIED = NO` is checked on every P2P gate run.

---

## Unchanged (forbidden)

Consensus Kernel, Transaction Kernel, Genesis parameters, issuance / RES / draw / supply / classify / spend formula bodies, and ConnectBlock consensus behavior were **not** modified by this TASK.
