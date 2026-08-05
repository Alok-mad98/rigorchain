# RFC-014: Frozen Convergence Target (Conv-Decouple-01)

> **SUPERSEDED IN PART by SECURITY-001-A8 (Sigma-PoW-01).**
> This RFC correctly froze `RES_PROBABILITY_TARGET` and decoupled `T` from `target_prev`.
> Its claim that `target_prev` must remain inside `bind_commit` for σ domain separation is
> **overridden**: A-8 places `B_{h-1}` verbatim in σ slot `[64..95]`, so the parent header
> (and therefore its nBits) is committed by the seed itself. `target_prev` survives only as
> a PCORB witness field checked against the parent header. See
> `docs/security/SECURITY_001_A8_REPORT.md` and RFC-000 v1.1 / RFC-008 v1.5.
>
> The frozen constant `T = floor(2^240 / MIN_CONVERGENCE_AMOUNT)` (v1.1 / A-6) remains normative.


> ## ⚠ §2.1 SUPERSEDED BY [RFC-014 v1.1](./RFC-014-v1.1.md) (2026-08-02)
>
> The value of `RES_PROBABILITY_TARGET` given in §2.1 below (`2^240`) **is wrong** and must
> not be implemented. It was derived at a normalised weight `f = 1`, but the implemented
> weight is the convergence amount **in satoshis**, so at the minimum admissible weight the
> right-hand side of the draw inequality exceeded the digest range by a factor of ≈1526 and
> the eligibility filter was vacuous.
>
> **Corrected value** (RFC-014 v1.1 §2.1):
>
> ```
> RES_PROBABILITY_TARGET := floor(2^240 / MIN_CONVERGENCE_AMOUNT)
>                         = 0x00000000002af31dc4611873bf3f70834acdae9f0f4f534f5d60585a5f1c1a3c
> ```
>
> The *intent* of §2.1 — per-input probability `p = 2^-16` at 1 ORU — is unchanged and is
> what v1.1 actually delivers. Everything else in this document, including the §5.4
> issuance economics, the decoupling from PoW difficulty, and the `target_prev` /
> `bind_commit` binding, **remains normative and correct**.
>
> Read §2.1 below as a historical record of the defect. See
> [`docs/security/SECURITY-001-A-FINAL-REPORT.md`](../security/SECURITY-001-A-FINAL-REPORT.md)
> for the finding (A-6).

**Version:** 1.0 (Conv-Decouple-01) — FINAL, §2.1 superseded by v1.1
**Status:** Consensus-Critical, Pre-Mainnet, FINAL (activated by `TASK-CONV-T-ACTIVATION-002`, authorized by `H_CONV_T_SCOPE_RECALIBRATION_VERDICT.md`)
**Scope:** Constitutional freeze of the RES convergence probability target as a
constant independent of the PoW difficulty target. Supersedes the inequality
right-hand side semantics of `RFC-008` §6.1/§6.2/§7.1 once activated.

**This document MUST be interpreted under RFC-000-FINAL Constitutional Nomenclature.**

> **v1.0 changelog (2026-05-25)**: Initial draft. Introduces
> `RES_PROBABILITY_TARGET = 2^240` as an immutable constitutional constant.
> Permanently decouples RES convergence eligibility from PoW difficulty.
> Preserves `target_prev` cryptographic binding inside `bind_commit` and inside
> the RES Mint witness, for **replay-domain separation only**. No hard fork:
> mainnet is not yet live.

---

## 0. Absolute Principle (MUST)

The RES convergence inequality right-hand side MUST be the constitutional
constant `RES_PROBABILITY_TARGET`. It MUST NOT be derived from PoW difficulty
(`parent.nBits`), from ASERT state, from chain tip, from time, from any header
field, or from any other variable consensus quantity. Any deviation is a
consensus violation.

This RFC supersedes every prior statement in `RFC-008` §6 and §7 that named the
PoW-difficulty-derived `T` as the inequality RHS. `target_prev` is retained
for **σ-preimage replay-domain separation only**, per §3 below.

---

## 1. Background and Motivation

### 1.1 Discovery

The pre-activation audit captured in
`@./docs/research/res_simulation/RES_VARIABLE_T_THERMODYNAMIC_ANALYSIS.md`
demonstrated that the prior implementation, which sourced the RES inequality
target from `SetCompact(parent.nBits)`, exhibits the closed-form ASERT
equilibrium property:

```
p_per_input  =  T_eq / 2^256  =  1 / (target_spacing · hashpower)
RES_per_year ≈  N · BLOCKS_PER_YEAR / (target_spacing · hashpower)
             =  N · 87.66 / hashpower         (target_spacing = 600 s)
```

Therefore RES annual issuance is `O(1/H)` and collapses to numerical zero for any
realistic mining hashpower (`H ≥ 10⁶ h/s` yields `RES/yr ≤ 1.1·10⁻²`).

### 1.2 Adjudication record

| Predecessor | Verdict | Result |
|---|---|---|
| `H-RES-PROBABILITY-FREEZE-001` | RES-CONSTANT-FREEZE-READY at `2^240` (under hypothetical constant-T model) | Numerical value accepted; architectural premise corrected by `TASK-RFC-AMENDMENT-RES-CONSTANT-001` HALT report. |
| `TASK-RES-VARIABLE-T-THERMODYNAMIC-ANALYSIS-001` | Variable-T rejected as monetary mechanism. PATH B-PRIME recommended. | Architectural direction set. |
| `H-RES-ARCHITECTURE-VERDICT-003` | PATH B-PRIME adopted. `T_conv = 2^240` confirmed optimal under decoupled architecture. Variable-T REJECTED (final, irreversible). | Drafting authorized. |

This RFC operationalizes that adjudication record at the protocol-document layer.

---

## 2. Normative Definition

### 2.1 The constant — ⚠ SUPERSEDED, see [RFC-014 v1.1 §2.1](./RFC-014-v1.1.md)

> The block below records the **defective** v1.0 value. It is retained for provenance only.
> Do not implement it; see the banner at the top of this document.

```
RES_PROBABILITY_TARGET  :=  2^240        // WRONG — vacuous at f in satoshis
                        =  0x0001000000000000000000000000000000000000000000000000000000000000
                        // (Bitcoin uint256S / GetHex display; internal LE byte[30]=0x01)
                           (256-bit big-endian, one bit set at position 240,
                            all other bits zero)
                        =  1766847064778384329583297500742918515827483896875618958121606201292619776
                           (decimal)
```

Per-input convergence probability:

```
p_per_input  :=  RES_PROBABILITY_TARGET / 2^256
              =  2^{-16}
              =  1 / 65536
              =  1.52587890625 · 10^{-5}
```

### 2.2 The inequality (consensus rule, immutable)

```
RES CONVERGENCE ELIGIBILITY:

    candidate i is eligible
        iff
    |F| >= 1  AND  f_i >= 1  AND  H_i  <  RES_PROBABILITY_TARGET · f_i
```

Where:
- `H_i` is the σ_i digest of candidate `i`, as defined in `RFC-008` §3 / §5 (100-byte preimage, double-SHA-256). The σ derivation is **unchanged** by this RFC.
- `RES_PROBABILITY_TARGET` is the 256-bit constant defined in §2.1.
- `f_i` is the RES weight of candidate `i` as defined in `RFC-000` §7 (Mint Cost or transaction fee, integer, ≥ 1).
- `F` is the set of valid candidate transactions per `RFC-010` §1 (Step 2).
- RFC-015 supersedes the prior `|F| >= 2` participant rule. The minimum
  cardinality is now `|F| >= 1`; the minimum economic participation requirement
  remains `f_i >= 1` / at least 1 ORU converged.

### 2.3 Comparison rule (implementation-bound)

The cross-multiplication form of `RFC-008` §8 is **unchanged**. Eligibility is
computed as

```
left   :=  (uint512) H_i
right  :=  (uint512) RES_PROBABILITY_TARGET  *  (uint512) f_i
eligible_i  :=  ( left < right )
```

Winner selection across eligible candidates remains
`H_i · f_j < H_j · f_i` (no division, no floating point, exact 512-bit arithmetic).

### 2.4 Encoding canonicity

`RES_PROBABILITY_TARGET` MUST be materialized as a `uint256` constant in
`Consensus::Params` (or equivalent). It MUST NOT be encoded via the compact
target format (`SetCompact`), MUST NOT be re-derived from any other quantity at
runtime, and MUST NOT vary across networks (mainnet, testnet, devtest, regtest,
signet all use the same value).

---

## 3. Sigma-CR-01 Preservation (Replay-Domain Binding)

### 3.1 What is preserved

The σ_i preimage layout is **byte-identical** to `RFC-008` v1.1:

```
σ_i  =  Hash256( txid_i  ||  C  ||  bind_commit  ||  LE32(h) )
bind_commit  =  Hash256( B_{h-k}  ||  LE32(target_prev) )
target_prev  =  parent_header.nBits   (uint32, little-endian)
```

The following are **NOT changed** by this RFC:

| Element | Status |
|---|---|
| 100-byte σ preimage | UNCHANGED |
| `bind_commit` 36-byte preimage | UNCHANGED |
| Inclusion of `target_prev` in `bind_commit` | UNCHANGED (mandatory) |
| Inclusion of `target_prev` in the RES Mint witness (`RFC-005` §line 320) | UNCHANGED (mandatory) |
| `target_prev` byte width (uint32 LE) | UNCHANGED |
| Ed25519 signing scope over σ | UNCHANGED |
| Bootstrap-bind constant for `1 <= h < k` (`RFC-000` §"Binding rule") | UNCHANGED |

### 3.2 Mandatory declaration

> **`target_prev` is preserved for replay-domain binding,
>  NOT for convergence probability derivation.**

This statement is normative. Implementations and downstream RFCs MUST treat
`target_prev` as:

1. **Cross-difficulty domain-separation tag** inside `bind_commit`, providing
   the property defended by `Sigma-CR-01` (`RFC-008` v1.1 §3.2.2).
2. **Replay-equivalence witness field** persisted under the Ed25519 signature
   of every RES Mint witness, enabling verifiers to re-derive `bind_commit`
   without consulting external state.

`target_prev` MUST NOT be:

3. Read as the inequality right-hand side.
4. Reinterpreted as the RES probability source.
5. Replaced by `RES_PROBABILITY_TARGET` inside σ derivation.

### 3.3 Why both fields are kept

Removing `target_prev` from `bind_commit` would re-open the cross-difficulty
replay vector identified in `audit_sigma_replay_domain_separation.md`. Removing
`target_prev` from the RES Mint witness would require an external state lookup
at proof verification (parent `nBits` query), breaking the self-proving property
of the witness. Both removals are forbidden.

Keeping both costs **zero** runtime work that is not already done today, and
preserves all `Sigma-CR-01` security claims byte-for-byte.

---

## 4. Architectural Declaration

### 4.1 Two independent control systems

The RIGOR monetary architecture, as locked by this RFC, consists of two
**intentionally decoupled** control systems:

```
┌──────────────────────────────────────┐   ┌──────────────────────────────────────┐
│ 1. PoW SECURITY CONTROLLER           │   │ 2. RES ISSUANCE CONTROLLER           │
│                                      │   │                                      │
│   - State:    nBits (compact target) │   │   - State:    RES_PROBABILITY_TARGET │
│   - Driver:   ASERT (per-block)      │   │   - Driver:   IMMUTABLE              │
│   - Objective:                       │   │   - Objective:                       │
│       block_time → target_spacing    │   │       p_per_input → 2^{-16}          │
│   - Inputs:   actual block times     │   │   - Inputs:   none                   │
│   - Outputs:  hashpower equilibrium  │   │   - Outputs:  RES issuance rate      │
│                                      │   │                                      │
│   Constitutional status: ALGORITHMIC │   │   Constitutional status: CONSTANT    │
└──────────────────────────────────────┘   └──────────────────────────────────────┘

           Coupling between systems: NONE (consensus-level)
           Shared cryptographic input: target_prev only,
                                       via bind_commit, for σ domain separation.
```

### 4.2 Consequence

- Hashpower changes affect block-time dynamics. They do **not** affect RES
  issuance rate.
- Mining-difficulty manipulation (e.g. brief hashpower withdrawals to inflate
  T) becomes a security-system-only attack vector; it has no monetary leverage.
- The RES issuance schedule is the same on a chain with `H = 100 h/s` and a
  chain with `H = 10²¹ h/s`.

### 4.3 Constitutional immutability

`RES_PROBABILITY_TARGET` joins the immutable post-mainnet set documented in
`RFC-008` §14:

| Constant | Source |
|---|---|
| `MAX_RES_SUPPLY` | `RFC-001` §4.3 |
| `RES_REWARD_AMOUNT` | `RFC-001` §4.2 |
| `ORU_INITIAL_BLOCK_SUBSIDY` / halving schedule / floor `125` | `RFC-001` §3 |
| σ preimage layout (100 bytes) | `RFC-008` §14 |
| Hash function (double-SHA-256) | `RFC-008` §14 |
| Inequality form (`H_i < T · f_i`) | `RFC-008` §14 |
| Cross-multiplication selection (`H_i · f_j < H_j · f_i`) | `RFC-008` §14 |
| Integer-only arithmetic, no floats | `RFC-008` §14 |
| `|F| ≥ 2` participant rule | `H-RES-PROBABILITY-FREEZE-001` §REQUIRED-3 |
| **`RES_PROBABILITY_TARGET = 2^240`** | **this RFC** |

Modification requires a hard fork after mainnet launch. Modification before
mainnet launch requires a new H-layer verdict revoking
`H-RES-ARCHITECTURE-VERDICT-003`.

---

## 5. Economic Justification (Summary)

Full analysis lives in:

- `@./docs/research/res_simulation/RES_PROBABILITY_SIMULATION_REPORT.md`
- `@./docs/research/res_simulation/RES_VARIABLE_T_THERMODYNAMIC_ANALYSIS.md`
- `@./docs/freeze/H_RES_001_PROBABILITY_FREEZE_VERDICT.md`
- `@./docs/freeze/H_RES_003_ARCHITECTURE_VERDICT.md`

Summary:

1. **Variable-T causes issuance collapse.** `RES/year ∝ 1/hashpower`.
   At `H = 10⁶ h/s`, `RES/yr ≈ 0.011`. At `H = 10⁹ h/s`,
   `RES/yr ≈ 1.1·10⁻⁵`. The protocol economically does not function under
   variable-T at any non-toy network.

2. **Variable-T creates hidden governance via ASERT.** The ASERT controller's
   objective is `block_time → target_spacing`; under variable-T, RES monetary
   policy is a passive externality of that objective. Hashpower migration and
   miner coordination become the de-facto RES central bank.

3. **Constant `RES_PROBABILITY_TARGET` restores:**
   - **Stable issuance** — `RES/yr ≈ 100` at the locked `N = 125` ORUM equilibrium.
   - **Replay independence** — no chain-state lookup is needed at the inequality
     right-hand side.
   - **Thermodynamic equilibrium** — RES system has a fixed point that does not
     drift with mining politics.
   - **Constitutional monetary predictability** — frozen 256-bit constant.

4. **`RES_PROBABILITY_TARGET = 2^240` is uniquely optimal.** Sensitivity
   analysis at `N = 125`:

   | Candidate | Issuance/yr | Saturation @ N=5000 | Cold-start @ N=2 | Verdict |
   |---:|---:|---:|---:|---|
   | 2^248 | 20,349 | 100% (degenerate) | 0.89 d | Rejected (degenerate) |
   | 2^244 | 1,581 | 70% | 14 d | Rejected (16× over-inflation) |
   | **2^240** | **100** | **7.3%** | **228 d** | **SELECTED** |
   | 2^236 | 6 | 0.5% | 3,641 d | Rejected (dead-chain risk) |

   `2^240` uniquely satisfies all four: anti-saturation, cold-start viability,
   long-term scarcity (~209,500 years to `MAX_RES_SUPPLY = 21,000,000 RES` at
   ~100 RES/yr), and mathematical
   elegance (`p = 2^{-16}` is the simplest power-of-two below the saturation
   cliff; `100 RES/yr @ N=125` is emergent, not engineered).

---

## 6. Migration Model (Mandatory)

### 6.1 Activation discipline

```
Activation gate            :  GENESIS-ONLY
Fork-height check          :  FORBIDDEN
versionbits / BIP-9 bit    :  FORBIDDEN
Soft-fork transition path   :  FORBIDDEN
Dual-rule (legacy + new) period : FORBIDDEN
Runtime configurability    :  FORBIDDEN  (constant is constitutional, not policy)
```

### 6.2 Pre-launch posture

`@./docs/freeze/MAINNET_LAUNCH_CHECKLIST.md` confirms RIGOR is
pre-mainnet. No long-lived public chain has committed RES inequality outcomes
under the variable-T regime irrevocably. Therefore:

```
FULL CHAIN REGENERATION IS CONSTITUTIONALLY PREFERRED.
```

All RIGOR networks (mainnet, testnet, devtest, regtest, signet) re-initialize
from new genesis blocks under `RES_PROBABILITY_TARGET`. Existing testnet /
devtest / regtest databases are archived (hash-attested) but not re-validated
under the new inequality.

### 6.3 Replay safety post-activation

Replay equivalence across nodes is **trivially preserved** because
`RES_PROBABILITY_TARGET` is a single 256-bit constant baked into
`Consensus::Params`. Determinism is bit-exact across compilers, platforms, and
endianness handling (the constant is a fixed-width integer, not a float, not a
compact-encoded target).

### 6.4 Activation is not part of this RFC

This RFC is a **drafting** artifact. Code activation is performed by
`TASK-CONV-T-ACTIVATION-001` after `H-RFC-014-REVIEW-001` approval, per the
H-RES-003 downstream task graph.

---

## 7. Downstream Patch References (Future Work)

This RFC does **not** edit any other RFC. It declares the **future** amendments
that will be issued under separate task IDs once `H-RFC-014-REVIEW-001` accepts
this draft:

### 7.1 `RFC-008-canonical-mathematical-specification.md`

- **§6.1** "Inequality definition": replace `T` with `RES_PROBABILITY_TARGET`
  on the inequality right-hand side. Add note that `target_prev` remains in
  `bind_commit` for replay-domain separation.
- **§6.2** "Computation rule": clarify that the right-hand side is a 256-bit
  constant, not a `SetCompact`-decoded value.
- **§7.1** "Must Use 512-bit": no semantic change; update example to
  `uint512 product = (uint512)RES_PROBABILITY_TARGET * (uint512)f_i`.
- **§14** "Final lock": item 4 updated; item enumeration extended to include
  `RES_PROBABILITY_TARGET = 2^240` as immutable.
- **§changelog**: add v1.3 — Conv-Decouple-01.

### 7.2 `RFC-001-deterministic-selection.md`

- **§4** "RES Mint": reference `RES_PROBABILITY_TARGET` as the eligibility
  threshold.

### 7.3 `RFC-005-validation-state-machine.md`

- **§7** "σ Usage Location": the eligibility check reads
  `RES_PROBABILITY_TARGET`; the witness `target_prev` field is **unchanged**
  (still required for `bind_commit` re-derivation).

### 7.4 `RFC-007-formal-verification-coq.md`

- **Issuance-rate lemma update only.** Substitute the symbolic `T` with the
  constant `RES_PROBABILITY_TARGET`. Proof structure is unchanged. Supply-bound
  theorem and σ-derivation lemmas are untouched.

### 7.5 `RFC-012-golden-test-vector-generator.md`

- **Golden vector regeneration requirement** under the new inequality RHS. The
  generation algorithm is unchanged; vector values change. Both
  `formal_verification/golden_vectors.json` and
  `formal_verification/rigor_golden_vectors.json` MUST be regenerated and
  re-signed in the same H-layer-blessed batch as the code activation.

### 7.6 Out of scope (this RFC does not amend)

`RFC-002`, `RFC-003`, `RFC-004`, `RFC-006`, `RFC-009`, `RFC-010`, `RFC-011`,
`RFC-013-*`, `RFC-ADDRESS-*`, `RFC-ARCH-*`, `RFC-000`, `RFC-000A` are
**unaffected** by Conv-Decouple-01.

---

## 8. Forbidden Changes (Drafting Scope)

This RFC document and its drafting task `TASK-RFC-014-DRAFT-001` MUST NOT
trigger any of:

| Surface | Status under this RFC |
|---|---|
| `core/src/**` source files | UNCHANGED |
| `core/src/consensus/**` | UNCHANGED |
| `core/src/kernel/chainparams.cpp` | UNCHANGED |
| `core/src/node/miner.cpp` | UNCHANGED |
| `core/src/validation.cpp` | UNCHANGED |
| `core/src/wallet/**` | UNCHANGED |
| `core/src/net*.{cpp,h}` (P2P) | UNCHANGED |
| Witness serialization | UNCHANGED |
| Transaction formats | UNCHANGED |
| Networking / wire protocol | UNCHANGED |
| Genesis parameters (any chain) | UNCHANGED |
| Existing test vectors | UNCHANGED (will be regenerated downstream) |
| Existing golden vectors | UNCHANGED (will be regenerated downstream) |
| Existing Coq proofs | UNCHANGED (will receive a constant-substitution delta downstream) |

Activation is performed by `TASK-CONV-T-ACTIVATION-001` only. This RFC is
**drafting-only**.

---

## 9. Mandatory Safety Inventory

`UNCHANGED by Conv-Decouple-01`:

| Item | Status |
|---|---|
| RES Mint witness byte layout (`RFC-005` §line 320 et seq.) | UNCHANGED |
| Transaction serialization | UNCHANGED |
| Block header structure | UNCHANGED |
| P2P wire format | UNCHANGED |
| Merkle root derivation | UNCHANGED |
| Convergence Root derivation (`RFC-000` §6) | UNCHANGED |
| σ_i preimage (100 bytes) | UNCHANGED |
| `bind_commit` preimage (36 bytes) | UNCHANGED |
| `target_prev` field width (uint32 LE) | UNCHANGED |
| Ed25519 signing scope over σ | UNCHANGED |
| Tie-break rule (`RFC-000` §9) | UNCHANGED |
| Cross-multiplication selection | UNCHANGED |
| Bootstrap-bind constant (`RFC-000` §"Binding rule") | UNCHANGED |
| Minimum convergence cardinality | SUPERSEDED BY RFC-015 (`|F| >= 1`) |
| ASERT controller | UNCHANGED |
| PoW validation (`BlockHash < SetCompact(parent.nBits)`) | UNCHANGED |
| Halving schedule | UNCHANGED |
| `MAX_RES_SUPPLY` | SUPERSEDED BY RFC-015 (`21,000,000 RES`) |
| `RES_REWARD_AMOUNT` | SUPERSEDED BY RFC-015 (`100 RES` per mint) |

**Only one consensus quantity changes:**

```
RHS source of the RES convergence inequality:
    SetCompact(target_prev)
        →
    RES_PROBABILITY_TARGET    (= 2^240, constant)
```

RFC-015 later supersedes only the RES mint amount and minimum convergence
cardinality. The RFC-014 `RES_PROBABILITY_TARGET = 2^240` rule remains
unchanged.

---

## 10. Conformance Statement

An implementation conforms to RFC-014 v1.0 if and only if all of the following
hold:

1. The RES convergence inequality is evaluated as
   `H_i  <  RES_PROBABILITY_TARGET · f_i`, with `RES_PROBABILITY_TARGET = 2^240`,
   computed in ≥ 512-bit precision per `RFC-008` §7.
2. `RES_PROBABILITY_TARGET` is a single 256-bit constant in `Consensus::Params`
   (or equivalent) with no per-network divergence.
3. `RES_PROBABILITY_TARGET` is never derived at runtime from `nBits`, ASERT
   state, time, or any header field.
4. `target_prev` continues to be folded into `bind_commit` per `RFC-008`
   v1.1 §3.2.1 (Sigma-CR-01) and continues to be persisted in the RES Mint
   witness per `RFC-005` §7.
5. The RFC-015 minimum cardinality rule `|F| >= 1` is enforced.
6. All other invariants enumerated in §9 are preserved byte-for-byte.

Any implementation violating any of (1)-(6) is a consensus-violating
implementation.

---

## 11. Revision Log

| Version | Date | Change |
|---|---|---|
| 1.0-DRAFT | 2026-05-25 | Initial draft. Conv-Decouple-01. Authorized by `H-RES-ARCHITECTURE-VERDICT-003`. Pending `H-RFC-014-REVIEW-001`. |

---

## 12. References

- `@./docs/rfcs/RFC-000-consensus-constitution.md`
- `@./docs/rfcs/RFC-001-deterministic-selection.md`
- `@./docs/rfcs/RFC-005-validation-state-machine.md`
- `@./docs/rfcs/RFC-007-formal-verification-coq.md`
- `@./docs/rfcs/RFC-008-canonical-mathematical-specification.md`
- `@./docs/rfcs/RFC-012-golden-test-vector-generator.md`
- `@./docs/freeze/H_RES_001_PROBABILITY_FREEZE_VERDICT.md`
- `@./docs/freeze/H_RES_003_ARCHITECTURE_VERDICT.md`
- `@./docs/research/res_simulation/RES_PROBABILITY_SIMULATION_REPORT.md`
- `@./docs/research/res_simulation/RES_CONSTANT_ACTIVATION_REPORT.md`
- `@./docs/research/res_simulation/RES_VARIABLE_T_THERMODYNAMIC_ANALYSIS.md`

---

**END OF RFC-014 v1.0 DRAFT**
