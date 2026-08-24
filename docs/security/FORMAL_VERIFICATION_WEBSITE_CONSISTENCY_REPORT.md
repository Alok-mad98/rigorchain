# Formal Verification Website Consistency Report

**Task:** `TASK-RIGOR-FORMAL-VERIFICATION-COVERAGE-AUDIT-P0-001`  
**Date:** 2026-08-20  
**Page:** `/documentation/formal-verification` → `web/rigorchain-site/src/pages/FormalVerificationPage.tsx`

## Required messaging checklist

| Requirement | Before audit | After audit |
|-------------|--------------|-------------|
| Publishes executable specifications | Implied (layers) | Explicit bullet in Overview |
| Publishes Coq verification models | Yes (“Strongly verified Coq model”) | Retained |
| Publishes golden vectors | Yes (stale count) | Corrected to **422** |
| Publishes verification tooling | Partially (gate/CLI mentions) | Explicit tooling sentence |
| Avoid “fully mathematically proven blockchain” | Yes (explicit reject) | Retained |
| Use “Formally verified protocol invariants with explicit verification boundaries.” | Missing exact sentence | Added |
| `equivalence_claimed = false` | Present | Retained |

## Defects found

1. **Stale counts:** Formal Verification page showed **402 / 117**; living lock is **422 / 215** (`golden_lock.json`).
2. **Missing allowed claim sentence** (exact wording from task).
3. **Incomplete “what we publish” list** (spec / Coq / vectors / tooling).
4. **Related pages also stale:** WhitepaperPage / StatusPage / ExperimentPage still cited **324 / 117** (operator freeze day).

## Changes applied (docs/site only)

- Update FormalVerification DarkCards to 422 shared / 215 Coq-enforced.
- Add Overview sentence with the allowed claim wording.
- List published artifacts: executable spec, Coq models, golden vectors, verification tooling (`run_differential_gate.sh`, hash CLI, σ/root CLI).
- Sync WhitepaperPage, StatusPage, ExperimentPage living counts (with historical freeze note where useful).

No consensus, wallet, miner, or RFC changes.
