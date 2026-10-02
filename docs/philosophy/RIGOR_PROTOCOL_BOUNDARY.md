# RIGOR Protocol Boundary

**Status:** Philosophy / Maintenance Document

```
consensus_authority = false
governance_layer = false
protocol_upgrade_mechanism = false
```

This document does not define consensus rules.

It explains the boundary between protocol principles, implementation maintenance, and security improvements.

It introduces no governance model, no voting mechanism, no upgrade process, and no soft-fork or hard-fork rule.

---

## Core Principle

The maintenance order is:

```
Protocol
    ↓
Consensus Invariants
    ↓
Implementation
    ↓
Maintenance
```

Protocol defines the rules.

Consensus invariants preserve the frozen properties.

Implementation realizes the protocol.

Software maintenance protects the implementation.

The protocol decides the code. The code does not decide the protocol.

Code is the form in which the protocol is implemented. A function, a file, a test, or a repository permission may be necessary to run the protocol. None of them becomes the protocol.

An implementation error does not become a new protocol. If behavior diverges from the frozen protocol, the implementation contains the defect. The correction brings the implementation back into conformance with the protocol.

A documentation error is corrected in the document. An incorrect description is not resolved by treating the error as a rule, and an incorrect implementation is not adopted as the definition the document should copy.

A test failure indicates an implementation problem relative to the recorded frozen properties. It does not change the protocol definition. A passing test does not create a rule.

---

## Mainnet Freeze Principle

Mainnet activation is the transition:

```
Development Phase
        ↓
Frozen Monetary Protocol
```

What freezes is the protocol:

- monetary properties;
- asset definitions;
- issuance logic;
- validation rules;
- Proof-of-Work identity;
- the transaction-validity boundary.

Those properties are recorded in the [Consensus Invariants](../protocol/RIGOR_CONSENSUS_INVARIANTS.md). This document does not restate them as a second specification and does not add a property.

Freezing the protocol does not freeze the software. After activation, maintenance may still:

- update the software;
- fix security defects;
- improve tools;
- optimize performance.

That work changes the implementation. It leaves the frozen rules in place. A change to monetary properties, asset definitions, issuance logic, validation rules, Proof-of-Work identity, or transaction validity is not maintenance of RIGOR Mainnet. It is a different protocol and a different network.

---

## Security Boundary Principle

RIGOR follows Bitcoin Core security research.

RIGOR does not follow Bitcoin Core protocol evolution.

External security fixes are evaluated under the [Security Patch Boundary](../security/RIGOR_SECURITY_PATCH_BOUNDARY.md). Each fix must answer three questions:

1. Does the target component exist in RIGOR?
2. Does the fix preserve RIGOR's frozen invariants?
3. Does the fix change which blocks or transactions are valid?

If the component does not exist, there is nothing to port. If the fix fails to preserve the frozen invariants, or if it changes which blocks or transactions are valid, it is not a security patch.

A change to any of the following is a protocol change:

- ORU / RES rules;
- `OP_CONVERGE`;
- asset classification;
- Proof-of-Work identity;
- the signature model;
- transaction templates.

Security fixes may improve the implementation.

Security fixes must not redefine the protocol.

A vulnerability is an implementation defect. Closing it protects the rules that already exist. It is not a reason to alter those rules so that the defective behavior becomes valid.

---

## Developer Role

```
Developer = Protocol Maintainer
```

```
Developer ≠ Protocol Owner
```

Developers may:

- fix implementation defects;
- improve security;
- maintain infrastructure;
- improve tools.

Developers cannot, by changing code, revise RIGOR Mainnet into a different monetary system. A code change does not authorize:

- a change to monetary rules;
- a change to asset meaning;
- a change to the consensus boundary.

Developers can still edit the software. Editing the software is maintenance when the frozen protocol stays the same. Publishing software that alters monetary rules, asset meaning, or the consensus boundary publishes an implementation of a different protocol. Participants tell the two apart by verification. Repository access is not consensus authority.

---

## Relationship With Existing Documents

This document does not replace:

- [RIGOR Principles](./RIGOR_PRINCIPLES.md)
- [RIGOR Consensus Invariants](../protocol/RIGOR_CONSENSUS_INVARIANTS.md)
- [RIGOR Security Patch Boundary](../security/RIGOR_SECURITY_PATCH_BOUNDARY.md)

It connects their principles into a single maintenance philosophy.

Read in that order, the three documents answer three different questions:

| Document | Question it answers |
|---|---|
| [RIGOR Principles](./RIGOR_PRINCIPLES.md) | Why the protocol stays predictable, and what maintainers commit to protect |
| [RIGOR Consensus Invariants](../protocol/RIGOR_CONSENSUS_INVARIANTS.md) | Which frozen properties define RIGOR Mainnet |
| [RIGOR Security Patch Boundary](../security/RIGOR_SECURITY_PATCH_BOUNDARY.md) | How an external security fix is classified |

The authority those documents already share stays in force:

```
Protocol
    ↓
Consensus Invariants
    ↓
Implementation
    ↓
Maintenance
```

Principles state the commitment. Invariants record the frozen properties. The implementation realizes those properties. Maintenance, including security work, protects that implementation.

This document does not sit above them. If it conflicts with the frozen protocol, the frozen protocol determines the intended behavior. If it conflicts with the Consensus Invariants on what is frozen, the invariants describe those properties and this document does not amend them. If it conflicts with the Security Patch Boundary on how a fix is classified, the boundary decides whether the fix may be adopted.

Security fixes may improve the implementation.

Security fixes must not redefine the protocol.
