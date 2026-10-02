# RIGOR Security Patch Boundary

## Status

**Status:** Design Principle / Security Boundary Document

This document defines the boundary for applying external security fixes to RIGOR.

It does not define consensus rules.

It does not authorize protocol changes.

It does not create governance or upgrade mechanisms.

**Principle:**

> RIGOR follows Bitcoin Core security research, not Bitcoin Core protocol evolution.

Bitcoin Core vulnerability fixes may be studied as security research. Bitcoin Core protocol evolution, new features, transaction-model changes, script-system changes, and economic-model changes are outside the RIGOR security-patch scope.

The frozen properties named below are the locks already recorded in the [Consensus Invariants](../protocol/RIGOR_CONSENSUS_INVARIANTS.md). This document checks patches against those locks. It does not amend them, and it does not restate them as a second specification.

```
consensus_authority = false
protocol_upgrade_mechanism = false
security_research_reference = true
```

---

## 1. Security Research vs Protocol Evolution

RIGOR distinguishes:

```
Security research
        ≠
Protocol evolution
```

Security fixes protect existing protocol properties.

Protocol evolution changes protocol properties.

Bitcoin Core releases may contain both.

Only the first category can be considered for RIGOR.

A release note, a version number, or a bundled feature is not evidence that the accompanying consensus change belongs in RIGOR. This document does not treat any Bitcoin Core version as a RIGOR upgrade target.

---

## 2. Patch Acceptance Questions

Every external security fix must answer three questions before it is classified.

### Question 1

Does the vulnerable component exist in RIGOR?

If it does not exist:

**IGNORE**

There is no RIGOR patch to take from that fix. The absence is not a backlog.

### Question 2

Does the patch preserve frozen consensus invariants?

The patch must not change:

- ORU issuance;
- RES supply;
- ORU → RES convergence;
- asset classification;
- transaction validity rules;
- Ed25519 authorization;
- `RGRPOW01` Proof-of-Work identity;
- genesis identity.

If it changes any of them:

**REJECT AS PROTOCOL CHANGE**

### Question 3

Does the patch change which blocks or transactions are valid?

If it does:

It is not a security patch.

It is a protocol change.

A fix is eligible to continue only when the component exists, the frozen invariants stay as they are, and the set of valid blocks and valid transactions stays the same.

```
Security Review Record

Issue:
Affected component:
Bitcoin reference:
Attack prevented:

RIGOR equivalent component:
Same attack surface:
YES / NO

Preserves frozen consensus invariants:
YES / NO

Changes valid blocks or transactions:
YES / NO

Decision:
APPLY
ADAPT
IGNORE
DO NOT PORT
```

A record with a blank field is incomplete. `Preserves frozen consensus invariants: NO` or `Changes valid blocks or transactions: YES` requires decision `DO NOT PORT`. This document does not authorize a protocol change.

---

## 3. Patch Classification

External security fixes fall into four classes.

### A — APPLY

The same component and the same attack surface exist in RIGOR. The fix can be adopted directly.

Adoption is direct only when all three questions pass. Equivalent protection for a crash, a race, a resource leak, a logging abuse path, or a corrupted-state path is the usual case. Valid blocks, valid transactions, asset ownership, and monetary rules stay the same.

### B — ADAPT

The attack surface exists, but RIGOR rules differ. The fix must be redesigned for RIGOR.

The Bitcoin diff is evidence of the defect. It is not the patch that lands in this tree. The review must show the same security goal and an implementation written against RIGOR components. The redesigned fix must still pass the three questions.

### C — IGNORE

The Bitcoin feature does not exist in RIGOR.

No equivalent attack surface exists. These fixes are not deferred work, and they are not an upgrade queue.

### D — DO NOT PORT

The change would pollute the frozen RIGOR protocol.

Do not port it. A patch in this class alters a protected consensus boundary or changes which blocks or transactions are valid. It is a protocol change, not a security patch. Porting it would publish an implementation of a different protocol.

| Decision | Class | When it is used |
|---|---|---|
| `APPLY` | A | Same component, same attack surface, three questions pass. |
| `ADAPT` | B | Attack surface exists; the fix is rewritten for RIGOR and the three questions pass. |
| `IGNORE` | C | The component does not exist in RIGOR. |
| `DO NOT PORT` | D | The change would alter the frozen protocol. |

---

## 4. Protected Consensus Boundaries

A security patch must not modify the following.

These are properties of the frozen protocol. The lines under each heading are the existing locks. This section does not introduce a parameter.

### Monetary

- ORU issuance
- RES maximum supply
- convergence formula

ORU issuance is the flat subsidy already enforced for every height ≥ 1, with genesis subsidy 0 and no halving term: `ORU_FLAT_BLOCK_SUBSIDY` = 128 ORU (`128 * COIN` = 12,800,000,000 tharns). RES maximum supply is `RES_MAX_SUPPLY` = 419,430,400,000,000 atoms = 2^22 RES. RES is created by destroying ORU. Coinbase does not create RES. The direction is ORU → RES only. The convergence formula is that existing destroy-to-mint predicate.

### Ledger

- UTXO model
- asset identity rules

Consensus state is one UTXO set and two native assets. Asset identity comes from script classification, not from an external registry.

### Execution

- script templates
- Ed25519 authorization
- signing domains

The accepted templates, the Ed25519 authorization path, and the signing domains `RGRORU01` and `RGRRES01` stay as they are. Public keys are 32 bytes. Signatures are 64 bytes.

### Network Identity

- Genesis identity
- `RGRPOW01` PoW identity

Mainnet genesis identity remains `e2ce92cfef740f92e8cc84a8541d8e760c776c668afd9b61740b3a4315241468`. Proof-of-Work remains `SHA3-256("RGRPOW01" || header_80)`. The header is 80 bytes. The domain tag is 8 bytes. The preimage is 88 bytes. Block identity remains `SHA256d(header_80)`.

Any modification of an item above is protocol replacement, not a security upgrade.

---

## 5. Security Fix Principle

A security patch corrects an implementation defect.

It does not redefine the protocol.

If the implementation diverges from the frozen protocol, correct the implementation.

Do not modify the protocol to fit an incorrect implementation.

A vulnerability in software is not evidence that the monetary rules should change.

Before merge, a reviewer still checks that the patch does not silently edit the consensus realization paths:

- `core/src/consensus/`
- `core/src/validation.cpp`
- `core/src/script/rigor_classify.cpp`
- `core/src/script/predicate_verify.cpp`
- `core/src/consensus/tx_verify.cpp`
- `core/src/pow.cpp`

This document does not approve a protocol change to those paths. Any such approval is outside this document. The suite `security_boundary_tests` pins the locks named above. A passing suite does not replace the three questions, and a failing suite does not redefine the protocol.

---

## 6. Relationship with Bitcoin Core

RIGOR tracks Bitcoin Core security research because RIGOR originated from Bitcoin Core architecture.

However:

RIGOR does not follow Bitcoin Core protocol evolution.

Not imported through this security-patch process:

- Taproot
- Schnorr
- Script expansion
- package relay
- cluster mempool
- BIP324
- monetary changes

This document does not schedule those items, and it does not name a Bitcoin Core version as a RIGOR upgrade target. They are not a future compatibility target, an upgrade path, or a required improvement. A change that alters a protected property is a protocol change, not a security patch.

Items whose components do not exist in RIGOR stay in class C. Items that would change issuance, assets, scripts, signatures, transaction validity, Proof-of-Work identity, or genesis identity stay in class D.

---

## 7. Relationship with Consensus Invariants

The frozen properties are recorded in [RIGOR Consensus Invariants](../protocol/RIGOR_CONSENSUS_INVARIANTS.md).

Consensus invariants define what cannot change.

The security patch boundary defines how security improvements are evaluated.

This document does not override the invariants. Where a proposed fix and the frozen protocol disagree, the frozen protocol determines the intended behavior. The implementation is corrected. The defect does not become a new protocol definition.

---

## 8. Relationship with RIGOR Principles

Long-term maintenance discipline is stated in [RIGOR Principles](../philosophy/RIGOR_PRINCIPLES.md).

Security discipline follows protocol permanence and minimal consensus principles.

Security research may strengthen the implementation. It stays inside the frozen protocol. A fix that changes the protocol is a different network, not a patch to this one.
