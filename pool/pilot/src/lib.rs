//! In-process closed testnet / Alpha pilot harness wiring the RIGOR pool stack.
//!
//! Composes `rigor-gbt` → `block-assembler` → `share-validator` → `stratum-server`
//! as an ORU-only mining coordination layer (Option A). RES mint construction is
//! intentionally absent — node automint / wallet Convergence own that path.

use std::sync::Arc;
use std::time::Duration;

use block_assembler::{JobCache, JobPublisher};
use rigor_gbt::{GbtAdapter, PollEvent, RigordRpc, TemplateCache, TipHash};
use stratum_server::{
    AuditEvent, BlockSubmitter, MetricsSnapshot, PayoutEngine, RpcBlockSubmitter,
    StratumOrchestrator, StratumServer, UnconfiguredBlockSubmitter,
};

pub mod config;
pub mod pg_adapter;
pub mod security_config;
pub use config::{load_resolved, PoolFileConfig, ResolvedPoolConfig};
pub use pg_adapter::PilotPgAdapter;
pub use security_config::{PoolProfile, PoolSecurityConfig};

#[derive(Debug, thiserror::Error)]
pub enum PilotError {
    #[error("gbt: {0}")]
    Gbt(#[from] rigor_gbt::GbtError),

    #[error("stratum: {0}")]
    Stratum(#[from] stratum_server::StratumError),

    #[error("config: {0}")]
    Config(String),
}

pub type PilotResult<T> = Result<T, PilotError>;

#[derive(Clone, Debug)]
pub struct PoolPilotConfig {
    pub rpc_url: String,
    pub rpc_user: Option<String>,
    pub rpc_password: Option<String>,
    pub stratum_listen: String,
    pub gbt_poll_interval: Duration,
    pub pplns_window: u64,
    /// Pool fee percent of coinbase ORU retained before miner PPLNS (0–100).
    pub oru_fee_percent: f64,
    /// Operator payout settlement review interval (seconds).
    pub payout_interval_secs: u64,
}

impl Default for PoolPilotConfig {
    fn default() -> Self {
        Self {
            rpc_url: "http://127.0.0.1:8761/".into(),
            rpc_user: None,
            rpc_password: None,
            stratum_listen: "127.0.0.1:3333".into(),
            gbt_poll_interval: Duration::from_secs(2),
            pplns_window: 1,
            oru_fee_percent: 0.0,
            payout_interval_secs: 3600,
        }
    }
}

impl PoolPilotConfig {
    pub fn from_env() -> Self {
        let mut cfg = Self::default();
        if let Ok(url) = std::env::var("RIGOR_RPC_URL") {
            cfg.rpc_url = url;
        }
        if let Ok(user) = std::env::var("RIGOR_RPC_USER") {
            cfg.rpc_user = Some(user);
        }
        if let Ok(pass) = std::env::var("RIGOR_RPC_PASSWORD") {
            cfg.rpc_password = Some(pass);
        }
        if let Ok(addr) = std::env::var("STRATUM_LISTEN") {
            cfg.stratum_listen = addr;
        }
        if let Ok(ms) = std::env::var("GBT_POLL_INTERVAL_MS") {
            if let Ok(v) = ms.parse::<u64>() {
                cfg.gbt_poll_interval = Duration::from_millis(v);
            }
        }
        if let Ok(w) = std::env::var("PPLNS_WINDOW") {
            if let Ok(v) = w.parse::<u64>() {
                cfg.pplns_window = v;
            }
        }
        if let Ok(fee) = std::env::var("ORU_FEE_PERCENT") {
            if let Ok(v) = fee.parse::<f64>() {
                cfg.oru_fee_percent = v;
            }
        }
        if let Ok(iv) = std::env::var("PAYOUT_INTERVAL") {
            if let Ok(v) = iv.parse::<u64>() {
                cfg.payout_interval_secs = v;
            }
        }
        cfg
    }
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub enum PilotTick {
    Idle,
    TemplatePublished { job_id: String },
    TipChanged { new_tip: String },
    SkippedIbd,
    Rejected(String),
}

#[derive(Clone, Debug, serde::Serialize, serde::Deserialize)]
pub struct OruPayoutStatus {
    pub fee_percent: f64,
    pub payout_interval_secs: u64,
    pub total_liability_oru: i64,
    pub pool_fee_retained_oru: i64,
    pub pending_workers: usize,
    pub rounds_finalized: u64,
    pub active_share_count: u64,
    /// Always "NONE" — RES is never paid by the pool.
    pub res_involvement: String,
}

#[derive(Clone, Debug, serde::Serialize, serde::Deserialize)]
pub struct PilotStatusReport {
    pub metrics: MetricsSnapshot,
    pub audit_events: Vec<AuditEvent>,
    pub active_jobs: usize,
    pub current_tip: Option<String>,
    pub miners_connected: usize,
    /// Difficulty-sum proxy for connected workers. Prefer agent `--stats` for exact H/s.
    pub hashrate_estimate_hs: f64,
    pub shares_accepted: u64,
    pub blocks_found: u64,
    pub oru_payout: OruPayoutStatus,
}

pub struct PoolPilot {
    pub config: PoolPilotConfig,
    pub templates: Arc<TemplateCache>,
    pub jobs: Arc<JobCache>,
    pub orchestrator: Arc<StratumOrchestrator>,
    adapter: GbtAdapter,
}

impl PoolPilot {
    /// Production entrypoint — fail-closed security config, ORU-only orchestrator.
    pub fn new(config: PoolPilotConfig) -> PilotResult<Self> {
        let security = PoolSecurityConfig::production_from_env()?;
        security.validate()?;
        let mut submitter = RpcBlockSubmitter::new(config.rpc_url.clone());
        if let (Some(user), Some(pass)) = (&config.rpc_user, &config.rpc_password) {
            submitter = submitter.with_auth(user.clone(), pass.clone());
        }
        Self::assemble(config, security, Arc::new(submitter))
    }

    /// Explicit devtest wiring for regtest / Alpha local smoke.
    ///
    /// Uses a real `RpcBlockSubmitter` when `[node]` points at a reachable URL.
    /// If `RIGOR_POOL_PAYOUT_SCRIPT_HEX` is set to a Witness v0/v1 script, that
    /// coinbase payout is used (required for live `submitblock` on RIGOR —
    /// legacy P2PKH placeholders are rejected as `bad-rigor-script-type`).
    pub fn devtest_in_process(config: PoolPilotConfig) -> PilotResult<Self> {
        let submitter: Arc<dyn BlockSubmitter> = if Self::rpc_looks_live(&config.rpc_url) {
            let mut s = RpcBlockSubmitter::new(config.rpc_url.clone());
            if let (Some(user), Some(pass)) = (&config.rpc_user, &config.rpc_password) {
                s = s.with_auth(user.clone(), pass.clone());
            }
            Arc::new(s)
        } else {
            Arc::new(UnconfiguredBlockSubmitter)
        };

        let mut security = PoolSecurityConfig::devtest();
        if let Ok(hex) = std::env::var("RIGOR_POOL_PAYOUT_SCRIPT_HEX") {
            if !hex.trim().is_empty() {
                let script = block_assembler::payout_script::payout_script_from_hex(hex.trim())
                    .map_err(|e| PilotError::Config(e.to_string()))?;
                security.payout_script = script;
            }
        }

        Self::assemble(config, security, submitter)
    }

    fn rpc_looks_live(url: &str) -> bool {
        let u = url.trim().trim_end_matches('/');
        // Placeholder used by in_memory_simulation / unit tests.
        !u.is_empty()
            && !u.ends_with(":1")
            && !u.contains("127.0.0.1:1")
            && u != "http://127.0.0.1:1"
    }

    fn assemble(
        config: PoolPilotConfig,
        security: PoolSecurityConfig,
        submitter: Arc<dyn BlockSubmitter>,
    ) -> PilotResult<Self> {
        let mut rpc = RigordRpc::new(&config.rpc_url);
        if let (Some(user), Some(pass)) = (&config.rpc_user, &config.rpc_password) {
            rpc = rpc.with_auth(user, pass);
        }

        let templates = Arc::new(TemplateCache::new());
        let jobs = Arc::new(JobCache::new());
        let payout = Arc::new(PayoutEngine::with_fee_percent(
            config.pplns_window,
            config.oru_fee_percent,
        ));

        let publisher = match security.profile {
            PoolProfile::Production => {
                JobPublisher::with_payout_script(jobs.clone(), security.payout_script.clone())
                    .map_err(|e| PilotError::Config(e.to_string()))?
            }
            // Prefer the script on PoolSecurityConfig (may be wallet Witness v0
            // injected via RIGOR_POOL_PAYOUT_SCRIPT_HEX). Fall back to defaults
            // only when that script is the legacy unspendable P2PKH placeholder
            // (unit tests / pure in-memory sim).
            PoolProfile::Devtest => {
                if security.payout_script
                    == block_assembler::UNSPENDABLE_DEVTEST_PAYOUT_SCRIPT.as_slice()
                {
                    JobPublisher::with_defaults(jobs.clone())
                } else {
                    JobPublisher::with_payout_script(jobs.clone(), security.payout_script.clone())
                        .map_err(|e| PilotError::Config(e.to_string()))?
                }
            }
        };

        let orchestrator = Arc::new(
            StratumOrchestrator::new(templates.clone(), jobs.clone(), payout)
                .with_publisher(publisher)
                .with_sessions(security.sessions)
                .with_submitter(submitter),
        );

        let adapter = GbtAdapter::new(rpc, templates.clone());

        Ok(Self {
            config,
            templates,
            jobs,
            orchestrator,
            adapter,
        })
    }

    pub fn in_memory_simulation() -> PilotResult<Self> {
        let config = PoolPilotConfig {
            rpc_url: "http://127.0.0.1:1/".into(),
            ..PoolPilotConfig::default()
        };
        Self::devtest_in_process(config)
    }

    pub fn poll_once(&mut self) -> PilotResult<PilotTick> {
        let tip_before = self.templates.current_tip().ok().flatten();
        let event = self.adapter.poll_once()?;
        let tip_after = self.templates.current_tip().ok().flatten();
        // Adapter already tip-invalidated + ingested a new ACTIVE template.
        // Only invalidate published jobs here — do NOT clear templates again
        // (that wiped ACTIVE and produced "no active template" / mass stale rejects).
        if tip_before.is_some() && tip_before != tip_after {
            if let Some(t) = tip_after.clone() {
                self.orchestrator.invalidate_jobs_on_tip_change(t)?;
            }
        }

        match event {
            PollEvent::TemplateCreated(_record) => {
                match self.orchestrator.publish_jobs_from_active_template() {
                    Ok(envelopes) => {
                        let job_id = envelopes
                            .first()
                            .map(|e| e.job_id.clone())
                            .unwrap_or_default();
                        Ok(PilotTick::TemplatePublished { job_id })
                    }
                    Err(e) => {
                        // Tip/job race after reorg: template cache briefly empty.
                        // Not an RPC auth failure — keep polling.
                        Ok(PilotTick::Rejected(format!(
                            "publish deferred (will retry): {e}"
                        )))
                    }
                }
            }
            PollEvent::TipChanged { .. } => Ok(PilotTick::TipChanged {
                new_tip: tip_after.map(|t| t.0).unwrap_or_default(),
            }),
            PollEvent::SkippedIbd => Ok(PilotTick::SkippedIbd),
            PollEvent::TemplateDeduped => Ok(PilotTick::Idle),
            PollEvent::Rejected(msg) => Ok(PilotTick::Rejected(msg)),
        }
    }

    pub fn simulate_tip_change(&self, new_tip: TipHash) -> PilotResult<()> {
        self.orchestrator.handle_tip_change(new_tip)?;
        Ok(())
    }

    pub fn ingest_fixture(
        &mut self,
        template: rigor_gbt::GbtTemplate,
        raw_json: String,
        tip: TipHash,
    ) -> PilotResult<PilotTick> {
        let record = self
            .templates
            .ingest(template, raw_json, tip)
            .map_err(PilotError::Gbt)?
            .ok_or_else(|| PilotError::Config("template deduped".into()))?;
        let _ = record;
        let envelopes = self.orchestrator.publish_jobs_from_active_template()?;
        let job_id = envelopes
            .first()
            .map(|e| e.job_id.clone())
            .unwrap_or_default();
        Ok(PilotTick::TemplatePublished { job_id })
    }

    pub fn status_report(&self) -> PilotResult<PilotStatusReport> {
        let active_jobs = self.jobs.get_active_jobs().map(|j| j.len()).unwrap_or(0);
        let current_tip = self.templates.current_tip().ok().flatten().map(|t| t.0);
        let metrics = self.orchestrator.metrics.snapshot();
        let miners_connected = self.orchestrator.sessions.session_count();
        let hashrate_estimate_hs = self.orchestrator.sessions.authorized_difficulty_sum();
        let snap = self.orchestrator.payout.snapshot();
        let rounds_finalized = snap
            .rounds
            .iter()
            .filter(|r| r.status == stratum_server::PayoutRoundStatus::Finalized)
            .count() as u64;
        Ok(PilotStatusReport {
            miners_connected,
            hashrate_estimate_hs,
            shares_accepted: metrics.shares_accepted,
            blocks_found: metrics.blocks_found,
            oru_payout: OruPayoutStatus {
                fee_percent: self.config.oru_fee_percent,
                payout_interval_secs: self.config.payout_interval_secs,
                total_liability_oru: self.orchestrator.payout.total_liability_oru(),
                pool_fee_retained_oru: snap.pool_fee_retained_oru,
                pending_workers: snap
                    .balances
                    .iter()
                    .filter(|b| b.oru_pending > 0)
                    .count(),
                rounds_finalized,
                active_share_count: snap.active_share_count,
                res_involvement: "NONE".to_string(),
            },
            metrics,
            audit_events: self.orchestrator.audit.history(),
            active_jobs,
            current_tip,
        })
    }

    pub fn run_stratum_blocking(&self) -> PilotResult<()> {
        let server = StratumServer::new(self.orchestrator.clone(), &self.config.stratum_listen);
        server.run()?;
        Ok(())
    }

    pub fn spawn_gbt_poll_thread(pilot: Arc<std::sync::Mutex<PoolPilot>>) {
        let interval = pilot
            .lock()
            .map(|p| p.config.gbt_poll_interval)
            .unwrap_or(Duration::from_secs(2));
        std::thread::spawn(move || {
            let mut last_err: Option<String> = None;
            let mut last_err_log = std::time::Instant::now()
                .checked_sub(Duration::from_secs(60))
                .unwrap_or_else(std::time::Instant::now);
            loop {
                let tick = pilot.lock().ok().map(|mut guard| guard.poll_once());
                match tick {
                    Some(Ok(PilotTick::TemplatePublished { job_id })) => {
                        eprintln!("rigor-pool: GBT template published job_id={job_id}");
                        last_err = None;
                    }
                    Some(Ok(PilotTick::TipChanged { new_tip })) => {
                        eprintln!("rigor-pool: tip changed → {new_tip}");
                        last_err = None;
                    }
                    Some(Ok(PilotTick::Rejected(msg))) => {
                        let msg = format!("GBT rejected: {msg}");
                        if last_err.as_deref() != Some(msg.as_str())
                            || last_err_log.elapsed() >= Duration::from_secs(15)
                        {
                            eprintln!("rigor-pool: {msg}");
                            last_err = Some(msg);
                            last_err_log = std::time::Instant::now();
                        }
                    }
                    Some(Err(e)) => {
                        let msg = e.to_string();
                        let hint = if msg.contains("rpc") || msg.contains("HTTP") || msg.contains("connect")
                        {
                            "check [node] rpc_host/rpc_port/user/password"
                        } else {
                            "transient pool/job race — polling continues"
                        };
                        if last_err.as_deref() != Some(msg.as_str())
                            || last_err_log.elapsed() >= Duration::from_secs(15)
                        {
                            eprintln!("rigor-pool: GBT poll note: {msg} ({hint})");
                            last_err = Some(msg);
                            last_err_log = std::time::Instant::now();
                        }
                    }
                    Some(Ok(_)) | None => {}
                }
                // Sleep in slices so a successful submitblock can wake us early.
                let slice = Duration::from_millis(100);
                let mut waited = Duration::ZERO;
                while waited < interval {
                    let wake = pilot
                        .lock()
                        .ok()
                        .map(|p| p.orchestrator.take_work_refresh())
                        .unwrap_or(false);
                    if wake {
                        break;
                    }
                    std::thread::sleep(slice);
                    waited = waited.saturating_add(slice);
                }
            }
        });
    }
}
