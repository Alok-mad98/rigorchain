use std::sync::{Arc, mpsc};
use std::time::Duration;

use crate::error::{MinerError, MinerResult};
use crate::job::{JobManager, JobSnapshot};
use crate::metrics::Metrics;
use crate::pow::{MiningController, MiningWork, ShareCandidate};
use crate::stratum::{PoolEndpoint, StratumClient, StratumEvent, DEFAULT_AGENT};
use crate::telemetry::{MinerShareTelemetry, ShareResultEvent};

#[derive(Clone, Debug)]
pub struct AgentConfig {
    pub pool: PoolEndpoint,
    pub worker: String,
    pub password: String,
    pub agent: String,
    pub threads: u32,
    pub job_stale_secs: u64,
}

impl AgentConfig {
    pub fn from_cli(pool: &str, worker: &str, password: &str, threads: u32) -> MinerResult<Self> {
        Ok(Self {
            pool: PoolEndpoint::parse(pool)?,
            worker: worker.to_string(),
            password: password.to_string(),
            agent: DEFAULT_AGENT.to_string(),
            threads: threads.max(1),
            job_stale_secs: 900,
        })
    }
}

/// Miner agent: Stratum session + job cache + multi-thread CPU mining loop.
pub struct MinerAgent {
    pub config: AgentConfig,
    pub client: StratumClient,
    pub jobs: JobManager,
    pub metrics: Arc<Metrics>,
    mining: Option<MiningController>,
    share_rx: Option<mpsc::Receiver<ShareCandidate>>,
}

impl MinerAgent {
    pub fn connect(config: AgentConfig) -> MinerResult<Self> {
        let client = StratumClient::connect(&config.pool)?;
        Ok(Self {
            jobs: JobManager::new(Duration::from_secs(config.job_stale_secs)),
            metrics: Arc::new(Metrics::new()),
            mining: None,
            share_rx: None,
            client,
            config,
        })
    }

    pub fn handshake(&mut self) -> MinerResult<()> {
        self.client
            .subscribe(Some(&self.config.agent))?;
        self.drain_pending_events()?;
        if !self
            .client
            .authorize(&self.config.worker, &self.config.password)?
        {
            return Err(MinerError::Stratum("authorize rejected".into()));
        }
        self.drain_pending_events()?;
        Ok(())
    }

    /// Process subscribe/authorize push lines already buffered by the Stratum client.
    fn drain_pending_events(&mut self) -> MinerResult<()> {
        loop {
            match self.client.read_event() {
                Ok(Some(ev)) => self.handle_event(&ev)?,
                Ok(None) => break,
                Err(MinerError::Io(e))
                    if e.kind() == std::io::ErrorKind::WouldBlock
                        || e.kind() == std::io::ErrorKind::TimedOut =>
                {
                    break;
                }
                Err(e) => return Err(e),
            }
        }
        Ok(())
    }

    pub fn start_mining(&mut self) -> MinerResult<()> {
        if self.mining.is_some() {
            return Ok(());
        }
        let session = self
            .client
            .session
            .as_ref()
            .ok_or_else(|| MinerError::Stratum("subscribe before mining".into()))?;
        let (tx, rx) = mpsc::channel();
        let controller = MiningController::start(
            self.config.threads,
            session.extranonce1.clone(),
            session.extranonce2_size as usize,
            self.metrics.clone(),
            tx,
        );
        self.mining = Some(controller);
        self.share_rx = Some(rx);
        self.sync_mining_work();
        Ok(())
    }

    pub fn poll_once(&mut self) -> MinerResult<Vec<StratumEvent>> {
        self.jobs.expire_old_jobs(self.metrics.as_ref());
        let mut events = Vec::new();
        match self.client.read_event() {
            Ok(Some(ev)) => {
                self.handle_event(&ev)?;
                events.push(ev);
            }
            Ok(None) => {}
            Err(MinerError::Io(e)) if e.kind() == std::io::ErrorKind::WouldBlock
                || e.kind() == std::io::ErrorKind::TimedOut =>
            {
                // No Stratum data yet — continue mining.
            }
            Err(e) => return Err(e),
        }
        Ok(events)
    }

    pub fn drain_shares(&mut self) -> MinerResult<u32> {
        let mut submitted = 0u32;
        let rx = match self.share_rx.as_ref() {
            Some(r) => r,
            None => return Ok(0),
        };
        while let Ok(share) = rx.try_recv() {
            if self.jobs.is_stale(&share.job_id) {
                MinerShareTelemetry::record_share(
                    &self.config.worker,
                    &share.job_id,
                    0,
                    ShareResultEvent::Stale,
                    Some("stale_job"),
                    self.jobs.get(&share.job_id).map(|j| j.difficulty),
                );
                continue;
            }
            let difficulty = self.jobs.get(&share.job_id).map(|j| j.difficulty);
            let accepted = self.client.submit_share(
                &self.config.worker,
                &share.job_id,
                &share.extranonce2_hex(),
                &share.ntime_hex(),
                &share.nonce_hex(),
                self.metrics.as_ref(),
            )?;
            MinerShareTelemetry::record_share(
                &self.config.worker,
                &share.job_id,
                0,
                if accepted {
                    ShareResultEvent::Accepted
                } else {
                    ShareResultEvent::Rejected
                },
                if accepted { None } else { Some("pool_rejected") },
                difficulty,
            );
            submitted += 1;
            if accepted {
                break;
            }
        }
        Ok(submitted)
    }

    pub fn sync_mining_work(&mut self) {
        let Some(controller) = self.mining.as_ref() else {
            return;
        };
        if let Some(job) = self.latest_active_job() {
            let session = self.client.session.as_ref();
            let en2_size = session.map(|s| s.extranonce2_size as usize).unwrap_or(4);
            let en1 = session
                .map(|s| s.extranonce1.clone())
                .unwrap_or_default();
            controller.set_work(MiningWork {
                job,
                extranonce1: en1,
                extranonce2_size: en2_size,
            });
        } else {
            controller.clear_work();
        }
    }

    fn latest_active_job(&self) -> Option<JobSnapshot> {
        self.jobs
            .active_jobs()
            .max_by_key(|j| j.received_at)
            .cloned()
    }

    fn handle_event(&mut self, ev: &StratumEvent) -> MinerResult<()> {
        match ev {
            StratumEvent::Notify(n) => {
                self.jobs.ingest_notify(n.clone(), self.metrics.as_ref())?;
                self.sync_mining_work();
                Ok(())
            }
            StratumEvent::NotifyRigor(r) => {
                self.jobs.attach_rigor_metadata(r.clone());
                Ok(())
            }
            StratumEvent::SetDifficulty(d) => {
                self.jobs.set_difficulty(d.difficulty);
                self.sync_mining_work();
                Ok(())
            }
            StratumEvent::SubmitResult { accepted, .. } => {
                if !accepted {
                    // Pool rejected — continue scanning.
                }
                Ok(())
            }
        }
    }
}
