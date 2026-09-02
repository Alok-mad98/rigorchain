# Security documentation

| Document | Role |
|----------|------|
| [RIGOR_RELEASE_GUARDIAN_FULL_PIPELINE_AUDIT_P0_092.md](./RIGOR_RELEASE_GUARDIAN_FULL_PIPELINE_AUDIT_P0_092.md) | P0-092 full Genesis Release Guardian pipeline after `Stage DATABASE_SECURITY`; `RELEASE_GUARDIAN_FULL_PIPELINE=PASS` |
| [RIGOR_DATABASE_SECURITY_CLOSURE_P0_091.md](./RIGOR_DATABASE_SECURITY_CLOSURE_P0_091.md) | P0-091 database-security closure; Guardian `Stage DATABASE_SECURITY`; `DATABASE_SECURITY_AUDIT_STATUS=CLOSED` |
| [RIGOR_WALLET_SQLITE_AUDIT_P0_089.md](./RIGOR_WALLET_SQLITE_AUDIT_P0_089.md) | P0-089 wallet SQLite crash-recovery / trust-boundary audit (`WALLET_SQLITE_AUDIT_STATUS=CLOSED`) |
| [RIGOR_PRODUCTION_REINDEX_RECONSTRUCTION_AUDIT_P0_088.md](./RIGOR_PRODUCTION_REINDEX_RECONSTRUCTION_AUDIT_P0_088.md) | P0-088 production `-reindex` field-level reconstruction (`PRODUCTION_FULL_REINDEX=PASS`) |
| [RIGOR_DATABASE_ARCHITECTURE_FULL_REAUDIT_P0_087.md](./RIGOR_DATABASE_ARCHITECTURE_FULL_REAUDIT_P0_087.md) | P0-087 CoinsDB / dual-asset / SQLite architecture re-audit (`DATABASE_AUDIT_STATUS=CLOSED`) |
| [RIGOR_DUAL_ASSET_ORU_RES_DATABASE_AUDIT_P0_086B.md](./RIGOR_DUAL_ASSET_ORU_RES_DATABASE_AUDIT_P0_086B.md) | P0-086B dual-asset ORU / RES CoinsDB persistence audit (BLOCKED) |
| [RIGOR_DATABASE_STATE_PERSISTENCE_SECURITY_AUDIT_P0_086.md](./RIGOR_DATABASE_STATE_PERSISTENCE_SECURITY_AUDIT_P0_086.md) | P0-086 CoinsDB fail-closed / CoinDiskFormatter CRC-32C; `DATABASE_RELEASE_GATE=PARTIAL` |
| [RIGOR_CONNECTBLOCK_CONSENSUS_CONVERGENCE_AUDIT_P0_086.md](./RIGOR_CONNECTBLOCK_CONSENSUS_CONVERGENCE_AUDIT_P0_086.md) | P0-086 ConnectBlock single-path / consensus-convergence audit (OPEN) |
| [RIGOR_POW_MINING_DIFFICULTY_EXPLORER_SECURITY_REAUDIT_P0_085.md](./RIGOR_POW_MINING_DIFFICULTY_EXPLORER_SECURITY_REAUDIT_P0_085.md) | P0-085 PoW / mining / GBT / ConnectBlock / explorer re-audit (`POW_SECURITY_REAUDIT=BLOCKED`) |
| [RIGOR_POW_BOUNDARY_SECURITY_AUDIT_P0_081.md](./RIGOR_POW_BOUNDARY_SECURITY_AUDIT_P0_081.md) | P0-081 PoW boundary audit (header_80 / SHA3 digest / compact target / ASERT) |
| [RIGOR_CRYPTOGRAPHIC_BOUNDARY_SECURITY_AUDIT_P0_080.md](./RIGOR_CRYPTOGRAPHIC_BOUNDARY_SECURITY_AUDIT_P0_080.md) | P0-080 cryptographic boundary audit (unique SHA256d / SHA3-PoW / Ed25519 / SignatureHash) |
| [RIGOR_CONVERGENCE_DRAW_SECURITY_AUDIT_P0_079.md](./RIGOR_CONVERGENCE_DRAW_SECURITY_AUDIT_P0_079.md) | P0-079 Convergence Draw security audit (single resolver; deterministic winner) |
| [RIGOR_SCRIPT_OPCODE_SECURITY_AUDIT_P0_078.md](./RIGOR_SCRIPT_OPCODE_SECURITY_AUDIT_P0_078.md) | P0-078 script/opcode boundary audit (single predicate; no Script VM) |
| [RIGOR_WALLET_SECURITY_BASELINE_P0_077.md](./RIGOR_WALLET_SECURITY_BASELINE_P0_077.md) | P0-077 wallet security baseline (request firewall; wallet is not consensus authority) |
| [RIGOR_P2P_SECURITY_BOUNDARY_P0_076.md](./RIGOR_P2P_SECURITY_BOUNDARY_P0_076.md) | P0-076 P2P security boundary (wire firewall; Consensus Kernel unchanged) |
| [RIGOR_CONSENSUS_KERNEL_ADVERSARIAL_SECURITY_P0_063.md](./RIGOR_CONSENSUS_KERNEL_ADVERSARIAL_SECURITY_P0_063.md) | P0-063 adversarial security baseline on the CLOSED Consensus / Transaction Kernel boundary |
| [RIGOR_CONVERGE_SINGLE_PATH_HARDENING_P0_011.md](./RIGOR_CONVERGE_SINGLE_PATH_HARDENING_P0_011.md) | P0 post-audit hardening: H-1 paid-winner index, H-2 docs, H-3 SOLO mining boundary |
| [RIGOR_CONVERGE_SINGLE_PATH_IMPLEMENTATION_AUDIT.md](./RIGOR_CONVERGE_SINGLE_PATH_IMPLEMENTATION_AUDIT.md) | P0 single-path CONVERGE implementation audit (consensus / script / mint / template / pool / Coq); conditional pass on H-1 |
| [RIGOR_DOCUMENTATION_SHIPPING_POLISH_REPORT.md](./RIGOR_DOCUMENTATION_SHIPPING_POLISH_REPORT.md) | Docs polish: shipping `E = 246` / `C = 1024`; labeled RFC-014 `65536` history |
| [RIGOR_PROTOCOL_CONSISTENCY_FINAL_AUDIT.md](./RIGOR_PROTOCOL_CONSISTENCY_FINAL_AUDIT.md) | P0 protocol consistency final audit (code / spec / Coq / Python / vectors / whitepaper / website) |
| [RIGOR_WHITEPAPER_TECHNICAL_TRACEABILITY_MATRIX.md](./RIGOR_WHITEPAPER_TECHNICAL_TRACEABILITY_MATRIX.md) | Whitepaper claim → implementation → formal artifact → status |
| [RIGOR_WEBSITE_CONSISTENCY_FINAL_REPORT.md](./RIGOR_WEBSITE_CONSISTENCY_FINAL_REPORT.md) | Documentation pages: terminology, links, claims, navigation |
| [RIGOR_CLAIM_WORDING_AUDIT.md](./RIGOR_CLAIM_WORDING_AUDIT.md) | `equivalence_claimed`, forbidden slogans, allowed FV sentence |
| [FORMAL_VERIFICATION_PUBLIC_ARCHITECTURE_REPORT.md](./FORMAL_VERIFICATION_PUBLIC_ARCHITECTURE_REPORT.md) | Public `verification/` release architecture (no consensus changes) |
| [HOW_TO_VERIFY_RIGOR.md](./HOW_TO_VERIFY_RIGOR.md) | External reproduce guide: Coq + golden vectors + differential gate |
| [RIGOR_FORMAL_CLAIM_TRACEABILITY.md](./RIGOR_FORMAL_CLAIM_TRACEABILITY.md) | Public claim → spec → Coq → vector status table |
| [FORMAL_VERIFICATION_COVERAGE_AUDIT.md](./FORMAL_VERIFICATION_COVERAGE_AUDIT.md) | P0 full FV coverage map (verified / partial / outside boundary) |
| [FORMAL_VERIFICATION_ASSUMPTION_BOUNDARY.md](./FORMAL_VERIFICATION_ASSUMPTION_BOUNDARY.md) | What RIGOR proves vs does not claim; `equivalence_claimed = false` |
| [FORMAL_CLAIM_TRACEABILITY_MATRIX.md](./FORMAL_CLAIM_TRACEABILITY_MATRIX.md) | Whitepaper formal claims → proof artifacts |
| [FORMAL_GOLDEN_VECTOR_COVERAGE_REPORT.md](./FORMAL_GOLDEN_VECTOR_COVERAGE_REPORT.md) | 422-vector family coverage + missing-boundary assessment |
| [FORMAL_VERIFICATION_WEBSITE_CONSISTENCY_REPORT.md](./FORMAL_VERIFICATION_WEBSITE_CONSISTENCY_REPORT.md) | `/documentation/formal-verification` wording + count sync |
| [RIGOR_RES_MONETARY_POLICY_SECURITY_AUDIT.md](./RIGOR_RES_MONETARY_POLICY_SECURITY_AUDIT.md) | P0 RES monetary-policy security audit (supply, mint, maturity, reorg, formal coverage) |
| [RES_MONETARY_POLICY_FINAL_AUDIT_REPORT.md](./RES_MONETARY_POLICY_FINAL_AUDIT_REPORT.md) | Final verification P0: C++/Coq/Python/golden/docs alignment (RFC-017) |
| [RES_MONETARY_FORMAL_VERIFICATION_GAP_ANALYSIS.md](./RES_MONETARY_FORMAL_VERIFICATION_GAP_ANALYSIS.md) | RFC-017 schedule FV checklist (DONE / PARTIAL / MISSING) |
| [RES_MONETARY_GOLDEN_VECTOR_UPDATE_REPORT.md](./RES_MONETARY_GOLDEN_VECTOR_UPDATE_REPORT.md) | Golden-vector sufficiency + parity-gate note |
| [RIGOR_RES_FORMAL_COVERAGE_GAP_EVAL.md](./RIGOR_RES_FORMAL_COVERAGE_GAP_EVAL.md) | L-1 follow-up: Coq P1 close-out (winner-must-mint + dual-pool **YES** as model); accepted mismatches §9 |
| [PRE-MAINNET-CHECKLIST.md](./PRE-MAINNET-CHECKLIST.md) | **Operator entry** — pre-mainnet security gates (solo; no pool) |
| [minimal-script-architecture.md](./minimal-script-architecture.md) | Minimal script architecture — UTXO foundation, native dual assets, attack-surface reduction |
| [RIGOR_MINIMAL_SCRIPT_CONSISTENCY_AUDIT.md](./RIGOR_MINIMAL_SCRIPT_CONSISTENCY_AUDIT.md) | Consistency audit: implementation vs whitepaper vs website |
| [RIGOR_SCRIPT_TYPE_REGISTRY.md](./RIGOR_SCRIPT_TYPE_REGISTRY.md) | Official supported / rejected script families |
| [RIGOR_SCRIPT_SECURITY_BOUNDARY.md](./RIGOR_SCRIPT_SECURITY_BOUNDARY.md) | Why scripts are restricted; inherited vs RIGOR limits |
| [RIGOR_MINIMAL_SCRIPT_FINAL_HARDENING_REPORT.md](./RIGOR_MINIMAL_SCRIPT_FINAL_HARDENING_REPORT.md) | Final hardening: naming cleanup, wallet gaps, remaining risks |
| [RIGOR_MINIMAL_SCRIPT_DEBT_CLEANUP_EVAL.md](./RIGOR_MINIMAL_SCRIPT_DEBT_CLEANUP_EVAL.md) | Post-hardening leftover eval: keep P2WSH, omit `cvg2()`, clean P2TR/ORB2 |
| [PRODUCTION_SECURITY_MODEL.md](./PRODUCTION_SECURITY_MODEL.md) | Production security model |
| [PRE-MAINNET-WINDOW-REORG-RES-ATTACKS.md](./PRE-MAINNET-WINDOW-REORG-RES-ATTACKS.md) | Window / reorg / RES monopoly attack suite |
| [PRE-MAINNET-SOLO-INVENTORY.md](./PRE-MAINNET-SOLO-INVENTORY.md) | Solo-mining release scope lock + hook inventory (no pool) |
| [PRE-MAINNET-SOLO-SECURITY.md](./PRE-MAINNET-SOLO-SECURITY.md) | Solo mempool/mint/GBT security suite + Go/No-Go |
| [RES-ORU-MATURITY-INVENTORY.md](./RES-ORU-MATURITY-INVENTORY.md) | ORU coinbase vs body RES mint spendability inventory |
| [RES-ORU-MATURITY.md](./RES-ORU-MATURITY.md) | Consensus maturity rule (immature RES mint / ORU coinbase) |
| [SECURITY_TEST_MATRIX.md](./SECURITY_TEST_MATRIX.md) | Test matrix fragments (window + solo + maturity) |
| [SECURITY-001-A-FINAL-REPORT.md](./SECURITY-001-A-FINAL-REPORT.md) | Pre-mainnet remediation summary |
| [SECURITY_001_A7_REPORT.md](./SECURITY_001_A7_REPORT.md) | A-7 |
| [SECURITY_001_A8_REPORT.md](./SECURITY_001_A8_REPORT.md) | A-8 |
| [PROTOCOL_TERMINOLOGY_ALIGNMENT.md](./PROTOCOL_TERMINOLOGY_ALIGNMENT.md) | Public terminology |
| [PROTOCOL_DOCUMENTATION_ALIGNMENT_REPORT.md](./PROTOCOL_DOCUMENTATION_ALIGNMENT_REPORT.md) | Documentation alignment |
| [PUBLIC_REPOSITORY_STRUCTURE_REPORT.md](./PUBLIC_REPOSITORY_STRUCTURE_REPORT.md) | Public repo structure |
| [CONSENSUS_CODE_UNCHANGED.md](./CONSENSUS_CODE_UNCHANGED.md) | Consensus unchanged attestation |
| [README_ARCHITECTURE_ALIGNMENT_REPORT.md](./README_ARCHITECTURE_ALIGNMENT_REPORT.md) | Prior README alignment audit |

Vulnerability reporting: [../../SECURITY.md](../../SECURITY.md).

Historical task drafts: [../archive/security/](../archive/security/).
