# RIGOR Principles

**Status:** Foundational philosophy document.

This document does not define consensus rules.
It does not introduce governance.
It does not authorize protocol modification.

Consensus authority is the frozen RIGOR protocol.

The production consensus implementation is the executable realization of that protocol.

The documented consensus invariants describe the protocol properties.

If an implementation diverges from the frozen protocol, the implementation contains the defect and must be corrected.

The defect does not redefine the protocol.

Where documentation and implementation disagree, the frozen protocol definition determines the intended behavior.

The implementation must be reviewed and corrected to conform to the protocol.

```
consensus_authority = false
governance_layer = false
protocol_upgrade_mechanism = false
```

---

## The Purpose of RIGOR

RIGOR is built around the belief that a monetary protocol gains long-term credibility not through continuous modification, but through predictable rules.

The purpose of a monetary blockchain is not to constantly evolve into a different system.

Its purpose is to provide a stable environment where participants can understand the rules and make independent decisions.

Two kinds of work follow from that purpose.

Software maintenance changes how an implementation is written, built, and operated: defect fixes, performance work, wallets, and tools. That work is ordinary development.

A monetary protocol change alters the rules participants rely on: what may be issued, which assets exist, and which transactions are valid. That work is not maintenance of the same system. It specifies a different one.

Predictable rules are what let a participant decide without waiting to learn which rule set the maintainers will prefer next.

---

## Protocol Permanence

Monetary rules are not development parameters.

Consensus rules are not product features.

The development team does not own the protocol.

[Consensus Invariants](../protocol/RIGOR_CONSENSUS_INVARIANTS.md) describe the frozen properties of RIGOR Mainnet: monetary issuance, asset identity, transaction validity, Proof-of-Work identity, the ledger model, and genesis identity. The production implementation realizes those properties. This document does not restate them as a second specification.

A protocol change that alters monetary properties, asset rules, validation rules, or transaction validity is not an upgrade.

It is a new protocol.

RIGOR Mainnet activation represents the transition from development to a frozen monetary protocol.

After activation, implementations may continue to improve.

The protocol rules they implement remain fixed.

Maintenance changes the implementation.

Protocol changes create a different network.

Developers can edit the software repository. Publishing a different validation rule does not revise RIGOR Mainnet in place. A network that runs those different rules is a separate blockchain, with separate monetary properties. Participants can tell the two apart by verifying the rules, not by treating the repository as the protocol.

RIGOR does not seek continuous protocol evolution as a design goal. Stability and predictability are considered fundamental properties of a monetary system.

The same distinction applies to the absence of a mechanism inside consensus that retargets those properties. The invariants record that RIGOR Mainnet contains no governance layer, no voting mechanism, no version-bit monetary upgrade path, and no emergency consensus override. Adding one would be a new protocol, not a feature of this one.

---

## Protocol Before Implementation

RIGOR distinguishes between the protocol and its implementations.

```
Protocol
    ↓
Consensus Specification
    ↓
Reference Implementation
    ↓
Running Software
```

The protocol is the authority.

The code is the implementation of the protocol.

A software defect does not redefine the protocol.

The protocol defines:

- monetary rules;
- asset rules;
- transaction validity;
- Proof-of-Work identity;
- ledger state transitions;
- genesis identity.

The [Consensus Invariants](../protocol/RIGOR_CONSENSUS_INVARIANTS.md) describe these frozen properties. The reference implementation realizes them. Running software executes that implementation.

Software implementations execute these rules.

The existence of a code repository does not grant ownership over the protocol.

A code change may improve security, performance, usability, or reliability while preserving the protocol.

A code change that alters the frozen protocol does not upgrade RIGOR Mainnet.

It creates an implementation of a different protocol.

---

## Minimal Consensus Surface

RIGOR intentionally limits consensus complexity.

The [Consensus Invariants](../protocol/RIGOR_CONSENSUS_INVARIANTS.md) describe a single UTXO ledger, two native assets, fixed script templates, and deterministic validation. Relative to that boundary, the protocol does not include:

- account state;
- smart contract execution;
- an external asset registry;
- governance-controlled consensus;
- upgrade mechanisms affecting monetary rules.

Each of those omissions is a property of the frozen protocol. This section does not add a rule that forbids them by declaration. It records why the surface stays small. The Consensus Invariants describe that surface. The implementation realizes it.

Every additional consensus feature increases the number of possible failure modes. A new consensus state, a new execution engine, or a new way to retarget validity must be specified, implemented, and independently checked. Each one adds cases in which two implementations can disagree about which chain is valid.

Limiting that surface keeps the rules a participant must understand within a stable set. Wallet features, monitoring, and other software outside consensus can still grow. They do not expand the set of facts a full node must accept.

---

## Security Without Protocol Pollution

Security maintenance is reviewed under the [Security Patch Boundary](../security/RIGOR_SECURITY_PATCH_BOUNDARY.md). That document classifies proposed fixes. This section does not replace that classification and does not authorize an exception to it.

Security fixes preserve the protocol by correcting implementation defects.

They do not redefine the protocol.

A vulnerability in software is not evidence that the underlying monetary rules should change.

RIGOR follows Bitcoin Core security research.

Bitcoin security fixes are not automatically RIGOR protocol changes.

Bitcoin Core is a source of knowledge about vulnerabilities and engineering mitigations. It is not the protocol authority for RIGOR. A fix is not imported because another project shipped it. Each security improvement must pass three questions:

1. Does the vulnerable component exist in RIGOR?
2. Does the fix preserve RIGOR consensus invariants?
3. Does the fix modify what transactions or blocks are valid?

A fix passes only when the component exists, the consensus invariants stay as they are, and the set of valid transactions and valid blocks stays the same.

If the component does not exist, there is no corresponding RIGOR change to take from that fix.

If the fix does not preserve the invariants, or if it changes which transactions or blocks are valid, it is not a security patch.

A security patch that changes any of the following is not a security patch:

- ORU issuance;
- RES issuance;
- `OP_CONVERGE` meaning;
- asset classification;
- signature rules;
- Proof-of-Work identity;
- transaction templates.

It is a protocol change.

A vulnerability fix may close a crash, a race, a resource leak, or a corrupted-state path. Those defects are implementation defects. Closing them is security work when the answers to the three questions stay inside the boundary above.

---

## Monetary Neutrality

The project maintains infrastructure.

The project develops software.

The project does not operate a monetary policy.

The project does not promise market outcomes.

The market determines economic value.

Issuance on RIGOR Mainnet is a consensus rule, enforced by validation. It is not a setting the project adjusts in response to price, adoption, or treasury conditions. Project-controlled resources are governed by the [Treasury & Market Neutrality Policy](../../TREASURY_MARKET_NEUTRALITY_POLICY.md). This section does not amend that policy.

The policy separates protocol work and public infrastructure from market price formation. The project may maintain software and infrastructure. It does not make a price, a floor, or a return an objective, and it does not use treasury activity as an instrument for those outcomes.

Nothing in this document is a statement about what ORU or RES will be worth.

---

## Developers as Maintainers, Not Owners

The existence of developers does not create ownership over the protocol.

Developers maintain software implementations. They write, review, and publish code. Repository permission is permission to change that software. It is not a title to the ledger, and it is not a consensus key.

Participants maintain network consensus through independent verification. A full node checks issuance, supply, transactions, asset ownership, and chain history by running an implementation. That check lets the participant see whether the running software conforms to the protocol. A website, an explorer, or a developer statement can help someone look. It does not settle the protocol.

The purpose of open source is to allow independent review and independent operation. Anyone can read the implementation, recompute the published rules, and run a node without asking the project for permission. That is how a participant decides whether the software they run still implements the protocol they intend to use.

Developers can change the code. Maintaining software remains their work. This document does not give them authority to treat a consensus change as an upgrade of RIGOR Mainnet. If they publish software that alters monetary properties, asset rules, validation rules, or transaction validity, they have published an implementation of a different protocol. Operators who verify can accept that software or continue to run the rules they already check.

---

## Principle of Verification Over Trust

RIGOR is designed around verification rather than reliance on authority.

Participants do not need to trust developers, websites, explorers, or statements.

They can independently verify:

- monetary issuance;
- asset rules;
- transaction validity;
- Proof-of-Work identity;
- ledger state.

The role of developers is to publish and maintain software.

The role of participants is to verify the rules they choose to run.

Trust may assist adoption.

Verification defines the protocol.

---

## RIGOR Commitment

RIGOR commits to:

- preserve deterministic monetary rules;
- preserve transparent verification;
- separate security improvements from protocol changes;
- avoid unnecessary consensus complexity;
- maintain public documentation;
- allow independent participants to verify the network rules.

These are commitments about how the software is maintained. They are not a consensus mechanism, and they do not prevent a later contributor from publishing different code. A contribution that changes consensus validity is classified as a new protocol. Independent verification is how participants recognize that classification.

The long-term value of a monetary protocol comes from the confidence that its rules will remain understandable, predictable, and independently verifiable.

That sentence is about the credibility of the rules. It is not a promise of market price or of economic return.

---

## Relationship With Other Documents

This document is the public statement of long-term development philosophy. It is part of the public record for developers, miners, node operators, and users. It is not an internal operating procedure.

It does not override the documents below. The Consensus Invariants describe frozen protocol properties. The implementation realizes those properties. Security patches repair implementations. They do not modify frozen monetary rules. On security review, the Security Patch Boundary decides whether a fix may be adopted. On project economic conduct, the Treasury Neutrality Policy decides.

| Document | Purpose |
|---|---|
| [Consensus Invariants](../protocol/RIGOR_CONSENSUS_INVARIANTS.md) | Describes frozen consensus properties |
| [Security Patch Boundary](../security/RIGOR_SECURITY_PATCH_BOUNDARY.md) | Defines which security improvements may be adopted |
| [Treasury Neutrality Policy](../../TREASURY_MARKET_NEUTRALITY_POLICY.md) | Defines project economic conduct |
| RIGOR Principles | Defines long-term development philosophy |
