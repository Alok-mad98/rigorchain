# SPEC-003: RIGOR Consensus Rules

**Document status:** Production specification  
**Track:** Formal Specification Program  
**Status:** ESTABLISHED  
**Baseline:** RFC-020 shipping issuance (continuous `destroy_cost(h)`, flat 128 ORU)  
**Parents:** [SPEC-000](./SPEC-000-CONSENSUS-SPECIFICATION.md), [SPEC-001](./SPEC-001-STATE-MODEL.md), [SPEC-002](./SPEC-002-ECONOMIC-MODEL.md)  
**Normative sources:** `RFC-000`, `RFC-008`, `RFC-009`, `RFC-020`  
**Freeze authority:** `MAINNET_PROTOCOL_FREEZE_DECLARATION.md` / `historical verification record`

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
| **5** | RES Mint Validation | Body-tx destroy-to-mint, `CheckResMintArithmetic`, closed-form cap (RFC-020) |
| **6** | State Transition Validation | Deterministic successor, UTXO and supply recurrence (see SPEC-001) |
| **7** | Commit Validation | Persist successor state, update chain tip, replay/reorg consistency |

**Normative evaluation order:**

```
Commit ∘ StateTransition ∘ RESMint ∘ ORUSupply ∘ Transactions ∘ ChainContext ∘ Header
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
OpuHash(SHA256d(header)) ≥ ExpandCompact(nBits)
```

where `nBits` is the compact target encoded in the block header, `OpuHash` is the
frozen mix described in `core/doc/opushash_v3.md`, and PoW evaluation uses the
frozen integer comparison rules. Genesis is exempt from the mix-loop check after
a valid compact-target encoding.

---

### CR-004: Difficulty Adjustment Valid

**Purpose:** Ensure the header's compact target is the correct ASERT output given
the parent header (not the candidate's own timestamp).

**Failure condition:** The header's `nBits` value differs from aserti3-2d computed
from:

- anchor = parent if parent height is 0, otherwise the height-1 block
- `eval_time` / `eval_height` = parent `nTime` / parent height
- per-block target clamp of `[0.875, 1.125]` versus the parent compact target
- `powLimit` as an absolute ceiling

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
including a PoW-derived mint price (`nBits` as a mint price) or a non-genesis
activation bridge.

Frozen parameters include at minimum: `RES_COIN = 100000000`,
`ORU_FLAT_BLOCK_SUBSIDY = 12800000000`, `RES_DESTROY_COST_GENESIS = 102400000000`,
`RES_DESTROY_POLE = 33554432`, `RES_DESTROY_SHIFT = 40`,
closed-form `RES_MAX_SUPPLY = 419430400000000`,
mint predicate `R > 0 → R · (2^25 + h)^2 ≤ B << 40`, and genesis-only activation.

---

### CR-013: Protocol Version Consistency

**Purpose:** Ensure block and transaction formats conform to the frozen post-freeze
rule bundle (RFC-020 mainnet semantics).

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
Witness v1 RES mint outputs fail classification; or signature verification
fails over committed fields.

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

### CR-031: Converge Destroy Valid

**Purpose:** ORU destroyed by Converge transactions MUST be fully and exactly accounted.

**Failure condition:** `C(h+1)` does not equal the sum of ORU amounts destroyed by
all valid Converge transactions in the block; or a Converge transaction destroys
a non-positive amount (`nValue ≤ 0`). Minting `R ≥ 1` is rejected separately when
`R · (2^25 + h)^2 > B << 40`.

---

### CR-032: ORU Supply Recurrence Preserved

**Purpose:** Observational issued-minus-converge accounting must not be confused
with spendable UTXO ORU.

**Failure condition:** Using an extra-UTXO ORU scalar as a ConnectBlock reject.
Circulating ORU is the UTXO projection. RFC-018 destroys and unclaimed coinbase
are destruction channels that the issued-minus-converge identity does not list.

---

### CR-033: Amount Overflow Bound

**Purpose:** No output, input-sum, or fee/converge accumulator may be negative
or exceed `MAX_MONEY`. This is an overflow sanity bound (`10_000_000_000 * COIN`),
**not** an ORU issuance cap.

The value is bounded on both sides. Upward, cumulative issuance must stay under
it for the life of the chain (~1485 years at the 128 ORU/block subsidy). Downward,
it must leave room to *add* two in-range amounts without leaving `int64`:
`MAX_MONEY ≤ INT64_MAX / 4` is asserted at compile time. A bound near `INT64_MAX`
would make two individually valid amounts unsummable, and every accumulator that
range-checks after adding would then overflow before its own check could reject.
Accumulators MUST therefore check with `AddMoneyOverflows(running, addend)`
*before* adding, never `running += addend; if (!MoneyRange(running))`.

**Failure condition:** `nValue < 0` or `nValue > MAX_MONEY` (and the same for
input sums / accumulators). Observational `S_ORU < 0` is not a consensus reason.

---

## 7. RES Mint Rules

These rules implement RFC-020 body-tx destroy-to-mint. Consensus evaluates
`CheckResMintArithmetic`, not the display destroy cost.

### CR-040: Mint Predicate

**Purpose:** A body transaction that creates RES MUST pay the confirming height’s
destroy cost.

Let `B` be the ORU amount of the transaction’s single `OP_CONVERGE` destroy and
`R` the sum of its Witness v1 RES outputs. Consensus never evaluates the
display destroy cost. It evaluates:

```
R > 0  →  R · (2^25 + h)^2 ≤ B << 40
R = 0  →  unconstrained
```

evaluated by `CheckResMintArithmetic(R, B, h)`. Integer only; 128-bit
intermediate. Mint-output maturity 100 is reorg protection.

**Failure condition:** A body mint fails the predicate, or RES is created without
an `OP_CONVERGE` destroy.

---

### CR-041: Mint Determinism

**Purpose:** Given identical `State(h)` and `Block(h+1)`, mint accept/reject MUST
be unique.

The same body transactions at the same height MUST produce the same
`CheckResMintArithmetic` verdict and the same `body_mint_atoms` on every
correct validator.

**Failure condition:** Two correct validators accept one mint and reject the
same mint for the same `(R, B, h)`.

---

### CR-042: Price Independent of PoW

**Purpose:** `destroy_cost(h)` and the mint predicate are pure height functions. PoW /
`nBits` / ASERT MUST NOT define the destroy cost.

**Failure condition:** Any mint predicate uses `nBits`, ASERT output, tip target,
time, or any header-derived quantity in place of height `h`.

---

### CR-043: Plain Destroy and Non-Zero Price

**Purpose:** `R = 0` is a legal plain destroy. The display destroy cost never hits zero,
so there is no horizon that disables mint by a zero sentinel.

**Failure condition:** A plain destroy (`R = 0`) is rejected as a mint failure; or
a zero / negative destroy is read as authorizing unbounded RES.

---

### CR-044: Mint Amount Bounded by Inequality

**Purpose:** A successful mint MAY credit any `R ≥ 0` admitted by
`R · (2^25 + h)^2 ≤ B << 40` (or `R = 0`).

**Failure condition:** A mint is accepted when `R > 0` and
`R · (2^25 + h)^2 > B << 40`.

---

### CR-045: RES Cap Preserved

**Purpose:** RES supply MUST not exceed the closed-form constitutional maximum
`419430400000000` atoms (`2^22` RES).

**Failure condition:** `S_RES(h) > 419430400000000`; or a mint is accepted when
the cap is already reached.

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
S_RES(h+1) ≠ S_RES(h) + body_mint_atoms(h+1)
```

---

### CR-053: Invariants Preserved

**Purpose:** All consensus-critical state invariants (SPEC-001 §8, SI-001…SI-013)
MUST hold at `h+1` if they held at `h` and the block is valid.

**Failure condition:** Any SI invariant violated at `State(h+1)` that is implied
by valid transition from `State(h)` (e.g., chain continuity, script-as-compliance,
identity domain, mint-arithmetic coherence).

---

## 9. Commit Rules

### CR-060: State Commit

**Purpose:** Upon successful validation, all consensus-critical components of
`State(h+1)` MUST be atomically committed.

**Failure condition:** Partial commit visible to validation (e.g., UTXO updated but
supply counters not, or mint outputs committed without `CheckResMintArithmetic`).

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
`historical verification record`.

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
| 0.3 | 2026-09-12 | RFC-020: continuous destroy_cost(h), flat 128 ORU, cap `2^22`; CR-040..045 rewritten |
| 0.2 | 2026-09-12 | RFC-020: RES Mint Validation; CR-040..045 |
| 0.1 | 2026-06-05 | `historical verification record` — initial consensus rules specification |
