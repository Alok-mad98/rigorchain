//! PostgreSQL adapter for WinnerEngine persistence (TASK-MINING-017).

use crate::error::WinnerResult;
use crate::persist::{CandidateRepository, InMemoryWinnerStore, WinnerRepository};
#[cfg(feature = "postgres")]
use crate::persist::PgWinnerStore;
use crate::types::{CandidateBlockRecord, DrawReplayResult, WinnerJobRecord, WinnerJobStatus};

/// Winner persistence backend selector.
pub enum WinnerPgBackend {
    Memory(InMemoryWinnerStore),
    #[cfg(feature = "postgres")]
    Postgres(PgWinnerStore),
}

impl WinnerPgBackend {
    pub fn open() -> WinnerResult<Self> {
        let backend = rigor_pool_pg::PoolBackend::open()
            .map_err(|e| crate::error::WinnerError::Persist(e.to_string()))?;
        #[cfg(feature = "postgres")]
        if backend.is_postgres() {
            let pool = backend.pool_cloned().map_err(|e| {
                crate::error::WinnerError::Persist(e.to_string())
            })?;
            return Ok(Self::Postgres(PgWinnerStore::new(pool)));
        }
        let _ = backend;
        Ok(Self::Memory(InMemoryWinnerStore::new()))
    }

    pub fn memory() -> Self {
        Self::Memory(InMemoryWinnerStore::new())
    }

    pub fn is_postgres(&self) -> bool {
        match self {
            Self::Memory(_) => false,
            #[cfg(feature = "postgres")]
            Self::Postgres(_) => true,
        }
    }

    pub fn metrics(&self) -> rigor_pool_pg::MetricsSnapshot {
        #[cfg(feature = "postgres")]
        if let Self::Postgres(s) = self {
            return s.metrics_snapshot();
        }
        rigor_pool_pg::PoolBackend::memory().metrics_snapshot()
    }
}

pub fn persist_golden_draw(
    backend: &mut WinnerPgBackend,
    candidate: CandidateBlockRecord,
    draw: &DrawReplayResult,
) -> WinnerResult<(i64, i64)> {
    let cand_id = match backend {
        WinnerPgBackend::Memory(s) => CandidateRepository::insert_candidate(s, candidate)?,
        #[cfg(feature = "postgres")]
        WinnerPgBackend::Postgres(s) => CandidateRepository::insert_candidate(s, candidate)?,
    };
    let job = WinnerJobRecord {
        id: 0,
        candidate_block_id: cand_id,
        status: WinnerJobStatus::Provisional,
        mint_required: draw.mint_required,
        eligible_count: draw.eligible_count,
        convergence_root_display: draw.convergence_root_display.clone(),
        winner_txid_display: draw.draw_winner_txid_display.clone(),
        preview_audit: draw.clone(),
        token_nonce: None,
    };
    let winner_id = match backend {
        WinnerPgBackend::Memory(s) => WinnerRepository::insert_winner_job(s, job)?,
        #[cfg(feature = "postgres")]
        WinnerPgBackend::Postgres(s) => WinnerRepository::insert_winner_job(s, job)?,
    };
    Ok((cand_id, winner_id))
}

pub fn get_winner_by_candidate(
    backend: &WinnerPgBackend,
    candidate_block_id: i64,
) -> WinnerResult<Option<WinnerJobRecord>> {
    match backend {
        WinnerPgBackend::Memory(s) => WinnerRepository::get_by_candidate_block(s, candidate_block_id),
        #[cfg(feature = "postgres")]
        WinnerPgBackend::Postgres(s) => {
            WinnerRepository::get_by_candidate_block(s, candidate_block_id)
        }
    }
}
