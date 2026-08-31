-- RIGOR Explorer / Indexer MVP (closed testnet, read-only)
-- Deterministic indexing constraints:
-- - txid is the canonical identity for all relationships/URLs/keys
-- - wtxid is informational only (stored in transactions.wtxid, never used as key)

BEGIN;

CREATE TABLE IF NOT EXISTS chain_state (
  indexer_name TEXT PRIMARY KEY,
  network TEXT NOT NULL,
  last_height BIGINT NOT NULL DEFAULT -1,
  last_block_hash TEXT,
  critical_root_mismatch BOOLEAN NOT NULL DEFAULT FALSE,
  updated_at TIMESTAMPTZ NOT NULL DEFAULT now()
);

CREATE TABLE IF NOT EXISTS blocks (
  block_hash TEXT PRIMARY KEY,
  height BIGINT NOT NULL UNIQUE,

  time BIGINT NOT NULL,
  bits TEXT NOT NULL,
  res_probability_target TEXT NOT NULL,

  merkleroot TEXT NOT NULL,
  draw_candidate_root TEXT NOT NULL,

  rigor_res_minted INTEGER NOT NULL,
  rigor_winner TEXT NOT NULL,

  cumulative_convergence TEXT,
  opus_hash TEXT,
  tx_count INTEGER NOT NULL,

  bind_hash TEXT NOT NULL,
  bind_height BIGINT NOT NULL,

  committed_root TEXT NOT NULL,
  computed_root TEXT NOT NULL,
  mismatch BOOLEAN NOT NULL,

  imported_at TIMESTAMPTZ NOT NULL DEFAULT now()
);

CREATE TABLE IF NOT EXISTS transactions (
  txid TEXT PRIMARY KEY,
  wtxid TEXT,

  block_hash TEXT NOT NULL REFERENCES blocks(block_hash) ON DELETE CASCADE,
  block_height BIGINT NOT NULL,
  block_tx_index INTEGER NOT NULL,

  version INTEGER,
  locktime BIGINT,
  is_coinbase BOOLEAN NOT NULL DEFAULT FALSE,

  raw_hex TEXT
);

CREATE TABLE IF NOT EXISTS inputs (
  txid TEXT NOT NULL REFERENCES transactions(txid) ON DELETE CASCADE,
  vin_index INTEGER NOT NULL,

  prev_txid TEXT,
  prev_vout INTEGER,

  sequence BIGINT,
  coinbase BOOLEAN NOT NULL DEFAULT FALSE,

  txinwitness JSONB,
  script_sig JSONB,

  PRIMARY KEY (txid, vin_index)
);

CREATE TABLE IF NOT EXISTS outputs (
  txid TEXT NOT NULL REFERENCES transactions(txid) ON DELETE CASCADE,
  vout_index INTEGER NOT NULL,

  value_atoms BIGINT NOT NULL,
  asset_kind TEXT NOT NULL CHECK (asset_kind IN ('ORU','RES','NONE','INVALID')),

  script_pubkey TEXT NOT NULL,
  address TEXT,

  witness_version INTEGER,
  program_hex TEXT,

  PRIMARY KEY (txid, vout_index)
);

-- Materialized ORU UTXO view for fast address/balance reads
CREATE TABLE IF NOT EXISTS oru_utxos (
  txid TEXT NOT NULL,
  vout_index INTEGER NOT NULL,

  height BIGINT NOT NULL,
  amount_atoms BIGINT NOT NULL,

  script_pubkey TEXT NOT NULL,
  address TEXT,

  spent BOOLEAN NOT NULL DEFAULT FALSE,
  spending_txid TEXT,
  spending_vin_index INTEGER,

  PRIMARY KEY (txid, vout_index),
  FOREIGN KEY (txid, vout_index) REFERENCES outputs(txid, vout_index) ON DELETE CASCADE
);

-- One row per (block_hash, convergence_txid) candidate row
CREATE TABLE IF NOT EXISTS convergence_events (
  block_hash TEXT NOT NULL REFERENCES blocks(block_hash) ON DELETE CASCADE,
  block_height BIGINT NOT NULL,

  convergence_txid TEXT NOT NULL REFERENCES transactions(txid) ON DELETE CASCADE,

  selection_digest_h TEXT,
  draw_eligible BOOLEAN NOT NULL,
  convergence_type TEXT,

  bind_hash TEXT NOT NULL,
  bind_height BIGINT NOT NULL,

  convergence_root TEXT NOT NULL,
  res_probability_target TEXT NOT NULL,

  committed_root TEXT NOT NULL,
  computed_root TEXT NOT NULL,
  mismatch BOOLEAN NOT NULL,

  mint_amount BIGINT NOT NULL DEFAULT 0,

  PRIMARY KEY (block_hash, convergence_txid)
);

-- One row per RES output created by a mint transaction
CREATE TABLE IF NOT EXISTS res_mints (
  mint_txid TEXT NOT NULL REFERENCES transactions(txid) ON DELETE CASCADE,
  mint_vout_index INTEGER NOT NULL,

  block_hash TEXT NOT NULL REFERENCES blocks(block_hash) ON DELETE CASCADE,
  block_height BIGINT NOT NULL,

  winner_convergence_txid TEXT NOT NULL REFERENCES transactions(txid) ON DELETE CASCADE,

  mint_amount BIGINT NOT NULL,

  bind_hash TEXT NOT NULL,
  bind_height BIGINT NOT NULL,

  convergence_root TEXT NOT NULL,
  draw_eligible BOOLEAN NOT NULL,

  res_probability_target TEXT NOT NULL,

  committed_root TEXT NOT NULL,
  computed_root TEXT NOT NULL,

  proof_items JSONB NOT NULL,

  PRIMARY KEY (mint_txid, mint_vout_index)
);

-- One row per RES output created by a RES-spending transaction
CREATE TABLE IF NOT EXISTS res_transfers (
  spend_txid TEXT NOT NULL REFERENCES transactions(txid) ON DELETE CASCADE,
  spend_vin_index INTEGER NOT NULL,

  spend_block_hash TEXT NOT NULL REFERENCES blocks(block_hash) ON DELETE CASCADE,

  res_input_txid TEXT NOT NULL,
  res_input_vout_index INTEGER NOT NULL,

  res_output_txid TEXT NOT NULL,
  res_output_vout_index INTEGER NOT NULL,

  res_amount_atoms BIGINT NOT NULL,

  PRIMARY KEY (spend_txid, spend_vin_index, res_output_vout_index)
);

COMMIT;

