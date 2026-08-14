use std::sync::atomic::{AtomicU64, Ordering};
use std::sync::Arc;

use serde::{Deserialize, Serialize};

#[derive(Debug, Default)]
pub struct PoolMetrics {
    pub shares_accepted: AtomicU64,
    pub shares_rejected: AtomicU64,
    pub blocks_found: AtomicU64,
    /// C-5: blocks the node accepted. `blocks_found - blocks_submitted` is the
    /// count of solutions the pool failed to get on chain, and must be alerted on.
    pub blocks_submitted: AtomicU64,
    /// C-5: `submitblock` returned an error (no miner credit was recorded).
    pub blocks_submit_failed: AtomicU64,
    /// C-5: a found block could not be assembled.
    pub blocks_unsubmittable: AtomicU64,
    /// Legacy counter; Option A keeps this at zero (pool never mints RES).
    pub res_mint_events: AtomicU64,
    pub jobs_notified: AtomicU64,
    pub reorg_events: AtomicU64,
    /// H-5a: failed `mining.authorize` attempts.
    pub auth_failures: AtomicU64,
    /// H-5b: submissions whose claimed worker did not match the session identity.
    pub impersonation_attempts: AtomicU64,
}

impl PoolMetrics {
    pub fn snapshot(&self) -> MetricsSnapshot {
        MetricsSnapshot {
            shares_accepted: self.shares_accepted.load(Ordering::Relaxed),
            shares_rejected: self.shares_rejected.load(Ordering::Relaxed),
            blocks_found: self.blocks_found.load(Ordering::Relaxed),
            blocks_submitted: self.blocks_submitted.load(Ordering::Relaxed),
            blocks_submit_failed: self.blocks_submit_failed.load(Ordering::Relaxed),
            blocks_unsubmittable: self.blocks_unsubmittable.load(Ordering::Relaxed),
            res_mint_events: self.res_mint_events.load(Ordering::Relaxed),
            jobs_notified: self.jobs_notified.load(Ordering::Relaxed),
            reorg_events: self.reorg_events.load(Ordering::Relaxed),
            auth_failures: self.auth_failures.load(Ordering::Relaxed),
            impersonation_attempts: self.impersonation_attempts.load(Ordering::Relaxed),
        }
    }
}

#[derive(Clone, Debug, PartialEq, Eq, Serialize, Deserialize)]
pub struct MetricsSnapshot {
    pub shares_accepted: u64,
    pub shares_rejected: u64,
    pub blocks_found: u64,
    pub blocks_submitted: u64,
    pub blocks_submit_failed: u64,
    pub blocks_unsubmittable: u64,
    pub res_mint_events: u64,
    pub jobs_notified: u64,
    pub reorg_events: u64,
    pub auth_failures: u64,
    pub impersonation_attempts: u64,
}

impl MetricsSnapshot {
    /// C-5 invariant: every found block must reach the node. A non-zero value here
    /// means miners may be owed for work that never entered the chain.
    pub fn unaccounted_blocks(&self) -> u64 {
        self.blocks_found.saturating_sub(self.blocks_submitted)
    }
}

pub type SharedMetrics = Arc<PoolMetrics>;
