use block_assembler::{JobId, PublishedJobSnapshot};
use std::collections::HashSet;
use std::sync::{Arc, RwLock};

use crate::error::{ValidationErrorCode, ValidatorError, ValidatorResult};
use crate::types::JobValidationState;

/// In-memory replay protection with optional persistence hook.
pub struct ReplayGuard {
    seen_share_ids: RwLock<HashSet<String>>,
    job_stats: RwLock<std::collections::HashMap<JobId, JobValidationState>>,
}

impl ReplayGuard {
    pub fn new() -> Self {
        Self {
            seen_share_ids: RwLock::new(HashSet::new()),
            job_stats: RwLock::new(std::collections::HashMap::new()),
        }
    }

    pub fn check_and_record_share(
        &self,
        share_id: &str,
        job: &PublishedJobSnapshot,
    ) -> ValidatorResult<()> {
        if !job.state.is_servable() {
            return Err(ValidatorError::validation(
                ValidationErrorCode::ErrStaleJob,
                format!("job {:?} is not ACTIVE", job.state),
            ));
        }

        let mut seen = self.seen_share_ids.write().map_err(lock_err)?;
        if !seen.insert(share_id.to_string()) {
            return Err(ValidatorError::validation(
                ValidationErrorCode::ErrReplayShare,
                format!("duplicate share_id {share_id}"),
            ));
        }
        Ok(())
    }

    pub fn record_accepted_share(&self, share_id: &str, job_id: &JobId, pow_hash: &[u8; 32]) {
        if let Ok(mut stats) = self.job_stats.write() {
            let entry = stats.entry(job_id.clone()).or_insert_with(|| JobValidationState {
                job_id: job_id.clone(),
                accepted_shares: 0,
                rejected_shares: 0,
                last_share_id: None,
                last_pow_hash: None,
            });
            entry.accepted_shares += 1;
            entry.last_share_id = Some(share_id.to_string());
            entry.last_pow_hash = Some(hex::encode(pow_hash));
        }
    }

    pub fn record_rejected_share(&self, job_id: &JobId) {
        if let Ok(mut stats) = self.job_stats.write() {
            let entry = stats.entry(job_id.clone()).or_insert_with(|| JobValidationState {
                job_id: job_id.clone(),
                accepted_shares: 0,
                rejected_shares: 0,
                last_share_id: None,
                last_pow_hash: None,
            });
            entry.rejected_shares += 1;
        }
    }

    pub fn get_validation_state(&self, job_id: &JobId) -> Option<JobValidationState> {
        self.job_stats.read().ok()?.get(job_id).cloned()
    }

    pub fn is_share_seen(&self, share_id: &str) -> bool {
        self.seen_share_ids
            .read()
            .map(|s| s.contains(share_id))
            .unwrap_or(false)
    }

    /// Hook for future persistence layer.
    pub fn export_seen_shares(&self) -> Vec<String> {
        self.seen_share_ids
            .read()
            .map(|s| s.iter().cloned().collect())
            .unwrap_or_default()
    }

    pub fn import_seen_shares(&self, ids: impl IntoIterator<Item = String>) {
        if let Ok(mut seen) = self.seen_share_ids.write() {
            seen.extend(ids);
        }
    }
}

impl Default for ReplayGuard {
    fn default() -> Self {
        Self::new()
    }
}

pub type SharedReplayGuard = Arc<ReplayGuard>;

fn lock_err<E: std::fmt::Display>(e: E) -> ValidatorError {
    ValidatorError::Internal(format!("lock poisoned: {e}"))
}

#[cfg(test)]
mod tests {
    use super::*;
    use block_assembler::{
        BodyMintPlaceholder, CandidateBlockId, CoinbaseInfo, CommitmentInfo, JobState,
    };
    use rigor_gbt::{RecordId, TemplateVersion, TipHash};

    fn dummy_job() -> PublishedJobSnapshot {
        PublishedJobSnapshot {
            job_id: JobId("0000000000000001".into()),
            candidate_block_id: CandidateBlockId(1),
            record_id: RecordId::new_v4(),
            tip_hash: TipHash("aa".repeat(32)),
            template_version: TemplateVersion::INITIAL,
            previousblockhash: "00".repeat(32),
            height: 1,
            state: JobState::Active,
            coinbase: CoinbaseInfo {
                coinb1_hex: "01".into(),
                coinb2_hex: "02".into(),
                extranonce2_size: 4,
                scriptsig_hex: "03".into(),
                convergence_root_hex: "00".repeat(64),
                payout_script_hex: "76".into(),
                coinbase_value: 50,
            },
            commitments: CommitmentInfo {
                cvge_script_hex: None,
                rigor_script_hex: "6a08".into(),
                witness_commitment_script_hex: None,
                prev_nbits_hex: "1c00ff00".into(),
            },
            body_mint: BodyMintPlaceholder {
                slot_index: 2,
                reserved: false,
                mint_required: false,
                body_mint_txid: None,
                placeholder_txid: String::new(),
            },
            merkle_branch: vec![],
            version: 0x20000000,
            nbits: "207fffff".into(),
            ntime: 0,
            prevhash: "00".repeat(32),
            block_target: "207fffff".into(),
            assembly_hash: [0u8; 32],
            created_at: 0,
            state_changed_at: 0,
        }
    }

    #[test]
    fn rejects_duplicate_share_id() {
        let guard = ReplayGuard::new();
        let job = dummy_job();
        guard.check_and_record_share("s1", &job).unwrap();
        let err = guard.check_and_record_share("s1", &job).unwrap_err();
        assert!(matches!(
            err,
            ValidatorError::Validation {
                code: ValidationErrorCode::ErrReplayShare,
                ..
            }
        ));
    }

    #[test]
    fn rejects_stale_job() {
        let guard = ReplayGuard::new();
        let mut job = dummy_job();
        job.state = JobState::Invalidated;
        let err = guard.check_and_record_share("s2", &job).unwrap_err();
        assert!(matches!(
            err,
            ValidatorError::Validation {
                code: ValidationErrorCode::ErrStaleJob,
                ..
            }
        ));
    }
}
