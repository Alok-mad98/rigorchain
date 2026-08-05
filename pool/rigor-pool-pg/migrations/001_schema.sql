-- TASK-MINING-017: PostgreSQL persistence schema (logical DDL from POSTGRESQL_PERSISTENCE_ARCHITECTURE.md)

CREATE TABLE IF NOT EXISTS miners (
    id BIGSERIAL PRIMARY KEY,
    username VARCHAR(64) NOT NULL UNIQUE,
    oru_payout_address VARCHAR(128) NOT NULL DEFAULT '',
    created_at TIMESTAMPTZ NOT NULL DEFAULT now()
);

CREATE TABLE IF NOT EXISTS workers (
    id BIGSERIAL PRIMARY KEY,
    miner_id BIGINT NOT NULL REFERENCES miners(id),
    worker_name VARCHAR(128) NOT NULL,
    password_hash VARCHAR(256) NOT NULL DEFAULT '',
    current_difficulty DOUBLE PRECISION NOT NULL DEFAULT 1.0,
    last_share_at TIMESTAMPTZ,
    UNIQUE (miner_id, worker_name)
);

CREATE TABLE IF NOT EXISTS gbt_templates (
    record_id UUID PRIMARY KEY,
    tip_hash CHAR(64) NOT NULL,
    longpollid VARCHAR(128) NOT NULL UNIQUE,
    gbt_json JSONB NOT NULL,
    payload_hash CHAR(64) NOT NULL,
    ingested_at TIMESTAMPTZ NOT NULL DEFAULT now()
);

CREATE TABLE IF NOT EXISTS pool_sequence (
    name VARCHAR(32) PRIMARY KEY,
    next_value BIGINT NOT NULL
);

INSERT INTO pool_sequence (name, next_value) VALUES ('job_serial', 1)
ON CONFLICT (name) DO NOTHING;

CREATE TABLE IF NOT EXISTS pool_chain_cursor (
    id INT PRIMARY KEY CHECK (id = 1),
    current_tip CHAR(64) NOT NULL DEFAULT '',
    last_gbt_at TIMESTAMPTZ,
    ibd BOOLEAN NOT NULL DEFAULT false
);

INSERT INTO pool_chain_cursor (id, current_tip) VALUES (1, '')
ON CONFLICT (id) DO NOTHING;

CREATE TABLE IF NOT EXISTS candidate_blocks (
    id BIGSERIAL PRIMARY KEY,
    job_id VARCHAR(32) NOT NULL UNIQUE,
    record_id UUID NOT NULL,
    gbt_longpollid VARCHAR(128) NOT NULL DEFAULT '',
    prev_block_hash CHAR(64) NOT NULL,
    tip_hash CHAR(64) NOT NULL DEFAULT '',
    height INT NOT NULL,
    convergence_root CHAR(64) NOT NULL DEFAULT '',
    mint_required BOOLEAN NOT NULL DEFAULT false,
    body_mint_reserved BOOLEAN NOT NULL DEFAULT false,
    status VARCHAR(32) NOT NULL DEFAULT 'ACTIVE',
    block_hash CHAR(64),
    submit_error TEXT,
    snapshot_jsonb JSONB,
    created_at TIMESTAMPTZ NOT NULL DEFAULT now(),
    expires_at TIMESTAMPTZ
);

CREATE INDEX IF NOT EXISTS idx_candidate_blocks_status ON candidate_blocks(status);
CREATE INDEX IF NOT EXISTS idx_candidate_blocks_record ON candidate_blocks(record_id);

CREATE TABLE IF NOT EXISTS winner_jobs (
    id BIGSERIAL PRIMARY KEY,
    candidate_block_id BIGINT NOT NULL UNIQUE REFERENCES candidate_blocks(id),
    status VARCHAR(32) NOT NULL DEFAULT 'PROVISIONAL',
    mint_required BOOLEAN NOT NULL DEFAULT false,
    eligible_count INT NOT NULL DEFAULT 0,
    winner_txid CHAR(64),
    bind_hash CHAR(64) NOT NULL DEFAULT '',
    draw_target_prev_nbits CHAR(8) NOT NULL DEFAULT '',
    convergence_txids JSONB NOT NULL DEFAULT '[]',
    preview_audit JSONB NOT NULL DEFAULT '{}',
    token_nonce UUID,
    created_at TIMESTAMPTZ NOT NULL DEFAULT now()
);

CREATE TABLE IF NOT EXISTS mint_authorization_tokens (
    token_nonce UUID PRIMARY KEY,
    winner_job_id BIGINT NOT NULL REFERENCES winner_jobs(id),
    candidate_block_id BIGINT NOT NULL REFERENCES candidate_blocks(id),
    draw_winner_txid CHAR(64) NOT NULL,
    convergence_root CHAR(64) NOT NULL,
    bind_hash CHAR(64) NOT NULL,
    target_prev INT NOT NULL,
    height INT NOT NULL,
    winner_convergence_amount BIGINT NOT NULL DEFAULT 0,
    -- A-5 R2: witness v1 program the winning convergence declared; the mint pays exactly it.
    res_payout_program CHAR(64) NOT NULL DEFAULT '',
    res_probability_target CHAR(64) NOT NULL DEFAULT '',
    authorized_at BIGINT NOT NULL,
    expires_at BIGINT NOT NULL,
    replay_cert_tag VARCHAR(64) NOT NULL DEFAULT '',
    status VARCHAR(32) NOT NULL DEFAULT 'AUTHORIZED',
    consumed BOOLEAN NOT NULL DEFAULT false
);

CREATE TABLE IF NOT EXISTS mint_jobs (
    id BIGSERIAL PRIMARY KEY,
    winner_job_id BIGINT REFERENCES winner_jobs(id),
    stratum_job_id VARCHAR(32) NOT NULL UNIQUE,
    body_mint_txid CHAR(64) NOT NULL DEFAULT '',
    body_mint_hex_enc BYTEA,
    res_payout_script_hash CHAR(64) NOT NULL DEFAULT '',
    status VARCHAR(32) NOT NULL DEFAULT 'PENDING',
    sigma_digest_hex CHAR(64) NOT NULL DEFAULT '',
    built_at TIMESTAMPTZ,
    submitted_at TIMESTAMPTZ
);

CREATE TABLE IF NOT EXISTS treasury_utxos (
    id BIGSERIAL PRIMARY KEY,
    txid CHAR(64) NOT NULL,
    vout INT NOT NULL,
    mint_job_id BIGINT REFERENCES mint_jobs(id),
    amount_atoms BIGINT NOT NULL,
    script_pubkey_hex TEXT NOT NULL DEFAULT '',
    status VARCHAR(32) NOT NULL DEFAULT 'AVAILABLE',
    confirmations_at_reserve INT NOT NULL DEFAULT 0,
    reserved_at TIMESTAMPTZ,
    UNIQUE (txid, vout)
);

CREATE TABLE IF NOT EXISTS shares (
    id BIGSERIAL PRIMARY KEY,
    share_id VARCHAR(256) NOT NULL UNIQUE,
    worker_id BIGINT NOT NULL REFERENCES workers(id),
    candidate_block_id BIGINT NOT NULL REFERENCES candidate_blocks(id),
    job_id VARCHAR(32) NOT NULL,
    share_diff DOUBLE PRECISION NOT NULL,
    is_block BOOLEAN NOT NULL DEFAULT false,
    orphaned BOOLEAN NOT NULL DEFAULT false,
    pow_hash CHAR(64) NOT NULL DEFAULT '',
    ntime CHAR(8) NOT NULL DEFAULT '',
    nonce CHAR(8) NOT NULL DEFAULT '',
    created_at TIMESTAMPTZ NOT NULL DEFAULT now()
);

CREATE INDEX IF NOT EXISTS idx_shares_worker_created ON shares(worker_id, created_at DESC);

CREATE TABLE IF NOT EXISTS payout_rounds (
    id BIGSERIAL PRIMARY KEY,
    candidate_block_id BIGINT NOT NULL UNIQUE REFERENCES candidate_blocks(id),
    block_hash CHAR(64),
    coinbase_oru_atoms BIGINT NOT NULL DEFAULT 0,
    status VARCHAR(32) NOT NULL DEFAULT 'OPEN',
    payout_txid CHAR(64),
    finalized_at TIMESTAMPTZ
);

CREATE TABLE IF NOT EXISTS payout_lines (
    id BIGSERIAL PRIMARY KEY,
    payout_round_id BIGINT NOT NULL REFERENCES payout_rounds(id),
    worker_id BIGINT NOT NULL REFERENCES workers(id),
    oru_atoms BIGINT NOT NULL,
    UNIQUE (payout_round_id, worker_id)
);

CREATE TABLE IF NOT EXISTS reorg_events (
    id BIGSERIAL PRIMARY KEY,
    old_tip CHAR(64) NOT NULL,
    new_tip CHAR(64) NOT NULL,
    depth INT,
    orphaned_job_ids JSONB NOT NULL DEFAULT '[]',
    detected_at TIMESTAMPTZ NOT NULL DEFAULT now(),
    UNIQUE (old_tip, new_tip)
);

CREATE TABLE IF NOT EXISTS audit_events (
    id BIGSERIAL PRIMARY KEY,
    category VARCHAR(64) NOT NULL,
    message TEXT NOT NULL,
    detail JSONB NOT NULL DEFAULT '{}',
    candidate_block_id BIGINT REFERENCES candidate_blocks(id),
    mint_job_id BIGINT REFERENCES mint_jobs(id),
    reorg_event_id BIGINT REFERENCES reorg_events(id),
    ts TIMESTAMPTZ NOT NULL DEFAULT now()
);

CREATE TABLE IF NOT EXISTS pilot_status_snapshots (
    id BIGSERIAL PRIMARY KEY,
    snapshot_jsonb JSONB NOT NULL,
    created_at TIMESTAMPTZ NOT NULL DEFAULT now()
);
