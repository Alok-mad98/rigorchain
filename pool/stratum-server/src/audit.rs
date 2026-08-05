use std::sync::{Arc, RwLock};

use serde::Serialize;

#[derive(Clone, Debug, Serialize)]
pub struct AuditEvent {
    pub timestamp: u64,
    pub category: String,
    pub message: String,
}

pub struct AuditLog {
    events: RwLock<Vec<AuditEvent>>,
    max_entries: usize,
}

impl AuditLog {
    pub fn new(max_entries: usize) -> Self {
        Self {
            events: RwLock::new(Vec::new()),
            max_entries,
        }
    }

    pub fn record(&self, category: impl Into<String>, message: impl Into<String>) {
        if let Ok(mut events) = self.events.write() {
            events.push(AuditEvent {
                timestamp: unix_now(),
                category: category.into(),
                message: message.into(),
            });
            if events.len() > self.max_entries {
                let drain = events.len() - self.max_entries;
                events.drain(0..drain);
            }
        }
    }

    pub fn history(&self) -> Vec<AuditEvent> {
        self.events.read().map(|e| e.clone()).unwrap_or_default()
    }
}

fn unix_now() -> u64 {
    use std::time::{SystemTime, UNIX_EPOCH};
    SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .map(|d| d.as_secs())
        .unwrap_or(0)
}

pub type SharedAuditLog = Arc<AuditLog>;
