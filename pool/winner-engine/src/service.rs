//! Production wiring helpers for pool integration (TASK-MINING-018).

use block_assembler::PublishedJobSnapshot;
use rigor_gbt::{record::unix_now, TemplateRecord};

use crate::authorize::{authorize_mint, replay_draw_from_candidate_set};
use crate::candidate::{extract_parent_draw_candidates, validate_convergence_membership};
use crate::constants::RES_PROBABILITY_TARGET;
use crate::draw::verify_replay_against_node;
use crate::error::{WinnerError, WinnerResult};
use crate::pg_adapter::WinnerPgBackend;
use crate::persist::{
    CandidateRepository, MintAuthorizationRepository, WinnerRepository,
};
use crate::reorg::{invalidate_winner, revoke_authorization, RevocationReason};
use crate::telemetry::WinnerTelemetry;
use crate::types::{
    AuthorizationContext, CandidateBlockRecord, CandidateBlockStatus, CandidateSet,
    DrawReplayResult, MintAuthorizationToken, WinnerJobRecord, WinnerJobStatus,
};

/// Authoritative preview of the draw the block being assembled pays.
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct WinnerPreviewResult {
    pub mint_required: bool,
    pub winner_txid_display: Option<String>,
    /// C over the parent's convergence set.
    pub convergence_root_display: String,
    pub draw_target_prev_nbits: String,
    /// B_{h-1}, the σ seed and the PCORB `bind_hash`.
    pub sigma_seed: [u8; 32],
    /// A-5 R2 payout target declared by the winning convergence.
    pub winner_res_program: Option<[u8; 32]>,
    pub draw: DrawReplayResult,
}

/// Runtime facade over memory or PostgreSQL persistence.
pub struct WinnerRuntime {
    backend: WinnerPgBackend,
}

impl WinnerRuntime {
    pub fn open() -> WinnerResult<Self> {
        Ok(Self {
            backend: WinnerPgBackend::open()?,
        })
    }

    pub fn memory() -> Self {
        Self {
            backend: WinnerPgBackend::memory(),
        }
    }

    pub fn is_postgres(&self) -> bool {
        self.backend.is_postgres()
    }

    /// Authoritative preview — full convergence draw replay (TASK-MINING-018).
    pub fn preview_winner(
        &self,
        template: &TemplateRecord,
        job: &PublishedJobSnapshot,
    ) -> WinnerResult<WinnerPreviewResult> {
        let (set, draw) = self.extract_and_replay(template, job)?;
        let winner_res_program = if draw.winner_index >= 0 {
            set.res_programs.get(draw.winner_index as usize).copied()
        } else {
            None
        };
        Ok(WinnerPreviewResult {
            mint_required: draw.mint_required,
            winner_txid_display: draw.draw_winner_txid_display.clone(),
            // C over the parent's set, which is what the mint proof commits. The job's own
            // coinbase root covers the set this block commits and must not be substituted.
            convergence_root_display: draw.convergence_root_display.clone(),
            draw_target_prev_nbits: template.gbt.prev_nbits.clone(),
            sigma_seed: set.sigma_seed,
            winner_res_program,
            draw,
        })
    }

    /// Persist candidate + provisional winner job on GBT publish.
    pub fn register_published_job(
        &mut self,
        template: &TemplateRecord,
        job: &PublishedJobSnapshot,
    ) -> WinnerResult<i64> {
        let draw = self.replay_for_job(template, job)?;
        WinnerTelemetry::record_preview(job, &draw);
        let candidate = candidate_from_job(template, job, &draw);
        let cand_id = match &mut self.backend {
            WinnerPgBackend::Memory(s) => CandidateRepository::insert_candidate(s, candidate)?,
            #[cfg(feature = "postgres")]
            WinnerPgBackend::Postgres(s) => CandidateRepository::insert_candidate(s, candidate)?,
        };
        let winner_job = WinnerJobRecord {
            id: 0,
            candidate_block_id: cand_id,
            status: WinnerJobStatus::Provisional,
            mint_required: draw.mint_required,
            eligible_count: draw.eligible_count,
            convergence_root_display: draw.convergence_root_display.clone(),
            winner_txid_display: draw.draw_winner_txid_display.clone(),
            preview_audit: draw,
            token_nonce: None,
        };
        match &mut self.backend {
            WinnerPgBackend::Memory(s) => WinnerRepository::insert_winner_job(s, winner_job)?,
            #[cfg(feature = "postgres")]
            WinnerPgBackend::Postgres(s) => WinnerRepository::insert_winner_job(s, winner_job)?,
        };
        Ok(cand_id)
    }

    /// Block-found path: issue mint authorization token when AUTH rules pass.
    pub fn authorize_mint_for_block(
        &mut self,
        template: &TemplateRecord,
        job: &PublishedJobSnapshot,
        treasury_available: bool,
        paused_new_jobs: bool,
    ) -> WinnerResult<Option<MintAuthorizationToken>> {
        let (set, draw) = self.extract_and_replay(template, job)?;
        if !draw.mint_required {
            return Ok(None);
        }

        let candidate_block_id = self.resolve_candidate_id(job)?;
        if let Some(job_rec) = self.get_winner_job(candidate_block_id)? {
            self.update_winner_status(job_rec.id, WinnerJobStatus::Validated)?;
        }

        let node_draw_convergence_root = set.convergence_root;
        let ctx = AuthorizationContext {
            draw: draw.clone(),
            candidate_set: set,
            candidate_block: candidate_from_job(template, job, &draw),
            node_draw_convergence_root,
            template_bits: template.gbt.bits.clone(),
            template_prev_nbits: template.gbt.prev_nbits.clone(),
            current_tip: job.tip_hash.0.clone(),
            paused_new_jobs,
            treasury_available,
            now_unix: unix_now(),
            consumed_token_nonces: vec![],
        };

        let token = authorize_mint(&ctx)?;
        WinnerTelemetry::record_authorize(job, &draw);
        self.save_token(token.clone())?;
        if let Some(job_rec) = self.get_winner_job(candidate_block_id)? {
            self.update_winner_status(job_rec.id, WinnerJobStatus::Authorized)?;
        }
        Ok(Some(token))
    }

    /// Reorg / tip change: invalidate winner and revoke authorization.
    pub fn invalidate_candidate(
        &mut self,
        candidate_block_id: i64,
        reason: RevocationReason,
    ) -> WinnerResult<()> {
        if let Some(mut job) = self.get_winner_job(candidate_block_id)? {
            invalidate_winner(&mut job, reason.clone());
            self.update_winner_status(job.id, job.status)?;
        }
        if let Some(mut token) = self.get_token_by_candidate(candidate_block_id)? {
            revoke_authorization(&mut token, reason)?;
            self.save_token(token)?;
        }
        self.revoke_tokens_for_candidate(candidate_block_id)?;
        self.update_candidate_status(candidate_block_id, CandidateBlockStatus::Orphaned)?;
        Ok(())
    }

    pub fn invalidate_job(
        &mut self,
        job: &PublishedJobSnapshot,
        reason: RevocationReason,
    ) -> WinnerResult<()> {
        let candidate_block_id = self.resolve_candidate_id(job)?;
        if let Some(winner_job) = self.get_winner_job(candidate_block_id)? {
            let draw = winner_job.preview_audit.clone();
            WinnerTelemetry::record_invalidate(job, &draw, &reason);
            WinnerTelemetry::record_revoke(job, &draw, &reason);
        }
        self.invalidate_candidate(candidate_block_id, reason)
    }

    fn resolve_candidate_id(&self, job: &PublishedJobSnapshot) -> WinnerResult<i64> {
        match &self.backend {
            WinnerPgBackend::Memory(s) => {
                if let Ok(Some(c)) = CandidateRepository::get(s, job.candidate_block_id.0 as i64)
                {
                    if c.job_id == job.job_id.0 {
                        return Ok(c.id);
                    }
                }
                for c in CandidateRepository::list_active(s)? {
                    if c.job_id == job.job_id.0 {
                        return Ok(c.id);
                    }
                }
                Err(WinnerError::Persist(format!(
                    "candidate not found for job {}",
                    job.job_id.0
                )))
            }
            #[cfg(feature = "postgres")]
            WinnerPgBackend::Postgres(s) => {
                if let Ok(Some(c)) = CandidateRepository::get(s, job.candidate_block_id.0 as i64)
                {
                    if c.job_id == job.job_id.0 {
                        return Ok(c.id);
                    }
                }
                for c in CandidateRepository::list_active(s)? {
                    if c.job_id == job.job_id.0 {
                        return Ok(c.id);
                    }
                }
                Err(WinnerError::Persist(format!(
                    "candidate not found for job {}",
                    job.job_id.0
                )))
            }
        }
    }

    fn replay_for_job(
        &self,
        template: &TemplateRecord,
        job: &PublishedJobSnapshot,
    ) -> WinnerResult<DrawReplayResult> {
        let (_set, draw) = self.extract_and_replay(template, job)?;
        Ok(draw)
    }

    /// Resolve the draw this job pays and independently replay it.
    ///
    /// The node's answer is taken as the candidate set, but every σ, eligibility flag and the
    /// winner are recomputed locally and compared. A divergence means the pool and the node
    /// disagree about consensus, so the run stops rather than minting on either answer.
    fn extract_and_replay(
        &self,
        template: &TemplateRecord,
        job: &PublishedJobSnapshot,
    ) -> WinnerResult<(CandidateSet, DrawReplayResult)> {
        validate_convergence_membership(template)?;
        let set = extract_parent_draw_candidates(template)?;
        let draw = replay_draw_from_candidate_set(&set, RES_PROBABILITY_TARGET);

        let node_draw = template
            .gbt
            .convergence_draw
            .as_ref()
            .expect("extract_parent_draw_candidates rejects a template without one");
        verify_replay_against_node(node_draw, &draw)?;

        // The block still commits its own convergence set, and the assembler's C for it must
        // match what the job published. Unrelated to the draw being paid.
        let assembler_root =
            block_assembler::compute_convergence_root(&template.gbt.convergence_txids);
        let assembler_root_hex = block_assembler::native_to_hash_hex(&assembler_root);
        if assembler_root_hex != job.coinbase.convergence_root_hex {
            return Err(WinnerError::Validation(
                "convergence_root mismatch between template and published job".into(),
            ));
        }
        Ok((set, draw))
    }

    fn get_winner_job(&self, candidate_block_id: i64) -> WinnerResult<Option<WinnerJobRecord>> {
        match &self.backend {
            WinnerPgBackend::Memory(s) => {
                WinnerRepository::get_by_candidate_block(s, candidate_block_id)
            }
            #[cfg(feature = "postgres")]
            WinnerPgBackend::Postgres(s) => {
                WinnerRepository::get_by_candidate_block(s, candidate_block_id)
            }
        }
    }

    fn update_winner_status(&mut self, id: i64, status: WinnerJobStatus) -> WinnerResult<()> {
        match &mut self.backend {
            WinnerPgBackend::Memory(s) => WinnerRepository::update_status(s, id, status),
            #[cfg(feature = "postgres")]
            WinnerPgBackend::Postgres(s) => WinnerRepository::update_status(s, id, status),
        }
    }

    fn save_token(&mut self, token: MintAuthorizationToken) -> WinnerResult<()> {
        match &mut self.backend {
            WinnerPgBackend::Memory(s) => MintAuthorizationRepository::save_token(s, token),
            #[cfg(feature = "postgres")]
            WinnerPgBackend::Postgres(s) => MintAuthorizationRepository::save_token(s, token),
        }
    }

    fn get_token_by_candidate(
        &self,
        candidate_block_id: i64,
    ) -> WinnerResult<Option<MintAuthorizationToken>> {
        match &self.backend {
            WinnerPgBackend::Memory(s) => {
                MintAuthorizationRepository::get_token_by_candidate_block(s, candidate_block_id)
            }
            #[cfg(feature = "postgres")]
            WinnerPgBackend::Postgres(s) => {
                MintAuthorizationRepository::get_token_by_candidate_block(s, candidate_block_id)
            }
        }
    }

    fn revoke_tokens_for_candidate(&mut self, candidate_block_id: i64) -> WinnerResult<()> {
        match &mut self.backend {
            WinnerPgBackend::Memory(s) => {
                MintAuthorizationRepository::revoke_by_candidate_block(s, candidate_block_id)?;
            }
            #[cfg(feature = "postgres")]
            WinnerPgBackend::Postgres(s) => {
                MintAuthorizationRepository::revoke_by_candidate_block(s, candidate_block_id)?;
            }
        }
        Ok(())
    }

    fn update_candidate_status(
        &mut self,
        id: i64,
        status: CandidateBlockStatus,
    ) -> WinnerResult<()> {
        match &mut self.backend {
            WinnerPgBackend::Memory(s) => CandidateRepository::update_status(s, id, status),
            #[cfg(feature = "postgres")]
            WinnerPgBackend::Postgres(s) => CandidateRepository::update_status(s, id, status),
        }
    }
}

fn candidate_from_job(
    _template: &TemplateRecord,
    job: &PublishedJobSnapshot,
    draw: &DrawReplayResult,
) -> CandidateBlockRecord {
    CandidateBlockRecord {
        id: job.candidate_block_id.0 as i64,
        job_id: job.job_id.0.clone(),
        record_id: job.record_id,
        prev_block_hash: job.previousblockhash.clone(),
        tip_hash: job.tip_hash.0.clone(),
        height: job.height,
        convergence_root_display: draw.convergence_root_display.clone(),
        mint_required: draw.mint_required,
        status: CandidateBlockStatus::Active,
        body_mint_reserved: job.body_mint.reserved,
    }
}
