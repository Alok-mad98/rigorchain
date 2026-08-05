//! In-process closed testnet pilot harness wiring the full RIGOR pool stack.
//!
//! Composes `rigor-gbt` → `block-assembler` → `share-validator` → `stratum-server`
//! → `mint-engine` without consensus changes.

use std::sync::Arc;
use std::time::Duration;

use block_assembler::{JobCache, JobPublisher};
use mint_engine::{MintEngine, RigordRpcConfig, RpcBlockSubmitter, UnconfiguredBlockSubmitter};
use rigor_gbt::{GbtAdapter, PollEvent, RigordRpc, TemplateCache, TipHash};
use stratum_server::{
    AuditEvent, BlockSubmitter, MetricsSnapshot, PayoutEngine, StratumOrchestrator, StratumServer,
};

pub mod pg_adapter;
pub mod security_config;
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
    pub mint_auto_submit: bool,
    pub pplns_window: u64,
}

impl Default for PoolPilotConfig {
    fn default() -> Self {
        Self {
            rpc_url: "http://127.0.0.1:8332/".into(),
            rpc_user: None,
            rpc_password: None,
            stratum_listen: "127.0.0.1:3333".into(),
            gbt_poll_interval: Duration::from_secs(2),
            mint_auto_submit: false,
            pplns_window: 1,
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
        if std::env::var("MINT_AUTO_SUBMIT")
            .map(|v| v == "1" || v.eq_ignore_ascii_case("true"))
            .unwrap_or(false)
        {
            cfg.mint_auto_submit = true;
        }
        if let Ok(w) = std::env::var("PPLNS_WINDOW") {
            if let Ok(v) = w.parse::<u64>() {
                cfg.pplns_window = v;
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

#[derive(Clone, Debug, serde::Serialize)]
pub struct PilotStatusReport {
    pub metrics: MetricsSnapshot,
    pub audit_events: Vec<AuditEvent>,
    pub active_jobs: usize,
    pub current_tip: Option<String>,
}

pub struct PoolPilot {
    pub config: PoolPilotConfig,
    pub templates: Arc<TemplateCache>,
    pub jobs: Arc<JobCache>,
    pub orchestrator: Arc<StratumOrchestrator>,
    adapter: GbtAdapter,
}

impl PoolPilot {
    /// Production entrypoint.
    ///
    /// SECURITY-001-E: every secret and destination is loaded from
    /// [`PoolSecurityConfig::production_from_env`], which refuses to produce a
    /// configuration when anything is missing or is a known-unsafe placeholder.
    /// There is no silent fallback to the devtest profile — see
    /// [`Self::devtest_in_process`] for the explicit opt-in.
    pub fn new(config: PoolPilotConfig) -> PilotResult<Self> {
        let rpc_cfg = RigordRpcConfig {
            url: config.rpc_url.clone(),
            user: config.rpc_user.clone(),
            password: config.rpc_password.clone(),
        };
        let security = PoolSecurityConfig::production_from_env(Some(rpc_cfg.clone()))?;
        security.validate()?;
        Self::assemble(config, security, Arc::new(RpcBlockSubmitter::new(rpc_cfg)))
    }

    /// Explicit devtest wiring for regtest and in-process simulation.
    ///
    /// Uses publicly-known material and a submission sink that refuses to submit,
    /// so a devtest process can never book payouts for real work.
    pub fn devtest_in_process(config: PoolPilotConfig) -> PilotResult<Self> {
        Self::assemble(
            config,
            PoolSecurityConfig::devtest(),
            Arc::new(UnconfiguredBlockSubmitter),
        )
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
        let payout = Arc::new(PayoutEngine::new(config.pplns_window));

        let mut mint_cfg = security.mint;
        mint_cfg.auto_submit = config.mint_auto_submit && mint_cfg.rpc.is_some();

        let publisher = match security.profile {
            PoolProfile::Production => {
                JobPublisher::with_payout_script(jobs.clone(), security.payout_script.clone())
                    .map_err(|e| PilotError::Config(e.to_string()))?
            }
            PoolProfile::Devtest => JobPublisher::with_defaults(jobs.clone()),
        };

        let orchestrator = Arc::new(
            StratumOrchestrator::with_mint_engine(
                templates.clone(),
                jobs.clone(),
                payout,
                Arc::new(MintEngine::new(mint_cfg)),
            )
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
        if tip_before.is_some() && tip_before != tip_after {
            if let Some(t) = tip_after.clone() {
                self.orchestrator.handle_tip_change(t)?;
            }
        }

        match event {
            PollEvent::TemplateCreated(_record) => {
                let envelopes = self
                    .orchestrator
                    .publish_jobs_from_active_template()
                    .map_err(|e| PilotError::Config(e.to_string()))?;
                let job_id = envelopes
                    .first()
                    .map(|e| e.job_id.clone())
                    .unwrap_or_default();
                Ok(PilotTick::TemplatePublished { job_id })
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
            .map_err(|e| PilotError::Gbt(e))?
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
        Ok(PilotStatusReport {
            metrics: self.orchestrator.metrics.snapshot(),
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
        std::thread::spawn(move || loop {
            if let Ok(mut guard) = pilot.lock() {
                let _ = guard.poll_once();
            }
            std::thread::sleep(interval);
        });
    }
}
