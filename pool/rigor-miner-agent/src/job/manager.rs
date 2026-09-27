use std::collections::HashMap;
use std::time::{Duration, Instant};

use crate::error::MinerResult;
use crate::metrics::Metrics;
use crate::stratum::{MiningNotify, MiningNotifyRigor};

/// Display-only convergence metadata from pool .
/// Miner MUST NOT perform draw selection or winner validation locally.
#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct ConvergenceMetadata {
    pub convergence_root_hex: Option<String>,
    pub preview_winner_txid: Option<String>,
    pub mint_required: bool,
}

impl ConvergenceMetadata {
    pub fn display_summary(&self) -> String {
        format!(
            "mint_required={} winner={:?} root={:?}",
            self.mint_required,
            self.preview_winner_txid,
            self.convergence_root_hex.as_ref().map(|s| &s[..16.min(s.len())])
        )
    }
}

/// Cached work unit combining Stratum notify + optional RIGOR metadata.
#[derive(Clone, Debug, PartialEq)]
pub struct JobSnapshot {
    pub notify: MiningNotify,
    pub convergence: ConvergenceMetadata,
    pub difficulty: f64,
    pub received_at: Instant,
    pub stale: bool,
}

impl JobSnapshot {
    pub fn job_id(&self) -> &str {
        &self.notify.job_id
    }
}

#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct JobStatistics {
    pub jobs_received: u64,
    pub jobs_active: u64,
    pub jobs_stale: u64,
    pub clean_job_flushes: u64,
}

pub struct JobManager {
    active: HashMap<String, JobSnapshot>,
    stale: HashMap<String, JobSnapshot>,
    stats: JobStatistics,
    stale_after: Duration,
    default_difficulty: f64,
}

impl JobManager {
    pub fn new(stale_after: Duration) -> Self {
        Self {
            active: HashMap::new(),
            stale: HashMap::new(),
            stats: JobStatistics::default(),
            stale_after,
            default_difficulty: 1e-5,
        }
    }

    pub fn statistics(&self) -> &JobStatistics {
        &self.stats
    }

    pub fn active_jobs(&self) -> impl Iterator<Item = &JobSnapshot> {
        self.active.values()
    }

    pub fn get(&self, job_id: &str) -> Option<&JobSnapshot> {
        self.active.get(job_id)
    }

    pub fn set_difficulty(&mut self, difficulty: f64) {
        self.default_difficulty = difficulty;
        for job in self.active.values_mut() {
            job.difficulty = difficulty;
        }
    }

    pub fn ingest_notify(&mut self, notify: MiningNotify, metrics: &Metrics) -> MinerResult<()> {
        if notify.clean_jobs {
            self.flush_active_as_stale(metrics);
            self.stats.clean_job_flushes += 1;
        }
        let job_id = notify.job_id.clone();
        let snapshot = JobSnapshot {
            notify,
            convergence: ConvergenceMetadata::default(),
            difficulty: self.default_difficulty,
            received_at: Instant::now(),
            stale: false,
        };
        self.active.insert(job_id, snapshot);
        self.stats.jobs_received += 1;
        self.stats.jobs_active = self.active.len() as u64;
        Ok(())
    }

    pub fn attach_rigor_metadata(&mut self, rigor: MiningNotifyRigor) {
        if let Some(job) = self.active.get_mut(&rigor.job_id) {
            job.convergence = rigor.convergence;
        }
    }

    pub fn mark_stale(&mut self, job_id: &str, metrics: &Metrics) {
        if let Some(mut job) = self.active.remove(job_id) {
            job.stale = true;
            self.stale.insert(job_id.to_string(), job);
            self.stats.jobs_stale += 1;
            metrics.record_stale_job();
            self.stats.jobs_active = self.active.len() as u64;
        }
    }

    pub fn expire_old_jobs(&mut self, metrics: &Metrics) {
        let now = Instant::now();
        let expired: Vec<String> = self
            .active
            .iter()
            .filter(|(_, j)| now.duration_since(j.received_at) > self.stale_after)
            .map(|(id, _)| id.clone())
            .collect();
        for id in expired {
            self.mark_stale(&id, metrics);
        }
    }

    pub fn is_stale(&self, job_id: &str) -> bool {
        !self.active.contains_key(job_id)
    }

    fn flush_active_as_stale(&mut self, metrics: &Metrics) {
        let ids: Vec<String> = self.active.keys().cloned().collect();
        for id in ids {
            self.mark_stale(&id, metrics);
        }
    }
}

impl Default for JobManager {
    fn default() -> Self {
        Self::new(Duration::from_secs(900))
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::metrics::Metrics;

    fn sample_notify(job_id: &str, clean: bool) -> MiningNotify {
        MiningNotify {
            job_id: job_id.into(),
            prevhash: "00".repeat(64),
            coinb1: "01".into(),
            coinb2: "02".into(),
            merkle_branch: vec![],
            version: "20000000".into(),
            nbits: "207fffff".into(),
            ntime: "65000000".into(),
            clean_jobs: clean,
        }
    }

    #[test]
    fn job_cache_lifecycle() {
        let metrics = Metrics::new();
        let mut mgr = JobManager::new(Duration::from_secs(60));
        mgr.ingest_notify(sample_notify("j1", false), &metrics).unwrap();
        assert_eq!(mgr.statistics().jobs_active, 1);
        mgr.attach_rigor_metadata(MiningNotifyRigor {
            job_id: "j1".into(),
            convergence: ConvergenceMetadata {
                mint_required: true,
                preview_winner_txid: Some("aa".repeat(64)),
                convergence_root_hex: Some("00".repeat(64)),
            },
        });
        assert!(mgr.get("j1").unwrap().convergence.mint_required);
        mgr.mark_stale("j1", &metrics);
        assert!(mgr.is_stale("j1"));
        assert_eq!(metrics.snapshot().stale_jobs, 1);
    }

    #[test]
    fn clean_jobs_flushes_active() {
        let metrics = Metrics::new();
        let mut mgr = JobManager::default();
        mgr.ingest_notify(sample_notify("a", false), &metrics).unwrap();
        mgr.ingest_notify(sample_notify("b", true), &metrics).unwrap();
        assert_eq!(mgr.statistics().jobs_active, 1);
        assert_eq!(mgr.statistics().clean_job_flushes, 1);
    }
}
