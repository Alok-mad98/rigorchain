# Draw recomputation note (RFC-017 collateral)

| Field | Value |
|-------|-------|
| **Trigger** | Regenerating `phase3_golden_vectors.json` for E=246 / scheduled rewards |
| **Module** | `Phase4FConsensus/ConsensusDrawAudit.v` |
| **Audit date** | 2026-08-19 |

## What broke

`tools/generate_phase4f_golden_traces.py` had been frozen since the
`SigmaCR01 → SigmaPoW01` rename. Rebuilding it for the new JSON exposed that
`ConsensusDrawAudit.recompute_winner` ranked by **higher weight / higher txid**,
while `Selection.pick_better_tx` ranks by **lower σ-cross-product / lower txid**.

On the 14-row baseline the two relations coincided; on 84 RunSelection vectors
they agree on only half the rows under the wrong rule.

## What was proved after the fix

| Item | Status |
|------|--------|
| Rule = protocol (`cand_better` mirrors `sigma_product_lt` + txid LE order) | Implemented |
| Independent Python mirror vs golden `winner_index` | 84/84 |
| `audit_match_count = 168`, `audit_mismatch_count = 0` | Closed by `vm_compute` |
| `draw_circularity_closed` | Closed; derived from empty mismatch filter (no axioms) |

## Proof engineering (not a weakening)

The ∀-packaging over 168 rows uses the empty-mismatch filter rather than
`repeat destruct` on `In`. Both styles rest on the **same** `vm_compute` of the
selection rule against every row. Expanding a 168-way case split does not add
evidence; it only inflates proof terms. See the discussion in the session that
closed RFC-017 formal work.

## Remaining Admits / Axioms

None for `draw_circularity_closed` / `audit_match_count_value`.

## Scope limit

Eligibility bits remain imported kernel evidence (hash kernel out of Phase-4F).
The audit checks selection given `(eligible, σ, weight, txid)`, not re-derivation
of eligibility from digests.
