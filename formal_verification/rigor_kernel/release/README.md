# Formal Verification Release Package

Third-party audit package for the RIGOR **critical consensus kernel**.

**Public claim (only):** Formally verified critical consensus invariants
(Coq consensus kernel only).

This directory is the entry point for an external auditor. It does
not claim C++ equivalence, full-node correctness, network security,
wallet security, or cryptographic-primitive security.

## Package map

| File | Purpose |
|---|---|
| `AUDIT_PACKAGE.md` | **Unique auditor entry:** replay, Category A, non-claims |
| `VerificationScope.md` | What is verified, and what is not |
| `TheoremIndex.md` | Top-level theorems, statements, assumptions, binary traces |
| `ProofEvidenceIndex.md` | Requirement → theorem → proof → validator → vector → result |
| `BinaryArtifactIndex.md` | Extracted binary and golden-vector results |
| `ReproductionGuide.md` | How to rebuild and re-check the cone |
| `AuditorChecklist.md` | Step-by-step audit checklist |
| `PublicClaims.md` | Allowed and forbidden public wording |
| `VersionManifest.md` | Toolchain and host stamp; **not** a false commit claim |
| `ProofSourceManifest.md` | Worktree inventory of `.v` / markdown / scripts |
| `SourceManifest.sha256` | sha256 lock of cone sources |
| `TheoremClassification.md` | Category A / B / C |
| `KernelCompletenessMatrix.md` | Requirement → theorem → validator → mutation |
| `IndependentAuditReplay.md` | One-click auditor replay + P0-113 finding history |
| `independent_audit_replay.sh` | Minimal-dep one-shot of claimed results |
| `RefinementMapping.md` | Six checkers ↔ production paths; no production refinement |
| `VerifiedCompilerPath.md` | CompCert intent; documented TCB; no forced switch |
| `OpenProperties.md` | Unproved liveness / fairness / refinement / crypto / binary |
| `KernelInventory.md` | P0-119 map: proved / partial / open / TCB / routes |
| `TCBInventory.md` | Checking-path TCB (Coq, OCaml, OS, hardware) |
| `TrustDebtMatrix.md` | Claim vs missing proof vs difficulty |
| `P0-119-DecisionReport.md` | Completeness / claim / theorem→binary decisions |
| `KernelDeepeningReport.md` | P0-120~125 theorem / category / mutation deepening |
| `KernelCompletenessReport.md` | P0-126~130 completeness inventory / M1–M23 |
| `ExtractionBoundary.md` | Layer 1→2 source identity; Layer 2→3 not claimed |
| `CompilerBoundary.md` | Level-5 compiler items, all OPEN |
| `FormalTrustRoadmap.md` | Trust levels 0–5; current Level 3+ / Level 4 foundation; ORU independent domain |
| `FinalProofInventory.md` | Every classified theorem: statement, assumptions, mutations |
| `FormalConeMap.md` | Specification → checker-contract layer map |
| `FinalClaimsBoundary.md` | TRUE / FALSE public-claim freeze |
| `FinalAssumptionAudit.md` | Admitted / Axiom / Print Assumptions audit |
| `MutationCompletenessAudit.md` | M1–M41 class coverage (no new mutants) |
| `final_adversarial_audit.sh` | Third-party attack surface: claims, TCB, counts |
| `freeze_source_manifest.sh` | Publisher-only: write `SourceManifest.sha256` |
| `check_source_manifest.sh` | Read-only bidirectional source lock |

Faithful extract + tests: `../scripts/extract_and_test.sh`.
Differential verify (fragment only): `../scripts/diff_verify.sh`.
Verified-compiler rehearsal: `../scripts/verified_compiler_path.sh`.
| `check_release.sh` | Package completeness gate |

Supporting evidence lives in `../evidence/` and `../kernel_binary/`.

## How to start

```
git clone <this-repository>
cd formal_verification/rigor_kernel
less release/AUDIT_PACKAGE.md
./release/check_source_manifest.sh
./release/independent_audit_replay.sh
./run_kernel_critical.sh
```

A complete run prints `FORMAL_KERNEL_PHASE_RELEASE=PASS`.
