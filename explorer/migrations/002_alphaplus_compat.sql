-- AlphaPlus explorer compatibility: optional bind/RES metadata; no consensus fields.
-- Fresh installs apply 001 then this file. Existing DBs apply this once.

ALTER TABLE blocks ALTER COLUMN bind_hash DROP NOT NULL;
ALTER TABLE blocks ALTER COLUMN bind_height DROP NOT NULL;
ALTER TABLE blocks ALTER COLUMN res_probability_target DROP NOT NULL;

ALTER TABLE convergence_events ALTER COLUMN bind_hash DROP NOT NULL;
ALTER TABLE convergence_events ALTER COLUMN bind_height DROP NOT NULL;
ALTER TABLE convergence_events ALTER COLUMN res_probability_target DROP NOT NULL;

ALTER TABLE res_mints ALTER COLUMN bind_hash DROP NOT NULL;
ALTER TABLE res_mints ALTER COLUMN bind_height DROP NOT NULL;
ALTER TABLE res_mints ALTER COLUMN res_probability_target DROP NOT NULL;
