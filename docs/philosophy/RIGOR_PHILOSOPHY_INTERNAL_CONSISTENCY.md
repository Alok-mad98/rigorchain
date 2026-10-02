# RIGOR Philosophy Internal Consistency

**Status:** Philosophy Maintenance Record

```
consensus_authority = false
governance_layer = false
protocol_upgrade_mechanism = false
```

This document does not define consensus rules.

It explains how RIGOR principles remain internally consistent and guide long-term maintenance decisions.

It is not a protocol specification, a governance model, a voting system, or an upgrade process.

---

## 1. What Does Internal Consistency Mean?

Internal consistency means the core principles do not contradict one another.

The core principles are:

Protocol defines the rules.

Consensus invariants preserve the frozen properties.

Implementation realizes the protocol.

Maintenance improves the implementation.

These statements constrain each other.

If the protocol defines the rules, code cannot create the protocol. A repository, a test suite, or a developer decision is not a source of rules.

If mainnet activation freezes the monetary protocol, maintenance cannot change monetary properties. A release that alters those properties is not maintenance of the same protocol.

If a security fix protects the implementation, the security fix cannot be an entrance for changing the protocol. Closing a defect leaves the frozen properties as they are.

If the developer is a maintainer, the developer does not own the protocol. Permission to edit software is not permission to redefine what the network recognizes as valid.

The long-term order these principles share is:

```
Protocol
    ↓
Frozen Properties
    ↓
Implementation
    ↓
Maintenance
```

---

## 2. Consistency Verification Model

A maintenance decision is checked from the principle down to the change:

```
Philosophy
      ↓
Protocol Boundary
      ↓
Consensus Invariants
      ↓
Implementation Rules
      ↓
Maintenance Decisions
```

Each layer answers one question.

**Philosophy** answers why the protocol stays stable. Predictable rules are the point. Continuous redesign of the money is not.

**Protocol Boundary** answers which changes belong to the protocol and which belong to maintenance. A change that alters what is valid is a protocol change. A change that leaves validity alone can be reviewed as maintenance.

**Consensus Invariants** answer which properties are frozen. They record the properties. They do not authorize a later edit of those properties.

**Implementation** answers how the frozen properties are realized. Reference code and running software execute the rules. They do not replace the rules when they diverge from them.

**Maintenance** answers how the implementation is repaired and improved. Security fixes, bug fixes, performance work, and tools belong here when they preserve the frozen properties.

A decision that skips a layer is not internally consistent. A patch cannot be called maintenance at the bottom if it fails the boundary question above it.

---

## 3. Consistency Tests

Three tests check that a proposed change still fits the principles.

### Test 1: Authority Consistency

Question: who defines the rules?

The required order is:

```
Protocol > Implementation
```

The check fails if any of the following is treated as the source of the protocol:

- code becomes the source of the protocol;
- tests define the protocol;
- developers define the rules.

Conclusion: an implementation error is an implementation problem. It does not redefine the protocol. The document that misstates a frozen property is corrected. The implementation that diverges from the frozen protocol is corrected. Neither error becomes a new definition.

### Test 2: Boundary Consistency

Question: what may be changed as ordinary maintenance?

Allowed:

- security fixes;
- bug fixes;
- network hardening;
- tooling improvements;
- performance optimization.

Ordinary maintenance must not change:

- ORU rules;
- RES rules;
- issuance logic;
- `OP_CONVERGE`;
- asset classification;
- Proof-of-Work identity;
- transaction validity.

If it changes any of those, the result is a **Protocol Change**.

It is not **Maintenance**.

### Test 3: Future Decision Consistency

Any future modification must answer:

- Does this change valid blocks?
- Does this change valid transactions?
- Does this change monetary properties?
- Does this change frozen invariants?
- Does this change protocol identity?

If any answer is yes, the modification is a protocol change.

If every answer is no, it can be reviewed as implementation maintenance.

These questions do not create an upgrade procedure. They classify a proposal that already exists. A yes answer is not a request that then becomes eligible through voting, a version bit, or a scheduled activation. It identifies a different protocol.

---

## 4. Examples

### Security Fix

Example: a logging-abuse fix.

Result: **Implementation Maintenance**.

Reason: it does not change which blocks are valid. The frozen properties stay as they are. The fix corrects the implementation.

### Protocol Change

Example: a modification of the RES issuance rule.

Result: **Protocol Change**.

Reason: it changes a monetary property. Maintenance of the existing protocol does not include that change. Software that validates a different issuance rule is an implementation of a different protocol.

### Bitcoin Compatibility Proposal

Example: introducing Taproot.

Judgment: does it sit inside RIGOR's current security boundary?

Taproot is not a component of the frozen RIGOR protocol. If the proposal changes transaction templates or validation rules, the result is a **Protocol Change**.

It is not an ordinary security upgrade. Following Bitcoin Core security research does not mean following Bitcoin Core protocol evolution. A feature that redefines which transactions are valid is not imported as a patch.

---

## 5. Final Principle

RIGOR does not preserve itself by preventing all changes.

RIGOR preserves itself by distinguishing:

changes that improve the implementation

from

changes that redefine the protocol.

Security fixes may improve the implementation.

Security fixes must not redefine the protocol.

---

## 6. Relationship With Existing Documents

This document does not replace:

- [RIGOR Principles](./RIGOR_PRINCIPLES.md)
- [RIGOR Protocol Boundary](./RIGOR_PROTOCOL_BOUNDARY.md)
- [RIGOR Consensus Invariants](../protocol/RIGOR_CONSENSUS_INVARIANTS.md)
- [RIGOR Security Patch Boundary](../security/RIGOR_SECURITY_PATCH_BOUNDARY.md)

It only explains the logical consistency among those files.

Principles say why the protocol stays predictable. The protocol boundary separates protocol changes from maintenance. The consensus invariants record what is frozen. The security patch boundary classifies external fixes.

This file does not sit above them, and it is not a new protocol authority. If it conflicts with the frozen protocol, the frozen protocol determines the intended behavior. If it conflicts with the consensus invariants on what is frozen, the invariants describe those properties. If it conflicts with the security patch boundary on how a fix is classified, the boundary decides.
