# RIGOR Architecture

## Core Principle

**Script-as-Asset.**

Asset definition is derived from `scriptPubKey`, not separate data fields.

```
TxOut = (nValue, scriptPubKey)
Asset = inferred from scriptPubKey pattern
```

## Design Constraints

### Storage Invariant

- `CTxOut` structure **must not** change.
- Consensus-related output serialization **must not** change.
- Wire & consensus compatibility: 100% compatible with Bitcoin native storage/transmission formats.

### Parse, Don't Persist

Asset types are inferred from script parsing, not additional stored fields.

```cpp
// pseudocode
AssetType GetAssetType(const CScript& script) {
    if (IsWitnessV1RES(script)) return ASSET_RES;
    if (IsWitnessV0ORU(script)) return ASSET_ORU;
    return ASSET_NONE;
}
```

## Layer Model

| Layer | Scope | Bitcoin Core Equivalent |
|-------|-------|------------------------|
| L1 Consensus | Block validation, script, selection | `validation.cpp`, `script/` |
| L2 Assets | ORU / RES semantics | New layer in `consensus_rigor/` |
| L3 Wallet | Key management, signing | `wallet/` |
| L4 Interface | RPC, REST, P2P protocol | `rpc/`, `rest/`, `net/` |

## Key Directories

- `core/src/consensus_rigor/` — All RIGOR consensus logic (isolated, no external dependencies except primitives).
- `core/src/script/` — Script system (replaced by StateTag, execution disabled).
- `core/src/primitives/` — Transaction and block primitives (unchanged structure, asset field added to `transaction.h`).

## Compatibility Guarantee

- Existing Bitcoin RPC calls continue to work for native assets.
- New RPCs expose RIGOR-specific state (`getconvergencestatus`, `getdeterministicstatehash`).
- Block serialization is byte-identical to Bitcoin for non-RIGOR nodes (they will reject RIGOR blocks as invalid, which is expected).

## Determinism

All consensus functions must satisfy:

```
same input -> same bytes output
```

Across platforms, compilers, and execution environments.

## References

- `RFC-000-consensus-constitution.md` — Semantic constitution.
- `RFC-ARCH-000-engineering-constitution.md` — Engineering constitution.
- `convergence-consensus-v1.md` — Convergence rules.
