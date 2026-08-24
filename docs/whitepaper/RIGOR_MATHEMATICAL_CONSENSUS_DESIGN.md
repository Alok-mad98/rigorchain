# RIGOR Mathematical Consensus Design

**Task:** `TASK-RIGOR-DOCUMENTATION-CONSOLIDATION-P0-003`  
**Canonical name:** `docs/whitepaper/RIGOR_MATHEMATICAL_CONSENSUS_DESIGN.md`  
**Date:** 2026-08-22  
**Type:** Technical mathematical specification  
**Scope:** Whitepaper / protocol mathematics / consensus engineers  
**Code changes:** **NONE**  
**Consensus changes:** **NONE**

```
equivalence_claimed = false
hash_kernel_shared_binary_with_cpp = false
refinement_proved = false
```

This document is the **unique mathematical technical authority** for
“How does the mathematics run?”

Public contents: `R(h)`, `W(k)`, `burn()`, `σ`, winner comparator,
OpuHash, ASERT.

It is a **mathematical extraction** from the current repository.

It is **not**:

- a C++ proof
- a formal refinement proof
- an implementation-equivalence claim

Public protocol identity:
[`RIGOR_PROTOCOL_IDENTITY.md`](./RIGOR_PROTOCOL_IDENTITY.md).

Formal verification identity:
[`RIGOR_FORMAL_VERIFICATION_DESIGN.md`](./RIGOR_FORMAL_VERIFICATION_DESIGN.md).

Archived RFC corpus:
[`../archive/rfc/`](../archive/rfc/).

---

## 1. Asset issuance mathematics

### ORU — Height-indexed subsidy

```
R(h) = 1000 ORU >> min(⌊h / 210000⌋, 3)
```

Height-based subsidy. Three halvings, then a perpetual tail.

| Parameter | Value |
|-----------|-------|
| Initial subsidy | 1000 ORU |
| Halving interval | 210000 blocks |
| Maximum shifts | 3 |
| Tail subsidy | 125 ORU |

| Height band | `R(h)` |
|-------------|--------|
| `0 ≤ h < 210000` | 1000 ORU |
| `210000 ≤ h < 420000` | 500 ORU |
| `420000 ≤ h < 630000` | 250 ORU |
| `h ≥ 630000` | 125 ORU forever |

There is no absolute ORU supply cap. `MAX_MONEY` is a per-value sanity
bound, not an ORU cap.

Source: `GetBlockSubsidy` in `core/src/validation.cpp`;
`core/src/consensus/rigor_consensus_lock.h`;
`core/src/consensus/amount.h`.

### RES — Deterministic issuance

```
k = ⌊h / 210000⌋
```

```
W(k) = floor(W0 · (k+1)^(−3/2))    for 0 ≤ k ≤ 250
W(k) = 0                           otherwise
```

Bounded schedule. Integer evaluation (the only shipping form):

```
e     = k + 1
scale = floor(sqrt(e³ · 2^32))
W(k)  = floor( (W0 · 2^16) / scale )
```

```
W(0) = 40 RES
S_RES ≤ 21,000,000 RES
```

Mint authorization: a non-zero mint occurs only if the parent draw names
a winner and `W(h) > 0`. Amount, if minted, is exactly `W(h)`. RES is not
a Proof-of-Work block reward.

Source: `ResRewardForEpoch`, `GetResMintReward` in
`core/src/consensus/rigor_consensus_lock.h`;
`core/src/consensus/rigor_res.cpp`.

### Fee-burn operator

```
burn(fee) = floor(fee / 2)     if fee > 0
burn(fee) = 0                  if fee ≤ 0
```

Transaction-derived. Not a UTXO. Not global monetary state.

Source: `OruResTransferFeeBurnShare` in
`core/src/consensus/rigor_consensus_lock.h`.

---

## 2. Convergence mathematics

Live mechanism: `OP_CONVERGE`.

Pipeline:

```
Commitment
        ↓
Candidate set F
        ↓
Root C
        ↓
σ_i
        ↓
Eligibility
        ↓
Winner comparator
        ↓
RES mint authorization
```

### Commitment

```
scriptPubKey = OP_PUSHBYTES_20 || HASH160(RES program) || OP_CONVERGE
```

`nValue ≥ 1 ORU`. The output is unspendable and is not inserted into the
UTXO set.

Source: `IsConvergeScript` in `core/src/script/interpreter.cpp`;
`AddCoins` skip in `core/src/coins.cpp`.

### Candidate set F

```
F ⊆ [h − 32, h − 1]
```

A draw may run when `|F| ≥ 1`. Weight `f_i` is the integer Convergence
amount.

Source: `CONVERGENCE_DRAW_WINDOW` in
`core/src/consensus/rigor_consensus_lock.h`;
`core/src/consensus/convergence_root.cpp`.

### Root C

```
Hash256(x) = SHA256(SHA256(x))
```

```
if |F| = 0:   C = 32 zero bytes
if |F| ≥ 1:   C = Hash256(concat(sort(txid)))
```

A singleton is hashed.

Source: `core/src/consensus/convergence_root_eligibility.cpp`.

### Selection digest σ_i

```
σ_i = Hash256( txid_i || C || B_{h−1} || LE32(h) )
```

Preimage length: 100 bytes. `B_{h−1}` is the parent block hash.

Source: `core/src/consensus/consensus_rigor/sigma_hash.h`;
`core/src/consensus/selection.cpp`.

### Eligibility

```
eligible  ⇔  |F| ≥ 1  ∧  f_i ≥ 1  ∧  σ_i < T · f_i
```

`T` is the frozen windowed probability target. `T` is not the ASERT /
`nBits` PoW target.

Source: `IsEligible` in `core/src/consensus/selection.cpp`;
`RES_PROBABILITY_TARGET` in `core/src/consensus/params.h`.

### Winner comparator

The only legal consensus form:

```
i ≺ j   iff   H_i · f_j  <  H_j · f_i
```

Tie: `txid_i < txid_j` (unsigned byte-wise). Floating-point ratio order
is not the consensus form.

If a winner exists and `W(h) > 0`, block `h` must contain exactly one
`OP_RES_MINT` paying `W(h)` RES to the winner’s declared HASH160.

Source: `core/src/consensus/convergence_logic.cpp`;
`core/src/consensus/rigor_math.h`; `core/src/consensus/rigor_res.cpp`.

---

## 3. Proof of Work

### OpuHash

```
base   = SHA256d(header)
mixed  = OpusMix(base)
pow    = SHA256d(base || mixed)
valid  ⇔  pow ≤ target
```

Buffer = 65536 bytes. Mix rounds = 256.

Source: `core/src/pow_opus.cpp`; `CheckProofOfWork` in `core/src/pow.cpp`.

### ASERT next target

```
time_error  = (eval_time − genesis_time) − eval_height · spacing
next_target = anchor_target × 2^(time_error / half_life)
```

| Parameter | Value |
|-----------|-------|
| Spacing | 600 seconds |
| Half-life | 172800 seconds |

Integer realization is Q16.16.

Source: `GetNextWorkASERT` in `core/src/pow.cpp`.

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

Create exception: authorized `OP_RES_MINT`.

**Transition**

```
UTXO_after = (UTXO_before ∪ outputs) \ spent
```

except `OP_CONVERGE` outputs are not inserted, and fee-burn never becomes
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

**Consensus impact: NONE**
