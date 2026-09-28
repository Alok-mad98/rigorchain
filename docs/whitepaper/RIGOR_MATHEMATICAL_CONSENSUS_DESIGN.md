# RIGOR Mathematical Consensus Design

**Document status:** Production specification  
**Date:** 2026-08-22  

This paper is the mathematical model of consensus: issuance, Convergence,
Proof-of-Work, and UTXO conservation. It does not claim C++ equivalence.

Code facts: [`../developer/CODE_SSOT_PROTOCOL_FACTS.md`](../developer/CODE_SSOT_PROTOCOL_FACTS.md).

```
equivalence_claimed = false
hash_kernel_shared_binary_with_cpp = false
refinement_proved = false
```

This document is the **unique mathematical technical authority** for
“How does the mathematics run?”

Public contents: `R(h)`, `destroy_cost(h)`, `CheckResMintArithmetic`, `destroy()`,
OpuHash (SHA3-256 / `RGRPOW01`), ASERT. σ is not a live mint input.

It is a **mathematical extraction** from the current repository.

It is **not**:

- a C++ proof
- a formal refinement proof
- an implementation-equivalence claim

Public protocol identity:
[`RIGOR_PROTOCOL_IDENTITY.md`](./RIGOR_PROTOCOL_IDENTITY.md).

Formal verification identity:
[`RIGOR_FORMAL_VERIFICATION_DESIGN.md`](./RIGOR_FORMAL_VERIFICATION_DESIGN.md).

RFC-019 and earlier texts are not protocol authority.

---

## 1. Asset issuance mathematics

### ORU — Height-indexed subsidy

```
R(0) = 0
R(h) = 128 ORU    for h ≥ 1
```

Height-based subsidy. Genesis issues nothing. Flat forever. No halvings.

| Parameter | Value |
|-----------|-------|
| Genesis subsidy | 0 ORU |
| Flat subsidy (h ≥ 1) | 128 ORU |
| Halving interval | none |
| Maximum shifts | 0 |
| Tail subsidy | 128 ORU |

| Height band | `R(h)` |
|-------------|--------|
| `h = 0` | 0 ORU |
| `h ≥ 1` | 128 ORU forever |

There is no absolute ORU supply cap. `MAX_MONEY` is a per-value sanity
bound, not an ORU cap.

Source: `GetBlockSubsidy` in `core/src/consensus/rigor_issuance.cpp`;
`core/src/consensus/rigor_consensus_lock.h`;
`core/src/consensus/amount.h`.

### RES — Continuous destroy cost (RFC-020)

```
destroy_cost(h) = 1024 · (2²⁵ + h)² / 2⁵⁰          display (curve denominator 2⁵⁰)
R · (2²⁵ + h)² ≤ B << 40                   consensus (scaling factor 2⁴⁰)
```

`2⁵⁰` is the curve normalization denominator. It is not a consensus
shift. A display helper may evaluate the quotient with integer division
by `2⁵⁰` (`>> 50`). That floor is for reading only. Display
rounding/floor do not enter consensus.

`2⁴⁰` is the integer consensus scaling factor after `1024 = 2¹⁰` is
cancelled. It is not the `destroy_cost` denominator. Consensus never
divides by `2⁵⁰`; it evaluates `B << 40`.

Integer only. `destroy_cost(h)` is a **protocol cost**, not a market quote.
The price never hits zero; there is no mint horizon.

| Parameter | Value |
|-----------|-------|
| `RES_COIN` | 100000000 |
| `destroy_cost(0)` | 102400000000 (1024 ORU / 1 RES) |
| `destroy_cost(210000)` | 103685749152 |
| `destroy_cost(420000)` | 104979520049 |
| `RES_DESTROY_POLE` | `2²⁵` = 33554432 |
| `RES_DESTROY_SHIFT` | 40 (integer consensus scaling factor `B << 40`; not the `destroy_cost` denominator `2⁵⁰`) |
| `RES_MAX_SUPPLY` | 419430400000000 atoms (`2²²` RES) |

From block 210000 the destroy cost becomes 1036.85749152 ORU per RES.

Mint predicate (`CheckResMintArithmetic`):

```
R > 0  →  R · (2²⁵ + h)² ≤ B << 40
R = 0  →  unconstrained
```

`R = 0` is a legal plain destroy. A non-zero mint occurs only in a **body
transaction** that destroys ORU (`OP_CONVERGE`, amount `B`) and creates
Witness v1 RES outputs (sum `R`) in the **same transaction**. RES is not
a Proof-of-Work block reward. Mint-output maturity 100 is reorg protection.

The cap is closed form: `(S / P0) · H = 2⁷ / 2¹⁰ · 2²⁵ = 2²²` RES =
`419430400000000` atoms (`4194304` RES).

Source: `CheckResMintArithmetic`, `ResDerivedMaxSupply` in
`core/src/consensus/res_issuance.h`.

### Fee-destroy operator

```
destroy(fee) = floor(fee / 2)     if fee > 0
destroy(fee) = 0                  if fee ≤ 0
```

Transaction-derived. Not a UTXO. Not global monetary state.

Source: `OruResTransferFeeBurnShare` in
`core/src/consensus/rigor_consensus_lock.h`.

---

## 2. Convergence mathematics

Live mechanism: same-tx destroy-to-mint. `OP_CONVERGE` /
`RIGOR_MARKER_CONVERGE` (`0xbd`) destroys ORU amount `B`. The same
body transaction MAY mint Witness v1 RES summing to `R`.

Pipeline:

```
ORU destroyed (OP_CONVERGE, amount B)
        ↓
same body transaction
        ↓
CheckResMintArithmetic(R, B, h)
        ↓
Witness v1 RES outputs (sum R), if authorized
```

### Commitment

```
scriptPubKey = OP_PUSHBYTES_20 || HASH160(RES program) || 0xbd
```

The output is unspendable and is not inserted into the UTXO set. `R = 0`
(plain destroy) is legal. Positive `R` requires the mint predicate at the
confirming height.

Source: `IsConvergeScript` / `ParseConvergeScript` in
`core/src/script/rigor_classify.cpp`; `AddCoins` skip in `core/src/coins.cpp`.
There is no `interpreter.cpp`.

### Mint arithmetic

```
mint_authorized(h)  ⇔  h ≥ 0
CheckResMintArithmetic(R, B, h)  ⇔
    R = 0
    ∨  (h ≥ 0 ∧ R · (2²⁵ + h)² ≤ B << 40
        ∧ R ≤ RES_MAX_SUPPLY)
```

Cross-multiplication only. No division in consensus. The display destroy cost
is not a live mint input from σ, a candidate window, or a winner comparator.

Source: `CheckResMintArithmetic` in `core/src/consensus/res_issuance.h`.

### Mint locus

```
inputs        ORU only
outputs       exactly one OP_CONVERGE output, value B
              one or more RES outputs, total R
              optionally ORU change
```

Coinbase `OP_RES_MINT` is not the RFC-020 path.

---

## 3. Proof of Work

### OpuHash (SHA3-256 / `RGRPOW01`)

OpuHash is the consensus Proof-of-Work name. The digest is FIPS 202 SHA3-256:

```
preimage = "RGRPOW01" || header_80
OpuHash  = SHA3-256(preimage)
valid    ⇔  OpuHash ≤ target
```

Block identifier remains `SHA256d(header_80)`. Merkle and txid remain
SHA256d. σ is not a live mint input.

Source: `rigor_kernel::GetRigorPowHash` in
`core/src/rigor_kernel/pow_digest.h`; `CheckProofOfWork` in
`core/src/pow.cpp`.

### ASERT next target

Anchor is genesis while producing height 1, then height 1 thereafter.
Evaluation uses the **parent** header only.

```
time_error  = (parent_time − anchor_time) − (parent_height − anchor_height) · spacing
next_target = clamp_to_prev(anchor_target × 2^(time_error / half_life))
```

| Parameter | Value |
|-----------|-------|
| Spacing | 600 seconds |
| Half-life | **43200 seconds** (12 hours) |
| Per-block clamp | ±12.5% |

Integer realization is Q16.16 (`aserti3-2d`).

Source: `GetNextWorkASERT` in `core/src/pow.cpp`;
`MAINNET_POW_ASERT_HALF_LIFE` in `core/src/consensus/rigor_consensus_lock.h`.

---

## 4. UTXO mathematics

```
CTxOut = (value, scriptPubKey)
```

There is no parallel `(asset, value, script)` field.

```
AssetClass = f(scriptPubKey)
```

Classification is not issuance authority.

**Conservation** (ordinary transactions):

```
oru_in ≥ oru_out
res_in ≥ res_out
```

Create exception: authorized same-tx body mint at `destroy_cost(h)` (Witness v1
RES with `OP_CONVERGE`). Coinbase `OP_RES_MINT` is not this path.

**Transition**

```
UTXO_after = (UTXO_before ∪ outputs) \ spent
```

except `OP_CONVERGE` outputs are not inserted, and fee-destroy never becomes
an output.

Source: `core/src/primitives/transaction.h`;
`CheckTxInputs` in `core/src/consensus/tx_verify.cpp`;
`core/src/coins.cpp`; `core/src/validation.cpp`.

---

## 5. Formal boundary

```
equivalence_claimed = false
hash_kernel_shared_binary_with_cpp = false
refinement_proved = false
```

```
mathematical model ≠ implementation proof
```

The extraction states consensus equations. It does not prove C++
refinement, production-binary equivalence, or wallet/P2P/miner
equivalence. Verification boundary:
[`RIGOR_FORMAL_VERIFICATION_DESIGN.md`](./RIGOR_FORMAL_VERIFICATION_DESIGN.md).

---

## Final statement

This file is the unique mathematical consensus specification.

```
equivalence_claimed = false
hash_kernel_shared_binary_with_cpp = false
refinement_proved = false
```

