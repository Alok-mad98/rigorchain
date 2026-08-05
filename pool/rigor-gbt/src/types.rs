use serde::{Deserialize, Serialize};
use std::fmt;
use uuid::Uuid;

/// Primary key for a template lineage row (equals GBT Adapter `GbtRecord.record_id`).
#[derive(Clone, Copy, PartialEq, Eq, Hash, Serialize, Deserialize)]
pub struct RecordId(pub Uuid);

impl RecordId {
    pub fn new_v4() -> Self {
        Self(Uuid::new_v4())
    }
}

impl fmt::Display for RecordId {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{}", self.0)
    }
}

impl fmt::Debug for RecordId {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "RecordId({})", self.0)
    }
}

/// Monotonic per `tip_hash`; starts at 1 on new tip.
#[derive(Clone, Copy, PartialEq, Eq, PartialOrd, Ord, Hash, Serialize, Deserialize, Debug)]
pub struct TemplateVersion(pub u32);

impl TemplateVersion {
    pub const INITIAL: Self = Self(1);

    pub fn next(self) -> Self {
        Self(self.0.saturating_add(1))
    }
}

/// `getbestblockhash` at ingest (32-byte block hash, lowercase hex).
#[derive(Clone, PartialEq, Eq, Hash, Serialize, Deserialize)]
pub struct TipHash(pub String);

impl TipHash {
    pub fn as_str(&self) -> &str {
        &self.0
    }
}

impl fmt::Debug for TipHash {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "TipHash({})", self.0)
    }
}

impl fmt::Display for TipHash {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        write!(f, "{}", self.0)
    }
}

/// Template cache lifecycle states (TASK-MINING-005 §2.2).
#[derive(Clone, Copy, PartialEq, Eq, Hash, Serialize, Deserialize, Debug)]
#[serde(rename_all = "SCREAMING_SNAKE_CASE")]
pub enum TemplateState {
    Active,
    Stale,
    Invalidated,
    Expired,
    Archived,
}

impl TemplateState {
    pub fn is_servable(&self) -> bool {
        matches!(self, TemplateState::Active)
    }

    pub fn is_terminal(&self) -> bool {
        matches!(self, TemplateState::Archived)
    }
}

/// Stratum-facing job identifier (assigned on publish; never recycled).
#[derive(Clone, PartialEq, Eq, Hash, Serialize, Deserialize, Debug)]
pub struct JobId(pub String);
