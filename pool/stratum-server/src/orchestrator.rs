use std::sync::atomic::{AtomicBool, Ordering as AtomicOrdering};
use std::sync::Arc;

use block_assembler::{
    assemble_solved_block, JobPublisher, JobSnapshotApi, SolvedBlockContext,
};
use rigor_gbt::{TemplateCache, TemplateCacheApi, TipHash};
use share_validator::{
    BlockSubmission, ShareSubmission, ShareValidator, ValidationErrorCode, ValidationResult,
};

use crate::audit::{AuditLog, SharedAuditLog};
use crate::error::{StratumError, StratumResult};
use crate::jobs::{JobAssignmentEngine, StratumJobEnvelope};
use crate::metrics::{PoolMetrics, SharedMetrics};
use crate::notify_hub::{NotifyHub, SharedNotifyHub};
use crate::payout::SharedPayoutEngine;
use crate::protocol::{self, StratumResponse};
use crate::session::{MinerSession, SessionManager};
use crate::submit::{BlockSubmitter, UnconfiguredBlockSubmitter};
use crate::telemetry::PoolJobTelemetry;
use crate::winner::{self, WinnerPreview};
use crate::winner_service::SharedWinnerService;
use winner_engine::telemetry::WinnerTelemetry;
use winner_engine::RevocationReason;

/// ORU-only Stratum orchestrator (Option A / TASK-POOL-RES-BOUNDARY-IMPLEMENT-003).
///
/// Coordinates GBT → jobs → shares → ORU payout → submitblock.
/// Does **not** construct, authorize, or trigger RES body mints.
pub struct StratumOrchestrator {
    pub sessions: Arc<SessionManager>,
    pub jobs: JobAssignmentEngine,
    pub validator: ShareValidator,
    pub publisher: JobPublisher,
    pub templates: Arc<TemplateCache>,
    pub payout: SharedPayoutEngine,
    pub metrics: SharedMetrics,
    pub audit: SharedAuditLog,
    /// Optional draw preview for operator telemetry only — never gates mint construction.
    pub winner: Arc<SharedWinnerService>,
    /// C-5: authoritative sink for found blocks.
    pub submitter: Arc<dyn BlockSubmitter>,
    /// Pushes fresh `mining.notify` lines to live connections after tip changes.
    pub notify_hub: SharedNotifyHub,
    /// Set after a successful `submitblock` so the GBT poller skips its sleep
    /// and publishes replacement work immediately (cuts stale-share window).
    pub work_refresh: AtomicBool,
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
            Arc::new(SharedWinnerService::memory()),
        )
    }

    pub fn with_services(
        templates: Arc<TemplateCache>,
        jobs: Arc<block_assembler::JobCache>,
        payout: SharedPayoutEngine,
        winner: Arc<SharedWinnerService>,
    ) -> Self {
        let publisher = JobPublisher::with_defaults(jobs.clone());
        let validator = ShareValidator::with_defaults(jobs.clone(), templates.clone());
        Self {
            sessions: Arc::new(SessionManager::new("x")),
            jobs: JobAssignmentEngine::new(
                JobSnapshotApi::new(jobs),
                TemplateCacheApi::new(templates.clone()),
            ),
            validator,
            publisher,
            templates,
            payout,
            metrics: Arc::new(PoolMetrics::default()),
            audit: Arc::new(AuditLog::new(10_000)),
            winner,
            submitter: Arc::new(UnconfiguredBlockSubmitter),
            notify_hub: Arc::new(NotifyHub::new()),
            work_refresh: AtomicBool::new(false),
        }
    }

    pub fn request_work_refresh(&self) {
        self.work_refresh.store(true, AtomicOrdering::SeqCst);
    }

    pub fn take_work_refresh(&self) -> bool {
        self.work_refresh.swap(false, AtomicOrdering::SeqCst)
    }

    pub fn with_sessions(mut self, sessions: Arc<SessionManager>) -> Self {
        self.sessions = sessions;
        self
    }

    pub fn with_publisher(mut self, publisher: JobPublisher) -> Self {
        self.publisher = publisher;
        self
    }

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

        // Draw preview is operator telemetry only. Product surface is always ORU-only:
        // mint_required forced false; no mint authorize / construction.
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
                "job_id={} record_id={} oru_only=true draw_winner={:?}",
                job.job_id.0, job.record_id, preview.winner_txid
            ),
        );
        self.metrics
            .jobs_notified
            .fetch_add(1, std::sync::atomic::Ordering::Relaxed);

        PoolJobTelemetry::record_job_published(
            active_tpl.as_ref(),
            job.as_ref(),
            true,
            preview.winner_txid.as_deref(),
            false, // never advertise mint_required on the mining product surface
        );

        let envelope = self.jobs.envelope_from_job_with_preview(
            job.as_ref(),
            None, // do not surface RES winner as miner payout signal
            false,
        )?;

        // Push to live sessions (subscribe-only delivery left miners on stale jobs).
        if let Ok(line) = serde_json::to_string(&envelope.notify) {
            self.notify_hub.publish_lines(vec![format!("{line}\n")]);
            self.audit.record(
                "job.broadcast",
                format!("job_id={} hub_version={}", envelope.job_id, self.notify_hub.version()),
            );
        }

        Ok(vec![envelope])
    }

    /// Full tip change: invalidate jobs **and** clear the template cache active slot.
    /// Use for explicit reorg simulation. Prefer [`Self::invalidate_jobs_on_tip_change`]
    /// after `GbtAdapter::poll_once`, which already tip-changed + ingested a new ACTIVE.
    pub fn handle_tip_change(&self, new_tip: TipHash) -> StratumResult<()> {
        self.invalidate_jobs_on_tip_change(new_tip.clone())?;
        self.templates
            .handle_tip_changed(new_tip)
            .map_err(|e| StratumError::Internal(e.to_string()))?;
        Ok(())
    }

    /// Invalidate published jobs / payout / winner state for a tip change **without**
    /// wiping the template cache. `GbtAdapter::poll_once` already calls
    /// `TemplateCache::handle_tip_changed` then `ingest`s the replacement ACTIVE;
    /// calling [`Self::handle_tip_change`] afterwards cleared that ACTIVE and left
    /// miners on a broadcast job that was immediately Invalidated (mass stale rejects).
    pub fn invalidate_jobs_on_tip_change(&self, new_tip: TipHash) -> StratumResult<()> {
        let invalidated = self
            .publisher
            .on_tip_changed(new_tip)
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

        let _reverse = self.payout.reverse_finalized_for_jobs(&invalidated);
        self.payout.orphan_round_for_jobs(&invalidated);
        self.metrics
            .reorg_events
            .fetch_add(1, std::sync::atomic::Ordering::Relaxed);
        self.audit.record("reorg", "tip changed; jobs invalidated");
        Ok(())
    }

    fn handle_subscribe(
        &self,
        session_id: u64,
        req: &protocol::StratumRequest,
    ) -> StratumResult<Vec<String>> {
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
        let mut out = vec![protocol::serialize_response(&StratumResponse::ok(
            req.id.clone(),
            result,
        ))?];

        if let Ok(jobs) = self.jobs.active_jobs() {
            for job in jobs {
                out.push(
                    serde_json::to_string(&job.notify)
                        .map_err(|e| StratumError::Protocol(e.to_string()))?
                        + "\n",
                );
            }
        }
        Ok(out)
    }

    fn handle_authorize(
        &self,
        session_id: u64,
        req: &protocol::StratumRequest,
    ) -> StratumResult<Vec<String>> {
        let params = protocol::params_as_strings(&req.params)?;
        let worker = params
            .first()
            .ok_or_else(|| StratumError::Protocol("worker required".into()))?;
        let pass = params.get(1).map(|s| s.as_str()).unwrap_or("");
        if let Err(e) = self.sessions.authorize(session_id, worker, pass) {
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
        if let Some(session) = self.sessions.get(session_id) {
            out.push(protocol::serialize_set_difficulty(session.difficulty)?);
        }
        Ok(out)
    }

    fn handle_submit(
        &self,
        session_id: u64,
        req: &protocol::StratumRequest,
    ) -> StratumResult<Vec<String>> {
        let session = self
            .sessions
            .get(session_id)
            .ok_or_else(|| StratumError::Unauthorized("unknown session".into()))?;

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
        let job_id = params
            .get(1)
            .ok_or_else(|| StratumError::Protocol("job_id required".into()))?;
        let extranonce2_hex = params
            .get(2)
            .ok_or_else(|| StratumError::Protocol("extranonce2 required".into()))?;
        let ntime = u32::from_str_radix(params.get(3).map(|s| s.as_str()).unwrap_or("0"), 16)
            .map_err(|e| StratumError::Protocol(e.to_string()))?;
        let nonce = u32::from_str_radix(params.get(4).map(|s| s.as_str()).unwrap_or("0"), 16)
            .map_err(|e| StratumError::Protocol(e.to_string()))?;

        let envelope = match self.jobs.envelope_for_job_id(job_id) {
            Ok(e) => e,
            Err(e) => {
                // Stale / inactive job after tip change — reject the share, keep session.
                self.metrics
                    .shares_rejected
                    .fetch_add(1, std::sync::atomic::Ordering::Relaxed);
                self.audit.record(
                    "share.rejected",
                    format!("worker={worker} job={job_id} reason=stale_or_inactive err={e}"),
                );
                return Ok(vec![protocol::serialize_response(&StratumResponse::err(
                    req.id.clone(),
                    21,
                    "stale job",
                ))?]);
            }
        };
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
        self.audit
            .record("share.accepted", format!("worker={worker} job={}", envelope.job_id));

        if result.is_block_candidate {
            // Do not tear down the Stratum session if submitblock fails — the share
            // itself was valid. Block payout is booked only inside handle_block_candidate
            // after the node accepts (C-5).
            if let Err(e) = self.handle_block_candidate(worker, envelope, result, share) {
                self.audit.record(
                    "block.path_error_nonfatal",
                    format!("worker={worker} job={} err={e}", envelope.job_id),
                );
            }
        }

        protocol::serialize_response(&StratumResponse::ok(None, serde_json::json!(true)))
    }

    /// Found-block path: assemble ORU job (opaque GBT txs) and submitblock.
    /// Never calls mint-engine / try_mint / mint authorization.
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
            format!(
                "worker={worker} job={} pow={:?}",
                envelope.job_id, result.pow_hash
            ),
        );

        let block_ctx = SolvedBlockContext {
            extranonce1: share.extranonce1.clone(),
            extranonce2: share.extranonce2.clone(),
            ntime: share.ntime,
            nonce: share.nonce,
        };

        let block_hex = match assemble_solved_block(&template, job.as_ref(), &block_ctx) {
            Ok(pkg) => pkg.block_hex,
            Err(e) => {
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
        };

        // Advisory only — node is authoritative. allow_res_body_mint reflects that
        // opaque GBT may already contain a node automint; the pool did not add one.
        let block_val = self.validator.validate_block(BlockSubmission {
            job_id: job_id.clone(),
            record_id: Some(envelope.template_id),
            block_hex: block_hex.clone(),
            allow_res_body_mint: true,
        });
        if !block_val.accepted {
            self.audit.record(
                "block.prevalidation_failed",
                format!(
                    "worker={worker} job={} msg={:?}",
                    envelope.job_id, block_val.message
                ),
            );
        }

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
                // Tip advanced — wake GBT poller so miners get a new job ASAP.
                self.request_work_refresh();
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
