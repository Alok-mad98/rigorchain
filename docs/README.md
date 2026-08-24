# RIGOR documentation

Public entry is **three core whitepapers**. Historical development records live
under [`archive/`](./archive/) and are not required to understand the current design.

```
RIGOR is a dual-asset UTXO protocol built on script-derived asset
classification, stateless monetary architecture, and formally verified
consensus invariants.
```

```
equivalence_claimed = false
```

## Public cores

| Document | Question |
|----------|----------|
| [whitepaper/RIGOR_PROTOCOL_IDENTITY.md](./whitepaper/RIGOR_PROTOCOL_IDENTITY.md) | **What is RIGOR?** |
| [whitepaper/RIGOR_MATHEMATICAL_CONSENSUS_DESIGN.md](./whitepaper/RIGOR_MATHEMATICAL_CONSENSUS_DESIGN.md) | How does the mathematics run? |
| [whitepaper/RIGOR_FORMAL_VERIFICATION_DESIGN.md](./whitepaper/RIGOR_FORMAL_VERIFICATION_DESIGN.md) | Why is it trustworthy? |

## Start here (engineering / operator)

| Document | Purpose |
|----------|---------|
| [DOCUMENT_INVENTORY.md](./DOCUMENT_INVENTORY.md) | Keep / archive / delete classification |
| [archive/rfc/](./archive/rfc/) | Historical RFC corpus (not public identity) |
| [whitepaper/RIGOR_PROTOCOL_WHITEPAPER.md](./whitepaper/RIGOR_PROTOCOL_WHITEPAPER.md) | Official protocol narrative (not a fourth core) |
| [whitepaper/chapters/RIGOR_MINIMAL_SCRIPT_ARCHITECTURE.md](./whitepaper/chapters/RIGOR_MINIMAL_SCRIPT_ARCHITECTURE.md) | Minimal script architecture chapter |
| [WHITEPAPER.md](./WHITEPAPER.md) | Prior four-layer public narrative (kept) |
| [architecture/ARCHITECTURE.md](./architecture/ARCHITECTURE.md) | Implementation constraints |
| [architecture/RIGOR_EXECUTION_MODEL.md](./architecture/RIGOR_EXECUTION_MODEL.md) | Consensus / runtime boundary |
| [MAINNET_PROTOCOL_FREEZE_DECLARATION.md](./MAINNET_PROTOCOL_FREEZE_DECLARATION.md) | Consensus freeze status |
| [verification/FORMAL-VERIFICATION-STATUS.md](./verification/FORMAL-VERIFICATION-STATUS.md) | **Current freeze** — correspondence boundary, operator entry |
| [verification/FORMAL-VERIFICATION-FREEZE.md](./verification/FORMAL-VERIFICATION-FREEZE.md) | Freeze checklist (maintenance mode) |
| [verification/FORMAL-VERIFICATION-VERDICT.md](./verification/FORMAL-VERIFICATION-VERDICT.md) | What is (and is not) proved in the Coq model |
| [formal/RIGOR_FORMAL_VERIFICATION_CURRENT_STATE.md](./formal/RIGOR_FORMAL_VERIFICATION_CURRENT_STATE.md) | Classification freeze snapshot (2026-08-20; guaranteed / not-guaranteed) |
| [verification/GOLDEN-VECTOR-DISCREPANCIES.md](./verification/GOLDEN-VECTOR-DISCREPANCIES.md) | Intentional golden-vector splits |
| [formal/RIGOR_FORMAL_VERIFICATION_ARCHITECTURE_ASSESSMENT_REPORT.md](./formal/RIGOR_FORMAL_VERIFICATION_ARCHITECTURE_ASSESSMENT_REPORT.md) | Architecture assessment (model vs correspondence vs RC0) |
| [formal/RIGOR_L2_L3_REFINEMENT_FEASIBILITY.md](./formal/RIGOR_L2_L3_REFINEMENT_FEASIBILITY.md) | L2↔L3 refinement feasibility (not an implementation plan; honesty flags stay false) |

## Public mathematics and engineering specs

| Path | Contents |
|------|----------|
| [whitepaper/RIGOR_MATHEMATICAL_CONSENSUS_DESIGN.md](./whitepaper/RIGOR_MATHEMATICAL_CONSENSUS_DESIGN.md) | How the mathematics runs (public) |
| [archive/rfc/](./archive/rfc/) | Historical RFC corpus — not the public protocol specification |
| [specification/](./specification/) | State model, consensus rules, invariants, trust anchors |

## Security

| Path | Contents |
|------|----------|
| [security/RIGOR_RES_MONETARY_POLICY_SECURITY_AUDIT.md](./security/RIGOR_RES_MONETARY_POLICY_SECURITY_AUDIT.md) | P0 RES monetary-policy security audit |
| [security/PRODUCTION_SECURITY_MODEL.md](./security/PRODUCTION_SECURITY_MODEL.md) | Production security model |
| [security/minimal-script-architecture.md](./security/minimal-script-architecture.md) | Minimal script architecture (UTXO, dual native assets, formal-verification prerequisite) |
| [security/RIGOR_MINIMAL_SCRIPT_CONSISTENCY_AUDIT.md](./security/RIGOR_MINIMAL_SCRIPT_CONSISTENCY_AUDIT.md) | Minimal-script consistency audit vs implementation |
| [security/RIGOR_SCRIPT_TYPE_REGISTRY.md](./security/RIGOR_SCRIPT_TYPE_REGISTRY.md) | Official script type registry |
| [security/RIGOR_SCRIPT_SECURITY_BOUNDARY.md](./security/RIGOR_SCRIPT_SECURITY_BOUNDARY.md) | Script security boundary |
| [security/SECURITY-001-A-FINAL-REPORT.md](./security/SECURITY-001-A-FINAL-REPORT.md) | Pre-mainnet remediation summary |
| [security/SECURITY_001_A7_REPORT.md](./security/SECURITY_001_A7_REPORT.md) | A-7 closure |
| [security/SECURITY_001_A8_REPORT.md](./security/SECURITY_001_A8_REPORT.md) | A-8 closure |
| [security/PROTOCOL_TERMINOLOGY_ALIGNMENT.md](./security/PROTOCOL_TERMINOLOGY_ALIGNMENT.md) | Current terminology |

Task-level drafts and intermediate audits: [`archive/security/`](./archive/security/).

## Domain guides

| Path | Contents |
|------|----------|
| [mining/](./mining/) | Pool / GBT / assembler specs |
| [explorer/](./explorer/) | Explorer architecture and display rules (independent product) |
| [wallet/](./wallet/) | Wallet RPC / UI minimum specs |
| [rigor-qt/](./rigor-qt/) | Desktop GUI notes |
| [infrastructure/](./infrastructure/) | Deployment topology |
| [constitution/](./constitution/) | Execution constitution |
| [SCRIPT_STRUCTURE.md](./SCRIPT_STRUCTURE.md) | Script / witness structure reference |
| [security/minimal-script-architecture.md](./security/minimal-script-architecture.md) | Why the script surface is small (security explanation) |

## Archives

| Path | Contents |
|------|----------|
| [archive/rfc/](./archive/rfc/) | Historical RFC corpus (not public identity) |
| [archive/migration/](./archive/migration/) | One-time explorer development-observation notes |
| [archive/proof/](./archive/proof/) | PROOF-000 … PROOF-034 development series |
| [archive/security/](./archive/security/) | Historical SECURITY-001 task records |
| [archive/architecture/](./archive/architecture/) | Large knowledge-base snapshots |
| [archive/mining/](./archive/mining/) | Pilot / task mining docs |

## Developer build docs

Node build and RPC hardening: [`../core/doc/`](../core/doc/).
