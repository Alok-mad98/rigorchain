//! Thread-safe Winner Engine bridge for Stratum (TASK-MINING-018).

use std::sync::{Arc, Mutex};

use block_assembler::PublishedJobSnapshot;
use rigor_gbt::TemplateRecord;
use winner_engine::{MintAuthorizationToken, RevocationReason, WinnerPreviewResult, WinnerRuntime};

use crate::error::{StratumError, StratumResult};
use crate::winner::WinnerPreview;

pub struct SharedWinnerService {
    inner: Arc<Mutex<WinnerRuntime>>,
}

impl SharedWinnerService {
    pub fn memory() -> Self {
        Self {
            inner: Arc::new(Mutex::new(WinnerRuntime::memory())),
        }
    }

    pub fn open() -> StratumResult<Self> {
        let runtime = WinnerRuntime::open()
            .map_err(|e| StratumError::Internal(format!("winner runtime: {e}")))?;
        Ok(Self {
            inner: Arc::new(Mutex::new(runtime)),
        })
    }

    pub fn is_postgres(&self) -> bool {
        self.inner
            .lock()
            .map(|r| r.is_postgres())
            .unwrap_or(false)
    }

    pub fn preview_winner(
        &self,
        template: &TemplateRecord,
        job: &PublishedJobSnapshot,
    ) -> StratumResult<WinnerPreview> {
        let runtime = self
            .inner
            .lock()
            .map_err(|e| StratumError::Internal(format!("winner lock: {e}")))?;
        let preview = runtime
            .preview_winner(template, job)
            .map_err(|e| StratumError::Validation(e.to_string()))?;
        Ok(map_preview(preview))
    }

    pub fn register_published_job(
        &self,
        template: &TemplateRecord,
        job: &PublishedJobSnapshot,
    ) -> StratumResult<()> {
        let mut runtime = self
            .inner
            .lock()
            .map_err(|e| StratumError::Internal(format!("winner lock: {e}")))?;
        runtime
            .register_published_job(template, job)
            .map_err(|e| StratumError::Internal(format!("winner persist: {e}")))?;
        Ok(())
    }

    pub fn authorize_block_found(
        &self,
        template: &TemplateRecord,
        job: &PublishedJobSnapshot,
    ) -> StratumResult<Option<MintAuthorizationToken>> {
        let mut runtime = self
            .inner
            .lock()
            .map_err(|e| StratumError::Internal(format!("winner lock: {e}")))?;
        runtime
            .authorize_mint_for_block(template, job, true, false)
            .map_err(|e| StratumError::Validation(format!("mint authorization denied: {e}")))
    }

    pub fn invalidate_jobs(
        &self,
        jobs: &[Arc<PublishedJobSnapshot>],
        reason: RevocationReason,
    ) -> StratumResult<()> {
        let mut runtime = self
            .inner
            .lock()
            .map_err(|e| StratumError::Internal(format!("winner lock: {e}")))?;
        for job in jobs {
            runtime
                .invalidate_job(job.as_ref(), reason.clone())
                .map_err(|e| StratumError::Internal(format!("winner invalidate: {e}")))?;
        }
        Ok(())
    }
}

fn map_preview(p: WinnerPreviewResult) -> WinnerPreview {
    WinnerPreview {
        mint_required: p.mint_required,
        winner_txid: p.winner_txid_display,
        convergence_root_hex: p.convergence_root_display,
        draw_target_prev_nbits: p.draw_target_prev_nbits,
    }
}
