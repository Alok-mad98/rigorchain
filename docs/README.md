# RIGOR documentation

Public entry points for the protocol. Historical development records live under
[`archive/`](./archive/) and are not required to understand the current design.

## Start here

| Document | Purpose |
|----------|---------|
| [WHITEPAPER.md](./WHITEPAPER.md) | Protocol narrative |
| [architecture/ARCHITECTURE.md](./architecture/ARCHITECTURE.md) | Implementation constraints |
| [MAINNET_PROTOCOL_FREEZE_DECLARATION.md](./MAINNET_PROTOCOL_FREEZE_DECLARATION.md) | Consensus freeze status |
| [verification/FORMAL-VERIFICATION-VERDICT.md](./verification/FORMAL-VERIFICATION-VERDICT.md) | What is (and is not) proved |

## Normative rules

| Path | Contents |
|------|----------|
| [rfc/](./rfc/) | RFC-000 constitution; RFC-008 / RFC-009 mathematics & serialization; RFC-014 draw target |
| [specification/](./specification/) | State model, consensus rules, invariants, trust anchors |

## Security

| Path | Contents |
|------|----------|
| [security/PRODUCTION_SECURITY_MODEL.md](./security/PRODUCTION_SECURITY_MODEL.md) | Production security model |
| [security/SECURITY-001-A-FINAL-REPORT.md](./security/SECURITY-001-A-FINAL-REPORT.md) | Pre-mainnet remediation summary |
| [security/SECURITY_001_A7_REPORT.md](./security/SECURITY_001_A7_REPORT.md) | A-7 closure |
| [security/SECURITY_001_A8_REPORT.md](./security/SECURITY_001_A8_REPORT.md) | A-8 closure |
| [security/PROTOCOL_TERMINOLOGY_ALIGNMENT.md](./security/PROTOCOL_TERMINOLOGY_ALIGNMENT.md) | Current terminology |

Task-level drafts and intermediate audits: [`archive/security/`](./archive/security/).

## Domain guides

| Path | Contents |
|------|----------|
| [mining/](./mining/) | Pool / GBT / assembler specs |
| [explorer/](./explorer/) | Explorer architecture and display rules |
| [wallet/](./wallet/) | Wallet RPC / UI minimum specs |
| [rigor-qt/](./rigor-qt/) | Desktop GUI notes |
| [infrastructure/](./infrastructure/) | Deployment topology |
| [constitution/](./constitution/) | Execution constitution |
| [SCRIPT_STRUCTURE.md](./SCRIPT_STRUCTURE.md) | Script / witness structure reference |

## Archives

| Path | Contents |
|------|----------|
| [archive/proof/](./archive/proof/) | PROOF-000 … PROOF-034 development series |
| [archive/security/](./archive/security/) | Historical SECURITY-001 task records |
| [archive/architecture/](./archive/architecture/) | Large knowledge-base snapshots |
| [archive/mining/](./archive/mining/) | Pilot / task mining docs |

## Developer build docs

Node build and RPC hardening: [`../core/doc/`](../core/doc/).
