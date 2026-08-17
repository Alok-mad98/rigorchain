-- Persist C (getconvergenceresult.convergence_root) separately from the draw root.
-- committed_root / computed_root / draw_candidate_root remain the eligible-set root.

ALTER TABLE blocks ADD COLUMN IF NOT EXISTS c_root TEXT;
