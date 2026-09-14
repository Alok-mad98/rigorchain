# RIGOR Public Whitepaper

**Aligned:** production `core/src/` (2026-09-04)  
**Type:** Public narrative. Not a consensus specification.

```
equivalence_claimed = false
documentation_consensus_authority = false
```

**Whitepaper:** [`whitepaper/RIGOR_PROTOCOL_WHITEPAPER.md`](./whitepaper/RIGOR_PROTOCOL_WHITEPAPER.md).  
Protocol SSOT: [`protocol/RFC-020.md`](./protocol/RFC-020.md), [`protocol/RFC-018.md`](./protocol/RFC-018.md).  
Implementation facts: [`developer/CODE_SSOT_PROTOCOL_FACTS.md`](./developer/CODE_SSOT_PROTOCOL_FACTS.md).  
Short companions: [`whitepaper/RIGOR_PROTOCOL_IDENTITY.md`](./whitepaper/RIGOR_PROTOCOL_IDENTITY.md),
[`whitepaper/RIGOR_MATHEMATICAL_CONSENSUS_DESIGN.md`](./whitepaper/RIGOR_MATHEMATICAL_CONSENSUS_DESIGN.md),
[`whitepaper/RIGOR_FORMAL_VERIFICATION_DESIGN.md`](./whitepaper/RIGOR_FORMAL_VERIFICATION_DESIGN.md).

Protocol specifications are normative SSOT.
Implementations realize the frozen protocol.
If this file disagrees with RFC-020 / RFC-018, the RFC is correct.

---

## Abstract

RIGOR is a **dual-asset UTXO protocol**. Native assets are **ORU** and **RES**.
Asset class is inferred from `scriptPubKey`. There is no token registry, account
ledger, contract storage, or global asset table.

**ORU** is the utility asset: fees, miner subsidy, and Convergence destroy.
**RES** is the derived-cap reserve. RES is **not mined**. A body transaction
that destroys ORU (`OP_CONVERGE`) MAY mint Witness v1 RES in the same
transaction when `R > 0 → R · (2²⁵ + h)² ≤ B << 40`.

**OpuHash** is the consensus Proof-of-Work name. The digest is SHA3-256 over
the domain-tagged header:

```
OpuHash(header) = SHA3-256("RGRPOW01" || header_80)
valid           ⇔  OpuHash(header) ≤ target
```

Difficulty follows ASERT (mainnet half-life 43200 seconds). Block identity
remains SHA256d of the 80-byte header.

The formal system verifies protocol models and finite vectors. The production
C++ implementation is not claimed to be formally equivalent.

```
equivalence_claimed = false
```

---

## 1. Identity

RIGOR is a two-asset UTXO protocol with a closed script-compliance surface.

```
RIGOR =
    2 Native Assets (ORU, RES)
  +
    Closed monetary script templates
  +
    UTXO-local validation
  +
    Ed25519 authentication
  +
    OpuHash (SHA3-256 / RGRPOW01) + ASERT
  +
    Deterministic Reserve Issuance
  +
    Deflationary Fee Destroy
  +
    Formal verification model
  +
    No global asset state
```

There is **no Script VM**. Bytes are classified. Spends go through
`VerifyRigorPredicate`.

---

## 2. Assets

Classification is `AssetTypeFromScriptPubKey(scriptPubKey)`
(`core/src/script/rigor_classify.cpp`). Script alone does not mint.

| Asset | Live script | Role |
|-------|-------------|------|
| **ORU** | Witness v0 key-hash; Convergence destroy `0xbd` | Subsidy, fees, Convergence weight |
| **RES** | Witness v1 32-byte program | Derived-cap reserve; not a PoW reward |
| **NONE** | Zero-value `RIGOR_MARKER_RETURN` (`0x6a`) | Data carrier only |

### ORU

```
R(0) = 0
R(h) = 128 ORU    for h ≥ 1
```

Flat forever. No halvings. No absolute ORU cap. `MAX_MONEY` is a per-value
overflow bound.

### RES

```
destroy_cost(h) = 1024 · (2²⁵ + h)² / 2⁵⁰          display only
R > 0  →  R · (2²⁵ + h)² ≤ B << 40         consensus
R = 0  →  unconstrained
```

`RES_COIN = 100000000` (eight decimals, same as ORU). Closed-form cap
`RES_MAX_SUPPLY = 419430400000000` atoms (`2²²` RES = `4194304` RES). It is
not a declared 21,000,000. A mint is an ordinary body transaction:
ORU inputs, one `OP_CONVERGE` destroy `B`, Witness v1 RES outputs summing
to `R`. Consensus checks `CheckResMintArithmetic(R, B, h)`.

**No Convergence destroy → no new RES.** Coinbase `0xbb` is not the RFC-020
issuance path. Retired: draw, lottery, `W(k)`, `GetResMintReward`, epoch
`RATE(k)`, ORU halvings, σ, window 32, `RES_COIN = 1e6`.

---

## 3. Convergence

Live lock: `RIGOR_MARKER_CONVERGE` (`0xbd`).

```
scriptPubKey = PUSHBYTES_20 || HASH160(RES program) || 0xbd
nValue      ≥ 1 atom
```

The output is unspendable and is **not** inserted into the UTXO set. Witness
v1 `CVG2` is retired and not creatable. The same body transaction MAY mint
Witness v1 RES when `R > 0 → R · (2²⁵ + h)² ≤ B << 40`.
`R = 0` is a legal plain destroy. `destroy_cost(h)` is a protocol cost, not a market
price.

There is no draw window, no σ, no winner selection, and no coinbase mint
prize. Mint-output maturity 100 is reorg protection, not a draw wait.

---

## 4. Mining

OpuHash discovers valid headers and orders blocks. Mining **pays ORU**.
Mining **does not mint RES**.

```
GBT (node-assembled coinbase) → OpuHash nonce search → submitblock
```

SOLO copies node GBT bytes (`default_coinbase_tx`). Pool products pay ORU
shares only. Changing a payout address cannot create RES.

---

## 5. Script surface

Live consensus templates (`ClassifyRigorTxout`):

| Template | Encoding | Asset | Spendable |
|----------|----------|-------|-----------|
| ORU P2WPKH | Witness v0, 20-byte program | ORU | Yes (Ed25519) |
| RES v1 | Witness v1, 32-byte program (not CVG2/RES2) | RES | Yes (Ed25519) |
| RES v1 mint outputs | Witness v1 32-byte program | RES | After maturity 100 |
| Converge | `<HASH160> 0xbd` | ORU | **No** (destroy; may pair with RES mint) |
| NULL_DATA | `0x6a` + pushes | NONE | No |

Not creatable: P2PKH, P2SH, P2WSH, Taproot, secp256k1 CHECKSIG, CVG2, RES2.

---

## 6. Verification boundary

Formal verification proves properties of the **protocol model** and locks
finite golden vectors. It does not prove C++ refinement.

Living lock (`formal_verification/golden_lock.json`): **433** shared vectors,
**226** Coq-enforced.

```
equivalence_claimed = false
hash_kernel_shared_binary_with_cpp = false
refinement_proved = false
```

---

## 7. What this paper does not claim

- Protocol-to-binary equivalence
- That the RFC-020 RES cap fills on a calendar date
- That a website, explorer, wallet, or pool is an authority
- Investment yield or price

Consensus defines reality. Observers recompute public bytes.

---

## References

- [`whitepaper/RIGOR_PROTOCOL_IDENTITY.md`](./whitepaper/RIGOR_PROTOCOL_IDENTITY.md)
- [`whitepaper/RIGOR_MATHEMATICAL_CONSENSUS_DESIGN.md`](./whitepaper/RIGOR_MATHEMATICAL_CONSENSUS_DESIGN.md)
- [`whitepaper/RIGOR_FORMAL_VERIFICATION_DESIGN.md`](./whitepaper/RIGOR_FORMAL_VERIFICATION_DESIGN.md)
- [`whitepaper/RIGOR_PROTOCOL_WHITEPAPER.md`](./whitepaper/RIGOR_PROTOCOL_WHITEPAPER.md)
- [`MAINNET_PROTOCOL_FREEZE_DECLARATION.md`](./MAINNET_PROTOCOL_FREEZE_DECLARATION.md)
- [`SCRIPT_STRUCTURE.md`](./SCRIPT_STRUCTURE.md)
- [`specification/SPEC-008-PROOF-OF-WORK.md`](./specification/SPEC-008-PROOF-OF-WORK.md)
- [`specification/RIGOR_OPCODE_SPECIFICATION.md`](./specification/RIGOR_OPCODE_SPECIFICATION.md)
