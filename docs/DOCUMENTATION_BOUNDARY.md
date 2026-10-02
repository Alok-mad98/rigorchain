# RIGOR Documentation Boundary

This document separates public protocol documentation from private engineering documentation.

It does not define consensus rules. It does not add a governance mechanism, a voting system, or an upgrade process.

```
consensus_authority = false
governance_layer = false
protocol_upgrade_mechanism = false
```

RIGOR documentation is separated into:

**Public protocol documentation**

- protocol identity
- consensus invariants
- security boundaries
- design principles

**Private engineering documentation**

- audits
- reviews
- release checks
- operational records

Public documents describe what RIGOR is.

Private documents record how maintainers evaluate and operate RIGOR.

A public synchronization copies named protocol documents. It does not copy the documentation tree as a whole.

The public order stays:

```
Protocol
    ↓
Consensus Invariants
    ↓
Implementation
    ↓
Maintenance
```

Public entry points:

| Topic | Document |
|---|---|
| Long-term principles | [RIGOR Principles](philosophy/RIGOR_PRINCIPLES.md) |
| Protocol, invariants, implementation, and maintenance | [Protocol Boundary](philosophy/RIGOR_PROTOCOL_BOUNDARY.md) |
| How later decisions stay consistent with those principles | [Philosophy Internal Consistency](philosophy/RIGOR_PHILOSOPHY_INTERNAL_CONSISTENCY.md) |
| Frozen properties | [Consensus Invariants](protocol/RIGOR_CONSENSUS_INVARIANTS.md) |
| Security maintenance boundary | [Security Patch Boundary](security/RIGOR_SECURITY_PATCH_BOUNDARY.md) |

Security fixes may improve the implementation.

Security fixes must not redefine the protocol.
