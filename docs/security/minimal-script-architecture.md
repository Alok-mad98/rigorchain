# Minimal Script Architecture

**Task:** `TASK-RIGOR-MINIMAL-SCRIPT-ARCHITECTURE-WHITEPAPER-001`  
**Layer:** Security documentation  
**Consensus impact:** **NONE**  
**`equivalence_claimed`:** `false`

Canonical whitepaper chapter:
[`../whitepaper/chapters/RIGOR_MINIMAL_SCRIPT_ARCHITECTURE.md`](../whitepaper/chapters/RIGOR_MINIMAL_SCRIPT_ARCHITECTURE.md)
(also bound into [`../whitepaper/RIGOR_PROTOCOL_WHITEPAPER.md`](../whitepaper/RIGOR_PROTOCOL_WHITEPAPER.md)).

Website: `/documentation/security/minimal-script-architecture`

Related:

- [UTXO / architecture](../architecture/ARCHITECTURE.md)
- [Dual-asset model](../whitepaper/chapters/02-two-assets.md)
- [Formal verification](../verification/FORMAL-VERIFICATION-STATUS.md)
- [Zero Trust verification](../WHITEPAPER.md) (prior narrative); live verifier `/zero-trust-verification`

This note is architecture and security explanation only. It does not change consensus, wallet, or mining code.

---

## Principle

RIGOR inherits Bitcoin’s UTXO accounting and reduces what scripts may do.

RIGOR minimizes script complexity rather than eliminating scripts. The protocol intentionally restricts execution semantics to preserve deterministic verification and reduce consensus attack surface.

RIGOR does not attempt to compete with programmable blockchain platforms. It focuses on monetary integrity, deterministic consensus, verifiable ownership, and a minimized attack surface.

> Every additional execution capability creates additional consensus responsibility.

---

## Native assets, not overlays

> ORU and RES are two native asset identities inside the RIGOR UTXO system. Neither asset is a secondary representation of another asset.

They are **not** colored coins, inscriptions, Ordinals, Runes, smart-contract tokens, wrapped assets, or external accounting layers. Asset class is inferred from `scriptPubKey`. Conservation is per asset pool.

| Asset | Role | Script |
|-------|------|--------|
| **ORU** | Medium of exchange (fees, subsidy, Convergence weight) | Spendable: Witness v0. CVG2 commitment: Witness v1 (ORU-domain, unspendable) |
| **RES** | Store of value (capped; minted after Convergence) | Witness v1, not CVG2 |

There is no parent-child asset relationship. Both sit at the same protocol layer.

---

## What consensus allows

| Family | Use | Not |
|--------|-----|-----|
| Witness v0 | ORU spends; Bitcoin-compatible P2WPKH / P2WSH layer | A general-purpose VM |
| Witness v1 | RES Ed25519 ownership; CVG2 Convergence commitments | Bitcoin Taproot / BIP341 |
| `OP_RETURN` (value 0) | Commitments | Spendable value |

RIGOR does **not** provide a general-purpose VM, arbitrary smart contracts, a programmable execution layer, Turing-complete execution, or unrestricted programmable state.

RIGOR **does** provide constrained transaction validation, asset-specific script rules, and deterministic verification logic.

RIGOR minimizes script complexity rather than eliminating scripts. The protocol intentionally restricts execution semantics to preserve deterministic verification and reduce consensus attack surface.

**Witness v1 is not Bitcoin Taproot.** RIGOR does not inherit BIP341 execution semantics. Control blocks, annex, and tapscript are absent from the consensus path. Spend signatures are Ed25519 only (`SCRIPT_ERR_SECP256K1_DISABLED` for secp256k1 CHECKSIG).

Actual execution surfaces:

| Surface | Role |
|---------|------|
| Witness v0 | ORU P2WPKH / native 32-byte Ed25519 spend |
| P2WSH | Inner witness script at `SigVersion::WITNESS_V0` — largest inherited surface |
| Witness v1 | RES Ed25519 key-path (`RES2` or native); not BIP341 |
| Signature verification | Ed25519 only |
| CVG2 validation | Structural ORU-domain commitment; unspendable; not in the UTXO set |

P2WSH remains the largest inherited execution surface: an inner witness script at `SigVersion::WITNESS_V0` with Ed25519-only CHECKSIG. New P2PKH / P2SH / bare outputs are consensus-invalid (`bad-rigor-script-type` / `bad-txns-asset-script-nonstandard`).

CVG2 outputs are protocol-controlled ORU-domain burns. They are validated in-block and are not inserted into the UTXO set. Convergence is a deterministic selection mechanism, not a smart contract.

---

## Why this is a security property

```
Less capability
       ↓
Less complexity
       ↓
Smaller attack surface
       ↓
Stronger mathematical verification
```

Complex script systems enlarge audit scope, edge cases, and formal-verification difficulty. RIGOR’s minimal script design is not a limitation. It is a prerequisite for stronger mathematical assurance.

Formal verification of limited, deterministic state transitions is realistic. Formal verification of an unbounded contract VM is not the problem this protocol chose.

```
equivalence_claimed = false
```

See [`../verification/FORMAL-VERIFICATION-VERDICT.md`](../verification/FORMAL-VERIFICATION-VERDICT.md).

Consistency audit: [`RIGOR_MINIMAL_SCRIPT_CONSISTENCY_AUDIT.md`](./RIGOR_MINIMAL_SCRIPT_CONSISTENCY_AUDIT.md).
Type registry: [`RIGOR_SCRIPT_TYPE_REGISTRY.md`](./RIGOR_SCRIPT_TYPE_REGISTRY.md).
Security boundary: [`RIGOR_SCRIPT_SECURITY_BOUNDARY.md`](./RIGOR_SCRIPT_SECURITY_BOUNDARY.md).

---

## Code audit (read-only)

Confirmed implementation (no edits in this task):

- `IsAllowedScript` — `core/src/consensus/tx_check.cpp`
- Asset classification — `AssetTypeFromScriptPubKey` in `core/src/primitives/transaction.cpp`
- Interpreter families — `core/src/script/interpreter.cpp` (`VerifyWitnessProgram`)
- Taproot path removed — `core/src/validation.cpp`
- CVG2 excluded from UTXO set — `core/src/coins.cpp`

Unchanged by this documentation task: `core/src/kernel/`, `core/src/consensus/`, `pow.cpp`, `validation.cpp`, script interpreter, wallet, miner.
