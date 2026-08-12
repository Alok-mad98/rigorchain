# SPEC-002: RIGOR Economic Model (ORU / RES)

**Task:** `TASK-SPEC-003`  
**Track:** Formal Specification Program (Formal-Friendly Runtime Refactor compatible)  
**Status:** ESTABLISHED  
**Baseline:** `mainnet-freeze-rfc015-2026-05-26`  
**Parents:** `SPEC-000`, `SPEC-001`  
**Normative sources:** `RFC-000`, `RFC-008`, `RFC-014`, `RFC-015` (and the frozen ORU schedule RFCs)  
**Freeze authority:** `MAINNET_PROTOCOL_FREEZE_DECLARATION.md` / `TASK-MAINNET-FREEZE-001`

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
- `M(h) ∈ {0,1}` is the **RES mint indicator** for block `h`:
  `M(h)=1` iff **RES is minted** (authorized and successfully created) in block `h`.

### 2.3 Immutable economic constants (frozen)

The following are treated as **immutable constants** at mainnet genesis under the
freeze declaration:

| Symbol | Meaning | Frozen value |
|--------|---------|--------------|
| `RES_REWARD_AMOUNT` | RES minted per successful deterministic consensus allocation | `100.00 RES` (= `10,000` atoms) |
| `RES_MAX_SUPPLY` | Maximum total RES supply | `21,000,000 RES` (= `2,100,000,000` atoms) |
| `CONVERGENCE_DRAW_MIN_TXS_FOR_RES` | Draw participation gate on candidate set `F` | `1` (i.e. `|F| ≥ 1`) |
| `MIN_CONVERGENCE_AMOUNT` | Minimum ORU converged per candidate tx | `1 ORU` |

The ORU subsidy schedule is frozen by the ratified ORU RFC(s) (not restated here);
its minimum tail-subscription constraint (e.g. `ORU_MIN_BLOCK_SUBSIDY`) is treated
as a frozen constant if defined by those RFCs.

---

## 3. Authoritative vs Observational Economic Fields

### 3.1 Authoritative (consensus-critical)

These quantities are **authoritative**: they are part of consensus state and may
affect block validity:

- `S_ORU(h)` and its recurrence via `R(h)` and `C(h)`
- `S_RES(h)` and its recurrence via `M(h)` and `RES_REWARD_AMOUNT`
- The cap constraint `S_RES(h) ≤ RES_MAX_SUPPLY`
- The restriction `M(h) ∈ {0,1}` (at most one successful RES mint per block)

### 3.2 Observational / diagnostic (non-authoritative)

The following are explicitly **non-authoritative** (diagnostic only). They may
be exported or checked for audits but MUST NOT affect block acceptance:

- Derived cumulative subsidy sums `Σ_{i=1..h} R(i)`
- Per-block or cumulative UTXO aggregates computed by scanning and classifying UTXO
- Any runtime mirror / witness / catalog / “alignment” export produced by the
  Formal-Friendly Runtime Refactor track

If an observational field disagrees with an authoritative quantity, the
observational field is wrong (or incomplete) and must not change consensus.

---

## 4. ORU Economic Specification

### 4.1 ORU supply recurrence (authoritative)

**Normative law:**

\[
S_{\text{ORU}}(h+1) = S_{\text{ORU}}(h) + R(h+1) - C(h+1)
\]

with:

- `S_ORU(0)` defined by genesis (frozen)
- `R(h) ≥ 0` for all `h ≥ 1`
- `C(h) ≥ 0` for all `h ≥ 1`

### 4.2 ORU supply non-negativity (authoritative)

**Invariant (ORU-I1):**

\[
\forall h:\ S_{\text{ORU}}(h) \ge 0
\]

**Falsifiable failure condition:** A block is invalid if, after applying its
subsidy and convergence accounting per the law above, it would require
`S_ORU(h) < 0`.

### 4.3 ORU convergence accounting (authoritative)

`C(h)` MUST equal the sum of all ORU amounts committed by valid **convergence transactions**
in block `h`.

**Falsifiable failure conditions:**

- A block is invalid if it causes `C(h)` to include any amount not backed by a
  valid convergence commitment event.
- A block is invalid if a valid convergence commitment event is omitted from `C(h)`.

### 4.4 ORU subsidy schedule (authoritative, parameterized)

`R(h)` is defined by the frozen ORU subsidy schedule RFC(s). This specification
does not duplicate that schedule, but requires that:

- `R(h)` is a pure function of height `h` under frozen constants.
- Any minimum tail emission constraint (if defined) is preserved.

**Falsifiable failure conditions:**

- A block is invalid if it credits ORU subsidy not equal to `R(h)`.
- A block is invalid if it violates the schedule’s minimum subsidy constraint (if any).

---

## 5. RES Economic Specification

### 5.1 RES supply recurrence (authoritative)

**Normative law (RFC-015):**

\[
S_{\text{RES}}(0) = 0,\quad
S_{\text{RES}}(h+1) = S_{\text{RES}}(h) + \texttt{RES\_REWARD\_AMOUNT} \cdot M(h+1)
\]

where `M(h) ∈ {0,1}`.

### 5.2 Single-mint restriction (authoritative)

**Invariant (RES-I1):**

\[
\forall h \ge 1:\ M(h) \in \{0,1\}
\]

**Falsifiable failure condition:** A block is invalid if it contains two
successful RES mints at the same height or otherwise forces `M(h) ∉ {0,1}`.

### 5.3 RES cap enforcement (authoritative)

**Invariant (RES-I2):**

\[
\forall h:\ S_{\text{RES}}(h) \le \texttt{RES\_MAX\_SUPPLY}
\]

**Falsifiable failure conditions:**

- A block is invalid if it causes `S_RES(h)` to exceed `RES_MAX_SUPPLY`.
- A block is invalid if it attempts to mint a positive amount when
  `S_RES(h−1) = RES_MAX_SUPPLY`.

### 5.4 RES mint amount (authoritative)

**Invariant (RES-I3):** If `M(h)=1` then the minted quantity is exactly
`RES_REWARD_AMOUNT` (not smaller, not larger, not variable).

**Falsifiable failure condition:** A block is invalid if it authorizes a RES
mint but credits any amount other than `RES_REWARD_AMOUNT`.

### 5.5 RES monotonicity up to cap (authoritative)

**Invariant (RES-I4):**

\[
\forall h \ge 1:\ S_{\text{RES}}(h) \ge S_{\text{RES}}(h-1)
\]

with strict increase iff `M(h)=1`.

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
  not define the RES inequality RHS (RFC-014).
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
- RES recurrence, single-mint restriction, and cap preservation
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
`TASK-MAINNET-FREEZE-001`.

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
| 0.1 | 2026-06-05 | `TASK-SPEC-003` — initial ORU/RES economic model specification |

