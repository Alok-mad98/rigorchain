# SSOT Hierarchy

**Status:** Governance. Does not change RFC text, C++, Coq, or claim flags.

```
DOCUMENTATION_STATUS=FROZEN
refinement_proved=false
LEVEL_5_CLAIMED=false
CPP_VERIFIED=false
```

Protocol specification is normative SSOT.
Implementation follows protocol SSOT.
Current implementation may temporarily lead migration, but does not
define protocol.

## Level 1

**Protocol Authority**

- RFC-020 — [`../protocol/RFC-020.md`](../protocol/RFC-020.md)
- RFC-018 — [`../protocol/RFC-018.md`](../protocol/RFC-018.md)
- Consensus specification — [`../specification/SPEC-000-CONSENSUS-SPECIFICATION.md`](../specification/SPEC-000-CONSENSUS-SPECIFICATION.md)

## Level 2

**Implementation**

- C++ node (`core/src/`) — realization, not authority

## Level 3

**Verification Evidence**

- Tests
- Golden vectors
- CI

## Level 4

**Presentation**

- RPC
- Wallet
- Explorer

## Conflict rules

| Conflict | Winner |
|----------|--------|
| Implementation vs protocol | **Protocol wins.** |
| Tests vs implementation | Investigate the implementation (tests are evidence). |
| Documentation vs protocol | **Update the document.** |

## Forbidden

```
Code is SSOT
Implementation is authority
C++ defines protocol
```

## Allowed

```
Protocol specification is normative SSOT.
Implementation follows protocol SSOT.
Current implementation may temporarily lead migration, but does not define protocol.
```

See [`SSOT_POLICY.md`](./SSOT_POLICY.md).
