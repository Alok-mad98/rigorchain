//! Leftover lottery winner-service stub. Not on the RFC-020 mining path.
//!
//! Stratum / pilot must not construct or depend on a live winner engine.

/// Empty leftover handle. The orchestrator no longer holds this type.
#[derive(Debug, Default)]
pub struct SharedWinnerService;

impl SharedWinnerService {
    pub fn memory() -> Self {
        Self
    }
}
