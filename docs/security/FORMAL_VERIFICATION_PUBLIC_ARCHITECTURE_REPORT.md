# Formal Verification Public Release Architecture Report

**Task:** `TASK-RIGOR-FORMAL-VERIFICATION-PUBLIC-RELEASE-ARCHITECTURE-P0-001`  
**Date:** 2026-08-20  
**Scope:** Documentation, repository organization, public verification exposure only.  
**No** consensus / wallet / miner / RFC file changes.

```
equivalence_claimed = false
```

---

## Objective

Expose the completed formal-verification work as a **public verification architecture**: a navigable `verification/` tree, external how-to, claim table, website sections, and whitepaper chapter wording that match the honesty boundary.

---

## What was created / updated

| Deliverable | Path |
|-------------|------|
| Public verification root | `verification/README.md` + mapped subdirs |
| How-to reproduce | `docs/security/HOW_TO_VERIFY_RIGOR.md` |
| Public claim table | `docs/security/RIGOR_FORMAL_CLAIM_TRACEABILITY.md` |
| Website Formal Verification | `web/rigorchain-site/src/pages/FormalVerificationPage.tsx` |
| Whitepaper Ch.8 enhancement | `docs/whitepaper/RIGOR_PROTOCOL_WHITEPAPER.md` |
| This report | `docs/security/FORMAL_VERIFICATION_PUBLIC_ARCHITECTURE_REPORT.md` |

Canonical *machines* remain under `formal_verification/` (Coq, Python, JSON, gate). The top-level `verification/` tree is an **index layout** so GitHub visitors see `coq/monetary-policy`, `golden-vectors`, `differential-tests` without duplicating `.v` / JSON blobs.

---

## Architecture decision

| Choice | Rationale |
|--------|-----------|
| Index READMEs → `formal_verification/**` | Single source of truth; avoid drift of copied Coq |
| Keep `docs/verification/*` status pages | Operator freeze / discrepancy catalog unchanged |
| Living counts **422 / 215** | Matches `golden_lock.json` |

---

## Public messaging (PART 7)

Reviewed Formal Verification site page and whitepaper Ch.8 for forbidden phrases. Required positive wording used:

> Formally verified protocol invariants with explicit verification boundaries.

(or the PART 7 variant: *within an explicitly defined verification boundary*).

Honesty flags remain displayed. No “bug free” / “zero risk” / “fully proven blockchain” claims introduced.

---

## Reproduction contract

External PASS path documented in `HOW_TO_VERIFY_RIGOR.md`:

1. `run_differential_gate.sh` → PASS  
2. Optional `coqc` full `_CoqProject` → EXIT 0  
3. Optional schedule parity test → EXIT 0  

---

## Out of scope (intentional)

- Moving or rewriting consensus C++  
- Adding golden vectors  
- Flipping `equivalence_claimed`  
- Push / merge / tag
