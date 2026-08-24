-- Explorer/indexer RFC-018 fee-burn statistics (Option C).
-- Not consensus. ON DELETE CASCADE follows blocks so reorg truncateFromHeight
-- drops disconnected heights; remaining prefix cumulative stays valid.

BEGIN;

CREATE TABLE IF NOT EXISTS rfc018_fee_burn_stats (
  height BIGINT PRIMARY KEY,
  block_hash TEXT NOT NULL REFERENCES blocks(block_hash) ON DELETE CASCADE,
  burned_oru BIGINT NOT NULL DEFAULT 0,
  cumulative_burned_oru BIGINT NOT NULL DEFAULT 0
);

CREATE INDEX IF NOT EXISTS rfc018_fee_burn_stats_block_hash_idx
  ON rfc018_fee_burn_stats (block_hash);

COMMIT;
