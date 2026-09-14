# SPEC-001: RIGOR State Model

**Document status:** Production specification  
**Track:** Formal Specification Program  
**Status:** ESTABLISHED  
**Baseline:** RFC-020 shipping issuance (continuous `destroy_cost(h)`, flat 128 ORU)  
**Parent:** [SPEC-000 Consensus Specification](./SPEC-000-CONSENSUS-SPECIFICATION.md)  
**Normative sources:** `RFC-000`, `RFC-008`, `RFC-018`, `RFC-020`  
**Freeze authority:** `MAINNET_PROTOCOL_FREEZE_DECLARATION.md` / `historical verification record`

---

## Document Role

This document is the **authoritative state specification** for RIGOR. It defines:

- What constitutes **system state**
- How state is represented **conceptually**
- Which portions of state are **consensus-critical**

This document does not prove theorems, does not modify consensus, and does not
describe how any particular runtime stores or computes state.

---

## 1. Purpose

**State** is the mathematical object transformed by consensus.

At each height `h`, the system occupies a state `State(h)`. Consensus validity
is evaluated over **state transitions**: given `State(h)` and a candidate block
at height `h+1`, the protocol either rejects the block or produces a uniquely
determined successor `State(h+1)`.

Block validity is therefore a predicate over pairs `(State(h), Block(h+1))`.
Replay and reorganization are sequences of such transitions applied to the same
initial state.

All honest validators applying the same block sequence MUST converge to identical
`State(h)` at every height.

---

## 2. State Categories

RIGOR state is partitioned into four categories. Each category has distinct
authority, purpose, and formal-verifiability status.

### 2.1 Consensus State

| Attribute | Value |
|-----------|-------|
| **Purpose** | Authoritative ledger and protocol context required to validate the next block and to replay history |
| **Authority** | **Authoritative** — defines accept/reject and successor state |
| **Formally verifiable** | **Target yes** — subject of formal model and proofs ([SPEC-005](./SPEC-005-CONSENSUS-INVARIANTS.md)) |

Consensus state is the only category that MAY influence block validity.

### 2.2 Runtime State

| Attribute | Value |
|-----------|-------|
| **Purpose** | Node-local machinery that computes, caches, or mirrors consensus results for performance and audit |
| **Authority** | **Non-authoritative** — MUST NOT feed back into validation |
| **Formally verifiable** | **Partial** — mirrors and witnesses are alignment evidence, not spec truth |

Runtime state includes caches, diagnostic mirrors, and formal-program export
pipelines introduced by the Formal-Friendly Runtime Refactor (FVR) track.

### 2.3 Observational State

| Attribute | Value |
|-----------|-------|
| **Purpose** | Human- and tool-readable projections for monitoring, debugging, and formal audit |
| **Authority** | **Non-authoritative** — derived from consensus or runtime mirrors |
| **Formally verifiable** | **Partial** — exports can be checked against spec predicates; they do not define them |

Observational state MUST NOT change block validity (see §5).

### 2.4 External State

| Attribute | Value |
|-----------|-------|
| **Purpose** | Environment outside the protocol's formal authority |
| **Authority** | **None** for consensus — treated as trust anchors when relied upon |
| **Formally verifiable** | **No** — outside the consensus state space |

External state is cataloged in [SPEC-004 Trust Anchors](./SPEC-004-TRUST-ANCHORS.md).

---

## 3. Consensus State

**Consensus State** at height `h` is written `State(h)` or `S(h)`.

Minimal consensus state is the smallest tuple of values that:

1. Determines whether block `h+1` is valid
2. Determines `State(h+1)` when block `h+1` is accepted
3. Is reconstructible by replay from genesis (or an agreed checkpoint)

```
S(h) = (
  ChainTip,
  BlockHeight,
  S_ORU,
  S_RES,
  UTXO,
  ProtocolVersion,
  FrozenParameters,
  MintContext,
  PoWContext
)
```

`MintContext` and `PoWContext` are consensus-critical extensions required by
RIGOR's body-tx destroy-to-mint and difficulty rules. They are listed separately
because their failure modes differ from ledger fields.

---

### 3.1 Chain Tip

**Definition:** The identifier and header-derived context of the active block at
height `h`, including the block hash at `h`, parent linkage, cumulative chainwork,
and header fields required for PoW validation and replay binding (`nBits`, time).

**Why consensus depends on it:** Fork choice selects the active chain by greatest
chainwork. PoW validation and ASERT difficulty update are evaluated relative to
the active tip and its ancestors.

**Failure condition:** Two honest nodes disagree on the active tip given the same
block history; or chainwork fails to increase along the active chain.

---

### 3.2 Block Height

**Definition:** The non-negative integer `h` indexing the active chain tip, with
genesis at `h = 0`. Height MUST increase by exactly one per accepted block on
the active chain.

**Why consensus depends on it:** Subsidy schedule and supply recurrence are height-indexed.

**Failure condition:** Accepted block sequence with non-consecutive heights, or
two nodes assigning different heights to the same active tip hash.

---

### 3.3 ORU Supply State

**Definition:** The cumulative ORU supply `S_ORU(h) ∈ ℤ≥0` after applying block
`h`, together with the per-block subsidy `R(h)` and Converge destruction `C(h)`
when needed for validation.

Recurrence (normative):

```
S_ORU(h+1) = S_ORU(h) + R(h+1) − C(h+1)
```

**Why consensus depends on it:** ORU conservation and subsidy rules are consensus
predicates. Invalid subsidy or undeclared destruction MUST cause block rejection.

**Failure condition:** `S_ORU(h) < 0`; subsidy at height `h` differs from the frozen
schedule; or Converge destruction in a valid block is not reflected in supply update.

---

### 3.4 RES Supply State

**Definition:** The cumulative RES supply `S_RES(h) ∈ ℤ≥0` with `S_RES(0) = 0`,
and per-block body mint quantity `body_mint_atoms(h) ∈ ℤ≥0` equal to the sum of
Witness v1 RES outputs created by body transactions that pass
`CheckResMintArithmetic` at height `h`.

Recurrence (normative, RFC-020):

```
S_RES(h+1) = S_RES(h) + body_mint_atoms(h+1)
S_RES(h)   ≤ 419430400000000
```

A block MAY contain zero or more mint transactions; each MUST satisfy
`R · (2²⁵+h)² ≤ B << 40`.

**Why consensus depends on it:** RES minting is bounded by the derived
constitutional cap (`419430400000000` atoms). Excess mint or mint that fails
the mint predicate MUST be rejected.

**Failure condition:** `S_RES(h) > 419430400000000`; or RES created without
`CheckResMintArithmetic`.

---

### 3.5 UTXO Set

**Definition:** A finite map from unspent outpoints `(txid, index)` to output
records `(nValue, scriptPubKey)`. Asset type and state semantics are **derived**
by parsing script and witness data; no parallel asset field exists in output storage.
Script is the compliance layer: classification is script-derived; legitimacy still
requires conservation, convergence / mint predicates, and deterministic consensus checks.

**Why consensus depends on it:** Transaction validity is UTXO-based. Every spend
MUST reference an existing unspent output; every output MUST satisfy conservation
and script rules.

**Failure condition:** Spend of non-existent or already-spent outpoint; negative
output value; or an asset class that cannot be classified from script / witness
structure (legitimacy failures are reported under the applicable CR / SI).

---

### 3.6 Protocol Version

**Definition:** The frozen rule-set identity governing which transaction and block
formats are valid on the active network. For mainnet this is the genesis-activated,
post-freeze rule bundle (RFC-020 continuous destroy cost, witness V0/V1 only,
no transition bridges).

This is **not** node software version. Software version strings MUST NOT enter
consensus hashes or validation predicates.

**Why consensus depends on it:** Block and transaction parsing, witness acceptance,
and mint-arithmetic predicates are version-governed.

**Failure condition:** Acceptance of a block whose mint or subsidy semantics
diverge from the frozen RFC-020 constant set.

---

### 3.7 Frozen Consensus Parameters

**Definition:** The immutable constant set required to evaluate validity without
external configuration. Includes at minimum:

| Parameter | Normative value / rule |
|-----------|------------------------|
| `RES_COIN` | `100000000` (same eight-decimal atom as `COIN`) |
| `RES_DESTROY_COST_GENESIS` | `102400000000` (1024 ORU) |
| `RES_DESTROY_POLE` | `2²⁵` = `33554432` blocks |
| `RES_DESTROY_SHIFT` | `40` |
| `ORU_FLAT_BLOCK_SUBSIDY` | `12800000000` (128 ORU for every `h ≥ 1`) |
| Mint predicate | `R > 0 → R · (2²⁵+h)² ≤ B << 40` (`CheckResMintArithmetic`) |
| `RES_MAX_SUPPLY` | `419430400000000` atoms (`2²²` RES) |
| Activation model | Genesis-only |

Network-scoped PoW constants (target spacing, ASERT half-life, genesis hash)
are frozen per network but MUST NOT alter mainnet consensus semantics.

**Why consensus depends on it:** Destroy cost, supply cap, and mint authorization
are parameter-driven. Parameter drift is a hard-fork class change.

**Failure condition:** Validation uses a parameter value not in the frozen set;
or mainnet semantics diverge from the constitutional constant set without Level-5
governance.

---

### 3.8 Mint / Convergence Destroy Context (consensus-critical extension)

**Definition:** Per-block body-tx mint and convergence-destroy trace: each mint
transaction’s single `OP_CONVERGE` destroy `B`, Witness v1 RES outputs summing to
`R`, confirming height `h`, and the `CheckResMintArithmetic` accept/reject
for that `(R, B, h)`. `R = 0` is a legal plain destroy. Mint-output maturity 100
is reorg protection.

**Why consensus depends on it:** RES mint authorization is a function of the
confirming block’s own body transactions and height. Failed mint arithmetic
MUST reject the block.

**Failure condition:** RES created without `CheckResMintArithmetic`; or the
mint price taken from PoW / `nBits`.

---

### 3.9 PoW Context (consensus-critical extension)

**Definition:** ASERT difficulty state: compact target at tip, height-1 (or genesis) anchor,
and header fields needed to verify `OpuHash(SHA256d(header)) < ExpandCompact(nBits)`.

**Why consensus depends on it:** PoW validity and fork choice depend on difficulty
state. `destroy_cost(h)` is independent of PoW / `nBits`. Parent compact target MUST NOT
define the mint destroy cost.

**Failure condition:** Header accepted with insufficient work; ASERT update
violates frozen rule; or PoW target used as the mint price.

---

## 4. Runtime State

Runtime state exists to compute and persist consensus results efficiently. It
MUST be derivable from consensus state and MUST NOT alter validation outcomes.

| Runtime artifact | Description | Affects consensus? |
|------------------|-------------|-------------------|
| **Validation caches** | Memoized results of prior validation passes | **NO** |
| **Supply ledger caches** | Cached cumulative ORU/RES counters mirrored from connect path. ObservationalMetrics (W1); not spendable UTXO. Cap authority is the RFC-020 derived `RES_MAX_SUPPLY`. | **NO** |
| **Runtime supply mirror** | Post-connect observational copy of supply fields (FVR-B) | **NO** |
| **Coq field hook map** | Name mapping from runtime export fields to formal model fields (FVR-C) | **NO** |
| **Invariant checker state** | Last-run result of formal invariant verification on mirror (FVR-A, FVR-E) | **NO** |
| **Witness chain store** | In-memory per-**block-hash** supply witnesses (FVR-F). Not height-keyed (W7 fence) | **NO** |
| **Coq catalog rows** | Historical witness rows formatted for formal catalog consumption (FVR-G) | **NO** |
| **CHAS alignment vectors** | Canonical-hash alignment export derived from catalog (FVR-H) | **NO** |
| **Equivalence candidate records** | Comparable-field status without equivalence claim (FVR-I) | **NO** |
| **Coq-readable candidate export** | Structured candidate layer for proof tooling (FVR-J) | **NO** |
| **Global candidate aggregation** | Cross-height candidate equivalence index (FVR-K) | **NO** |
| **Closure gate status** | Equivalence closure predicate evaluation (FVR-L) | **NO** |
| **Issuance catalog** | Formal issuance / mint-predicate catalog | **NO** |
| **Phase-5 summary snapshot** | Consolidated FVR-A..N status for audit (FVR-N) | **NO** |
| **Mempool** | Unconfirmed transaction pool | **NO** |
| **Peer tables** | Network peer connectivity | **NO** |

**Invariant:** Observational FVR mirrors, witness hashes, and Coq catalog rows MUST NOT
be read by block validation predicates. Debug assertions on mirror inconsistency MUST
NOT change mainnet consensus acceptance. W1 ledger caches are ObservationalMetrics
only (post-validation index/explorer). They MUST NOT be ConnectBlock predicates.

---

## 5. Observational State

Observational state is produced for operators, auditors, and formal-program tooling.
It is a **read-only projection** of consensus or runtime mirrors.

### 5.1 Examples

| Observational artifact | Source | Role |
|------------------------|--------|------|
| **Runtime supply export** | Runtime mirror (FVR-D) | Height-indexed ORU/RES ledger snapshot |
| **Runtime supply witness** | Witness chain (FVR-F) | Per-block-hash invariant-checked snapshot (not height-keyed) |
| **Invariant verification report** | Invariant checker (FVR-E) | Pass/fail with stable violation ids |
| **Phase-5 supply cell grid** | Export payload (FVR-D) | Fixed-size audit grid for cross-layer diff |
| **Deterministic state digest** | Derived hash of authoritative fields | Replay audit fingerprint (diagnostic) |
| **Phase-5 summary export** | FVR-N consolidation | Layer status and honesty boundary |
| **Equivalence candidate exports** | FVR-I, FVR-J, FVR-K | Candidate comparability, not proof |
| **Closure and impossibility exports** | FVR-L, FVR-M | Gap catalog |

### 5.2 Non-authority rule

**Observational state cannot change block validity.**

Specifically:

- A missing, stale, or internally inconsistent observational export MUST NOT
  cause a valid block to be rejected or an invalid block to be accepted.
- Diagnostic state digests summarize authoritative state; they are not an
  independent truth source.
- Formal audit exports MAY report `equivalence_claimed = false` and MUST NOT
  be interpreted as proof of runtime–spec alignment unless closed under [SPEC-007](./SPEC-007-EQUIVALENCE-BOUNDARIES.md).

---

## 6. External State

External state lies outside the protocol's formal authority. It MAY influence
liveness and operator experience but MUST NOT define consensus outcomes.

| External source | Role | Consensus impact |
|-----------------|------|------------------|
| **Network messages** | Block and transaction propagation | None on validity (given same block history) |
| **System clocks** | Local timing for non-consensus subsystems | None on header rules beyond specified bounds |
| **Filesystem / database** | Persistence of blocks and indexes | None if replay reconstructs same `S(h)` |
| **User wallets** | Key management, signing, labeling | None |
| **External RPC consumers** | Applications reading node exports | None |
| **Golden vector corpora** | Regression recordings of past runtime bytes | None on validity; trust anchor for alignment |

Reliance on any external source for correctness arguments MUST be declared in
[SPEC-004 Trust Anchors](./SPEC-004-TRUST-ANCHORS.md).

---

## 7. State Transition Model

Consensus is a **deterministic state transition system**.

### 7.1 Master transition

Let `Block(h+1)` denote a candidate block extending the active tip at height `h`.
Let `ValidBlock(h+1)` denote the conjunction of all validity predicates under
frozen parameters and `S(h)`.

```
If ValidBlock(h+1) holds:
    S(h+1) = Transition(S(h), Block(h+1))
Else:
    S(h+1) is undefined (block rejected; S(h) unchanged)
```

`Transition` is a total function on valid inputs. For valid inputs, the output
is unique.

### 7.2 Transition decomposition (normative order)

The transition MUST decompose into ordered phases. Reordering phases is a
specification violation:

```
Transition = Commit ∘ SupplyUpdate ∘ Transactions ∘ Header
```

| Phase | Input | Effect |
|-------|-------|--------|
| **Header** | `S(h)`, block header | Verify PoW, timestamps, difficulty update |
| **Transactions** | UTXO at `h`, block tx list | Apply spends and creates; evaluate body mint / `OP_CONVERGE` destroys |
| **SupplyUpdate** | Body mint atoms, convergence destroys, subsidy | Update `S_ORU`, `S_RES` via `body_mint_atoms` |
| **Commit** | Scratch state | Persist `S(h+1)` |

Invalidity at any phase MUST abort the transition and leave `S(h)` unchanged.

### 7.3 Replay and reorg

**Replay:** `S(h) = Transition(...Transition(Transition(S(0), Block(1)), ...), Block(h))`.

**Reorg:** Switching active tip to an alternate valid chain MUST yield the same
`S(h)` as sequential application of that chain from genesis.

---

## 8. Consensus-Critical State Invariants

Placeholder invariant catalog (superseded by authoritative [SPEC-005 Consensus Invariants](./SPEC-005-CONSENSUS-INVARIANTS.md) §12 mapping).

| ID | Name | Description |
|----|------|-------------|
| **SI-001** | Chain continuity | Active chain heights are consecutive from genesis; each block references its parent hash on the active chain |
| **SI-002** | UTXO consistency | Every spend references an existing unspent output; no duplicate spends; output values are non-negative |
| **SI-003** | ORU supply non-negativity | `S_ORU(h) ≥ 0` at all heights |
| **SI-004** | RES supply cap | `S_RES(h) ≤ 419430400000000` at all heights |
| **SI-005** | Frozen parameter preservation | All validation uses the frozen RFC-020 constant set |
| **SI-006** | ORU conservation | Non-coinbase ORU flows preserve input–output balance per transaction validity rules |
| **SI-007** | RES mint predicate | Mint transactions MAY exist in a block; each MUST satisfy `R · (2²⁵+h)² ≤ B << 40` |
| **SI-008** | Chainwork monotonicity | Cumulative chainwork strictly increases along the active chain |
| **SI-009** | Mint arithmetic coherence | A RES-creating body tx in block `h` is valid only if `CheckResMintArithmetic` accepts `(R, B, h)` |
| **SI-010** | Mint height binding | Mint authorization is a function of the confirming block’s body and height; maturity 100 is reorg protection |
| **SI-011** | Script-as-compliance | Asset *class* is derivable from script; no persisted parallel asset field; legitimacy via consensus |
| **SI-012** | Identity domain | Consensus references use transaction identity (txid), not witness-weighted alternate ids |
| **SI-013** | Deterministic transition | Identical `(S(h), Block(h+1))` yields identical accept/reject and identical `S(h+1)` when accepted |

Each invariant MUST admit a falsifying example or regression vector when instantiated in [SPEC-005](./SPEC-005-CONSENSUS-INVARIANTS.md).

---

## 9. Relationship to Formal Verification

```
State Specification (this document)
        │  defines: entities, categories, transition shape, SI placeholders
        ▼
Formal State Model (docs/formal/, executable oracle)
        │  defines: machine-readable types and transition function
        ▼
Coq State Definitions (formal_verification/coq/)
        │  defines: inductive state, transition lemmas
        ▼
Runtime Witness Alignment (Phase 5, FVR track)
           evidence: mirrors and exports; equivalence_claimed = false until proven
```

**State Specification is authoritative.**

The formal model and Coq definitions refine this document; they do not replace it.
Runtime witness alignment is evidence subject to [SPEC-007 Equivalence Boundaries](./SPEC-007-EQUIVALENCE-BOUNDARIES.md);
it does not redefine `S(h)`.

When a formal model field disagrees with this specification, the specification
governs intent classification; post-freeze runtime divergence is a governance
incident, not a silent spec amendment.

---

## 10. Relationship to Runtime Refactor

The **Formal-Friendly Runtime Refactor (FVR)** track (tasks FVR-A through FVR-N)
introduced runtime witnesses, mirrors, and Phase-5 export layers for formal audit.

**Binding statement:** FVR-A through FVR-N did **not** change Consensus State.

| Task range | What was introduced | Consensus impact |
|------------|---------------------|------------------|
| FVR-A | Runtime invariant checks on captured mirror | **NONE** |
| FVR-B | Runtime supply mirror after block connect | **NONE** |
| FVR-C | Coq field name hook on export | **NONE** |
| FVR-D | Diagnostic supply export | **NONE** |
| FVR-E | Active invariant verification report | **NONE** |
| FVR-F | Historical witness chain | **NONE** |
| FVR-G | Witness-to-Coq catalog integration | **NONE** |
| FVR-H | CHAS alignment vector export | **NONE** |
| FVR-I | Equivalence candidate identification | **NONE** |
| FVR-J | Coq-readable candidate export | **NONE** |
| FVR-K | Global candidate equivalence index | **NONE** |
| FVR-L | Equivalence closure gate | **NONE** |
| FVR-M | Issuance / mint-predicate catalog | **NONE** |
| FVR-N | Phase-5 summary and freeze-protocol link | **NONE** |

Runtime mirrors remain **observational only**. They MUST NOT appear in validation
predicates, monetary policy, or the authoritative definition of `S(h)`.

---

## 11. Freeze Compatibility

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

## 12. Task Output Status

| Field | Status |
|-------|--------|
| State Specification | **ESTABLISHED** |
| Consensus State Model | **ESTABLISHED** |
| Runtime State Model | **ESTABLISHED** |
| Observational State Model | **ESTABLISHED** |
| External State Model | **ESTABLISHED** |
| Consensus Impact | **NONE** |
| Equivalence Claimed | **false** |

---

## 13. Revision History

| Version | Date | Change |
|---------|------|--------|
| 0.2 | 2026-09-12 | RFC-020 body-tx destroy-to-mint: `body_mint_atoms`, pole predicate, cap `2²²` |
| 0.1 | 2026-06-05 | `historical verification record` — initial state model specification |
