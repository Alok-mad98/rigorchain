-- Fail-closed issuance generation. Old indexer DBs (RFC-019 cap 613454,
-- frozen 1024-as-current-price) must reindex; do not serve them.
-- Empty DBs (last_height < 0) are stamped at explorer startup.

BEGIN;

ALTER TABLE chain_state
  ADD COLUMN IF NOT EXISTS issuance_generation TEXT;

COMMIT;
