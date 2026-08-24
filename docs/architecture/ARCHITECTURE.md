# RIGOR Architecture

## Core Principle

**Script is the compliance/classification layer.**

The locking script defines the validity conditions under which an asset output can exist,
transfer, and remain spendable. Asset *class* is inferred from `scriptPubKey`. Asset
ownership and transfer are enforced through UTXO consumption, script validation,
signature verification, and consensus predicates. Script alone does not authorize
asset creation. RES creation requires block-level authorization, Deterministic
Reserve Issuance, and Convergence conditions.

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
PoW does not create RES. RES is deterministically issued after protocol-defined
conditions (Deterministic Reserve Issuance). A convergence transaction commits ORU;
PoW provides irreversible ordering for that allocation. Eligible RES-transfer ORU
fees are subject to Deflationary Fee Burn — a protocol destruction sink that is
not a UTXO, not a wallet balance, and not a global consensus supply state.

### Witness Surface

- Witness v0 (ORU / P2WPKH / P2WSH), Witness v1 (RES), and `OP_CONVERGE` only.
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

## RIGOR Monetary Architecture

RIGOR separates monetary functions. Do not mix the ORU supply ledger,
circulating UTXO supply, Convergence destruction, and Deflationary Fee Burn.

| Function | Asset / sink |
|----------|----------------|
| PoW security + fees | **ORU** (coinbase subsidy; 125 ORU tail forever) |
| Convergence destruction | **ORU** committed to an unspendable burn / draw entry |
| Deflationary Fee Burn | Eligible RES-transfer ORU fee share never becomes a UTXO |
| Deterministic Reserve Issuance | **RES** — capped; issued after protocol-defined mint authorization; **not mined** |

Script is the compliance/classification layer. Asset class is inferred from
`scriptPubKey`. Script alone does not authorize RES creation.

## Fee domain and Convergence settlement

- **Network fees are ORU-only** (POOL-F). There is no path where users pay transaction fees in RES.
- Convergence RES prizes pay the **draw winner**. The next block's coinbase carries `OP_RES_MINT` plus the tagged PCORB carrier. GBT fields: `default_res_mint`, `default_pcorb_carrier` (`core/doc/mining-pool-gbt.md`).
- `-miningresfeeaddr` is only required when the mempool has RES-domain fee outputs; it does not arm mint assembly.

## Terminology

See [docs/security/PROTOCOL_TERMINOLOGY_ALIGNMENT.md](../security/PROTOCOL_TERMINOLOGY_ALIGNMENT.md)
for the mapping from earlier “script-as-asset” language to the current
compliance/classification-layer framing, and from internal RFC-017 / RFC-018
identifiers to Deterministic Reserve Issuance and Deflationary Fee Burn.

## References

- `docs/rfcs/RFC-000-consensus-constitution.md` — Semantic constitution.
- `docs/rfcs/RFC-ARCH-000-engineering-constitution.md` — Engineering constitution.
- `core/doc/convergence-consensus-v1.md` — Convergence rules.
- `docs/FORMAL-VERIFICATION-VERDICT.md` — What is proved, and what is not.
- `core/doc/mining-pool-gbt.md` — GBT coinbase mint and PCORB carrier fields.
