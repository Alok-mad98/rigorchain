# Consensus

**Status:** Active index. Not a new consensus specification.  
**Issuance SSOT:** [`RFC-020.md`](./RFC-020.md)  
**Implementation:** `core/src/` (realization)

```
equivalence_claimed = false
documentation_consensus_authority = false
refinement_proved = false
```

Protocol specifications are normative SSOT.
Implementations realize the frozen protocol.
If this page disagrees with RFC-020 about the issuance rule, RFC-020 is correct.
If it disagrees with RFC-018 about the fee-burn rule, RFC-018 is correct.

This file does not restate economic parameters. It only points to the
documents that describe the current protocol.

## Live protocol

| Document | Role |
|----------|------|
| [`RFC-020.md`](./RFC-020.md) | Continuous RES destroy cost; flat 128 ORU for `h ≥ 1` |
| [`RFC-018.md`](./RFC-018.md) | RES-transfer ORU fee burn (still current) |
| [`MAINNET_PROTOCOL_FREEZE_DECLARATION.md`](./MAINNET_PROTOCOL_FREEZE_DECLARATION.md) | What is frozen |
| [`../specification/SPEC-000-CONSENSUS-SPECIFICATION.md`](../specification/SPEC-000-CONSENSUS-SPECIFICATION.md) | What production consensus does |
| [`../specification/SPEC-003-CONSENSUS-RULES.md`](../specification/SPEC-003-CONSENSUS-RULES.md) | Validation predicates |
| [`../specification/SPEC-008-PROOF-OF-WORK.md`](../specification/SPEC-008-PROOF-OF-WORK.md) | OpuHash = SHA3-256(`RGRPOW01` \|\| header_80) |
| [`../specification/RIGOR_OPCODE_SPECIFICATION.md`](../specification/RIGOR_OPCODE_SPECIFICATION.md) | Live script templates |

## Implementation

| Document | Role |
|----------|------|
| [`../developer/Architecture.md`](../developer/Architecture.md) | Classification / UTXO constraints |
| [`../architecture/RIGOR_EXECUTION_MODEL.md`](../architecture/RIGOR_EXECUTION_MODEL.md) | ConnectBlock / runtime boundary |
| [`../developer/CODE_SSOT_PROTOCOL_FACTS.md`](../developer/CODE_SSOT_PROTOCOL_FACTS.md) | Facts extracted from `core/src/` |

Historical RFCs (RFC-019 and earlier) live under [`../../archive/rfc/`](../../archive/rfc/).  
They are not protocol authority.
