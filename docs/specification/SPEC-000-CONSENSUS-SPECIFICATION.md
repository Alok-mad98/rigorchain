# SPEC-000: RIGOR Consensus Specification

> **SECURITY-001-A8 (Sigma-PoW-01):** σ = Hash256(txid ‖ C ‖ B_{h-1} ‖ LE32(h)); draw deferred one block; `bind_commit` deleted. See `docs/security/SECURITY_001_A8_REPORT.md`.


**Task:** `TASK-SPEC-001`  
**Track:** Formal Specification Program  
**Status:** ESTABLISHED (skeleton)  
**Baseline:** `mainnet-freeze-rfc015-2026-05-26`  
**Normative sources:** `RFC-000`, `RFC-008`, `RFC-009`, `RFC-014`, `RFC-015`  
**Freeze authority:** `MAINNET_PROTOCOL_FREEZE_DECLARATION.md` / `TASK-MAINNET-FREEZE-001`

---

## Document Role

This document is the **canonical specification layer** for RIGOR consensus. It states
what the system **SHOULD** do. It does not describe how any particular implementation
currently behaves, and it does not claim equivalence to runtime code.

Future formal models, proofs, and runtime-alignment evidence **MUST** reference this
specification structure. Where this skeleton is incomplete, the ratified RFC bundle
remains the interim normative source until the corresponding `SPEC-00N` document
is published.

**Falsifiability requirement:** Every objective and invariant stated here MUST admit
a counterexample or regression test. Vacuous or implementation-accident definitions
are specification defects.

---

## 1. Scope

### 1.1 RIGOR Mainnet

**RIGOR Mainnet** is the public, genesis-activated consensus network whose protocol
parameters, economics, and convergence semantics were frozen on **2026-05-26** under
`TASK-MAINNET-FREEZE-001` with final economics from `TASK-RFC015-OCCAM-REBALANCE-001`.

The specification governs:

- Block and transaction validity on the active chain
- Proof-of-work header acceptance and fork choice
- Dual-asset supply evolution (ORU and RES)
- Convergence draw selection and RES issuance
- Deterministic replay and reorg behavior

Networks other than mainnet (testnet, signet, regtest, devtest) MUST use the same
**consensus semantics** as mainnet; only non-consensus network parameters (genesis,
address prefixes, timing constants where explicitly network-scoped) MAY differ.

### 1.2 ORU (Operational Reserve Unit)

**ORU** is the base operational asset of RIGOR. ORU enters circulation through block
subsidy and exits circulation through valid **Converge** transactions that destroy ORU
as the cost of convergence participation.

ORU MUST satisfy:

- Conservation across non-coinbase transactions (inputs equal outputs plus fees)
- Non-negative output values
- Script-classified asset identity (no parallel asset field in output storage); legitimacy enforced by consensus validation

### 1.3 RES (Reserve Energy Standard)

**RES** is the secondary asset minted exclusively through the **convergence draw**
when a block satisfies draw eligibility and a unique winner is selected.

RES MUST satisfy:

- Mint amount per successful draw: **100 RES** (RFC-015)
- Maximum total supply: **21,000,000 RES** (RFC-015)
- At most **one** successful RES mint per block height
- No RES mint outside the convergence draw path

### 1.4 Proof-of-Work Consensus

RIGOR uses **Proof-of-Work (PoW)** for block production and fork choice.

The specification requires:

- Valid blocks MUST satisfy `Hash256(header) < ExpandCompact(nBits)` on the active parent context
- Difficulty adjustment MUST follow the frozen **ASERT** rule set
- Cumulative **chainwork** MUST increase strictly along the active chain
- PoW difficulty MUST NOT define the RES convergence inequality right-hand side (RFC-014)

### 1.5 Dual-Asset Model

RIGOR operates a **dual-asset** model:

| Asset | Role | Supply mechanism |
|-------|------|------------------|
| ORU | PoW security and economic coordination asset | Subsidy minus Converge destruction |
| RES | Independent scarce resource (not mined) | Convergence draw mint (bounded) |

Asset *class* MUST be inferred from **scriptPubKey** and witness structure (script as
compliance layer). Asset *legitimacy* MUST additionally satisfy the consensus rules for
that class. Consensus MUST NOT persist asset type as a separate field in `CTxOut` storage.

### 1.6 Protocol Freeze Statement

The following are **FINAL and IMMUTABLE** for public mainnet, modifiable only through
an explicit **Level-5 hard-fork constitutional process** (`PROTOCOL_CHANGE_SEVERITY_TABLE.md`):

| Locked item | Frozen value / rule |
|-------------|---------------------|
| `RES_PROBABILITY_TARGET` | `2^240` (RFC-014) |
| Inequality form | `H_i < T · f_i` with cross-multiply tie-break (RFC-000) |
| Inequality RHS source | Constitutional constant only — NOT PoW difficulty (RFC-014) |
| σ preimage layout | 100 bytes exactly (RFC-000 / RFC-008) |
| `B_{h-1}` (σ seed) | Parent block hash, verbatim in σ slot `[64..95]` (SECURITY-001-A8 / Sigma-PoW-01). Replaces `bind_commit`. |
| Draw timing | Deferred one block: height `h` pays the set committed by `h-1` (SECURITY-001-A8) |
| Draw gate | `\|F\| ≥ 1` (RFC-015) |
| RES per mint | 100 RES (RFC-015) |
| RES supply cap | 21,000,000 RES (RFC-015) |
| Activation model | Genesis-only (no fork height, no versionbits bridge) |

Variable-T sourcing of the convergence inequality RHS from PoW difficulty is
**permanently rejected**. Dual-T (PoW T ≠ RES T on the inequality path) is
**permanently forbidden**.

Full freeze text: `MAINNET_PROTOCOL_FREEZE_DECLARATION.md`.

---

## 2. System Model

### 2.1 Chain

A **Chain** is a finite sequence of blocks `B(0), B(1), …, B(h)` extending from
genesis, where each block references its parent and heights are consecutive.

The **active chain** is the valid chain with greatest cumulative chainwork; ties
MUST be broken deterministically.

### 2.2 Block

A **Block** consists of a header and an ordered transaction list. A block at height
`h+1` MUST reference the hash of its parent at height `h`.

Block validity is a conjunction of header validity, PoW validity, transaction
validity, draw validity, supply validity, and witness canonicality predicates.

### 2.3 Transaction

A **Transaction** is an ordered list of inputs and outputs. Each output carries
`(nValue, scriptPubKey)`. Asset semantics are derived from script parsing, not from
stored asset tags.

**txid** (consensus identity) is defined as the double-SHA-256 hash of the
canonical non-witness serialization.

### 2.4 UTXO

The **UTXO set** maps unspent outpoints `(txid, index)` to output records
`(nValue, scriptPubKey)`.

UTXO is the authoritative ledger of spendable outputs. State transitions MUST be
expressed as spending inputs and creating outputs; no account-balance model is
permitted as a consensus truth source.

### 2.5 ORU (asset domain)

Within the asset domain, **ORU** denotes outputs and flows classified by script
as the operational base asset. ORU supply at height `h` evolves as:

```
S_ORU(h+1) = S_ORU(h) + R(h+1) − C(h+1)
```

where `R(h)` is block subsidy and `C(h)` is ORU destroyed by valid Converge
transactions in block `h`.

### 2.6 RES (asset domain)

Within the asset domain, **RES** denotes outputs classified by script as the
reserve asset. RES supply at height `h` evolves as:

```
S_RES(0) = 0
S_RES(h+1) = S_RES(h) + mint_amount · M(h+1)
```

where `M(h) ∈ {0, 1}` and `mint_amount` is the frozen per-mint quantity (100 RES).

### 2.7 State

**State** at height `h` is the complete consensus-visible outcome of applying blocks
`B(0..h)`. All honest nodes applying the same block sequence MUST converge to
identical state.

### 2.8 Consensus State

**Consensus State** `S(h)` is the authoritative tuple required to validate the next
block and to replay history:

```
S(h) = ( ChainTip, UTXO, S_ORU, S_RES, DrawState, PoWState, ValidationCursor )
```

Only fields derivable from chain history and frozen constants belong in consensus
state. Mempool contents, wallet labels, and explorer indices are excluded.

| Component | Purpose |
|-----------|---------|
| `ChainTip` | Height, block hashes, chainwork, header fields needed for binding |
| `UTXO` | Spendable output set |
| `S_ORU`, `S_RES` | Supply counters |
| `DrawState` | Convergence set, root, digests, eligibility, winner trace per block |
| `PoWState` | ASERT difficulty context |
| `ValidationCursor` | Height-indexed validation progress |

Detailed field definitions: **SPEC-001 State Model**.

### 2.9 Runtime State

**Runtime State** is any additional node-local or operational state not required
for consensus validity (e.g., mempool, peer tables, RPC caches, non-consensus indexes).

Runtime state MUST NOT influence block acceptance, draw outcome, or supply counters.
Any claim that runtime state is congruent with consensus state requires formal proof
or vector-backed alignment evidence ([SPEC-006](./SPEC-006-RUNTIME-FORMAL-MAPPING.md), [SPEC-007](./SPEC-007-EQUIVALENCE-BOUNDARIES.md)).

---

## 3. Consensus Objectives

Each objective is a **SHOULD** statement: a property the protocol is specified to
satisfy. Satisfaction is falsifiable; failure is a specification violation or an
implementation defect relative to this spec.

### CO-1: Chain Validity

**Statement:** The active chain MUST be a well-formed sequence of valid blocks from
genesis, selected by greatest cumulative chainwork with deterministic tie-break.

**Falsifiable by:** Presenting a block sequence accepted as the active chain where
any block fails a validity predicate, heights are not consecutive, or a lower-work
chain is chosen over a higher-work valid chain.

### CO-2: Deterministic Block Acceptance

**Statement:** For fixed consensus state `S(h)` and candidate block `B(h+1)`, every
correct validator MUST reach the same accept-or-reject verdict.

**Falsifiable by:** Two honest validators disagreeing on acceptance given identical
`(S(h), B(h+1))`.

### CO-3: Deterministic State Transition

**Statement:** When block `B(h+1)` is accepted, the successor consensus state
`S(h+1)` MUST be uniquely determined by `(S(h), B(h+1))`.

**Falsifiable by:** Two honest validators accepting the same block but producing
non-identical `S(h+1)`.

### CO-4: Supply Integrity

**Statement:** ORU and RES supplies MUST evolve only according to the specified
subsidy, destruction, and mint rules; RES MUST never exceed its cap; no asset MUST
be created or destroyed outside defined paths.

**Falsifiable by:** A valid block sequence yielding `S_RES(h) > MAX_RES`, negative
supply, or ORU/RES conservation violation on a non-coinbase transaction.

### CO-5: Replay Determinism

**Statement:** Re-executing blocks `B(0..h)` from genesis (or an agreed checkpoint)
MUST yield identical `S(h)` regardless of validator instance or platform.

**Falsifiable by:** Divergent `S(h)` from two replays of the same block sequence.

### CO-6: Reorg Determinism

**Statement:** Switching active chain tip via reorganization MUST produce the same
consensus state as if the winning chain had been built sequentially from genesis.

**Falsifiable by:** Post-reorg state differing from sequential application of the
same winning chain.

### CO-7: Protocol Freeze Preservation

**Statement:** Validators MUST enforce all frozen constants and forbidden transitions
listed in §1.6. No block valid under pre-freeze variable-T semantics MAY be accepted
if it violates frozen RFC-014 / RFC-015 rules.

**Falsifiable by:** Accepting a block whose convergence eligibility or RES issuance
depends on PoW-derived inequality RHS, deprecated symbols, or superseded RFC-014-only
economics (`|F| ≥ 2`, 1 RES per draw, 27,182,818 cap).

---

## 4. Assumptions

Assumptions are explicit preconditions. They are not proven in this document.
Each assumption MUST be listed in **SPEC-004 Trust Anchors**.

### 4.1 Network Assumptions

- **A-NET-1:** Validators eventually receive the same ordered block history for the
  active chain (synchrony is not required for consensus correctness, only for liveness
  in practice).
- **A-NET-2:** Peer relay, address discovery, and ban scoring are non-consensus; their
  failure modes MUST NOT alter `S(h)`.

### 4.2 Hash Assumptions

- **A-HASH-1:** `Hash256(x) = SHA256(SHA256(x))` behaves as a deterministic function
  from byte strings to 256-bit values for all consensus inputs.
- **A-HASH-2:** Collision resistance of SHA-256 is assumed for economic security
  arguments; consensus correctness requires only deterministic evaluation.
- **A-HASH-3:** σ digests and PoW hashes MUST NOT be interpreted as asset values
  (domain separation, RFC-008 §1).

### 4.3 Timestamp Assumptions

- **A-TIME-1:** Block header times participate in ASERT and median-time-past rules
  as specified in the RFC bundle; wall-clock time on individual nodes is not a
  consensus input.
- **A-TIME-2:** Validators MUST reject blocks whose headers violate frozen timestamp
  predicates independent of local clock skew beyond specified bounds.

### 4.4 Node Assumptions

- **A-NODE-1:** Validators execute validation as a pure function of chain history
  and incoming blocks (no hidden global mutable consensus state).
- **A-NODE-2:** Storage, indexing, and RPC layers MAY cache consensus results but
  MUST NOT feed caches back into validation predicates.
- **A-NODE-3:** Integer arithmetic for comparisons and cross-multiplication MUST
  be exact; floating-point MUST NOT appear on consensus paths (RFC-000 §2).

### 4.5 Trust Anchors (summary)

See §5 and [SPEC-004 Trust Anchors](./SPEC-004-TRUST-ANCHORS.md) (items A-1 through A-5).

---

## 5. Trust Anchors

Trust anchors are components or environments **accepted without machine-checked proof**
in the current formal program. They MUST be named explicitly; silent reliance is forbidden.

| Anchor | Role | Verified how (today) | Formal status |
|--------|------|----------------------|---------------|
| **SHA-256 / Hash256** | PoW, txid, σ, convergence root | Golden vectors; cross-implementation checks | Not proven equivalent to FIPS 180-4 in Coq |
| **PoW hash evaluation** | Header validity | Vector-backed C++ / Python / spec agreement | Structural model only |
| **System clock** | Non-consensus operations (logging, RPC timeouts) | Out of consensus scope | N/A |
| **External networking** | Block and tx propagation | Out of consensus scope | N/A |
| **uint512 cross-multiply** | Winner selection inequality | Convention + magnitude bound argument | Overflow bound not yet proven (A-2) |
| **σ → integer byte order** | Eligibility comparison | Shared LE convention + vectors | No single RFC pin (A-1) |
| **F membership predicate** | Convergence candidate set | Partial cross-check | Set equality vs runtime **UNKNOWN** (A-3) |
| **Golden vectors** | Regression oracle for bytes | Frozen recordings of runtime | Authoritative for bytes, not for intent |
| **RFC bundle** | Normative intent | Human ratification + freeze | Source for this specification layer |

Anything not listed here but relied upon by proofs or alignment claims MUST be added
before equivalence is asserted (**Rule 9**: equivalence claims prohibited unless formally proven).

---

## 6. Non-Goals

This specification explicitly excludes the following. Their presence in node software
MUST NOT be interpreted as protocol features unless promoted by a future Level-5 change.

| Non-goal | Rationale |
|----------|-----------|
| **Smart contracts** | No Turing-complete VM; no general on-chain computation |
| **Scripting extensions** | Witness V0/V1 and frozen Converge leaf only; no Taproot/BIP341 semantics |
| **Proof-of-Stake** | Fork choice is PoW / chainwork-based only |
| **Governance modifications** | No admin keys, upgrade proxies, or consensus-layer voting |
| **Mutable monetary policy** | ORU subsidy schedule and RES cap/mint are frozen at mainnet genesis semantics |
| **Account balances** | UTXO model only; no global balance table as truth source |
| **External oracle consensus** | Asset validity from script verification only |
| **Indexers as consensus** | Explorers and wallets are observational |

---

## 7. Relationship to Formal Verification

### 7.1 Layer hierarchy

```
  SPECIFICATION (this document family)
        │  defines: objectives, entities, SHOULD behaviors
        ▼
  FORMAL MODEL (Coq, executable oracle)
        │  defines: machine-readable refinements of spec predicates
        ▼
  PROOF (Coq theorems, alignment audits)
           proves: model satisfies spec properties; or documents gaps
```

**Specification precedes proofs.** No theorem MAY be stated as protocol truth until
the corresponding spec predicate exists and is falsifiable.

**Specification precedes runtime alignment.** Runtime behavior is evidence subject to
alignment; it does not define the specification.

### 7.2 Authority rule

**Specification supersedes implementation.**

When frozen runtime behavior diverges from ratified specification intent, the
divergence is classified as:

1. **Pre-freeze:** implementation defect to be corrected toward spec.
2. **Post-freeze:** Level-5 governance incident; neither silent spec amendment nor
   silent code change is permitted.

Post-freeze operative behavior for what the network accepts today is documented in
`MAINNET_PROTOCOL_FREEZE_DECLARATION.md`; that declaration does not grant
implementation the power to redefine SHOULD semantics retroactively.

### 7.3 Related artifacts

| Layer | Location |
|-------|----------|
| Specification (this track) | `docs/specification/SPEC-*.md` |
| RFC normative bundle | `docs/rfcs/RFC-000` … `RFC-015` |
| Formal model | `formal_verification/coq/`, `formal_verification/spec/` |
| Proof catalogs | `docs/formal/`, `formal_verification/coq/Phase*` |
| Runtime (evidence only) | Frozen C++ consensus at baseline tag |

Detailed invariant mapping: [SPEC-005 Consensus Invariants](./SPEC-005-CONSENSUS-INVARIANTS.md).

---

## 8. Freeze Compatibility

### 8.1 No consensus modification

This document is **specification only**. It introduces no consensus rule changes,
no new constants, no serialization changes, and no activation logic.

### 8.2 Subordination to freeze declaration

Nothing in this document supersedes:

- `MAINNET_PROTOCOL_FREEZE_DECLARATION.md`
- `TASK-MAINNET-FREEZE-001`
- `TASK-RFC015-OCCAM-REBALANCE-001` (final mainnet economics)

Where this skeleton summarizes frozen rules, the freeze declaration and ratified RFCs
 prevail on any wording conflict until SPEC-001…007 refine and ratify the specification
 layer through the formal program governance process.

### 8.3 Baseline reference

| Field | Value |
|-------|-------|
| Baseline commit | `c05f1a94beece54df872dc89c08715bd555fd987` |
| Release tag | `mainnet-freeze-rfc015-2026-05-26` |

---

## 9. Task Output Status

| Field | Status |
|-------|--------|
| Specification Layer | **INITIALIZED** |
| Consensus Specification | **ESTABLISHED** |
| State Specification | **ESTABLISHED** (`SPEC-001`) |
| Economic Specification | **ESTABLISHED** (`SPEC-002`) |
| Trust Anchor Specification | **ESTABLISHED** (`SPEC-004`) |
| Consensus Impact | **NONE** |
| Equivalence Claimed | **false** |

---

## 10. Revision History

| Version | Date | Change |
|---------|------|--------|
| 0.1 | 2026-06-05 | `TASK-SPEC-001` — initial consensus specification skeleton |
