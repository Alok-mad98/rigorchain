# RIGOR Script Security Boundary

**Task:** `TASK-RIGOR-MINIMAL-SCRIPT-ARCHITECTURE-FINAL-HARDENING-001`  
**Layer:** Security architecture  
**Consensus impact:** **NONE**  
**`equivalence_claimed`:** `false`

Registry of types: [RIGOR_SCRIPT_TYPE_REGISTRY.md](./RIGOR_SCRIPT_TYPE_REGISTRY.md)

RIGOR minimizes script complexity rather than eliminating scripts. The protocol intentionally restricts execution semantics to preserve deterministic verification and reduce consensus attack surface.

---

## Why RIGOR restricts scripts

A monetary ledger must be independently replayed. Every extra execution capability is extra consensus responsibility.

Security objectives:

1. Reduce attack surface
2. Avoid general-purpose execution risks
3. Preserve deterministic validation
4. Keep consensus auditable
5. Maintain formal verification feasibility

---

## What RIGOR is

- Bitcoin-derived UTXO architecture
- native dual-asset system (ORU and RES)
- restricted script environment
- deterministic validation system

## What RIGOR is not

- a general-purpose programmable blockchain
- a smart contract platform
- an EVM-compatible chain
- a Taproot extension
- a token layer
- a colored coin system
- an inscription / Rune system

---

## Inherited vs restricted

| Bitcoin-inherited surfaces | RIGOR restrictions |
|----------------------------|--------------------|
| UTXO accounting (`TxOut = (nValue, scriptPubKey)`) | No parallel asset field; class inferred from script |
| Script interpreter (witness v0 inner scripts) | Closed output set; new P2PKH/P2SH invalid |
| Signature validation | Ed25519 only; secp256k1 CHECKSIG disabled |

RIGOR restrictions in one line:

- limited asset semantics (two native identities + CVG2 commitment)
- no VM
- no arbitrary contracts
- no Taproot / BIP341 semantics
- fixed validation paths (v0 P2WPKH/P2WSH/native Ed25519; v1 RES key-path; CVG2 structural)

```
Less capability
       ↓
Less complexity
       ↓
Smaller attack surface
       ↓
Stronger mathematical verification
```

---

## Asset identity

ORU and RES are:

- native UTXO assets
- same protocol layer
- consensus-defined identities

They are **not** colored coins, wrapped assets, inscriptions, Runes, or external contracts.

Asset identity derives from script classification (`AssetTypeFromScriptPubKey`). Legitimacy still requires conservation, mint predicates, and Convergence rules.

Spendable ORU is Witness v0. CVG2 is Witness v1 **ORU-domain** (unspendable). RES is Witness v1 that is not CVG2.

---

## Execution surfaces that remain

Scripts exist:

| Surface | Bound |
|---------|--------|
| Witness v0 | ORU spends |
| P2WSH | Largest inherited opcode surface; Ed25519 CHECKSIG |
| Witness v1 | RES key-path; CVG2 commitment |
| Signature verification | Ed25519 |
| CVG2 validation | Structure + no UTXO insert + no spend |

Formal verification is realistic because these paths are finite. `equivalence_claimed` remains **false**.
