# SPEC-003: RIGOR Consensus Rules

**Task:** `TASK-SPEC-004`  
**Track:** Formal Specification Program  
**Status:** ESTABLISHED  
**Baseline:** `mainnet-freeze-rfc015-2026-05-26`  
**Parents:** [SPEC-000](./SPEC-000-CONSENSUS-SPECIFICATION.md), [SPEC-001](./SPEC-001-STATE-MODEL.md), [SPEC-002](./SPEC-002-ECONOMIC-MODEL.md)  
**Normative sources:** `RFC-000`, `RFC-008`, `RFC-009`, `RFC-014`, `RFC-015`  
**Freeze authority:** `MAINNET_PROTOCOL_FREEZE_DECLARATION.md` / `TASK-MAINNET-FREEZE-001`

---

## Document Role

This document is the **authoritative specification of RIGOR consensus validation
rules**. It states what constitutes a valid block, what constitutes a valid chain
transition, and which consensus predicates MUST hold before `State(h+1)` may exist.

This document is specification-only:

- No code changes
- No consensus changes
- No proofs
- **Equivalence Claimed:** false

---

## 1. Purpose

**Consensus Rules** determine whether the transition

```
Transition(State(h), Block(h+1))
```

is permitted.

A block at height `h+1` is **valid** if and only if **all** consensus predicates
in this document succeed when evaluated against `State(h)` and `Block(h+1)`.

If any predicate fails, the block is **rejected** and `State(h+1)` does not exist.
`State(h)` remains the sole authoritative consensus state.

---

## 2. Consensus Validation Layers

Consensus validation is a **strictly ordered hierarchy**. Each layer MUST succeed
before the next layer is evaluated. Reordering layers is a specification violation.

| Layer | Name | Scope |
|-------|------|-------|
| **1** | Header Validation | Block header structure, PoW, difficulty, timestamps |
| **2** | Chain Context Validation | Height, parent linkage, frozen parameters, protocol version |
| **3** | Transaction Validation | UTXO spends, conservation, script, coinbase |
| **4** | ORU Supply Validation | Subsidy, converge sum, ORU recurrence (see SPEC-002) |
| **5** | RES Draw Validation | Candidate set, eligibility, winner, mint authorization (RFC-015) |
| **6** | State Transition Validation | Deterministic successor, UTXO and supply recurrence (see SPEC-001) |
| **7** | Commit Validation | Persist successor state, update chain tip, replay/reorg consistency |

**Normative evaluation order:**

```
Commit ∘ StateTransition ∘ RESDraw ∘ ORUSupply ∘ Transactions ∘ ChainContext ∘ Header
```

Invalidity at any layer aborts validation immediately. No partial successor state
is produced.

---

## 3. Header Validation Rules

### CR-001: Valid Header Structure

**Purpose:** Ensure the block header is well-formed under the frozen serialization
and field layout rules.

**Failure condition:** Header cannot be parsed; required fields are absent or
malformed; merkle root does not commit to the block's transaction list; or header
encoding violates canonical byte layout.

---

### CR-002: Previous Block Exists

**Purpose:** Ensure the block extends an existing block on the active chain.

**Failure condition:** The header's parent hash does not match the hash of the
active tip at height `h`, or the parent block is not present in the committed
chain history.

---

### CR-003: Proof-of-Work Satisfied

**Purpose:** Ensure the block demonstrates sufficient work under the parent's
difficulty context.

**Failure condition:**

```
Hash256(header) ≥ ExpandCompact(nBits)
```

where `nBits` is the compact target encoded in the block header and PoW evaluation
uses the frozen integer comparison rules.

---

### CR-004: Difficulty Adjustment Valid

**Purpose:** Ensure the header's compact target is the correct ASERT output given
the parent's difficulty state and header times.

**Failure condition:** The header's `nBits` value differs from the ASERT update
computed from the frozen anchor parameters, parent compact target, and timestamp
constraints.

---

### CR-005: Timestamp Constraints

**Purpose:** Ensure header time satisfies median-time-past and related frozen
timestamp predicates relative to the active chain.

**Failure condition:** Header time violates any frozen timestamp rule (e.g., too
far in the future relative to median-time-past, or violates minimum spacing rules
where applicable).

---

## 4. Chain Context Rules

### CR-010: Height Continuity

**Purpose:** Ensure the candidate block extends the chain at the next consecutive
height.

**Failure condition:** Candidate block height is not exactly `h + 1` relative to
the active tip at height `h`.

---

### CR-011: Chain Continuity

**Purpose:** Ensure the block is a direct extension of the active chain with
unbroken parent linkage and monotonically increasing cumulative chainwork.

**Failure condition:** Parent hash mismatch; fork that does not satisfy chainwork
selection rules; or cumulative chainwork does not strictly increase upon extension.

---

### CR-012: Frozen Parameter Consistency

**Purpose:** Ensure validation uses only the immutable constitutional constant set.

**Failure condition:** Any predicate references a non-frozen parameter value,
including but not limited to:

- PoW-derived convergence inequality RHS (variable-T)
- Deprecated symbols for convergence target
- Pre-RFC-015 economics (`|F| ≥ 2`, 1 RES per draw, superseded cap)
- Non-genesis activation bridges or dual-rule coexistence

Frozen parameters include at minimum: `RES_PROBABILITY_TARGET = 2^240`, `k = 10`,
`RES_REWARD_AMOUNT`, `RES_MAX_SUPPLY`, draw gate `|F| ≥ 1`, bootstrap bind hash
for `h < k`, and genesis-only activation.

---

### CR-013: Protocol Version Consistency

**Purpose:** Ensure block and transaction formats conform to the frozen post-freeze
rule bundle (RFC-014 + RFC-015 mainnet semantics).

**Failure condition:** Block validity depends on superseded rule sets, unsupported
witness versions, or protocol features outside the frozen scope (e.g., Taproot
semantics in convergence proofs, account-balance models).

---

## 5. Transaction Rules

### CR-020: UTXO Exists

**Purpose:** Every non-coinbase input MUST spend an existing unspent output.

**Failure condition:** Any input references an outpoint `(txid, index)` not present
in `UTXO` at height `h`.

---

### CR-021: No Double Spend

**Purpose:** No output may be spent more than once within a block or across the
active UTXO set.

**Failure condition:** Duplicate input outpoints within the block, or spend of an
output already consumed by an earlier transaction in the same block.

---

### CR-022: Value Conservation

**Purpose:** Asset quantities MUST be conserved across non-coinbase transactions.

**Failure condition:** For any valid non-coinbase transaction, input sums do not
equal output sums plus fees within the applicable asset domain; or any output
value is negative.

---

### CR-023: Asset Class Derivable

**Purpose:** Asset *class* MUST be inferred from `scriptPubKey` and witness structure
(script as compliance layer). Script classification does not replace the remaining
consensus checks that establish legitimacy for that class.

**Failure condition:** An output's asset class cannot be derived from `scriptPubKey`
and witness structure; or consensus relies on a persisted asset field parallel to
script storage.

---

### CR-024: Script Validity

**Purpose:** Every spend MUST satisfy the frozen script and witness verification
rules for its asset class, including witness canonicality.

**Failure condition:** Script verification fails; witness encoding is non-canonical
where canonicality is required; Converge proof leaf violates Leaf-Freeze-01;
RES mint proof fields do not match canonical serialized form; or signature
verification fails over committed fields.

---

### CR-025: Coinbase Uniqueness

**Purpose:** Exactly one coinbase transaction exists per block and it satisfies
subsidy rules.

**Failure condition:** Zero or multiple coinbase transactions; coinbase value does
not match the frozen subsidy rule for height `h+1`; or coinbase structure violates
frozen layout rules.

---

## 6. ORU Supply Rules

These rules refine [SPEC-002](./SPEC-002-ECONOMIC-MODEL.md) as consensus predicates.

### CR-030: Subsidy Valid

**Purpose:** The coinbase MUST credit exactly the ORU block subsidy `R(h+1)` defined
by the frozen subsidy schedule.

**Failure condition:** Coinbase subsidy at height `h+1` differs from `R(h+1)`, or
violates any minimum tail subsidy constraint defined by the frozen schedule.

---

### CR-031: Converge Burn Valid

**Purpose:** ORU destroyed by Converge transactions MUST be fully and exactly accounted.

**Failure condition:** `C(h+1)` does not equal the sum of ORU amounts destroyed by
all valid Converge transactions in the block; or a Converge transaction destroys
less than `MIN_CONVERGENCE_AMOUNT` (1 ORU).

---

### CR-032: ORU Supply Recurrence Preserved

**Purpose:** Cumulative ORU supply MUST satisfy the authoritative recurrence.

**Failure condition:** After applying block `h+1`:

```
S_ORU(h+1) ≠ S_ORU(h) + R(h+1) − C(h+1)
```

---

### CR-033: ORU Supply Non-Negative

**Purpose:** ORU supply MUST never become negative.

**Failure condition:** `S_ORU(h+1) < 0`.

---

## 7. RES Draw Rules

These rules implement the frozen convergence draw semantics (RFC-015 economics
within RFC-014 probability law).

### CR-040: Draw Eligibility

**Purpose:** Define which convergence candidates participate in the draw and which
may be selected.

Let `F` be the finite set of mint-classified candidate transaction identities in
the block. For each candidate `i` with weight `f_i ≥ 1`:

```
bind_commit = Hash256(B_{h-k} ‖ LE32(target_prev))
σ_i       = Hash256(txid_i ‖ C ‖ bind_commit ‖ LE32(h+1))    // 100 bytes exact
H_i       = uint256 interpretation of σ_i
```

**Eligibility predicate:**

```
eligible_i ⟺ |F| ≥ 1  ∧  f_i ≥ 1  ∧  H_i < RES_PROBABILITY_TARGET · f_i
```

where `RES_PROBABILITY_TARGET = 2^240` and comparison uses exact integer arithmetic
(cross-multiplication permitted; floating-point forbidden).

**Failure condition:** Eligibility evaluated with `|F| < 1` as sufficient; `f_i < 1`;
σ preimage length ≠ 100 bytes; incorrect `bind_commit` or height slot; or
inequality uses any source other than `RES_PROBABILITY_TARGET` for the RHS.

---

### CR-041: Draw Determinism

**Purpose:** Given identical `State(h)` and `Block(h+1)`, draw outputs MUST be unique.

Draw outputs include: sorted candidate set `F`, convergence root `C`, digests
`H_i`, eligibility flags, and winner selection outcome.

**Failure condition:** Two correct validators produce different `C`, different
`H_i`, different eligibility sets, or different winners for the same inputs.

---

### CR-042: Single Winner

**Purpose:** At most one winner is selected by deterministic argmin under the
cross-multiply order.

**Winner order:** `i` precedes `j` iff `H_i · f_j < H_j · f_i` using exact
integer multiplication. Tie-break: smaller `txid` (unsigned byte-wise order).

**Failure condition:** Multiple winners selected; winner is not the unique argmin;
or tie-break does not use txid order.

---

### CR-043: Single Mint

**Purpose:** At most one successful RES mint per block height.

**Failure condition:** Block contains two or more successful RES mints; or `M(h+1) ∉ {0,1}`.

---

### CR-044: Mint Amount Exact

**Purpose:** A successful RES mint MUST credit exactly `RES_REWARD_AMOUNT`.

**Failure condition:** RES mint exists but credited amount ≠ `RES_REWARD_AMOUNT`
(100 RES = 10,000 atoms).

---

### CR-045: RES Cap Preserved

**Purpose:** RES supply MUST not exceed the constitutional maximum.

**Failure condition:**

```
S_RES(h+1) > RES_MAX_SUPPLY
```

or mint attempted when `S_RES(h) = RES_MAX_SUPPLY`.

---

### CR-046: Draw Not Derived From PoW

**Purpose:** PoW difficulty MUST NOT define convergence probability.

**Failure condition:** Any draw eligibility or winner predicate uses `nBits`, ASERT
output, tip target, time, or any header-derived quantity as the inequality RHS
in place of `RES_PROBABILITY_TARGET`. PoW-derived `target_prev` is permitted
**only** inside `bind_commit` for replay-domain separation.

---

## 8. State Transition Rules

These rules refine [SPEC-001](./SPEC-001-STATE-MODEL.md) as consensus predicates.

### CR-050: Deterministic Transition

**Purpose:** The successor state MUST be a pure function of `(State(h), Block(h+1))`
when the block is valid.

**Failure condition:** Two correct validators accept the same block but produce
different `State(h+1)`; or validation depends on non-deterministic inputs (unordered
iteration, local clock, network order, caches, observational exports).

---

### CR-051: UTXO Consistency

**Purpose:** The post-transition UTXO set MUST equal the result of applying all
valid spends and creates in the block to `UTXO` at `h`.

**Failure condition:** Post-transition UTXO contains spent outputs, omits created
outputs, or disagrees with the spend/create semantics of valid transactions.

---

### CR-052: State Recurrence Valid

**Purpose:** Supply components MUST satisfy authoritative recurrences (SPEC-002).

**Failure condition:**

```
S_ORU(h+1) ≠ S_ORU(h) + R(h+1) − C(h+1)
```
or
```
S_RES(h+1) ≠ S_RES(h) + RES_REWARD_AMOUNT · M(h+1)
```

---

### CR-053: Invariants Preserved

**Purpose:** All consensus-critical state invariants (SPEC-001 §8, SI-001…SI-013)
MUST hold at `h+1` if they held at `h` and the block is valid.

**Failure condition:** Any SI invariant violated at `State(h+1)` that is implied
by valid transition from `State(h)` (e.g., chain continuity, script-as-compliance,
identity domain, draw–mint coherence).

---

## 9. Commit Rules

### CR-060: State Commit

**Purpose:** Upon successful validation, all consensus-critical components of
`State(h+1)` MUST be atomically committed.

**Failure condition:** Partial commit visible to validation (e.g., UTXO updated but
supply counters not, or draw outcome committed without corresponding mint rule check).

---

### CR-061: Chain Tip Update

**Purpose:** The active chain tip MUST advance to height `h+1` with the new block
hash, updated chainwork, and updated PoW context.

**Failure condition:** Tip height, tip hash, or chainwork does not reflect the
accepted block; or tip advances without all prior layers succeeding.

---

### CR-062: Replay Consistency

**Purpose:** Re-executing blocks `B(1..h+1)` from genesis MUST yield identical
`State(h+1)` on all correct validators.

**Failure condition:** Replay of the same block sequence produces divergent
`State(h+1)` across validators or platforms.

---

### CR-063: Reorg Consistency

**Purpose:** Switching to an alternate valid chain with greater work MUST produce
the same `State(h')` as sequential application of that chain from genesis.

**Failure condition:** Post-reorg state differs from sequential replay of the
winning chain; or reorg preserves observational artifacts but not authoritative
consensus fields.

---

## 10. Consensus Failure Model

If **any** rule **CR-001** through **CR-063** fails when evaluating
`Transition(State(h), Block(h+1))`:

1. **Block(h+1) is rejected.**
2. **State(h+1) does not exist.**
3. **State(h) remains authoritative.**
4. **No partial transition exists** — no layer may commit successor fields while
   leaving others at `State(h)`.

Rejection is total and atomic. Observational exports, runtime mirrors, and audit
witnesses MUST NOT alter this model.

---

## 11. Relationship to Formal Verification

```
Consensus Rules (this document)
        │  defines: CR-* predicates, layer order, failure model
        ▼
Formal Consensus Predicates (docs/formal/, executable oracle)
        │  defines: machine-readable validation conditions
        ▼
Coq Theorems (formal_verification/coq/)
        │  proves: predicates imply safety, determinism, supply properties
        ▼
Runtime Witness Alignment (Phase 5, FVR track)
           evidence: mirrors and exports; equivalence_claimed = false until proven
```

**This document is authoritative.**

Formal models and proofs refine these rules; they do not replace them. Runtime
alignment is evidence only and does not redefine validity.

---

## 12. Relationship to Runtime Refactor

The Formal-Friendly Runtime Refactor track (FVR-A through FVR-N) introduced
runtime supply mirrors, witness chains, invariant checkers, catalog rows, CHAS
alignment exports, equivalence candidates, closure gates, and Phase-5 summary
snapshots.

**Binding statement:** FVR-A through FVR-N produce **evidence only**. They do
**not** participate in CR-* decisions.

| Property | Status |
|----------|--------|
| FVR artifacts read by validation predicates | **FORBIDDEN** |
| FVR invariant failure causes block rejection | **FORBIDDEN** (mainnet) |
| FVR exports affect `State(h+1)` definition | **FORBIDDEN** |
| FVR role | Audit and alignment evidence only |

---

## 13. Freeze Compatibility

This document is **specification only**. It documents existing frozen rules; it
does not introduce new ones.

| Guarantee | Status |
|-----------|--------|
| No consensus modifications | **Confirmed** |
| No monetary-policy modifications | **Confirmed** |
| No protocol-freeze modifications | **Confirmed** |

Nothing in this document supersedes `MAINNET_PROTOCOL_FREEZE_DECLARATION.md` or
`TASK-MAINNET-FREEZE-001`.

---

## 14. Task Output Status

| Field | Status |
|-------|--------|
| Consensus Rules Specification | **ESTABLISHED** |
| Header Rules | **ESTABLISHED** |
| Transaction Rules | **ESTABLISHED** |
| ORU Rules | **ESTABLISHED** |
| RES Rules | **ESTABLISHED** |
| State Transition Rules | **ESTABLISHED** |
| Consensus Impact | **NONE** |
| Equivalence Claimed | **false** |

---

## 15. Revision History

| Version | Date | Change |
|---------|------|--------|
| 0.1 | 2026-06-05 | `TASK-SPEC-004` — initial consensus rules specification |
