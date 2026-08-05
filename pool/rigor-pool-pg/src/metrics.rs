use std::sync::atomic::{AtomicU64, Ordering};

/// Counters for persisted vs in-memory backend usage (TASK-MINING-017 §6).
#[derive(Debug, Default)]
pub struct PersistenceMetrics {
    pub pg_inserts: AtomicU64,
    pub pg_updates: AtomicU64,
    pub pg_reorgs: AtomicU64,
    pub memory_fallback_ops: AtomicU64,
}

impl PersistenceMetrics {
    pub fn new() -> Self {
        Self::default()
    }

    pub fn record_pg_insert(&self) {
        self.pg_inserts.fetch_add(1, Ordering::Relaxed);
    }

    pub fn record_pg_update(&self) {
        self.pg_updates.fetch_add(1, Ordering::Relaxed);
    }

    pub fn record_pg_reorg(&self) {
        self.pg_reorgs.fetch_add(1, Ordering::Relaxed);
    }

    pub fn record_memory_fallback(&self) {
        self.memory_fallback_ops.fetch_add(1, Ordering::Relaxed);
    }

    pub fn snapshot(&self) -> MetricsSnapshot {
        MetricsSnapshot {
            pg_inserts: self.pg_inserts.load(Ordering::Relaxed),
            pg_updates: self.pg_updates.load(Ordering::Relaxed),
            pg_reorgs: self.pg_reorgs.load(Ordering::Relaxed),
            memory_fallback_ops: self.memory_fallback_ops.load(Ordering::Relaxed),
        }
    }
}

#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct MetricsSnapshot {
    pub pg_inserts: u64,
    pub pg_updates: u64,
    pub pg_reorgs: u64,
    pub memory_fallback_ops: u64,
}
