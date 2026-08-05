use std::collections::HashMap;
use std::sync::{Mutex, MutexGuard};

use crate::error::{MintError, MintResult};
use crate::types::{BodyMintArtifact, MintJobRef, MintRecord, MintState};

pub struct MintStateStore {
    inner: Mutex<HashMap<String, MintRecord>>,
}

impl Default for MintStateStore {
    fn default() -> Self {
        Self {
            inner: Mutex::new(HashMap::new()),
        }
    }
}

impl MintStateStore {
    pub fn new() -> Self {
        Self::default()
    }

    fn lock(&self) -> MintResult<MutexGuard<'_, HashMap<String, MintRecord>>> {
        self.inner
            .lock()
            .map_err(|_| MintError::State("mint state lock poisoned".into()))
    }

    pub fn register_built(
        &self,
        mint_job: MintJobRef,
        artifact: BodyMintArtifact,
        now: u64,
    ) -> MintResult<MintRecord> {
        let mut guard = self.lock()?;
        let record = MintRecord {
            mint_job: mint_job.clone(),
            state: MintState::Built,
            artifact: artifact.clone(),
            created_at: now,
            state_changed_at: now,
        };
        guard.insert(mint_job.job_id.clone(), record.clone());
        Ok(record)
    }

    pub fn mark_submitted(&self, job_id: &str, now: u64) -> MintResult<MintRecord> {
        let mut guard = self.lock()?;
        let rec = guard
            .get_mut(job_id)
            .ok_or_else(|| MintError::State(format!("no mint record for job {job_id}")))?;
        rec.state = MintState::Submitted;
        rec.state_changed_at = now;
        Ok(rec.clone())
    }

    pub fn revoke_for_jobs(&self, job_ids: &[String], now: u64) -> MintResult<Vec<MintRecord>> {
        let mut guard = self.lock()?;
        let mut revoked = Vec::new();
        for job_id in job_ids {
            if let Some(rec) = guard.get_mut(job_id) {
                if matches!(rec.state, MintState::Built | MintState::Submitted) {
                    rec.state = MintState::Revoked;
                    rec.state_changed_at = now;
                    revoked.push(rec.clone());
                }
            }
        }
        Ok(revoked)
    }

    pub fn orphan_for_jobs(&self, job_ids: &[String], now: u64) -> MintResult<Vec<MintRecord>> {
        let mut guard = self.lock()?;
        let mut orphaned = Vec::new();
        for job_id in job_ids {
            if let Some(rec) = guard.get_mut(job_id) {
                if matches!(rec.state, MintState::Submitted) {
                    rec.state = MintState::Orphaned;
                    rec.state_changed_at = now;
                    orphaned.push(rec.clone());
                }
            }
        }
        Ok(orphaned)
    }

    pub fn get(&self, job_id: &str) -> MintResult<Option<MintRecord>> {
        let guard = self.lock()?;
        Ok(guard.get(job_id).cloned())
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::types::RES_REWARD_ATOMS;
    use rigor_gbt::RecordId;

    fn sample_artifact() -> BodyMintArtifact {
        BodyMintArtifact {
            body_mint_txid: "dd".repeat(32),
            raw_tx_hex: "0100".into(),
            amount_atoms: RES_REWARD_ATOMS,
            winner_txid: "aa".repeat(32),
            witness_item_count: 12,
            sigma_digest_hex: "11".repeat(64),
        }
    }

    #[test]
    fn state_transitions_on_reorg() {
        let store = MintStateStore::new();
        let job = MintJobRef {
            winner_job_id: None,
            job_id: "j1".into(),
            record_id: RecordId::new_v4(),
            winner_txid: "aa".repeat(32),
        };
        store.register_built(job.clone(), sample_artifact(), 1).unwrap();
        store.mark_submitted("j1", 2).unwrap();
        let revoked = store.revoke_for_jobs(&["j1".into()], 3).unwrap();
        assert_eq!(revoked.len(), 1);
        assert_eq!(revoked[0].state, MintState::Revoked);
    }
}
