# RFC-FEE-RATE-AUCTION-CONSENSUS-MODEL

**Title:** Fee Rate Auction and Consensus Weight (`f_i`) Model  
**Status:** FINAL LOCK (normative specification)  
**Scope:** Transaction fee identity, wire-size metric, mempool / block-assembly auction, and consensus weight `f_i`  
**Non-scope:** Consensus code changes, wallet implementation, mempool implementation, mining implementation  
**Related:** `docs/rfc/RFC-000-consensus-constitution.md` §7 (weight skeleton), RFC-001 (selection inequality), RFC-008 (mathematical form), RFC-014 (`T`); informative wallet alignment note: `docs/audit/RIGOR_FEE_FINAL_LOCK_ALIGNMENT.md` (when present)

---

## 1. Motivation

RIGOR separates two economic surfaces that Bitcoin conflates under a single “fee / feerate” UX:

1. **Network fee identity** — how much ORU a transaction burns as fee, measured on serialized wire bytes.
2. **Consensus weight `f_i`** — the integer that enters the draw inequality `H_i < T · f_i` and winner cross-multiplication.

Without a locked specification:

- Bitcoin **weight / vsize / witness discount** semantics can leak into fee ranking.
- Floating-point feerate can diverge across nodes.
- `fee_rate` can be mistakenly treated as a consensus input.
- `f_i` can be transformed (log, β-power, scaling, normalization), breaking deterministic selection.

This RFC freezes the finalized Fee Rate Auction model and the consensus `f_i` binding so documentation, tests, and future reviewers share one normative source.

---

## 2. Fee Model Definition

### 2.1 Identity

For every non-coinbase transaction `i`:

```
fee_i = sum(inputs_ORU) − sum(outputs_ORU)
```

Where:

- `inputs_ORU` is the sum of `nValue` over prevouts whose `scriptPubKey` classifies as ORU.
- `outputs_ORU` is the sum of `nValue` over outputs whose `scriptPubKey` classifies as ORU.
- RES inputs and RES outputs **MUST NOT** enter the fee identity.

### 2.2 Type and unit constraints

| Constraint | Requirement |
|---|---|
| Unit | Minimum ORU unit (satoshi-equivalent base unit) |
| Type | `uint64` |
| Sign | `fee_i MUST be >= 0` |
| Zero fee | `fee_i == 0` transactions **MUST NOT** enter candidate transaction selection (Fee Rate Auction candidate set) |

If `sum(inputs_ORU) < sum(outputs_ORU)`, the fee identity is undefined for a valid spend; such a transaction is invalid for ORU conservation and **MUST NOT** be treated as having a negative fee.

### 2.3 ORU-only fee asset policy

- Network fee **MUST** be denominated exclusively in ORU.
- RES **MUST NOT** be used as fee.
- Multi-asset fee splits **MUST NOT** be defined.

This matches the dual-pool spend rule: RES transfers conserve RES value; ORU inputs pay `fee_i`.

---

## 3. Transaction Size Definition

### 3.1 Definition

```
tx_size_bytes = serialized network transaction byte length
```

That is: the exact byte length of the transaction under the network wire serialization used on the p2p / block body path (canonical serialize with protocol version).

### 3.2 Forbidden size metrics

The following **MUST NOT** be used for fee rate, mempool ordering, or block packing under this RFC:

- Bitcoin **weight**
- Bitcoin **virtual size (vsize)**
- Witness discount / segregated-witness size multipliers
- Any size multiplier, discount factor, or “effective size” transform

`tx_size_bytes` is an exact integer byte count. No approximation.

---

## 4. Fee Rate Auction Mechanism

### 4.1 Fee rate (non-consensus)

```
fee_rate_i = fee_i / tx_size_bytes
```

`fee_rate_i` is a **conceptual** ratio used only to describe ordering. Implementations **MUST NOT** compute it with floating-point arithmetic.

### 4.2 Role boundary

| Use | Allowed? |
|---|---|
| Mempool ordering | YES |
| Block assembly / packing preference | YES |
| Consensus validity | **NO** |
| Convergence eligibility `H_i < T · f_i` | **NO** |
| Winner selection (cross-multiply on `H_i`, `f_i`) | **NO** |

**Rule:** Fee Rate Auction packs blocks. Consensus selects winners using `f_i`, never `fee_rate`.

### 4.3 Integer comparison (mandatory)

Transaction `a` is better than (outranks) transaction `b` if and only if:

```
a.fee * b.size > b.fee * a.size
```

Where:

- `a.fee`, `b.fee` are `uint64` ORU fee amounts (`fee_i`)
- `a.size`, `b.size` are `tx_size_bytes` (positive integer byte lengths)
- Multiplication **MUST** use sufficient integer width to avoid overflow (e.g. 128-bit intermediate)

**FORBIDDEN:** floating-point division, floating-point comparison, rounding of ratios.

---

## 5. Mempool Ordering Rules

1. Candidate transactions for the Fee Rate Auction **MUST** satisfy `fee_i >= 1` (equivalently: `fee_i == 0` **MUST NOT** enter the candidate set).
2. Ordering **MUST** be by descending fee rate under the integer cross-multiply of §4.3.
3. Tie-breaking among equal fee-rate comparisons is an implementation / policy detail of the mempool and **MUST NOT** become a consensus rule under this RFC.
4. Mempool acceptance and eviction heuristics that use fee rate remain **non-consensus**.

---

## 6. Block Construction Rules

1. Block assemblers **MAY** prefer higher fee-rate transactions when selecting which valid transactions to include, subject to block weight / size limits defined elsewhere.
2. Inclusion preference by fee rate **MUST NOT** alter:
   - transaction consensus validity,
   - convergence set membership rules,
   - eligibility inequality evaluation,
   - winner selection.
3. Once a block is formed, consensus validation depends only on block/transaction bytes and UTXO state — not on the assembler fee-rate sort key.
4. Coinbase and consensus-mandated body artifacts (e.g. RES body mint attachment where required by other RFCs) are outside the Fee Rate Auction ranking of ordinary fee-paying spends.

---

## 7. Consensus `f_i` Definition

### 7.1 Binding inequality (unchanged form)

For each candidate `i`:

```
H_i < T · f_i
```

With `T = RES_PROBABILITY_TARGET` as frozen by RFC-014. Winner order remains:

```
i ≺ j  iff  H_i · f_j < H_j · f_i
```

(exact integer cross-multiplication; no division).

### 7.2 Source of `f_i`

```
f_i =
  RES Mint:   Convergence amount
  RES Spend:  transaction fee (fee_i)
```

| Case | Definition of `f_i` | Unit |
|---|---|---|
| RES Mint | Convergence amount committed by the mint / converge path | Minimum ORU unit, `uint64` |
| RES Spend | `fee_i` as defined in §2 | Minimum ORU unit, `uint64` |

RFC-000 §7’s phrasing (“Mint Cost” / “transaction fee”) is interpreted by this RFC as the table above.

### 7.3 Mandatory constraints

- `f_i` **MUST** be `uint64`
- `f_i` **MUST** be deterministic given the transaction bytes and committed UTXO prevouts
- `f_i` **MUST** enter the inequality and winner comparison **without transformation**
- `f_i == 0` is rejected for consensus weight purposes (`bad-weight` / equivalent)

### 7.4 Forbidden transforms on `f_i`

The following **MUST NOT** be applied to `f_i` before consensus use:

- scaling
- logarithm
- exponentiation / power (`f_i^β` or similar)
- normalization
- floating-point conversion
- fee-rate substitution (`fee_rate` **MUST NOT** replace `f_i`)

---

## 8. RES Mint Handling

1. Classify a transaction as RES Mint when the mint path applies (ORU-domain feedstock producing RES mint outcomes under consensus mint rules).
2. Set `f_i = Convergence amount` (untransformed integer amount in minimum ORU units).
3. Network fee for the minting transaction, if any, remains ORU under §2 but **does not** redefine `f_i` for a RES Mint candidate: weight is convergence amount, not fee rate.
4. Eligibility and winner selection use §7 only.

---

## 9. RES Spend Handling

1. Classify a transaction as RES Spend when RES value is transferred under dual-pool rules (RES conservation + ORU fee inputs).
2. Compute `fee_i` per §2 (ORU-only).
3. Set `f_i = fee_i`.
4. `fee_rate_i` may influence whether the spend is packed into a block template; it **MUST NOT** enter `H_i < T · f_i`.

---

## 10. Determinism Requirements

| Object | Determinism rule |
|---|---|
| `fee_i` | Exact ORU conservation difference; same prevouts ⇒ same fee |
| `tx_size_bytes` | Exact wire serialize size; same tx bytes ⇒ same size |
| Fee-rate order | Same (`fee`, `size`) pairs ⇒ same cross-multiply comparison |
| `f_i` | Exact mint/spend classification + §7 source; no locality / time / mempool state |
| Consensus inequality | Integer-only; identical across all full nodes |

Non-determinism in mempool arrival time or relay gossip **MUST NOT** affect consensus evaluation of included transactions.

---

## 11. Security Considerations

1. **Consensus isolation of fee rate** — Allowing `fee_rate` into eligibility or winner selection would couple block packing strategy to consensus outcomes and enable assembler-discretionary bias beyond committed `f_i`.
2. **Integer cross-multiply** — Floating-point feerate comparison can disagree across platforms and create divergent local views of “best” transactions; for consensus weights, float is already forbidden by RFC-000.
3. **ORU-only fee** — RES-as-fee would mix asset domains and undermine conservation checks that keep RES issuance consensus-bound.
4. **Zero-fee exclusion from auction candidates** — Prevents free relay / free packing abuse in the auction surface; consensus still evaluates included txs by their own rules.
5. **No protocol minimum fee / multiplier** — Dynamic or governance-set fee parameters become soft forks of economic policy and attack surfaces for coercive fee floors.

---

## 12. Forbidden Future Modifications

The following are **FORBIDDEN** without a deliberate, separately ratified hard-fork RFC that explicitly supersedes this document:

| Forbidden change | Rationale |
|---|---|
| `fee_rate` entering consensus | Breaks §4.2 / §7 isolation |
| Floating-point arithmetic for fee rate or `f_i` | Non-determinism |
| Dynamic fee parameters | Soft economic governance in consensus path |
| Minimum protocol fee | Protocol-enforced rent; not part of FINAL LOCK |
| Fee multiplier / fee class / fee bucket | Hidden transforms of economic weight |
| EIP-1559-style base fee / elasticity | Account-like fee market foreign to UTXO auction |
| Bitcoin weight / vsize inheritance for fee sizing | Witness discount reintroduces non-byte size politics |
| Log / β / normalize / scale on `f_i` | Breaks deterministic selection fairness |
| RES or multi-asset network fee | Violates ORU-only fee asset policy |
| Parallel `asset` / fee fields on `CTxOut` | Violates script-is-compliance-layer storage invariant |

---

## 13. Normative Summary

```
fee_i          := sum(ORU in) − sum(ORU out)     // uint64, >= 0
tx_size_bytes  := wire serialize byte length     // no weight/vsize
fee_rate_i     := fee_i / tx_size_bytes          // conceptual; integer compare only
a ≻_auction b  := a.fee * b.size > b.fee * a.size
f_i (mint)     := Convergence amount             // consensus
f_i (spend)    := fee_i                          // consensus
eligible       := H_i < T · f_i                  // never uses fee_rate
```

Fee Rate Auction orders inclusion. Consensus weights probability. The two surfaces **MUST** remain separate.

---

## 14. Document Control

| Field | Value |
|---|---|
| Path | `docs/rfc/RFC-FEE-RATE-AUCTION-CONSENSUS-MODEL.md` |
| Task | `TASK-RFC-FEE-RATE-AUCTION-CONSENSUS-MODEL-AUDIT-001` |
| Implementation reference (informative) | `core/src/consensus_rigor/fee.h`, `fee.cpp` |
| Consensus code modified by this RFC | **NONE** (documentation only) |
