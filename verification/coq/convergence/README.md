# Coq — convergence

**Canonical paths:**

| Topic | File |
|-------|------|
| Selection / eligibility / window constants | `formal_verification/coq/Selection.v` |
| Order independence of `C` and winner | `formal_verification/coq/OrderIndependence.v` |
| Convergence_Root | `formal_verification/coq/ConvergeRoot.v` |
| Draw candidate root (eligible digest) | `formal_verification/coq/DrawCandidateRoot.v` |
| A-9-C unpaid exclusion | `formal_verification/coq/WindowExclusion.v` |
| σ PoW-01 layout | `formal_verification/coq/SigmaPoW01.v`, `CanonicalDrawEncoder.v` |
| Executable SHA-256d | `formal_verification/coq/RigorSha256.v`, `RigorHash.v` |
| Kernel harness (215 enforced vectors) | `formal_verification/coq/Kernel/GoldenHarness.v`, `NegativeCoreHarness.v` |

**Example theorems:** `convergence_root_order_independent`, `select_winner_order_independent`, `mint_cost_is_1024_oru`, `draw_pipeline_e2e` family.

**Boundary:** DCR is an eligible-list digest; coinbase / Merkle / header bind is implementation-level, not proved here.
