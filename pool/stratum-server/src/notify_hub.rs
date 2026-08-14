//! Broadcast hub for pushing `mining.notify` to live Stratum sessions.
//!
//! Without this, jobs are only delivered on `mining.subscribe`, so after a tip
//! change miners keep submitting the invalidated job and get 100% stale rejects.

use std::sync::atomic::{AtomicU64, Ordering};
use std::sync::{Arc, RwLock};

#[derive(Default)]
struct HubInner {
    /// Monotonic generation; each publish bumps this.
    version: u64,
    /// Latest notify JSON lines (already newline-terminated).
    lines: Vec<String>,
}

#[derive(Default)]
pub struct NotifyHub {
    inner: RwLock<HubInner>,
    /// Cheap signal for readers without taking the write lock.
    version: AtomicU64,
}

pub type SharedNotifyHub = Arc<NotifyHub>;

impl NotifyHub {
    pub fn new() -> Self {
        Self::default()
    }

    pub fn version(&self) -> u64 {
        self.version.load(Ordering::Acquire)
    }

    /// Replace the broadcast payload with new notify lines (`clean_jobs` already set).
    pub fn publish_lines(&self, lines: Vec<String>) {
        if lines.is_empty() {
            return;
        }
        if let Ok(mut inner) = self.inner.write() {
            inner.version = inner.version.saturating_add(1);
            inner.lines = lines;
            self.version.store(inner.version, Ordering::Release);
        }
    }

    /// If `seen_version` is behind, return new lines and the updated version.
    pub fn take_since(&self, seen_version: u64) -> Option<(u64, Vec<String>)> {
        let current = self.version();
        if current <= seen_version {
            return None;
        }
        let inner = self.inner.read().ok()?;
        if inner.version <= seen_version {
            return None;
        }
        Some((inner.version, inner.lines.clone()))
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn publish_bumps_version_and_delivers_once() {
        let hub = NotifyHub::new();
        assert!(hub.take_since(0).is_none());
        hub.publish_lines(vec!["notify1\n".into()]);
        let (v, lines) = hub.take_since(0).unwrap();
        assert_eq!(lines.len(), 1);
        assert!(hub.take_since(v).is_none());
        hub.publish_lines(vec!["notify2\n".into()]);
        let (v2, lines2) = hub.take_since(v).unwrap();
        assert!(v2 > v);
        assert_eq!(lines2[0], "notify2\n");
    }
}
