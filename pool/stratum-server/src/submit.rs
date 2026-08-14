//! Block submission sink (C-5) — independent of RES mint (Option A).
//!
//! Found blocks must reach the node whether or not a RES body mint exists in the
//! opaque GBT package. Miner credit is booked only after the node accepts.

use std::sync::Mutex;

use crate::error::{StratumError, StratumResult};

/// Sink for a fully assembled, PoW-solved block.
pub trait BlockSubmitter: Send + Sync {
    /// `Ok(())` means the node accepted the block.
    fn submit_block(&self, block_hex: &str) -> StratumResult<()>;

    /// Human-readable endpoint description for audit records.
    fn describe(&self) -> String;
}

/// Fail-closed default: no node endpoint configured.
#[derive(Clone, Copy, Debug, Default)]
pub struct UnconfiguredBlockSubmitter;

impl BlockSubmitter for UnconfiguredBlockSubmitter {
    fn submit_block(&self, _block_hex: &str) -> StratumResult<()> {
        Err(StratumError::Internal(
            "C-5: no rigord submitblock endpoint configured; refusing to credit an unsubmitted block"
                .into(),
        ))
    }

    fn describe(&self) -> String {
        "unconfigured".into()
    }
}

/// Submits via `submitblock` JSON-RPC against rigord.
pub struct RpcBlockSubmitter {
    rpc: rigor_gbt::RigordRpc,
    url: String,
}

impl RpcBlockSubmitter {
    pub fn new(url: impl Into<String>) -> Self {
        let url = url.into();
        Self {
            rpc: rigor_gbt::RigordRpc::new(url.clone()),
            url,
        }
    }

    pub fn with_auth(mut self, user: impl Into<String>, password: impl Into<String>) -> Self {
        self.rpc = self.rpc.with_auth(user, password);
        self
    }
}

impl BlockSubmitter for RpcBlockSubmitter {
    fn submit_block(&self, block_hex: &str) -> StratumResult<()> {
        self.rpc
            .submit_block(block_hex)
            .map_err(|e| StratumError::Internal(e.to_string()))
    }

    fn describe(&self) -> String {
        format!("rpc:{}", self.url)
    }
}

/// Deterministic in-memory submitter for tests.
pub struct RecordingBlockSubmitter {
    accept: bool,
    submitted: Mutex<Vec<String>>,
}

impl RecordingBlockSubmitter {
    pub fn accepting() -> Self {
        Self {
            accept: true,
            submitted: Mutex::new(Vec::new()),
        }
    }

    pub fn rejecting() -> Self {
        Self {
            accept: false,
            submitted: Mutex::new(Vec::new()),
        }
    }

    pub fn submitted(&self) -> Vec<String> {
        self.submitted.lock().map(|v| v.clone()).unwrap_or_default()
    }

    pub fn submission_count(&self) -> usize {
        self.submitted.lock().map(|v| v.len()).unwrap_or(0)
    }
}

impl BlockSubmitter for RecordingBlockSubmitter {
    fn submit_block(&self, block_hex: &str) -> StratumResult<()> {
        if let Ok(mut v) = self.submitted.lock() {
            v.push(block_hex.to_string());
        }
        if self.accept {
            Ok(())
        } else {
            Err(StratumError::Internal(
                "recording submitter: rejected".into(),
            ))
        }
    }

    fn describe(&self) -> String {
        format!("recording(accept={})", self.accept)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn unconfigured_submitter_fails_closed() {
        let err = UnconfiguredBlockSubmitter
            .submit_block("00")
            .unwrap_err()
            .to_string();
        assert!(err.contains("C-5"));
        assert!(err.contains("refusing to credit"));
    }

    #[test]
    fn recording_submitter_tracks_attempts() {
        let ok = RecordingBlockSubmitter::accepting();
        ok.submit_block("aabb").unwrap();
        assert_eq!(ok.submitted(), vec!["aabb".to_string()]);

        let bad = RecordingBlockSubmitter::rejecting();
        assert!(bad.submit_block("ccdd").is_err());
        assert_eq!(bad.submission_count(), 1);
    }
}
