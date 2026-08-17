# RIGOR Architecture

## Core Principle

**Script is the compliance layer.**

The locking script defines the validity conditions under which an asset output can exist,
transfer, and remain spendable. Asset *classification* (ORU / RES / none) is inferred from
`scriptPubKey` at parse time. Asset *legitimacy* is decided by the full consensus path —
UTXO commitments, script rules, economic proofs, and deterministic verification — not by
script parsing alone.

```
TxOut = (nValue, scriptPubKey)     # Bitcoin-native storage; never extended
Asset class = inferred from scriptPubKey pattern
Asset validity = UTXO + script + consensus + economic proofs
```

This supersedes the earlier shorthand “script is the asset,” which correctly forbade
parallel asset fields but overstated what script parsing alone settles.

## Design Constraints

### Storage Invariant

- `CTxOut` structure **must not** change.
- Consensus-related output serialization **must not** change.
- Wire & consensus compatibility: 100% compatible with Bitcoin native storage/transmission formats.
- No parallel asset field, tag, or identifier stored alongside the output.

### Parse Classification, Enforce Validity

Asset *types* are classified by script parsing. Asset *acceptance* still requires the
consensus rules for that class (conservation, convergence predicates, RES mint proofs, etc.).

```cpp
// pseudocode — classification only
AssetType GetAssetType(const CScript& script) {
    if (IsWitnessV1RES(script)) return ASSET_RES;
    if (IsWitnessV0ORU(script)) return ASSET_ORU;
    return ASSET_NONE;
}
```

### UTXO Supremacy

- No account model as a consensus truth source.
- No external balance database.
- No administrative asset registry.
- No oracle dependency.
- No consensus asset state outside UTXO validation.

Ownership changes only by spending old outputs and creating new ones.

### Dual-Asset Roles

| Asset | Role |
|-------|------|
| **ORU** | Provides PoW security and economic coordination: subsidy, fees, convergence participation cost |
| **RES** | Independent scarce resource; **RES minted** through deterministic consensus allocation; **not** mined |

**ORU provides PoW security and economic coordination.**  
PoW does not create RES. A convergence transaction commits ORU; PoW provides irreversible ordering for deterministic consensus allocation of RES.

### Witness Surface

- Witness v0 (ORU / P2WPKH / P2WSH) and Witness v1 (RES / CVG2) only.
- No Taproot / BIP341 semantics on the consensus path.
- No administrative keys, upgradeable proxies, or discretionary mint.

## Layer Model

| Layer | Scope | Bitcoin Core Equivalent |
|-------|-------|------------------------|
| L1 Consensus | Block validation, script, selection, RES draw | `validation.cpp`, `script/`, `consensus/` |
| L2 Assets | ORU / RES semantics under L1 rules | `consensus/`, `consensus_rigor/` |
| L3 Wallet | Key management, signing | `wallet/` |
| L4 Interface | RPC, REST, P2P protocol | `rpc/`, `rest/`, `net/` |

## Key Directories

- `core/src/consensus/` — Frozen consensus rules (draw, RES mint, params).
- `core/src/consensus_rigor/` — Additional RIGOR consensus helpers isolated from Bitcoin Core paths.
- `core/src/script/` — Script / witness verification.
- `core/src/primitives/` — Transaction and block primitives (**unchanged `CTxOut` storage**; no asset field).

## Compatibility Guarantee

- Existing Bitcoin RPC calls continue to work for native-shaped outputs.
- New RPCs expose RIGOR-specific state (`getconvergencestatus`, `getdeterministicstatehash`).
- Block header remains an 80-byte Bitcoin-compatible layout.
- Non-RIGOR nodes reject RIGOR blocks as invalid, which is expected.

## Determinism

All consensus functions must satisfy:

```
same input -> same bytes output
```

Across platforms, compilers, and execution environments.

## Fee domain and Convergence settlement

- **Network fees are ORU-only** (POOL-F). There is no path where users pay transaction fees in RES.
- Convergence RES prizes pay the **draw winner**. Local mining only *settles* that mint into the next block.
- Wallet product: Settings → **Auto-settle Convergence when mining locally** (default ON). See
  [docs/mining/AUTO_SETTLE_CONVERGENCE_WALLET.md](../mining/AUTO_SETTLE_CONVERGENCE_WALLET.md).
- Legacy node arg `-miningresfeeaddr` **arms** mint assembly; it is not a fee-receive address.

## Terminology

See [docs/security/PROTOCOL_TERMINOLOGY_ALIGNMENT.md](../security/PROTOCOL_TERMINOLOGY_ALIGNMENT.md)
for the mapping from earlier “script-as-asset” language to the current compliance-layer framing.

## References

- `docs/rfcs/RFC-000-consensus-constitution.md` — Semantic constitution.
- `docs/rfcs/RFC-ARCH-000-engineering-constitution.md` — Engineering constitution.
- `core/doc/convergence-consensus-v1.md` — Convergence rules.
- `docs/FORMAL-VERIFICATION-VERDICT.md` — What is proved, and what is not.
- `docs/pool/POOL_NODE_AUTOMINT_RUNBOOK.md` — Node automint ops (armed settlement + ORU fund UTXO).
- `docs/mining/AUTO_SETTLE_CONVERGENCE_WALLET.md` — Wallet auto-settle product wording.
