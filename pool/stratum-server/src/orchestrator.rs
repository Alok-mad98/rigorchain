use std::sync::Arc;

use block_assembler::{JobPublisher, JobSnapshotApi};
use rigor_gbt::{TemplateCache, TemplateCacheApi, TipHash};
use share_validator::{
    BlockSubmission, ShareSubmission, ShareValidator, ValidationErrorCode, ValidationResult,
};

use crate::audit::{AuditLog, SharedAuditLog};
use crate::error::{StratumError, StratumResult};
use crate::jobs::{JobAssignmentEngine, StratumJobEnvelope};
use crate::metrics::{PoolMetrics, SharedMetrics};
use crate::payout::SharedPayoutEngine;
use crate::protocol::{self, StratumResponse};
use crate::res_mint::{validate_mint_integrity, MintBlockContext, MintEngine};
use crate::session::{MinerSession, SessionManager};
use crate::telemetry::PoolJobTelemetry;
use crate::winner::confirm_winner_for_block;
use crate::winner::{self, WinnerPreview};
use crate::winner_service::SharedWinnerService;
use mint_engine::submit::{BlockSubmitter, UnconfiguredBlockSubmitter};
use mint_engine::telemetry::MintTelemetry;
use winner_engine::telemetry::WinnerTelemetry;
use winner_engine::RevocationReason;

pub struct StratumOrchestrator {
    pub sessions: Arc<SessionManager>,
    pub jobs: JobAssignmentEngine,
    pub validator: ShareValidator,
    pub publisher: JobPublisher,
    pub templates: Arc<TemplateCache>,
    pub payout: SharedPayoutEngine,
    pub metrics: SharedMetrics,
    pub audit: SharedAuditLog,
    pub mint_engine: Arc<MintEngine>,
    pub winner: Arc<SharedWinnerService>,
    /// C-5: authoritative sink for found blocks. Miner credit is booked only after
    /// this reports success, and it fails closed when unconfigured.
    pub submitter: Arc<dyn BlockSubmitter>,
}

impl StratumOrchestrator {
    pub fn new(
        templates: Arc<TemplateCache>,
        jobs: Arc<block_assembler::JobCache>,
        payout: SharedPayoutEngine,
    ) -> Self {
        Self::with_services(
            templates,
            jobs,
            payout,
            Arc::new(MintEngine::new(mint_engine::MintEngineConfig::devtest())),
            Arc::new(SharedWinnerService::memory()),
        )
    }

    pub fn with_mint_engine(
        templates: Arc<TemplateCache>,
        jobs: Arc<block_assembler::JobCache>,
        payout: SharedPayoutEngine,
        mint_engine: Arc<MintEngine>,
    ) -> Self {
        Self::with_services(
            templates,
            jobs,
            payout,
            mint_engine,
            Arc::new(SharedWinnerService::memory()),
        )
    }

    pub fn with_services(
        templates: Arc<TemplateCache>,
        jobs: Arc<block_assembler::JobCache>,
        payout: SharedPayoutEngine,
        mint_engine: Arc<MintEngine>,
        winner: Arc<SharedWinnerService>,
    ) -> Self {
        let publisher = JobPublisher::with_defaults(jobs.clone());
        let validator = ShareValidator::with_defaults(jobs.clone(), templates.clone());
        Self {
            sessions: Arc::new(SessionManager::new("x")),
            jobs: JobAssignmentEngine::new(JobSnapshotApi::new(jobs), TemplateCacheApi::new(templates.clone())),
            validator,
            publisher,
            templates,
            payout,
            metrics: Arc::new(PoolMetrics::default()),
            audit: Arc::new(AuditLog::new(10_000)),
            mint_engine,
            winner,
            submitter: Arc::new(UnconfiguredBlockSubmitter),
        }
    }

    /// Install the authenticator / vardiff-configured session manager (H-5a/H-5d).
    pub fn with_sessions(mut self, sessions: Arc<SessionManager>) -> Self {
        self.sessions = sessions;
        self
    }

    /// Install a validated coinbase publisher (C-6b).
    pub fn with_publisher(mut self, publisher: JobPublisher) -> Self {
        self.publisher = publisher;
        self
    }

    /// Install the block submission sink (C-5).
    pub fn with_submitter(mut self, submitter: Arc<dyn BlockSubmitter>) -> Self {
        self.submitter = submitter;
        self
    }

    pub fn handle_request(
        &self,
        session_id: u64,
        line: &str,
    ) -> StratumResult<Vec<String>> {
        let req = protocol::parse_request(line)?;
        self.audit.record(
            "stratum.rpc",
            format!("session={session_id} method={}", req.method),
        );

        match req.method.as_str() {
            "mining.subscribe" => self.handle_subscribe(session_id, &req),
            "mining.authorize" => self.handle_authorize(session_id, &req),
            "mining.submit" => self.handle_submit(session_id, &req),
            other => Err(StratumError::Protocol(format!("unknown method {other}"))),
        }
    }

    pub fn publish_jobs_from_active_template(&self) -> StratumResult<Vec<StratumJobEnvelope>> {
        let active_tpl = self
            .templates
            .get_active_template()
            .map_err(|e| StratumError::Internal(e.to_string()))?
            .ok_or_else(|| StratumError::Job("no active template".into()))?;

        let job = self
            .publisher
            .publish_from_template(&active_tpl)
            .map_err(|e| StratumError::Internal(e.to_string()))?;

        let preview = match self.winner.register_published_job(&active_tpl, job.as_ref()) {
            Ok(()) => match winner::preview_winner(&self.winner, active_tpl.as_ref(), job.as_ref())
            {
                Ok(p) => p,
                Err(e) => {
                    self.audit
                        .record("winner.preview.degraded", e.to_string());
                    WinnerTelemetry::record_preview_degraded(
                        job.as_ref(),
                        active_tpl.as_ref(),
                        &e.to_string(),
                    );
                    degraded_winner_preview(job.as_ref(), active_tpl.as_ref())
                }
            },
            Err(e) => {
                self.audit
                    .record("winner.register.degraded", e.to_string());
                WinnerTelemetry::record_preview_degraded(
                    job.as_ref(),
                    active_tpl.as_ref(),
                    &e.to_string(),
                );
                degraded_winner_preview(job.as_ref(), active_tpl.as_ref())
            }
        };

        self.audit.record(
            "job.publish",
            format!(
                "job_id={} record_id={} mint_required={} winner={:?}",
                job.job_id.0, job.record_id, preview.mint_required, preview.winner_txid
            ),
        );
        self.metrics.jobs_notified.fetch_add(1, std::sync::atomic::Ordering::Relaxed);

        PoolJobTelemetry::record_job_published(
            active_tpl.as_ref(),
            job.as_ref(),
            true,
            preview.winner_txid.as_deref(),
            preview.mint_required,
        );

        MintTelemetry::record_publish_observation(
            job.as_ref(),
            preview.mint_required,
            preview.winner_txid.as_deref(),
        );

        let envelope = self.jobs.envelope_from_job_with_preview(
            job.as_ref(),
            preview.winner_txid.clone(),
            preview.mint_required,
        )?;
        Ok(vec![envelope])
    }

    pub fn handle_tip_change(&self, new_tip: TipHash) -> StratumResult<()> {
        let invalidated = self
            .publisher
            .on_tip_changed(new_tip.clone())
            .map_err(|e| StratumError::Internal(e.to_string()))?;
        self.templates
            .handle_tip_changed(new_tip)
            .map_err(|e| StratumError::Internal(e.to_string()))?;

        let orphaned_jobs: Vec<std::sync::Arc<block_assembler::PublishedJobSnapshot>> = invalidated
            .iter()
            .filter_map(|jid| {
                self.publisher
                    .cache()
                    .get_job_by_id(jid)
                    .ok()
                    .flatten()
            })
            .collect();

        if let Ok(active_tpl) = self.templates.get_active_template() {
            for job in &orphaned_jobs {
                PoolJobTelemetry::record_job_invalidated(job.as_ref(), active_tpl.as_deref());
            }
        } else {
            for job in &orphaned_jobs {
                PoolJobTelemetry::record_job_invalidated(job.as_ref(), None);
            }
        }

        if let Err(e) = self
            .winner
            .invalidate_jobs(&orphaned_jobs, RevocationReason::Reorg)
        {
            self.audit.record("winner.invalidate.error", e.to_string());
        }

        // REORG-01: reverse FINALIZED PPLNS rounds for orphaned jobs, then orphan shares.
        let _reverse = self.payout.reverse_finalized_for_jobs(&invalidated);
        self.payout.orphan_round_for_jobs(&invalidated);
        let job_ids: Vec<String> = invalidated.iter().map(|j| j.0.clone()).collect();
        if let Ok(revoked) = self.mint_engine.revoke_for_jobs(&job_ids) {
            for rec in revoked {
                self.audit.record(
                    "res.mint.revoked",
                    format!("job={} txid={}", rec.mint_job.job_id, rec.artifact.body_mint_txid),
                );
            }
        }
        self.metrics.reorg_events.fetch_add(1, std::sync::atomic::Ordering::Relaxed);
        self.audit.record("reorg", "tip changed; jobs invalidated");
        Ok(())
    }

    fn handle_subscribe(&self, session_id: u64, req: &protocol::StratumRequest) -> StratumResult<Vec<String>> {
        let params = protocol::params_as_strings(&req.params)?;
        let agent = params.first().map(|s| s.as_str()).unwrap_or("");
        if !protocol::requires_opushash_agent(agent) {
            return Ok(vec![protocol::serialize_response(&StratumResponse::err(
                req.id.clone(),
                20,
                "agent must declare opushash capability",
            ))?]);
        }

        let session = self.sessions.subscribe(session_id, agent, 4);
        let result = serde_json::json!([
            [
                ["mining.set_difficulty", session.subscription_id.clone()],
                ["mining.notify", session.subscription_id.clone()]
            ],
            hex::encode(&session.extranonce1),
            session.extranonce2_size
        ]);
        let mut out = vec![protocol::serialize_response(&StratumResponse::ok(req.id.clone(), result))?];

        if let Ok(jobs) = self.jobs.active_jobs() {
            for job in jobs {
                out.push(serde_json::to_string(&job.notify).map_err(|e| StratumError::Protocol(e.to_string()))? + "\n");
            }
        }
        Ok(out)
    }

    fn handle_authorize(&self, session_id: u64, req: &protocol::StratumRequest) -> StratumResult<Vec<String>> {
        let params = protocol::params_as_strings(&req.params)?;
        let worker = params.first().ok_or_else(|| StratumError::Protocol("worker required".into()))?;
        let pass = params.get(1).map(|s| s.as_str()).unwrap_or("");
        if let Err(e) = self.sessions.authorize(session_id, worker, pass) {
            // H-5a: never leak whether the worker exists or the password was wrong.
            self.audit
                .record("auth.failed", format!("session={session_id} reason={e}"));
            self.metrics
                .auth_failures
                .fetch_add(1, std::sync::atomic::Ordering::Relaxed);
            return Ok(vec![protocol::serialize_response(&StratumResponse::err(
                req.id.clone(),
                24,
                "unauthorized worker",
            ))?]);
        }
        self.audit
            .record("auth.ok", format!("session={session_id} worker={worker}"));

        let mut out = vec![protocol::serialize_response(&StratumResponse::ok(
            req.id.clone(),
            serde_json::json!(true),
        ))?];
        // H-5d: tell the miner its actual share difficulty instead of silently
        // leaving it equal to the network target.
        if let Some(session) = self.sessions.get(session_id) {
            out.push(protocol::serialize_set_difficulty(session.difficulty)?);
        }
        Ok(out)
    }

    fn handle_submit(&self, session_id: u64, req: &protocol::StratumRequest) -> StratumResult<Vec<String>> {
        let session = self
            .sessions
            .get(session_id)
            .ok_or_else(|| StratumError::Unauthorized("unknown session".into()))?;

        // H-5b: the credited identity comes from the authenticated session, never
        // from the request parameters. Previously `params[0]` was trusted, so any
        // authorized connection could book shares to any other worker's balance.
        let worker = session
            .authorized_worker()
            .ok_or_else(|| StratumError::Unauthorized("not authorized".into()))
            .map(|w| w.to_string());
        let worker = match worker {
            Ok(w) => w,
            Err(_) => {
                return Ok(vec![protocol::serialize_response(&StratumResponse::err(
                    req.id.clone(),
                    24,
                    "not authorized",
                ))?])
            }
        };

        let params = protocol::params_as_strings(&req.params)?;
        // params[0] is the client-claimed worker name. It is only used to detect
        // and reject impersonation attempts; it is never used for accounting.
        if let Some(claimed) = params.first() {
            if claimed != &worker {
                self.audit.record(
                    "share.impersonation_rejected",
                    format!("session={session_id} authenticated={worker} claimed={claimed}"),
                );
                self.metrics
                    .impersonation_attempts
                    .fetch_add(1, std::sync::atomic::Ordering::Relaxed);
                return Ok(vec![protocol::serialize_response(&StratumResponse::err(
                    req.id.clone(),
                    24,
                    "worker does not match authenticated session identity",
                ))?]);
            }
        }
        let worker = worker.as_str();
        let job_id = params.get(1).ok_or_else(|| StratumError::Protocol("job_id required".into()))?;
        let extranonce2_hex = params.get(2).ok_or_else(|| StratumError::Protocol("extranonce2 required".into()))?;
        let ntime = u32::from_str_radix(params.get(3).map(|s| s.as_str()).unwrap_or("0"), 16)
            .map_err(|e| StratumError::Protocol(e.to_string()))?;
        let nonce = u32::from_str_radix(params.get(4).map(|s| s.as_str()).unwrap_or("0"), 16)
            .map_err(|e| StratumError::Protocol(e.to_string()))?;

        let envelope = self.jobs.envelope_for_job_id(job_id)?;
        let share_target = self
            .sessions
            .share_target_for_session(session_id, &envelope.block_target)
            .unwrap_or([0xff; 32]);

        let share_id = format!("{worker}:{job_id}:{extranonce2_hex}:{ntime:08x}:{nonce:08x}");
        let submission = ShareSubmission {
            share_id: share_id.clone(),
            job_id: block_assembler::JobId(job_id.clone()),
            record_id: Some(envelope.template_id),
            extranonce1: session.extranonce1.clone(),
            extranonce2: hex::decode(extranonce2_hex)
                .map_err(|e| StratumError::Protocol(e.to_string()))?,
            ntime,
            nonce,
            share_target,
        };

        let result = self.validator.validate_share(submission.clone());
        let response =
            self.process_validation_result(&session, worker, &result, &envelope, &submission)?;
        let mut out = vec![response];
        // H-5d: retarget this connection when its share rate drifts from target.
        if result.accepted {
            if let Some(next) = self
                .sessions
                .observe_accepted_share(session_id, rigor_gbt::record::unix_now())
            {
                self.audit.record(
                    "vardiff.retarget",
                    format!("session={session_id} worker={worker} difficulty={next}"),
                );
                out.push(protocol::serialize_set_difficulty(next)?);
            }
        }
        Ok(out)
    }

    fn process_validation_result(
        &self,
        session: &MinerSession,
        worker: &str,
        result: &ValidationResult,
        envelope: &StratumJobEnvelope,
        share: &ShareSubmission,
    ) -> StratumResult<String> {
        if !result.accepted {
            self.metrics
                .shares_rejected
                .fetch_add(1, std::sync::atomic::Ordering::Relaxed);
            self.audit.record(
                "share.rejected",
                format!(
                    "worker={worker} code={:?} msg={:?}",
                    result.error_code, result.message
                ),
            );
            let code = match result.error_code {
                Some(ValidationErrorCode::ErrReplayShare) => 21,
                Some(ValidationErrorCode::ErrStaleJob) => 21,
                Some(ValidationErrorCode::ErrInvalidOpouhash) => 23,
                _ => 20,
            };
            return protocol::serialize_response(&StratumResponse::err(
                None,
                code,
                result.message.as_deref().unwrap_or("rejected"),
            ));
        }

        self.metrics
            .shares_accepted
            .fetch_add(1, std::sync::atomic::Ordering::Relaxed);
        self.payout.credit_share(
            worker,
            &block_assembler::JobId(envelope.job_id.clone()),
            result.share_id.as_deref().unwrap_or(""),
            session.difficulty,
        );
        self.audit.record("share.accepted", format!("worker={worker} job={}", envelope.job_id));

        if result.is_block_candidate {
            self.handle_block_candidate(worker, envelope, result, share)?;
        }

        protocol::serialize_response(&StratumResponse::ok(None, serde_json::json!(true)))
    }

    /// Handle a share that solved the network target.
    ///
    /// C-5: block submission is decoupled from RES minting. The block is assembled
    /// and offered to the node whether or not a mint was required or succeeded, and
    /// the PPLNS round is finalized **only** after the node accepts it. Previously
    /// `record_block_found` ran unconditionally while the only `submit_block` call
    /// sat behind `mint_for_winner`'s `if !preview.mint_required { return }` guard,
    /// so the pool booked a debt to miners for blocks the network never saw.
    fn handle_block_candidate(
        &self,
        worker: &str,
        envelope: &StratumJobEnvelope,
        result: &ValidationResult,
        share: &ShareSubmission,
    ) -> StratumResult<()> {
        self.metrics
            .blocks_found
            .fetch_add(1, std::sync::atomic::Ordering::Relaxed);

        let template = self.jobs.template_for_job(envelope.template_id)?;
        let job_id = block_assembler::JobId(envelope.job_id.clone());
        let job = self
            .publisher
            .cache()
            .get_job_by_id(&job_id)
            .map_err(|e| StratumError::Internal(e.to_string()))?
            .ok_or_else(|| StratumError::Job("job missing for block candidate".into()))?;

        self.audit.record(
            "block.found",
            format!("worker={worker} job={} pow={:?}", envelope.job_id, result.pow_hash),
        );

        let block_ctx = MintBlockContext {
            extranonce1: share.extranonce1.clone(),
            extranonce2: share.extranonce2.clone(),
            ntime: share.ntime,
            nonce: share.nonce,
        };

        // C-5: winner confirmation and RES minting gate the *mint*, never the block.
        // Any failure below leaves `mint_block_hex` empty and we fall through to
        // submitting a mint-less block.
        let mint_block_hex = self.try_mint(worker, &template, job.as_ref(), &block_ctx);

        let block_hex = match mint_block_hex {
            Some(hex) => hex,
            None => {
                match mint_engine::assemble_block_without_mint(&template, job.as_ref(), &block_ctx)
                {
                    Ok(pkg) => pkg.block_hex,
                    Err(e) => {
                        // The job reserved a body-mint slot but no mint could be
                        // produced, so no block satisfying the mined merkle root
                        // exists. Lose the block loudly; credit nothing.
                        self.metrics
                            .blocks_unsubmittable
                            .fetch_add(1, std::sync::atomic::Ordering::Relaxed);
                        self.audit.record(
                            "block.unsubmittable",
                            format!("worker={worker} job={} err={e}", envelope.job_id),
                        );
                        return Err(StratumError::Internal(format!(
                            "C-5: found block for job {} cannot be assembled ({e}); \
                             no miner credit recorded",
                            envelope.job_id
                        )));
                    }
                }
            }
        };

        // Local structural pre-check. Advisory only — the node is authoritative.
        let block_val = self.validator.validate_block(BlockSubmission {
            job_id: job_id.clone(),
            record_id: Some(envelope.template_id),
            block_hex: block_hex.clone(),
            allow_res_body_mint: true,
        });
        if !block_val.accepted {
            self.audit.record(
                "block.prevalidation_failed",
                format!("worker={worker} job={} msg={:?}", envelope.job_id, block_val.message),
            );
        }

        // C-5: the node's answer is the only thing that may unlock miner credit.
        match self.submitter.submit_block(&block_hex) {
            Ok(()) => {
                self.metrics
                    .blocks_submitted
                    .fetch_add(1, std::sync::atomic::Ordering::Relaxed);
                let round_seq = self
                    .payout
                    .record_block_found_for_job(&job_id, template.gbt.coinbasevalue);
                self.audit.record(
                    "block.submitted",
                    format!(
                        "worker={worker} job={} sink={} round_seq={round_seq}",
                        envelope.job_id,
                        self.submitter.describe()
                    ),
                );
                Ok(())
            }
            Err(e) => {
                self.metrics
                    .blocks_submit_failed
                    .fetch_add(1, std::sync::atomic::Ordering::Relaxed);
                self.audit.record(
                    "block.submit_failed",
                    format!(
                        "worker={worker} job={} sink={} err={e}",
                        envelope.job_id,
                        self.submitter.describe()
                    ),
                );
                Err(StratumError::Internal(format!(
                    "C-5: submitblock failed for job {} ({e}); no miner credit recorded",
                    envelope.job_id
                )))
            }
        }
    }

    /// Confirm the draw winner and build the RES body mint if one is authorized.
    ///
    /// Returns the assembled block hex on success, `None` when no mint was required
    /// or the mint could not be produced. Never propagates errors — C-5 requires
    /// that neither a winner-engine fault nor a mint failure abort block submission.
    fn try_mint(
        &self,
        worker: &str,
        template: &rigor_gbt::TemplateRecord,
        job: &block_assembler::PublishedJobSnapshot,
        block_ctx: &MintBlockContext,
    ) -> Option<String> {
        if let Err(e) = confirm_winner_for_block(&self.winner, template, job) {
            self.audit.record(
                "winner.confirm.failed",
                format!("worker={worker} job={} err={e}", job.job_id.0),
            );
            return None;
        }

        let token = match self.winner.authorize_block_found(template, job) {
            Ok(Some(t)) => t,
            Ok(None) => {
                self.audit.record(
                    "res.mint.skipped",
                    format!("worker={worker} job={} reason=not_authorized", job.job_id.0),
                );
                return None;
            }
            Err(e) => {
                self.audit.record(
                    "winner.authorize.failed",
                    format!("worker={worker} job={} err={e}", job.job_id.0),
                );
                return None;
            }
        };

        match self
            .mint_engine
            .mint_for_authorized_winner(&token, template, job, block_ctx)
        {
            Ok(Some(outcome)) => {
                if !validate_mint_integrity(&outcome.artifact) {
                    self.audit.record(
                        "res.mint.error",
                        format!("worker={worker} reason=integrity_check_failed"),
                    );
                    return None;
                }
                self.metrics
                    .res_mint_events
                    .fetch_add(1, std::sync::atomic::Ordering::Relaxed);
                self.audit.record(
                    "res.mint",
                    format!(
                        "worker={worker} txid={} amount={} submitted={}",
                        outcome.artifact.body_mint_txid,
                        outcome.artifact.amount_atoms,
                        outcome.submitted
                    ),
                );
                Some(outcome.block.block_hex.clone())
            }
            Ok(None) => {
                self.audit.record(
                    "res.mint.skipped",
                    format!("worker={worker} job={} reason=not_required", job.job_id.0),
                );
                None
            }
            Err(e) => {
                self.audit
                    .record("res.mint.error", format!("worker={worker} err={e}"));
                None
            }
        }
    }
}

fn degraded_winner_preview(
    job: &block_assembler::PublishedJobSnapshot,
    template: &rigor_gbt::TemplateRecord,
) -> WinnerPreview {
    WinnerPreview {
        mint_required: false,
        winner_txid: None,
        convergence_root_hex: job.coinbase.convergence_root_hex.clone(),
        draw_target_prev_nbits: template.gbt.prev_nbits.clone(),
    }
}
