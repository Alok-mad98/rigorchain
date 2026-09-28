# SSOT policy

**Status:** Governance. Does not change RFC text, C++, Coq, or claim flags.  
**Documentation freeze:** `DOCUMENTATION_STATUS=FROZEN` remains.

```
equivalence_claimed = false
refinement_proved = false
LEVEL_5_CLAIMED = false
CPP_VERIFIED = false
ARCHIVE_NOT_NORMATIVE = true
```

## Normative SSOT (protocol)

The protocol authority is the live RFC corpus and the current consensus
specification that describes those RFCs:

| Layer | Authority |
|-------|-----------|
| Monetary issuance | [`../protocol/RFC-020.md`](../protocol/RFC-020.md) |
| RES-transfer ORU fee burn | [`../protocol/RFC-018.md`](../protocol/RFC-018.md) |
| Consensus description | [`../specification/SPEC-000-CONSENSUS-SPECIFICATION.md`](../specification/SPEC-000-CONSENSUS-SPECIFICATION.md) and the SPEC layer it indexes |

If two RFCs disagree, the later live RFC wins on its own scope
(RFC-020 issuance; RFC-018 fee burn). RFC-019 and earlier texts are not
protocol.

## Implementation (not protocol)

C++ under `core/src/` is a **realization** of the protocol SSOT.

Allowed sentences:

```
Protocol specification is normative SSOT.
Implementation follows protocol SSOT.
Current implementation may temporarily lead migration, but does not define protocol.
```

The third sentence is an `IMPLEMENTATION-AHEAD` label. It means the
node already contains a surface the RFC does not yet name, or still
carries a migration shim. It does **not** make the code protocol
authority.

Hierarchy: [`SSOT_HIERARCHY.md`](./SSOT_HIERARCHY.md).

Forbidden sentence:

```
Code is SSOT
```

Also forbidden: treating `GetBlockSubsidy`, `CheckResMintArithmetic`,
or any lock header as the source of the rule. Those files implement
the rule. The rule is in RFC-020 / RFC-018.

## Evidence (not protocol)

| Kind | Role |
|------|------|
| Unit / functional tests | Evidence that this realization matches the RFC on the exercised cases |
| Golden vectors | Evidence of numeric / accept-reject pins |
| `tools/res_continuous_price_schedule.py` | Numeric witness for RFC-020 tables (RFC-020 cites it; it does not replace the RFC) |

A green test run is not a refinement proof.

## Ranking

```
RFC-020 / RFC-018     →  what the protocol is
C++ realization       →  what this node does
tests / vectors       →  evidence
archive / comments    →  history
```

If C++ disagrees with the live RFC, that is an implementation defect or
an `IMPLEMENTATION-AHEAD` gap — not a silent protocol change.

If a comment, RPC field, or explorer label still says RFC-019 / `RATE(k)`
/ halving, that is residue. It is not authority.

## Formal flags (unchanged)

```
refinement_proved = false
LEVEL_5_CLAIMED = false
CPP_VERIFIED = false
```

See [`VerificationStatus.md`](./VerificationStatus.md).
