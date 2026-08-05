# RFC-000-FINAL: Consensus Constitution — Fully Frozen

**Status:** FINAL (IMMUTABLE)
**Version:** 1.1 (Sigma-PoW-01)
**Scope:** Entire Consensus System

> **v1.1 amendment (SECURITY-001-A8, codename Sigma-PoW-01)** — this amendment
> **overrides two previously frozen clauses of this constitution**. The override is
> deliberate, is recorded here, and is the sole authority for the change:
>
> - **§4, σ_i input (was "FULLY LOCKED")**: slot `[64..95]` is the parent block hash
>   `B_{h-1}` taken verbatim. It was previously `bind_commit = Hash256(B_{h-10} || LE32(target_prev))`.
>   The total length is still exactly 100 bytes and the field order and offsets are unchanged.
> - **§5, block offset (was "consensus-locked")**: the constant `k = 10` and the
>   bootstrap stand-in for heights `1 <= h < k` are **deleted**, not migrated. The parent
>   block hash exists at every height from 1 upward, so the bootstrap case has no subject.
>
> The reason for overriding a frozen clause is that the frozen form was insecure: every σ
> input except the height was chosen by whoever assembled the paying block, so the draw
> could be re-rolled offline for about 13 ms of one CPU core per block. See
> `docs/security/SECURITY_001_A8_REPORT.md`. Chain impact: invalidating; it merges with the
> A-5/A-6/A-7 chain reset.
>
> This amendment also moves the draw one block later: the block at height `h` pays the
> winner of the convergence set committed by block `h-1`. See §5.

---

## 0 META RULE

The protocol is defined ONLY by invariants.
Implementation is NOT trusted.
Behavior is NOT trusted.
Performance is NOT trusted.

ONLY byte-for-byte equivalence with the specification is valid.

If any implementation deviates:
→ INVALID

---

## 1 CORE PRINCIPLE

Consensus = Deterministic Selection

Consensus MUST NOT compute outcomes.
Consensus MUST ONLY select outcomes.

**Formal:**
```
winner = argmin_i (H_i / f_i)
```

---

## 2 SINGLE INEQUALITY LAW

```
H_i < T · f_i
```

**Implementation (MUST)**
```
H_i * f_j < H_j * f_i
```

**FORBIDDEN:**
- division
- floating point
- normalization
- approximation

---

## 3 CANONICAL DATA ENCODING

**Primitive Types**
- uint32  → 4 bytes (little-endian)
- uint64  → 8 bytes (little-endian)
- bytes32 → exactly 32 bytes

**Concatenation Rules**
```
concat(a, b, c) = a || b || c
(no delimiter)
```

**FORBIDDEN:**
- variable length
- implicit casting
- trimming
- big-endian

---

## 4 CANONICAL HASHING

**Hash Definition**
```
Hash256(x) = SHA256(SHA256(x))
```

**σ_i Input (FULLY LOCKED)**
```
offset  size  field
------  ----  --------------------------------------------------
0       32    txid_i
32      32    ConvergenceRoot (C)
64      32    B_{h-1}            (parent block hash, verbatim)
96      4     height (LE)        (payout height h)
TOTAL = 100 bytes EXACT
```

**Rules (MUST)**
```
len(input) MUST == 100
Otherwise → REJECT (bad-hash-length)
```

`B_{h-1}` is the hash of the parent block, taken verbatim from the `hashPrevBlock`
field of the header being validated. `height` is the height of the block that pays
the RES mint (`h`), not the height of the block that committed the drawn set (`h-1`).

**FORBIDDEN:**
- padding
- folding `B_{h-1}` through any further hash before placing it at offset 64
- substituting an ancestor at any depth other than 1
- optional fields
- input reordering

Superseded by SECURITY-001-A8 (Sigma-PoW-01): slot `[64..95]` previously held
`bind_commit = Hash256(B_{h-10} || LE32(target_prev))`. `bind_commit` and the separate
`target_prev` σ input no longer exist. `target_prev` survives only as a RES mint witness
field, checked against the parent header's `nBits`; it does not feed σ. Committing the
parent header's difficulty is now implicit, because `B_{h-1}` is a hash over that header
and the header contains its own `nBits`.

**Normative implementation:** `core/src/consensus_rigor/sigma_hash.h`.

---

## 5 SEED BLOCK

```
seed = B_{h-1} = hash of the parent block
```

**Binding rule (consensus-locked):**

- The σ seed MUST be the hash of the immediate parent of the block that pays the mint.
  For a block at height `h`, that is the block at height `h - 1`.
- Defined at every height `h >= 1`. There is no height at which the seed is undefined, so
  there is no bootstrap case and no stand-in constant.
- The draw resolved at height `h` runs over the convergence set `F` committed by block
  `h - 1`, with `C` the convergence root that block committed. Block `h - 1` **commits**
  the set; block `h` **pays** its winner. A block therefore plays two independent roles:
  it commits its own convergence set in its coinbase, and it pays the winner of the set
  committed by its parent.
- The producer of block `h` supplies no input to σ. `txid_i` and `C` are committed by the
  parent's merkle root and `B_{h-1}` is the output of the parent's proof of work, so
  changing any σ input costs one full proof-of-work solution.

Superseded by SECURITY-001-A8 (Sigma-PoW-01): this section previously fixed a bind depth
`k = 10` with `B_{h-10}` as the seed, and defined an all-zero 32-byte bootstrap stand-in
(`RFC000BootstrapBindBlockHash`) for heights `1 <= h < k`. The constant, its header
`core/src/consensus/bootstrap_bind.h`, its implementation map `core/doc/rfc000_bootstrap_bind.md`
and its unit tests are all deleted. No `bad-height` rejection for a missing bind ancestor
exists any more, because no ancestor beyond the parent is consulted.

---

## 6 CONVERGENCE ROOT

```
C = Hash256(concat(sort(txid_i)))
```

**Sorting Rules (FULLY LOCKED)**
```
Sort using:
unsigned byte-wise lexicographical order
Compare:
uint8_t per byte
```

**FORBIDDEN:**
- signed compare
- string compare
- locale

---

## 7 WEIGHT DEFINITION

```
f_i MUST be:
RES Mint  → Mint Cost
RES Spend → transaction fee
```

**Constraints**
```
f_i MUST be integer
f_i MUST >= 1
```

**FORBIDDEN:**
- log(f)
- sqrt(f)
- exponent
- normalization

---

## 8 INTEGER ARITHMETIC

ALL math MUST be integer-only

**Rules (MUST)**
```
Multiplication MUST use arbitrary precision
```

**FORBIDDEN:**
- overflow
- float
- rounding

---

## 9 TIE-BREAK RULE

```
IF:
H_i * f_j == H_j * f_i
THEN:
txid_i < txid_j → winner
```

---

## 10 BOUNDARY CONDITIONS

```
IF N < 2:
→ winner = NONE

IF f_i == 0:
→ REJECT (bad-weight)

IF F = ∅:
→ C = Hash256(empty)
→ winner = NONE
```

---

## 11 DETERMINISTIC ORDERING

All comparisons MUST produce total ordering

---

## 12 STATE ISOLATION

Consensus depends ONLY on:
- block data
- transaction data

**FORBIDDEN:**
- mempool
- cache
- system time
- node policy

---

## 13 V1 CONTAINER LAW

```
ScriptPubKey = OP_1 <32 bytes>
semantic = f(input_asset_domain)
```

**FORBIDDEN:**
- Taproot
- MAST
- control block
- script path

---

## 14 DUAL CHANNEL ISOLATION

```
Channel A: ORU → RES Mint
Channel B: RES → Spend
```

**FORBIDDEN:**
- witness determines semantics
- script determines semantics

---

## 15 PROOF ≠ EXECUTION

Witness is proof-only

---

## 16 EXECUTION TRACE

**Each candidate:**
```json
{
  "txid": "...",
  "f_i": "...",
  "H_i": "...",
  "sigma": "...",
  "eligible": true/false
}
```

**Global:**
```json
{
  "C": "...",
  "winner_index": n | null
}
```

**Rules (MUST)**
```
Trace MUST be identical across all implementations
Otherwise → INVALID
```

---

## 17 REJECT CODES

- bad-encoding
- bad-hash-length
- bad-weight
- bad-height
- bad-selection
- bad-root
- bad-boundary

---

## 18 DETERMINISM

All execution MUST be:
- deterministic
- stateless
- reproducible

---

## 19 REPLAY REQUIREMENT

Given identical inputs:
ALL nodes MUST produce:
- identical C
- identical H_i
- identical winner

---

## 20 NOMENCLATURE LOCK

Names are part of consensus.

**Rules (MUST)**
- Sigma           → randomness only
- SelectionProof  → selection proof
- ConvergeProof   → mint proof
- ORU             → energy unit
- RES             → economic asset
- Signature       → identity

**FORBIDDEN:**
- alias
- synonym
- reinterpretation

### 20.1 Chain Identity Lock

```
chain_name MUST be: "RIGOR"
```

This value is informational only.
It MUST NOT be used in:
- hashing
- serialization
- consensus logic

### 20.2 Bech32 HRP Lock

```
bech32_hrp MUST be: "RG"
```

This applies to:
- address encoding
- wallet display

It MUST NOT be used in:
- transaction validation
- script execution
- consensus rules

**FORBIDDEN:**
- using HRP inside hash input
- using chain_name in consensus
- deriving logic from address prefix

**NOMENCLATURE LOCK**
Changing role of a name
= CONSENSUS VIOLATION

---

## 21 IMPLEMENTATION COMPLIANCE

Valid implementation MUST:
1. Byte-identical encoding
2. Deterministic hashing
3. Integer-only math
4. Identical ordering
5. Identical trace

---

## 22 FORBIDDEN LIST

- Taproot
- MAST
- Script VM
- Floating math
- Dynamic rules
- Heuristics
- Time dependence
- Hidden state

---

## 23 FINAL SENTENCE

This protocol is:
- not programmable
- not extensible
- not upgradable

This is a closed deterministic selection system.
ANY deviation = consensus failure.

---

**END OF RFC-000-FINAL**
