//! Block submission sink (SECURITY-001-E / C-5).
//!
//! Found blocks must reach the node **independently** of whether a RES body mint
//! was required or succeeded, and miner credit must only be booked after the node
//! has accepted the block. This module isolates that side effect behind a trait so
//! the orchestrator can never accidentally credit a miner for a block that was
//! never submitted, and so the failure mode when no endpoint is configured is
//! "refuse and alert" rather than "silently succeed".
//!
//! Consensus impact: NONE — this is an off-chain RPC client. Acceptance remains
//! the node's decision; the pool only records what the node told it.

use std::sync::Mutex;

use crate::error::{MintError, MintResult};
use crate::rpc::{RigordRpc, RigordRpcConfig};

/// Sink for a fully assembled, PoW-solved block.
pub trait BlockSubmitter: Send + Sync {
    /// `Ok(())` means the node accepted the block. Anything else MUST be treated
    /// as "not on chain" by the caller (no payout credit).
    fn submit_block(&self, block_hex: &str) -> MintResult<()>;

    /// Human-readable endpoint description for audit records.
    fn describe(&self) -> String;
}

/// Fail-closed default: no node endpoint configured, so nothing can be submitted.
///
/// This exists so that an un-configured pool loudly loses the block instead of
/// quietly booking a liability to miners for a block the network never saw.
#[derive(Clone, Copy, Debug, Default)]
pub struct UnconfiguredBlockSubmitter;

impl BlockSubmitter for UnconfiguredBlockSubmitter {
    fn submit_block(&self, _block_hex: &str) -> MintResult<()> {
        Err(MintError::Rpc(
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
    rpc: RigordRpc,
    url: String,
}

impl RpcBlockSubmitter {
    pub fn new(config: RigordRpcConfig) -> Self {
        let url = config.url.clone();
        Self {
            rpc: RigordRpc::new(config),
            url,
        }
    }
}

impl BlockSubmitter for RpcBlockSubmitter {
    fn submit_block(&self, block_hex: &str) -> MintResult<()> {
        self.rpc.submit_block(block_hex)
    }

    fn describe(&self) -> String {
        format!("rpc:{}", self.url)
    }
}

/// Deterministic in-memory submitter for tests, dry-runs and audit replay.
///
/// Records every submission attempt so a test can assert that a block was
/// actually offered to the node, and can be flipped to reject so the "node said
/// no ⇒ no credit" path is exercised.
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
    fn submit_block(&self, block_hex: &str) -> MintResult<()> {
        if let Ok(mut v) = self.submitted.lock() {
            v.push(block_hex.to_string());
        }
        if self.accept {
            Ok(())
        } else {
            Err(MintError::Rpc("recording submitter: rejected".into()))
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
        // A rejected block was still *attempted* — the caller must not credit it.
        assert_eq!(bad.submission_count(), 1);
    }
}
