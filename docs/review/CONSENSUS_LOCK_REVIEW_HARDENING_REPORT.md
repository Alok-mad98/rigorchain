# Consensus Lock Review Hardening Report

**Task:** CONSENSUS_LOCK_REVIEW_HARDENING_001  
**Date:** 2026-09-15

```
protocol_changes = NONE
consensus_changes = NONE
implementation_behavior_changes = NONE
mathematical_identity_changes = NONE
```

Review of the Convergence consensus-lock family. No fourth lock. No RFC
or mint-predicate edit. Wording, documentation, and CI path filters only.

---

## 1. Purpose of the three locks

Keep this structure. Do not merge.

```
Consensus Protection
│
├── convergence_identity_test
│
├── convergence_predicate_shape_check
│
└── publication_boundary_check
```

**`convergence_identity_test`** protects the mathematical identity:

```
destroy_cost(h)
        ↓ exact integer-preserving equivalence
R · (2^25 + h)^2 ≤ B << 40
```

It is there to catch a changed shift, rounding/floor flowing back,
division flowing back, a display helper becoming consensus input, or a
silent change of meaning. Integers only. Seconds.

**`convergence_predicate_shape_check`** protects the arithmetic shape of
the Convergence mint predicate in `CheckResMintArithmetic`:
multiplication, squaring, scaling shift, and comparison remain explicit.
It does not define the entire consensus execution path.

保护 Convergence 铸造判定的核心算术形状：乘法、平方、缩放移位和比较保持显式存在。该锁不定义整个共识执行路径。

**`publication_boundary_check`** protects the public narrative. It forbids
an unscoped slogan such as “RIGOR is formally verified.” It is
documentation integrity, not a node rule.

---

## 2. Why three, not one

A merged job would hide which invariant moved.

- Vectors can still pass if a new operator sits beside an inequality that
  happens to remain true on the sampled points.
- A shape scan can still pass if the integers in the identity drift but
  the tokens `pole_h * pole_h` and `<< RES_DESTROY_SHIFT` remain.
- A documentation slogan can appear without any mint-arithmetic change.

Three cheap jobs are smaller than one opaque job that claims to “lock
consensus.”

---

## 3. Why not more locks

Consensus lock is not “forbid editing the project.” It is “prevent
unintentional damage to core mathematics and protocol identity.”

Not added: CL004, CL005, CL006, RFC checksum CI, markdown-wide builds,
Coq-on-docs, compile-on-copyedits.

If a later lock is ever justified, it should be another true protocol
identity (Convergence, PoW header, transaction serialization) — and the
count must stay small.

---

## 4. Actions cost

| Workflow | Jobs | Triggers | Cost |
|----------|------|----------|------|
| `consensus-lock.yml` | identity + shape | Mint predicate / lock constants / those two scripts | Two Python jobs, timeout 3 min each, typically seconds. No Coq, no node build. |
| `consensus-lock-publication.yml` | publication only | README, whitepaper, site `src`, policy, publication script | One Python job, seconds. Docs/web copy does **not** start identity vectors. |

Ordinary wallet, miner, explorer, and unrelated `core/` work is outside
these path filters.

Before this review, one workflow unioned consensus paths with
`docs/whitepaper/**` and `web/rigorchain-site/src/**`, so a prose-only
whitepaper PR also ran the integer vectors. That union is split. Still
three locks, not four.

Local: `bash tools/run-consensus-lock.sh` (all three, no network).

---

## 5. Files touched in this review

| File | Change |
|------|--------|
| `docs/consensus/CONSENSUS_LOCK_POLICY.md` | Gate 2 wording; lock is not a rule / RFC / Coq / refinement; no extra CLs |
| `tools/check-convergence-predicate-shape.py` | Docstring only; scan behaviour unchanged |
| `.github/workflows/consensus-lock.yml` | Identity + shape only; drop docs/web paths |
| `.github/workflows/consensus-lock-publication.yml` | Publication path filter |
| `docs/review/CONSENSUS_LOCK_HARDENING_REPORT.md` | Align Gate 2 sentence |
| `docs/review/CONSENSUS_LOCK_REVIEW_HARDENING_REPORT.md` | This report |

Unchanged: RFC-020, RFC-018, consensus parameters, serialization, RES
predicate, ORU subsidy, Coq, C++ consensus behaviour.

---

## 6. Confirmation

| Claim | Status |
|-------|--------|
| Protocol changes | NONE |
| Consensus changes | NONE |
| Implementation behavior changes | NONE |
| Mathematical identity changes | NONE |

The lock still protects the existing identity. It still does not replace
RFC governance. It still does not prove C++ refinement.

---

## 7. Remaining risks

- Path filters can miss a new public markdown tree outside
  `docs/whitepaper/`, `docs/WHITEPAPER.md`, `README.md`, and
  `web/rigorchain-site/src/`. That is accepted: the lock is not a
  whole-repository freeze.
- Identity vectors are a finite grid. They do not replace a proof of
  equivalence for every `(R, B, h)`.
- Shape check is textual. A cleverly equivalent rewrite that keeps
  `pole_h * pole_h` and `<< RES_DESTROY_SHIFT` could still pass; RFC
  review remains the change process.
- Freeze-gate still pins numbers separately. This family does not
  absorb that job.

Target state:

minimal consensus surface  
+ explicit mathematical identity protection  
+ low-maintenance CI gates  
+ no accidental consensus drift
