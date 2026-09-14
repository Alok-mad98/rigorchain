# SPEC-002: RIGOR Economic Model (ORU / RES)

**Document status:** Production specification  
**Track:** Formal Specification Program (Formal-Friendly Runtime Refactor compatible)  
**Status:** ESTABLISHED  
**Baseline:** RFC-020 shipping issuance (continuous `destroy_cost(h)`, flat 128 ORU)  
**Parents:** `SPEC-000`, `SPEC-001`  
**Normative sources:** `RFC-000`, `RFC-008`, `RFC-020`  
**Freeze authority:** `MAINNET_PROTOCOL_FREEZE_DECLARATION.md` / `historical verification record`


---

## Document Role

This document is the **authoritative economic specification** for RIGOR’s two
assets **ORU** and **RES**.

It defines **what the system SHOULD do** regarding supply, minting, convergence
commitments, and caps. It does not describe how any implementation computes these values, and it
does not claim equivalence to runtime artifacts.

This document is specification-only:

- No code changes
- No consensus changes
- No proofs
- **Equivalence Claimed:** false

---

## 1. Scope and Layer Boundary

This specification covers only the **economic layer** of consensus:

- ORU subsidy and convergence accounting
- RES mint accounting and cap enforcement
- Falsifiable economic invariants and failure conditions
- Diagnostic cross-checks (explicitly **non-authoritative**)

Out of scope:

- Cryptographic security arguments (collision resistance, unpredictability)
- Network / mempool policies and their economic side effects
- Market pricing, liquidity, and off-chain settlement behavior

---

## 2. Definitions and Notation

### 2.1 Height and state

- `h ∈ ℕ` is the active-chain height after applying block `h` (genesis at `h = 0`).
- `S_ORU(h)` is the cumulative ORU supply after block `h`.
- `S_RES(h)` is the cumulative RES supply after block `h`.

### 2.2 Per-block economic quantities

For each block at height `h ≥ 1`:

- `R(h) ∈ ℤ≥0` is the **ORU block subsidy** at height `h` (from the frozen subsidy schedule).
- `C(h) ∈ ℤ≥0` is the **ORU converge sum** for block `h`:
  the sum of ORU amounts committed by all valid **convergence transactions** in that block.
- `body_mint_atoms(h) ∈ ℤ≥0` is the sum of Witness v1 RES atoms created by body
  mint transactions in block `h` that pass `CheckResMintArithmetic`. A block MAY
  contain zero or more such transactions.

### 2.3 Immutable economic constants (frozen)

The following are treated as **immutable constants** at mainnet genesis under the
freeze declaration:

| Symbol | Meaning | Frozen value |
|--------|---------|--------------|
| `RES_COIN` | RES atom scale (same as `COIN`) | `100000000` |
| `ORU_FLAT_BLOCK_SUBSIDY` | ORU subsidy for every `h ≥ 1` | `12800000000` (128 ORU) |
| `RES_DESTROY_COST_GENESIS` | Display destroy cost at `h = 0` (ORU atoms per whole RES) | `102400000000` (1024 ORU) |
| `RES_DESTROY_POLE` | Hardening timescale | `2^25` = `33554432` blocks |
| `RES_DESTROY_SHIFT` | Forced by the genesis destroy cost | `40` |
| `destroy_cost(h)` | Display destroy cost (not a consensus input) | `P0 · (2^25 + h)^2 >> 50` for `h ≥ 0`, else `0` |
| Mint predicate | Body-tx destroy-to-mint inequality | `R > 0 → R · (2^25 + h)^2 ≤ B << 40`; `R = 0` unconstrained |
| `RES_MAX_SUPPLY` | Maximum total RES supply (closed form) | `419430400000000` atoms (`2^22` RES) |
| `RES_REWARD_EPOCH` | Observational calendar only | `210000` blocks |
| `MIN_CONVERGENCE_AMOUNT` | Smallest positive OP_CONVERGE destroy | `1` atom (`nValue > 0`). Not a mint price; `R ≥ 1` is gated only by the mint inequality |
| `MAX_MONEY` | Per-output / per-tx-input-sum / fee-accumulator overflow bound | `10_000_000_000 * COIN` (**not** an ORU supply cap; issuance is uncapped). Constrained by `MAX_MONEY ≤ INT64_MAX / 4` so two in-range amounts are summable |

The ORU subsidy is the RFC-020 flat schedule in §4.4. `ORU_MIN_BLOCK_SUBSIDY`
equals the same 128 ORU floor.

---

## 3. Authoritative vs Observational Economic Fields

### 3.1 Authoritative (consensus-critical)

These quantities are **authoritative**: they are part of consensus state and may
affect block validity:

- Spendable ORU and RES as classified UTXO (script → asset class)
- Body-tx mint via `CheckResMintArithmetic(R, B, h)` (Witness v1 RES outputs)
- Coinbase `OP_RES_MINT` is **not** the RFC-020 issuance path
- The closed-form identity `RES_MAX_SUPPLY = 419430400000000` atoms (`2^22` RES)
- `MAX_MONEY` as a per-output / per-tx-sum overflow bound
- RFC-018: half of ORU fees on RES-transfer txs is unclaimable by coinbase

Observational issued-minus-converge `S_ORU` and a running `S_RES` mint total
are **not** ConnectBlock predicates.

### 3.2 Observational / diagnostic (non-authoritative)

The following are explicitly **non-authoritative** (diagnostic only). They may
be exported or checked for audits but MUST NOT affect block acceptance:

- Derived cumulative subsidy sums `Σ_{i=1..h} R(i)`
- Observational issued-minus-converge `S_ORU` and running mint-total `S_RES`
- Extra-consensus UTXO scans used as diagnostics (the coins view during
  ConnectBlock is consensus; a parallel aggregate is not)
- Any runtime mirror / witness / catalog / “alignment” export produced by the
  Formal-Friendly Runtime Refactor track

If an observational field disagrees with an authoritative quantity, the
observational field is wrong (or incomplete) and must not change consensus.

---

## 4. ORU Economic Specification

### 4.1 ORU supply recurrence (authoritative)

**Normative law:** spendable ORU is the UTXO set. The observational identity

\[
S_{\text{ORU}}^{\text{issued}}(h+1) = S_{\text{ORU}}^{\text{issued}}(h) + R(h+1) - C(h+1)
\]

does **not** include RFC-018 RES-transfer fee destroys or unclaimed coinbase.
Circulating UTXO ORU additionally decreases by those amounts. ConnectBlock
MUST NOT reject on a mismatch between the observational scalar and the UTXO
sum (`bad-oru-supply-negative` is not a consensus reason).

### 4.2 ORU supply non-negativity (authoritative)

**Invariant (ORU-I1):** every classified ORU output amount is non-negative and
`≤ MAX_MONEY`. ConnectBlock does **not** reject on an extra-UTXO `S_ORU < 0`
scalar.

**Falsifiable failure condition:** A transaction or block is invalid if any
output, input-sum, or fee/converge accumulator is negative or exceeds
`MAX_MONEY`.

### 4.3 ORU convergence accounting (authoritative)

`C(h)` MUST equal the sum of all ORU amounts committed by valid **convergence transactions**
in block `h`.

**Falsifiable failure conditions:**

- A block is invalid if it causes `C(h)` to include any amount not backed by a
  valid convergence commitment event.
- A block is invalid if a valid convergence commitment event is omitted from `C(h)`.

### 4.4 ORU subsidy schedule (authoritative, parameterized)

`R(h)` is the RFC-020 flat subsidy:

- `R(0) = 0`
- `R(h) = 128` ORU (`12800000000` atoms) for every `h ≥ 1`

`R(h)` is a pure function of height. There are no halvings.

**Falsifiable failure conditions:**

- A block is invalid if it credits ORU subsidy not equal to `R(h)`.
- A block is invalid if it violates the schedule’s minimum subsidy constraint (if any).

---

## 5. RES Economic Specification

### 5.1 RES supply recurrence (authoritative)

**Normative law (RFC-020):**

\[
S_{\text{RES}}(0) = 0,\quad
S_{\text{RES}}(h+1) = S_{\text{RES}}(h) + \texttt{body\_mint\_atoms}(h+1)
\]

with `S_{\text{RES}}(h) \le 419430400000000`. `body_mint_atoms(h)` is the sum of
Witness v1 RES outputs from body transactions that pass `CheckResMintArithmetic`
at height `h`.

### 5.2 RES mint conservation (authoritative)

**Invariant (RES-I1):** RES enters circulation only through body-tx destroy-to-mint.
A block MAY contain zero or more mint transactions. Conservation requires
that every minted atom is accounted in `body_mint_atoms(h)` and that no other
path creates RES.

**Falsifiable failure condition:** A block is invalid if it creates RES outside
a body transaction that satisfies `CheckResMintArithmetic`, or if
`body_mint_atoms(h)` disagrees with the sum of those mint outputs.

### 5.3 RES cap enforcement (authoritative)

**Invariant (RES-I2):**

\[
\forall h:\ S_{\text{RES}}(h) \le 419430400000000
\]

**Falsifiable failure conditions:**

- A block is invalid if it causes `S_RES(h)` to exceed `419430400000000`.
- A block is invalid if it attempts to mint a positive amount when
  `S_RES(h−1) = 419430400000000`.

### 5.4 RES mint amount (authoritative)

**Invariant (RES-I3):** Each mint transaction with RES outputs summing to `R`
and a single `OP_CONVERGE` destroy `B` MUST satisfy

\[
R > 0 \implies R \cdot (2^{25}+h)^{2} \le B \cdot 2^{40}
\]

at the confirming height (`CheckResMintArithmetic`). `R = 0` is a legal
plain destroy (`B` unconstrained). The credited amount is **bounded by the
inequality**.

**Falsifiable failure condition:** A block is invalid if any body mint fails
`CheckResMintArithmetic`.

### 5.5 RES monotonicity up to cap (authoritative)

**Invariant (RES-I4):**

\[
\forall h \ge 1:\ S_{\text{RES}}(h) \ge S_{\text{RES}}(h-1)
\]

with strict increase iff `body_mint_atoms(h) > 0`.

**Falsifiable failure condition:** A block is invalid if it decreases total RES
supply across a valid transition.

---

## 6. Cross-checks (UTXO-based, diagnostic only)

These checks are **explicitly non-authoritative**. They exist to detect
implementation defects, indexer drift, or export inconsistencies. Failure of any
check in this section MUST NOT affect block validity.

### 6.1 Diagnostic RES-in-UTXO aggregate

Let `UTXO_RES_SUM(h)` denote the sum of all UTXO output values classified as RES
by script parsing at height `h`.

**Diagnostic expectation (not consensus):**

\[
UTXO\_RES\_SUM(h) = S_{\text{RES}}(h)
\]

**Known limitations:** This equality depends on a complete and correct script-based
classification and on the exact definition of “in circulation” for RES outputs.
Disagreement indicates a bug or a mismatch of classification scope; it does not
redefine `S_RES(h)`.

### 6.2 Diagnostic ORU-in-UTXO aggregate

Similarly define `UTXO_ORU_SUM(h)` as the sum of UTXO output values classified as
ORU.

**Diagnostic expectation (not consensus):** `UTXO_ORU_SUM(h)` matches the
in-circulation ORU definition used by the audit tool. This may differ from total
`S_ORU(h)` if protocol-defined non-UTXO sinks/sources exist; such differences must
be stated explicitly by the audit tool and MUST NOT change consensus quantities.

---

## 7. Assumptions and Trust Anchors (economic layer)

This specification’s economic statements rely on the following explicit
assumptions/trust anchors, cataloged in [SPEC-004 Trust Anchors](./SPEC-004-TRUST-ANCHORS.md):

- **A-ORDER:** The consensus state-transition phase ordering is fixed and
  deterministic (see `SPEC-001` §7).
- **A-HASH:** Hash evaluation is deterministic for all consensus inputs.
- **A-POW:** PoW context determines header validity and chain selection, but does
  not define `destroy_cost(h)` or the mint predicate (RFC-020).
- **A-NET:** Network propagation affects liveness but not the definition of
  accepted history given the same block sequence.
- **A-SCHEDULE:** The ORU subsidy schedule `R(h)` is a frozen, ratified
  height-indexed function.

---

## 8. Formal Properties (what can be proven vs out of scope)

### 8.1 Intended Coq-provable properties (targets)

The following are intended to be provable once the corresponding formal models
and invariants are defined (no claim of completion here):

- ORU recurrence correctness given a formal definition of `R(h)` and `C(h)`
- ORU non-negativity (subject to validity predicates on convergence commitments and outputs)
- RES recurrence, mint-arithmetic predicate, and cap preservation
- Monotonicity of cumulative supplies

### 8.2 Outside scope / trust-anchor dependent

- Economic security claims that require cryptographic hardness assumptions
- Market-level properties (price stability, liquidity formation)
- Any claim that a specific runtime export is equivalent to authoritative state
  without proofs (`equivalence_claimed = false` by default)

---

## 9. Freeze Compatibility

This document is **specification only**.

| Guarantee | Status |
|-----------|--------|
| No consensus modification | **Confirmed** |
| No monetary-policy modification | **Confirmed** |
| No state_hash modification | **Confirmed** |
| No protocol-freeze modification | **Confirmed** |

Nothing in this document supersedes `MAINNET_PROTOCOL_FREEZE_DECLARATION.md` or
`historical verification record`.

---

## 10. Task Output Status

| Field | Status |
|-------|--------|
| Economic Specification | **ESTABLISHED** |
| ORU Supply Rules | **ESTABLISHED** |
| RES Mint Rules | **ESTABLISHED** |
| Invariants & Falsifiable Failures | **ESTABLISHED** |
| Audit/Observational Fields | **ESTABLISHED** |
| Consensus Impact | **NONE** |
| Equivalence Claimed | **false** |

---

## 11. Revision History

| Version | Date | Change |
|---------|------|--------|
| 0.3 | 2026-09-12 | RFC-020 active: continuous `destroy_cost(h)`, flat 128 ORU, cap `2^22` RES |
| 0.2 | 2026-09-12 | RFC-020: continuous `destroy_cost(h)`, pole predicate, cap `2²²`, flat 128 ORU |
| 0.1 | 2026-06-05 | `historical verification record` — initial ORU/RES economic model specification |

