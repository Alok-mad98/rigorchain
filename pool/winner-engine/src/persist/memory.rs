use std::collections::{HashMap, HashSet};
use std::sync::Mutex;

use uuid::Uuid;

use crate::error::{WinnerError, WinnerResult};
use crate::persist::traits::{
    CandidateRepository, MintAuthorizationRepository, WinnerRepository,
};
use crate::types::{
    CandidateBlockRecord, CandidateBlockStatus, MintAuthorizationToken, WinnerJobRecord,
    WinnerJobStatus,
};

#[derive(Default)]
pub struct InMemoryWinnerStore {
    inner: Mutex<StoreInner>,
}

#[derive(Default)]
struct StoreInner {
    next_winner_id: i64,
    next_candidate_id: i64,
    winner_jobs: HashMap<i64, WinnerJobRecord>,
    winner_by_candidate: HashMap<i64, i64>,
    tokens: HashMap<Uuid, MintAuthorizationToken>,
    consumed_nonces: HashSet<Uuid>,
    candidates: HashMap<i64, CandidateBlockRecord>,
}

impl InMemoryWinnerStore {
    pub fn new() -> Self {
        Self::default()
    }
}

impl WinnerRepository for InMemoryWinnerStore {
    fn insert_winner_job(&mut self, mut job: WinnerJobRecord) -> WinnerResult<i64> {
        let mut g = self.inner.lock().map_err(|e| WinnerError::Persist(e.to_string()))?;
        g.next_winner_id += 1;
        job.id = g.next_winner_id;
        let id = job.id;
        g.winner_by_candidate.insert(job.candidate_block_id, id);
        g.winner_jobs.insert(id, job);
        Ok(id)
    }

    fn update_status(&mut self, id: i64, status: WinnerJobStatus) -> WinnerResult<()> {
        let mut g = self.inner.lock().map_err(|e| WinnerError::Persist(e.to_string()))?;
        let job = g
            .winner_jobs
            .get_mut(&id)
            .ok_or_else(|| WinnerError::Persist(format!("winner job {id} not found")))?;
        job.status = status;
        Ok(())
    }

    fn get_by_candidate_block(&self, candidate_block_id: i64) -> WinnerResult<Option<WinnerJobRecord>> {
        let g = self.inner.lock().map_err(|e| WinnerError::Persist(e.to_string()))?;
        Ok(g
            .winner_by_candidate
            .get(&candidate_block_id)
            .and_then(|id| g.winner_jobs.get(id).cloned()))
    }

    fn list_by_status(&self, status: WinnerJobStatus) -> WinnerResult<Vec<WinnerJobRecord>> {
        let g = self.inner.lock().map_err(|e| WinnerError::Persist(e.to_string()))?;
        Ok(g
            .winner_jobs
            .values()
            .filter(|j| j.status == status)
            .cloned()
            .collect())
    }
}

impl MintAuthorizationRepository for InMemoryWinnerStore {
    fn save_token(&mut self, token: MintAuthorizationToken) -> WinnerResult<()> {
        let mut g = self.inner.lock().map_err(|e| WinnerError::Persist(e.to_string()))?;
        g.tokens.insert(token.token_nonce, token);
        Ok(())
    }

    fn get_token(&self, nonce: Uuid) -> WinnerResult<Option<MintAuthorizationToken>> {
        let g = self.inner.lock().map_err(|e| WinnerError::Persist(e.to_string()))?;
        Ok(g.tokens.get(&nonce).cloned())
    }

    fn revoke_by_candidate_block(&mut self, candidate_block_id: i64) -> WinnerResult<usize> {
        let mut g = self.inner.lock().map_err(|e| WinnerError::Persist(e.to_string()))?;
        let mut count = 0;
        for token in g.tokens.values_mut() {
            if token.candidate_block_id == candidate_block_id
                && token.status == WinnerJobStatus::Authorized
            {
                token.status = WinnerJobStatus::Revoked;
                count += 1;
            }
        }
        Ok(count)
    }

    fn is_nonce_consumed(&self, nonce: Uuid) -> bool {
        self.inner
            .lock()
            .map(|g| g.consumed_nonces.contains(&nonce))
            .unwrap_or(false)
    }

    fn mark_nonce_consumed(&mut self, nonce: Uuid) -> WinnerResult<()> {
        let mut g = self.inner.lock().map_err(|e| WinnerError::Persist(e.to_string()))?;
        g.consumed_nonces.insert(nonce);
        Ok(())
    }

    fn get_token_by_candidate_block(
        &self,
        candidate_block_id: i64,
    ) -> WinnerResult<Option<MintAuthorizationToken>> {
        let g = self.inner.lock().map_err(|e| WinnerError::Persist(e.to_string()))?;
        Ok(g
            .tokens
            .values()
            .find(|t| t.candidate_block_id == candidate_block_id)
            .cloned())
    }
}

impl CandidateRepository for InMemoryWinnerStore {
    fn insert_candidate(&mut self, mut block: CandidateBlockRecord) -> WinnerResult<i64> {
        let mut g = self.inner.lock().map_err(|e| WinnerError::Persist(e.to_string()))?;
        g.next_candidate_id += 1;
        block.id = g.next_candidate_id;
        let id = block.id;
        g.candidates.insert(id, block);
        Ok(id)
    }

    fn update_status(&mut self, id: i64, status: CandidateBlockStatus) -> WinnerResult<()> {
        let mut g = self.inner.lock().map_err(|e| WinnerError::Persist(e.to_string()))?;
        let block = g
            .candidates
            .get_mut(&id)
            .ok_or_else(|| WinnerError::Persist(format!("candidate block {id} not found")))?;
        block.status = status;
        Ok(())
    }

    fn get(&self, id: i64) -> WinnerResult<Option<CandidateBlockRecord>> {
        let g = self.inner.lock().map_err(|e| WinnerError::Persist(e.to_string()))?;
        Ok(g.candidates.get(&id).cloned())
    }

    fn list_active(&self) -> WinnerResult<Vec<CandidateBlockRecord>> {
        let g = self.inner.lock().map_err(|e| WinnerError::Persist(e.to_string()))?;
        Ok(g
            .candidates
            .values()
            .filter(|b| b.status == CandidateBlockStatus::Active)
            .cloned()
            .collect())
    }
}
