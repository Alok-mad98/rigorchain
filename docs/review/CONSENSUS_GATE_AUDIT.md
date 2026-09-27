# Consensus Gate Audit

**Task:**
**Date:** 2026-09-15  
**Constraint:** inspect only; this file does not add a gate.

```
protocol_changes = NONE
consensus_changes = NONE
```

This audit records what already runs before any Convergence identity lock
is added. Existing workflows are not redesigned here.

---

## 1. GitHub Actions (`.github/workflows/`)

| Gate name | Purpose | Trigger frequency | Protects consensus? | Notes |
|-----------|---------|-------------------|---------------------|-------|
| `mainnet-freeze-gate` | Pin freeze-lock constants (pole, shift, subsidy, genesis hashes, OpuHash tag) via `tools/lint-mainnet-freeze-locks.py` | PR / `main` push, **path-filtered** to lock headers + freeze declaration | **Yes** — numeric pins and that `CheckResMintArithmetic` still uses pole/shift and `required <= supplied` | Text-only, 3 min. Does **not** prove conceptual cost ≡ integer predicate. Does **not** forbid `/` / float / oracle in the predicate body as a dedicated shape lock. |
| `consensus-kernel-integrity` | Kernel authority lints + Coq objects + refinement / differential / transaction-kernel gates | PR / `main` push, **path-filtered** to `formal_verification/**`, `core/src/consensus/**`, kernel, selected lints | **Yes** — Coq model and kernel process | Three jobs, up to 120 min, Coq 8.15.0. Expensive. Docs / web / wallet path changes skip it. Not a minimal Convergence identity lock. |
| `formal-integrity-gate` | Formal-claim slogans and SOT flags in formal docs | PR, **path-filtered** to `release/**`, `audit/formal_bridge/**`, `formal_verification/**`, `docs/formal/**` | **No** — documentation integrity for the formal pack | Whitepaper / README / site are **outside** this scan. Does not require `equivalence_claimed = false` on the math note. |
| `genesis-release-guardian` | Pre-release guardian stages (build node, security audits) | Tags `v*`, `workflow_dispatch` | Release process, not every merge | Up to 180 min. Not a PR merge lock for ordinary commits. |
| `release-build` | Multi-OS release artifacts | Tags `v*`, dispatch, `workflow_call` | Tooling / artifacts | Not a consensus-identity check. |
| `release-wallet-macos` | macOS wallet DMGs | Tags `v*`, dispatch | Wallet packaging | Explicitly not consensus. |

There is **no** workflow that runs on every pull request regardless of path.
There is **no** existing job named `convergence_identity_test`,
`convergence_predicate_shape_check`, or `publication_boundary_check`.

---

## 2. Local / in-tree consensus-related checks (not all are CI)

| Check | Purpose | Typical trigger | Consensus vs tooling |
|-------|---------|-----------------|----------------------|
| `core/src/consensus/res_issuance.h` `static_assert`s | Compile-time genesis pass/fail, cap, display probes | Node / test compile | Consensus compile-time, not a GitHub merge job by itself |
| `core/src/test/res_issuance_tests.cpp` | Runtime Boost tests of predicate vs ceil burn, no free mint, cap | `test_bitcoin` / `test_rigor` when the node is built | Consensus tests. **Not wired as a cheap GitHub job.** Uses wallet helpers (`ResMinBurnForAtoms`) that divide; not the identity-only lock. |
| `core/test/functional/feature_res_destroy_cost_mint.py` and related `feature_res_*` | End-to-end mint / maturity / reorg | Functional suite | Consensus behaviour. Requires a built node. Not a GitHub default gate. |
| `tools/lint-mainnet-freeze-locks.py` | Same as freeze-gate | Manual or freeze-gate | Consensus pins |
| `tools/lint-consensus-*.py` (authority, purity, surface, …) | Kernel process / authority | kernel-integrity workflow | Consensus-process, broad |
| `tools/res_continuous_price_schedule.py` | Numeric SSOT / display schedule | Manual / docs | **Display / tooling.** Must not be treated as the consensus predicate. |
| `formal_verification/run_consensus_kernel_*.sh` | Coq governance / readiness / refinement | kernel-integrity | Model, expensive |

---

## 3. Duplicate or oversized surfaces

- **Numeric pins already exist** in `mainnet-freeze-gate` (`RES_DESTROY_POLE`, `RES_DESTROY_SHIFT`, genesis destroy cost). A second checksum of every RFC or every markdown file would duplicate that work and inflate the trust surface.
- **`consensus-kernel-integrity` is already a large bundle** (authority lints + Coq + differential + transaction kernel). Adding Convergence identity tests *into* that workflow would make ordinary mint-doc edits pay for Coq.
- **Formal slogan scanning is split:** `formal-integrity-gate` covers the formal pack; public whitepaper / site / README are not in that job.
- **Compile-time `static_assert`s and Boost tests already encode genesis 1024 ORU.** They do not run unless C++ is compiled. They also mix display helpers (`>> 50`) with the predicate.

---

## 4. Documentation-only checks that must not block consensus merges

| Check | Why it must not become a consensus merge tax |
|-------|-----------------------------------------------|
| `formal-integrity-gate` | Claim wording for the Coq release pack. Failure is a documentation integrity failure, not a mint-rule failure. |
| Website / explorer / wallet copy | Product surfaces. Allowed to change without RFC. |
| Markdown builds, RFC checksum-of-every-file, “every formula duplicated in CI” | Maintenance surface, not the mint identity. |

A publication-boundary check **may** exist as a **separate, cheap** job. It must not compile C++, must not run Coq, and must not fire on every file in `audit/` or `archive/`.

---

## 5. Gap this lock is meant to close

The freeze gate pins **numbers**. The kernel workflow pins **model process**. Neither is a
minimal, always-cheap lock of:

```
destroy_cost(h)  → exact integer-preserving equivalence →  R · (2^25+h)^2 ≤ B << 40
```

Missing today as a dedicated merge surface:

1. Conceptual cost vs integer predicate, **identical accept/reject**, integers only.
2. Source-shape lock of `CheckResMintArithmetic` only (multiply, square, shift, compare).
3. Publication flags `equivalence_claimed = false`, `cpp_formal_equivalence = false`,
   `documentation_consensus_authority = false` on the public narrative — without scanning
   the whole repository.

Those three are sufficient. No further CI expansion is justified by this audit.
