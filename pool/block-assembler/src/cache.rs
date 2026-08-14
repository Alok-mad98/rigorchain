use crate::error::{AssemblerError, AssemblerResult};
use crate::types::{CandidateBlockId, JobId, JobState, PublishedJobSnapshot};
use rigor_gbt::{RecordId, TipHash};
use std::collections::HashMap;
use std::sync::{Arc, RwLock};

struct JobCacheInner {
    jobs: HashMap<JobId, Arc<PublishedJobSnapshot>>,
    by_candidate: HashMap<CandidateBlockId, JobId>,
    by_record: HashMap<RecordId, JobId>,
    history: Vec<JobId>,
    active: Vec<JobId>,
    current_tip: Option<TipHash>,
    next_serial: u64,
    used_ids: std::collections::HashSet<u64>,
}

/// Immutable JobSnapshot cache with replacement-only state transitions.
pub struct JobCache {
    inner: RwLock<JobCacheInner>,
    pub ttl_secs: u64,
    pub archive_delay_secs: u64,
}

impl JobCache {
    pub fn new() -> Self {
        Self::with_policy(300, 3600)
    }

    pub fn with_policy(ttl_secs: u64, archive_delay_secs: u64) -> Self {
        Self {
            inner: RwLock::new(JobCacheInner {
                jobs: HashMap::new(),
                by_candidate: HashMap::new(),
                by_record: HashMap::new(),
                history: Vec::new(),
                active: Vec::new(),
                current_tip: None,
                next_serial: 1,
                used_ids: std::collections::HashSet::new(),
            }),
            ttl_secs,
            archive_delay_secs,
        }
    }

    pub fn allocate_ids(&self) -> AssemblerResult<(JobId, CandidateBlockId)> {
        let mut inner = self.lock_write()?;
        let serial = inner.next_serial;
        inner.next_serial = inner.next_serial.saturating_add(1);
        if !inner.used_ids.insert(serial) {
            return Err(AssemblerError::Publish(format!(
                "job id serial collision at {serial}"
            )));
        }
        Ok((JobId(format!("{serial:016x}")), CandidateBlockId(serial)))
    }

    pub fn insert_created(&self, mut job: PublishedJobSnapshot) -> AssemblerResult<Arc<PublishedJobSnapshot>> {
        job.state = JobState::Created;
        self.insert_job(job)
    }

    pub fn publish_active(&self, mut job: PublishedJobSnapshot) -> AssemblerResult<Arc<PublishedJobSnapshot>> {
        let mut inner = self.lock_write()?;

        if inner.current_tip.as_ref() != Some(&job.tip_hash) {
            if inner.current_tip.is_some() {
                self.invalidate_all_active_stale(&mut inner)?;
            }
            inner.current_tip = Some(job.tip_hash.clone());
        } else {
            self.stale_same_tip_active(&mut inner, &job.tip_hash)?;
        }

        job.state = JobState::Active;
        let arc = self.store_job(&mut inner, job)?;
        inner.active.push(arc.job_id.clone());
        Ok(arc)
    }

    pub fn handle_tip_changed(&self, new_tip: TipHash) -> AssemblerResult<Vec<JobId>> {
        let mut inner = self.lock_write()?;
        let invalidated = self.invalidate_all_active_stale(&mut inner)?;
        inner.current_tip = Some(new_tip);
        inner.active.clear();
        Ok(invalidated)
    }

    pub fn handle_template_invalidated(&self, record_id: RecordId) -> AssemblerResult<Option<JobId>> {
        let mut inner = self.lock_write()?;
        let Some(job_id) = inner.by_record.get(&record_id).cloned() else {
            return Ok(None);
        };
        self.transition(&mut inner, &job_id, JobState::Invalidated)?;
        inner.active.retain(|id| id != &job_id);
        Ok(Some(job_id))
    }

    pub fn apply_ttl_expiry(&self) -> AssemblerResult<Vec<JobId>> {
        if self.ttl_secs == 0 {
            return Ok(Vec::new());
        }
        let mut inner = self.lock_write()?;
        let now = unix_now();
        let active_ids = inner.active.clone();
        let mut expired = Vec::new();
        for id in active_ids {
            if let Some(job) = inner.jobs.get(&id) {
                if job.state == JobState::Active
                    && now.saturating_sub(job.state_changed_at) >= self.ttl_secs
                {
                    self.transition(&mut inner, &id, JobState::Invalidated)?;
                    expired.push(id.clone());
                }
            }
        }
        inner.active.retain(|id| !expired.contains(id));
        Ok(expired)
    }

    pub fn apply_archival(&self) -> AssemblerResult<Vec<JobId>> {
        let mut inner = self.lock_write()?;
        let now = unix_now();
        let ids: Vec<JobId> = inner.jobs.keys().cloned().collect();
        let mut archived = Vec::new();
        for id in ids {
            let should = inner.jobs.get(&id).map(|j| {
                matches!(
                    j.state,
                    JobState::Stale | JobState::Invalidated
                ) && now.saturating_sub(j.state_changed_at) >= self.archive_delay_secs
            });
            if should == Some(true) {
                self.transition(&mut inner, &id, JobState::Archived)?;
                archived.push(id);
            }
        }
        inner.active.retain(|id| !archived.contains(id));
        Ok(archived)
    }

    pub fn get_active_jobs(&self) -> AssemblerResult<Vec<Arc<PublishedJobSnapshot>>> {
        let inner = self.lock_read()?;
        Ok(inner
            .active
            .iter()
            .filter_map(|id| inner.jobs.get(id).cloned())
            .filter(|j| j.state == JobState::Active)
            .collect())
    }

    pub fn get_job_by_id(&self, id: &JobId) -> AssemblerResult<Option<Arc<PublishedJobSnapshot>>> {
        let inner = self.lock_read()?;
        Ok(inner.jobs.get(id).cloned())
    }

    pub fn get_job_history(&self) -> AssemblerResult<Vec<Arc<PublishedJobSnapshot>>> {
        let inner = self.lock_read()?;
        Ok(inner
            .history
            .iter()
            .filter_map(|id| inner.jobs.get(id).cloned())
            .collect())
    }

    fn insert_job(&self, job: PublishedJobSnapshot) -> AssemblerResult<Arc<PublishedJobSnapshot>> {
        let mut inner = self.lock_write()?;
        self.store_job(&mut inner, job)
    }

    fn store_job(
        &self,
        inner: &mut JobCacheInner,
        job: PublishedJobSnapshot,
    ) -> AssemblerResult<Arc<PublishedJobSnapshot>> {
        let now = unix_now();
        let mut stored = job;
        stored.created_at = now;
        stored.state_changed_at = now;
        let id = stored.job_id.clone();
        let candidate = stored.candidate_block_id;
        let record = stored.record_id;
        let arc = Arc::new(stored);
        inner.jobs.insert(id.clone(), arc.clone());
        inner.by_candidate.insert(candidate, id.clone());
        inner.by_record.insert(record, id.clone());
        inner.history.push(id);
        Ok(arc)
    }

    fn stale_same_tip_active(&self, inner: &mut JobCacheInner, tip: &TipHash) -> AssemblerResult<()> {
        let stale_ids: Vec<JobId> = inner
            .active
            .iter()
            .filter_map(|id| inner.jobs.get(id))
            .filter(|j| j.tip_hash == *tip && j.state == JobState::Active)
            .map(|j| j.job_id.clone())
            .collect();
        for id in stale_ids {
            self.transition(inner, &id, JobState::Stale)?;
        }
        inner.active.retain(|id| {
            inner
                .jobs
                .get(id)
                .map(|j| j.state == JobState::Active)
                .unwrap_or(false)
        });
        Ok(())
    }

    fn invalidate_all_active_stale(&self, inner: &mut JobCacheInner) -> AssemblerResult<Vec<JobId>> {
        let ids: Vec<JobId> = inner
            .jobs
            .values()
            .filter(|j| matches!(j.state, JobState::Active | JobState::Stale | JobState::Created))
            .map(|j| j.job_id.clone())
            .collect();
        for id in &ids {
            let _ = self.transition(inner, id, JobState::Invalidated);
        }
        Ok(ids)
    }

    fn transition(
        &self,
        inner: &mut JobCacheInner,
        id: &JobId,
        new_state: JobState,
    ) -> AssemblerResult<()> {
        let existing = inner
            .jobs
            .get(id)
            .ok_or_else(|| AssemblerError::Cache(format!("job {id:?} missing")))?
            .clone();
        if existing.state == new_state {
            return Ok(());
        }
        Self::assert_transition(existing.state, new_state)?;
        let mut replacement = (*existing).clone();
        replacement.state = new_state;
        replacement.state_changed_at = unix_now();
        inner.jobs.insert(id.clone(), Arc::new(replacement));
        Ok(())
    }

    fn assert_transition(from: JobState, to: JobState) -> AssemblerResult<()> {
        let ok = matches!(
            (from, to),
            (JobState::Created, JobState::Active)
                | (JobState::Created, JobState::Invalidated)
                | (JobState::Active, JobState::Stale)
                | (JobState::Active, JobState::Invalidated)
                | (JobState::Active, JobState::Archived)
                | (JobState::Stale, JobState::Invalidated)
                | (JobState::Stale, JobState::Archived)
                | (JobState::Invalidated, JobState::Archived)
                | (JobState::Created, JobState::Stale)
        ) || from == to;
        if ok {
            Ok(())
        } else {
            Err(AssemblerError::Cache(format!(
                "illegal job transition {:?} -> {:?}",
                from, to
            )))
        }
    }

    fn lock_read(&self) -> AssemblerResult<std::sync::RwLockReadGuard<'_, JobCacheInner>> {
        self.inner
            .read()
            .map_err(|e| AssemblerError::Cache(format!("lock poisoned: {e}")))
    }

    fn lock_write(&self) -> AssemblerResult<std::sync::RwLockWriteGuard<'_, JobCacheInner>> {
        self.inner
            .write()
            .map_err(|e| AssemblerError::Cache(format!("lock poisoned: {e}")))
    }
}

fn unix_now() -> u64 {
    use std::time::{SystemTime, UNIX_EPOCH};
    SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .map(|d| d.as_secs())
        .unwrap_or(0)
}

impl Default for JobCache {
    fn default() -> Self {
        Self::new()
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::types::{BodyMintPlaceholder, CoinbaseInfo, CommitmentInfo};
    use rigor_gbt::{TemplateVersion, TipHash};

    fn dummy_job(serial: u64, tip_byte: u8) -> PublishedJobSnapshot {
        let mut bytes = [0u8; 32];
        bytes[31] = tip_byte;
        PublishedJobSnapshot {
            job_id: JobId(format!("{serial:016x}")),
            candidate_block_id: CandidateBlockId(serial),
            record_id: RecordId::new_v4(),
            tip_hash: TipHash(hex::encode(bytes)),
            template_version: TemplateVersion::INITIAL,
            previousblockhash: "00".repeat(32),
            height: 1,
            state: JobState::Created,
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
            version: 1,
            nbits: "1d00ffff".into(),
            ntime: 0,
            prevhash: "00".repeat(32),
            block_target: "1d00ffff".into(),
            assembly_hash: [0u8; 32],
            created_at: 0,
            state_changed_at: 0,
        }
    }

    #[test]
    fn job_and_candidate_ids_are_unique_and_paired() {
        let cache = JobCache::new();
        let (j1, c1) = cache.allocate_ids().unwrap();
        let (j2, c2) = cache.allocate_ids().unwrap();
        assert_ne!(j1, j2);
        assert_ne!(c1, c2);
        assert_eq!(c1.0 + 1, c2.0);
    }

    #[test]
    fn publish_stales_previous_active_on_same_tip() {
        let cache = JobCache::new();
        let j1 = dummy_job(1, 0xa);
        let id1 = j1.job_id.clone();
        cache.publish_active(j1).unwrap();
        let j2 = dummy_job(2, 0xa);
        cache.publish_active(j2).unwrap();
        let old = cache.get_job_by_id(&id1).unwrap().unwrap();
        assert_eq!(old.state, JobState::Stale);
        assert_eq!(cache.get_active_jobs().unwrap().len(), 1);
    }

    #[test]
    fn tip_change_invalidates_jobs() {
        let cache = JobCache::new();
        let j = dummy_job(1, 0xa);
        let id = j.job_id.clone();
        cache.publish_active(j).unwrap();
        let mut tip_b = [0u8; 32];
        tip_b[31] = 0xb;
        cache.handle_tip_changed(TipHash(hex::encode(tip_b))).unwrap();
        let job = cache.get_job_by_id(&id).unwrap().unwrap();
        assert_eq!(job.state, JobState::Invalidated);
        assert!(cache.get_active_jobs().unwrap().is_empty());
    }
}
