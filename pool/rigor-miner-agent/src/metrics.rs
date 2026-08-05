use std::sync::atomic::{AtomicU64, Ordering};
use std::time::Instant;

#[derive(Debug)]
pub struct Metrics {
    hashes: AtomicU64,
    shares_found: AtomicU64,
    shares_submitted: AtomicU64,
    shares_accepted: AtomicU64,
    shares_rejected: AtomicU64,
    stale_jobs: AtomicU64,
    started_at: Instant,
}

#[derive(Clone, Debug, Default, PartialEq, serde::Serialize)]
pub struct MetricsSnapshot {
    pub hashrate: f64,
    pub hashes: u64,
    pub shares_found: u64,
    pub shares_submitted: u64,
    pub shares_accepted: u64,
    pub shares_rejected: u64,
    pub stale_jobs: u64,
    pub uptime_secs: u64,
}

impl Metrics {
    pub fn new() -> Self {
        Self {
            hashes: AtomicU64::new(0),
            shares_found: AtomicU64::new(0),
            shares_submitted: AtomicU64::new(0),
            shares_accepted: AtomicU64::new(0),
            shares_rejected: AtomicU64::new(0),
            stale_jobs: AtomicU64::new(0),
            started_at: Instant::now(),
        }
    }

    pub fn record_hash(&self) {
        self.hashes.fetch_add(1, Ordering::Relaxed);
    }

    pub fn record_share_found(&self) {
        self.shares_found.fetch_add(1, Ordering::Relaxed);
    }

    pub fn record_share_submitted(&self) {
        self.shares_submitted.fetch_add(1, Ordering::Relaxed);
    }

    pub fn record_share_accepted(&self) {
        self.shares_accepted.fetch_add(1, Ordering::Relaxed);
    }

    pub fn record_share_rejected(&self) {
        self.shares_rejected.fetch_add(1, Ordering::Relaxed);
    }

    pub fn record_stale_job(&self) {
        self.stale_jobs.fetch_add(1, Ordering::Relaxed);
    }

    pub fn hashes(&self) -> u64 {
        self.hashes.load(Ordering::Relaxed)
    }

    pub fn hashrate_over(&self, elapsed_secs: f64, since_hashes: u64) -> f64 {
        if elapsed_secs <= 0.0 {
            return 0.0;
        }
        let current = self.hashes();
        let delta = current.saturating_sub(since_hashes);
        delta as f64 / elapsed_secs
    }

    pub fn snapshot(&self) -> MetricsSnapshot {
        let uptime = self.started_at.elapsed().as_secs();
        let hashes = self.hashes.load(Ordering::Relaxed);
        let hashrate = if uptime > 0 {
            hashes as f64 / uptime as f64
        } else {
            0.0
        };
        MetricsSnapshot {
            hashrate,
            hashes,
            shares_found: self.shares_found.load(Ordering::Relaxed),
            shares_submitted: self.shares_submitted.load(Ordering::Relaxed),
            shares_accepted: self.shares_accepted.load(Ordering::Relaxed),
            shares_rejected: self.shares_rejected.load(Ordering::Relaxed),
            stale_jobs: self.stale_jobs.load(Ordering::Relaxed),
            uptime_secs: uptime,
        }
    }
}

impl Default for Metrics {
    fn default() -> Self {
        Self::new()
    }
}
