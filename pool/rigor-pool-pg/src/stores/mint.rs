#[cfg(feature = "postgres")]
use crate::error::PgResult;
#[cfg(feature = "postgres")]
use crate::pool::PgPool;

/// Treasury UTXO reservation input.
#[derive(Clone, Debug)]
pub struct TreasuryReserveInput {
    pub txid: String,
    pub vout: i32,
    pub amount_atoms: i64,
    pub script_pubkey_hex: String,
    pub confirmations: i32,
}

/// Atomic mint_jobs + treasury_utxos writer (§5.4).
#[cfg(feature = "postgres")]
pub struct PgMintStore {
    pool: PgPool,
}

#[cfg(feature = "postgres")]
impl PgMintStore {
    pub fn new(pool: PgPool) -> Self {
        Self { pool }
    }

    pub fn reserve_mint_with_utxo(
        &self,
        stratum_job_id: &str,
        winner_job_id: Option<i64>,
        body_mint_txid: &str,
        body_mint_hex_enc: &[u8],
        sigma_digest_hex: &str,
        utxo: &TreasuryReserveInput,
    ) -> PgResult<i64> {
        let mut client = self.pool.get()?;
        let mut tx = client.transaction()?;
        let mint_row = tx.query_one(
            "INSERT INTO mint_jobs (winner_job_id, stratum_job_id, body_mint_txid, body_mint_hex_enc, \
             sigma_digest_hex, status, built_at) \
             VALUES ($1, $2, $3, $4, $5, 'BUILT', now()) RETURNING id",
            &[
                &winner_job_id,
                &stratum_job_id,
                &body_mint_txid,
                &body_mint_hex_enc,
                &sigma_digest_hex,
            ],
        )?;
        let mint_job_id: i64 = mint_row.get(0);
        tx.execute(
            "INSERT INTO treasury_utxos (txid, vout, mint_job_id, amount_atoms, script_pubkey_hex, \
             status, confirmations_at_reserve, reserved_at) \
             VALUES ($1, $2, $3, $4, $5, 'RESERVED', $6, now()) \
             ON CONFLICT (txid, vout) DO UPDATE SET \
             mint_job_id = EXCLUDED.mint_job_id, status = 'RESERVED', reserved_at = now()",
            &[
                &utxo.txid,
                &utxo.vout,
                &mint_job_id,
                &utxo.amount_atoms,
                &utxo.script_pubkey_hex,
                &utxo.confirmations,
            ],
        )?;
        tx.commit()?;
        self.pool.metrics.record_pg_insert();
        log::info!(
            "pg: atomic mint+treasury job_id={} mint_job_id={}",
            stratum_job_id,
            mint_job_id
        );
        Ok(mint_job_id)
    }

    pub fn revoke_inflight_mints(&self) -> PgResult<u64> {
        let mut client = self.pool.get()?;
        let mut tx = client.transaction()?;
        let n_mint = tx.execute(
            "UPDATE mint_jobs SET status = 'REVOKED' WHERE status IN ('BUILT', 'SUBMITTED')",
            &[],
        )?;
        let n_utxo = tx.execute(
            "UPDATE treasury_utxos SET status = 'RELEASED', mint_job_id = NULL \
             WHERE status = 'RESERVED'",
            &[],
        )?;
        tx.commit()?;
        self.pool.metrics.record_pg_reorg();
        log::info!(
            "pg: revoke inflight mints mint_jobs={} treasury_utxos={}",
            n_mint,
            n_utxo
        );
        Ok(n_mint)
    }

    pub fn list_inflight_job_ids(&self) -> PgResult<Vec<String>> {
        let mut client = self.pool.get()?;
        let rows = client.query(
            "SELECT stratum_job_id FROM mint_jobs WHERE status IN ('BUILT', 'SUBMITTED')",
            &[],
        )?;
        Ok(rows.iter().map(|r| r.get::<_, String>(0)).collect())
    }
}

#[cfg(not(feature = "postgres"))]
pub struct PgMintStore;
