# RES Monetary Policy — Golden Vector Update Report

**Task:** `TASK-RIGOR-RES-MONETARY-POLICY-FINAL-VERIFICATION-P0-001` §4  
**Date:** 2026-08-20  
**Corpus:** `formal_verification/phase3_golden_vectors.json`  
**Current vector count:** **402** (supersedes outdated “324” website copy)

---

## 1. Audit of existing RES monetary coverage

| Need | Existing coverage | IDs / mechanism |
|------|-------------------|-----------------|
| Genesis `k=0` / `W(0)` | Yes | Supply / mint-shape rows using `RES_REWARD_GENESIS`; height-1 mints |
| Early `k=1` | Yes | `NEG-007` `mint_genesis_W_at_epoch_1` (wrong amount at epoch 1) |
| Mid / terminal amounts | Partial | Schedule exercised via height in SUP / CROSS notes; not every `k` as its own vector |
| `k=250` last paying | Partial | Implicit via schedule functions; not a dedicated NEG pin row |
| `k=251` termination | Yes | `NEG-012` `schedule_exhausted_mint` |
| Wrong reward amount | Yes | `NEG-005` (`W-1`), `NEG-007` (wrong epoch) |
| Cap overshoot | Yes | `NEG-010`, `NEG-011` |
| Full table `k=0..251` | **Added as parity gate** | `tests/test_rfc017_schedule_parity.py` (C++ ≡ Python ≡ Coq pins) |

---

## 2. Why the full table is not duplicated into Phase-3 JSON

Regenerating hundreds of near-identical schedule pin vectors would:

- inflate NegativeCore / Phase-4F trace regeneration cost, and  
- hard-code the same integers already pinned by `static_assert`, `schedule_sum_value`, and `reward_pin_table`.

The final-verification task requires **100% identical** outputs for `k=0..251`. That requirement is met by the **parity test**, which fails the suite on any drift.

---

## 3. Sufficiency verdict

| Question | Answer |
|----------|--------|
| Are RES monetary golden vectors sufficient for reject-path coverage? | **Yes** for wrong-epoch, over-cap, and post-termination mint |
| Is numeric schedule coverage complete? | **Yes**, via parity gate + Coq pin table |
| Was Phase-3 JSON regenerated in this task? | **No** (not required once parity gate passes; avoids consensus-adjacent harness churn) |

---

## 4. Follow-ups (optional, not blocking)

- Add dedicated `NEG` rows for minting `W(0)` at heights in epochs 10 / 100 / 250 if product wants them Coq-enforced.  
- Update any remaining “324 golden vectors” public copy to **402** (website Formal Verification page updated in this task).
