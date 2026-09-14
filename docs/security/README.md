# Security documentation

**Document status:** Production documentation

This folder records the security model of the production protocol: consensus
admission, cryptography, persistence, mining, networking, and verification
honesty. Protocol specifications are normative SSOT.
Implementations realize the frozen protocol.

```
equivalence_claimed = false
refinement_proved = false
```

Vulnerability reporting: [`../../SECURITY.md`](../../SECURITY.md). Do not
expose JSON-RPC to the public Internet.

Reproduce independently: [HOW_TO_VERIFY_RIGOR.md](./HOW_TO_VERIFY_RIGOR.md).

Historical engineering copies: [`../../archive/p0-history/docs/security/`](../../archive/p0-history/docs/security/).

---

## Operator entry

| Document | Role |
|----------|------|
| [HOW_TO_VERIFY_RIGOR.md](./HOW_TO_VERIFY_RIGOR.md) | Reproduce Coq, golden vectors, and the differential gate |
| [PRODUCTION_SECURITY_MODEL.md](./PRODUCTION_SECURITY_MODEL.md) | Production security model |
| [PRE-MAINNET-CHECKLIST.md](./PRE-MAINNET-CHECKLIST.md) | Pre-mainnet security gates |
| [CONSENSUS_CODE_UNCHANGED.md](./CONSENSUS_CODE_UNCHANGED.md) | Consensus-unchanged attestation for documentation work |

---

## Consensus and monetary security

| Document | Result |
|----------|--------|
| [RIGOR_FULL_CONSENSUS_SECURITY_AUDIT_P0_010.md](./RIGOR_FULL_CONSENSUS_SECURITY_AUDIT_P0_010.md) | Consensus security audit |
| [RIGOR_CONNECTBLOCK_CONSENSUS_CONVERGENCE_AUDIT_P0_086.md](./RIGOR_CONNECTBLOCK_CONSENSUS_CONVERGENCE_AUDIT_P0_086.md) | ConnectBlock single-path / Convergence |
| [RIGOR_GENESIS_ORU_ZERO_ISSUANCE_AUDIT_P0_037.md](./RIGOR_GENESIS_ORU_ZERO_ISSUANCE_AUDIT_P0_037.md) | Genesis ORU issuance is zero |
| [RIGOR_RES_MONETARY_POLICY_SECURITY_AUDIT.md](./RIGOR_RES_MONETARY_POLICY_SECURITY_AUDIT.md) | RES supply, mint, maturity, reorg |
| [RES-ORU-MATURITY.md](./RES-ORU-MATURITY.md) | Immature RES mint / ORU coinbase |
| [RIGOR_CONVERGENCE_DRAW_SECURITY_AUDIT_P0_079.md](./RIGOR_CONVERGENCE_DRAW_SECURITY_AUDIT_P0_079.md) | Deterministic Convergence winner |
| [RIGOR_SCRIPT_OPCODE_SECURITY_AUDIT_P0_078.md](./RIGOR_SCRIPT_OPCODE_SECURITY_AUDIT_P0_078.md) | Closed predicate surface; no Script VM |
| [RIGOR_SCRIPT_SECURITY_BOUNDARY.md](./RIGOR_SCRIPT_SECURITY_BOUNDARY.md) | Why scripts are restricted |
| [RIGOR_SCRIPT_TYPE_REGISTRY.md](./RIGOR_SCRIPT_TYPE_REGISTRY.md) | Supported / rejected script families |

Historical verification references: P0-010, P0-037, P0-078, P0-079, P0-086.

---

## Cryptography and proof-of-work

| Document | Result |
|----------|--------|
| [RIGOR_CRYPTOGRAPHIC_BOUNDARY_SECURITY_AUDIT_P0_080.md](./RIGOR_CRYPTOGRAPHIC_BOUNDARY_SECURITY_AUDIT_P0_080.md) | SHA256d / SHA3-PoW / Ed25519 / SignatureHash |
| [RIGOR_POW_BOUNDARY_SECURITY_AUDIT_P0_081.md](./RIGOR_POW_BOUNDARY_SECURITY_AUDIT_P0_081.md) | `header_80` / SHA3 digest / compact target / ASERT |
| [RIGOR_SOLO_MINER_POW_ALIGNMENT_P0_098.md](./RIGOR_SOLO_MINER_POW_ALIGNMENT_P0_098.md) | `MINER_POW_ALIGNMENT=PASS` |
| [RIGOR_PRODUCTION_MINING_PATH_AUDIT_P0_099.md](./RIGOR_PRODUCTION_MINING_PATH_AUDIT_P0_099.md) | `PRODUCTION_MINING_PATH_AUDIT=PASS` |
| [RIGOR_MINING_TEMPLATE_SOLO_MINER_SECURITY_AUDIT_P0_014.md](./RIGOR_MINING_TEMPLATE_SOLO_MINER_SECURITY_AUDIT_P0_014.md) | GBT / SOLO template boundary |

Historical verification references: P0-014, P0-080, P0-081, P0-098, P0-099.

---

## Persistence and wallet trust boundary

| Document | Result |
|----------|--------|
| [RIGOR_DATABASE_STATE_PERSISTENCE_SECURITY_AUDIT_P0_086.md](./RIGOR_DATABASE_STATE_PERSISTENCE_SECURITY_AUDIT_P0_086.md) | CoinsDB fail-closed / CRC-32C |
| [RIGOR_DATABASE_ARCHITECTURE_FULL_REAUDIT_P0_087.md](./RIGOR_DATABASE_ARCHITECTURE_FULL_REAUDIT_P0_087.md) | `DATABASE_AUDIT_STATUS=CLOSED` |
| [RIGOR_PRODUCTION_REINDEX_RECONSTRUCTION_AUDIT_P0_088.md](./RIGOR_PRODUCTION_REINDEX_RECONSTRUCTION_AUDIT_P0_088.md) | `PRODUCTION_FULL_REINDEX=PASS` |
| [RIGOR_WALLET_SQLITE_AUDIT_P0_089.md](./RIGOR_WALLET_SQLITE_AUDIT_P0_089.md) | `WALLET_SQLITE_AUDIT_STATUS=CLOSED` |
| [RIGOR_DATABASE_SECURITY_CLOSURE_P0_091.md](./RIGOR_DATABASE_SECURITY_CLOSURE_P0_091.md) | `DATABASE_SECURITY_AUDIT_STATUS=CLOSED` |
| [RIGOR_WALLET_SECURITY_BASELINE_P0_077.md](./RIGOR_WALLET_SECURITY_BASELINE_P0_077.md) | Wallet is not consensus authority |

Historical verification references: P0-077, P0-086, P0-087, P0-088, P0-089, P0-091.

---

## Network and repository governance

| Document | Result |
|----------|--------|
| [RIGOR_P2P_SECURITY_BOUNDARY_P0_076.md](./RIGOR_P2P_SECURITY_BOUNDARY_P0_076.md) | P2P wire firewall |
| [RIGOR_NETWORK_SECURITY_BOUNDARY_AUDIT_P0_025.md](./RIGOR_NETWORK_SECURITY_BOUNDARY_AUDIT_P0_025.md) | Witness-mandatory P2P |
| [RIGOR_GITHUB_PRODUCTION_GOVERNANCE_GATE_P0_101.md](./RIGOR_GITHUB_PRODUCTION_GOVERNANCE_GATE_P0_101.md) | `GITHUB_PRODUCTION_GOVERNANCE_GATE=PASS` |
| [RIGOR_RELEASE_GUARDIAN_FULL_PIPELINE_AUDIT_P0_092.md](./RIGOR_RELEASE_GUARDIAN_FULL_PIPELINE_AUDIT_P0_092.md) | `RELEASE_GUARDIAN_FULL_PIPELINE=PASS` |
| [RIGOR_MAINNET_ALIGNMENT_FIX_P0_096.md](./RIGOR_MAINNET_ALIGNMENT_FIX_P0_096.md) | `MAINNET_ALIGNMENT_AUDIT=PASS` |

Historical verification references: P0-025, P0-076, P0-092, P0-096, P0-101.

---

## Formal verification honesty

Formal results are properties of the protocol **model**. They are not a C++
refinement proof.

| Document | Role |
|----------|------|
| [FORMAL_VERIFICATION_ASSUMPTION_BOUNDARY.md](./FORMAL_VERIFICATION_ASSUMPTION_BOUNDARY.md) | What is proved vs not claimed |
| [FORMAL_VERIFICATION_PUBLIC_ARCHITECTURE_REPORT.md](./FORMAL_VERIFICATION_PUBLIC_ARCHITECTURE_REPORT.md) | Public `verification/` architecture |
| [RIGOR_FORMAL_CLAIM_TRACEABILITY.md](./RIGOR_FORMAL_CLAIM_TRACEABILITY.md) | Claim → spec → Coq → vector |
| [RIGOR_CLAIM_WORDING_AUDIT.md](./RIGOR_CLAIM_WORDING_AUDIT.md) | Allowed formal-verification wording |
| [RIGOR_FORMAL_VERIFICATION_FREEZE_CERTIFICATE_P0_023.md](./RIGOR_FORMAL_VERIFICATION_FREEZE_CERTIFICATE_P0_023.md) | Freeze certificate |

Historical verification reference: P0-023.

---

## Additional audits

The remaining files in this folder are dated verification records. Each may
keep an audit date, reproduce command, manifest pointer, and PASS/FAIL
result. Internal task identifiers live under `archive/`.
