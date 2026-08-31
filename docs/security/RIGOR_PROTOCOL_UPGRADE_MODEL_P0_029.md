# RIGOR Protocol Upgrade Model (P0-029)

**Task:** `TASK-RIGOR-NETWORK-CAPABILITY-NEGOTIATION-ARCHITECTURE-AND-MIGRATION-P0-029-V2`  
**Phase:** 7 — Security boundary + activation classes  
**Date:** 2026-08-27  

Does **not** inherit: Bitcoin BIP9 as a P2P feature gate, NOP-soft-fork, unknown-witness anyone-can-spend.

Predicate-level upgrades remain [P0-026](./RIGOR_SOFT_FORK_UPGRADE_MODEL_P0_026.md). This document classifies **network** vs **consensus** upgrades so they cannot be confused.

---

## 0. Three upgrade classes

| Class | Needs consensus change? | Old node behavior | Example |
|-------|-------------------------|-------------------|---------|
| **Capability upgrade** | No | Still connects (`MIN` stays 1). Missing new bits → old relay. | Compact-block v2 encoding, extra addr compression, bandwidth |
| **Soft consensus tightening** | Yes (stricter checker on an **existing** template) | Still **accepts** the chain if it never produced now-invalid txs; new nodes reject more | Tighter Ed25519 canonical encoding on the same ScriptPubKey |
| **Hard consensus upgrade** | Yes (new template / new predicate) | **Rejects** new outputs/spends (`ASSET_INVALID`) | `FUTURE_SIGNATURE_V2` / `FUTURE_PREDICATE_V2` as live spend |

`FUTURE_*` bits in `RigorCapability` are **not** the activation of those predicates. They are reserved P2P names. Turning a new predicate on requires a node release that changes the classifier / `VerifyScript` — a coordinated hard fork. Advertising the bit on a v1 node does nothing (`enable = false`).

---

## 1. Network cannot affect consensus

Required path:

```
P2P Message
    ↓
Decode
    ↓
Validation Input
    ↓
Consensus   (classifier → VerifyRigorPredicate → ConnectBlock)
```

Forbidden path:

```
Network Capability  →  Asset State
```

Evidence:

- `CNodeCapabilities` is a `Peer` field in `net_processing.cpp`.
- No capability parameter on `ConnectBlock`, `CheckBlock`, `VerifyScript`, `AssetTypeFromScriptPubKey`, maturity, sighash.
- Coq `capability_change c = NoStateChange` and `capability_change c <> AssetStateChanged`.

---

## 2. Downgrade resistance

Attack: RIGOR v2 peer speaks to RIGOR v1 (or omits `RIGORCAP` / advertises `FUTURE_*`).

| Question | Answer |
|----------|--------|
| Connection? | Yes, if `nVersion >= RIGOR_PROTOCOL_MIN_VERSION` **and** `rigorcap` is sent before VERACK. |
| Security drop? | No. v1 still requires `NODE_WITNESS`, fail-closed getdata types, `CheckTransaction` on relay. |
| Consensus misjudge? | No. v1 classifier/predicate unchanged. |
| Asset rules change? | No. |
| `FUTURE_*` enabled on v1? | No. Known-but-not-local-active. |
| Unknown high bits enabled? | No. Masked out. |

Omitting `RIGORCAP` disconnects at VERACK. That is fail-closed for communication. `FUTURE_*` still cannot enable on a v1 node even if advertised.

---

## 3. Activation boundary

```
Feature Activation Boundary
        │
        ├─ capability bit + optional activation_epoch     (P2P only)
        │
        ├─ soft tightening of an existing predicate       (consensus; old nodes still sync)
        │
        └─ new ScriptPubKey template / new predicate      (hard fork; node upgrade required)
```

Bitcoin BIP9 `versionbits` on **block headers** is a leftover consensus FSM (`DEPLOYMENT_TESTDUMMY` inactive on mainnet). It is **not** the RIGOR capability channel and was not wired to P2P features in this task.

---

## 4. What this task must not become

- Restoring EvalScript / opcode VM / Miniscript
- Using `FUTURE_SIGNATURE_V2` as a silent ScriptPubKey interpreter switch
- Bumping `PROTOCOL_VERSION` (serialization) to mean a new spend rule
- Gating `ConnectBlock` on `HasCapability()`

Those would collapse the isolation this model exists to preserve.
