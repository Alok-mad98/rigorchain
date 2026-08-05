use uuid::Uuid;

use crate::error::WinnerResult;
use crate::types::{
    CandidateBlockRecord, MintAuthorizationToken, WinnerJobRecord, WinnerJobStatus,
};

/// Persistence boundary for `winner_jobs` (TASK-MINING-013 / TASK-MINING-017).
pub trait WinnerRepository: Send + Sync {
    fn insert_winner_job(&mut self, job: WinnerJobRecord) -> WinnerResult<i64>;
    fn update_status(&mut self, id: i64, status: WinnerJobStatus) -> WinnerResult<()>;
    fn get_by_candidate_block(&self, candidate_block_id: i64) -> WinnerResult<Option<WinnerJobRecord>>;
    fn list_by_status(&self, status: WinnerJobStatus) -> WinnerResult<Vec<WinnerJobRecord>>;
}

/// Persistence boundary for mint authorization tokens.
pub trait MintAuthorizationRepository: Send + Sync {
    fn save_token(&mut self, token: MintAuthorizationToken) -> WinnerResult<()>;
    fn get_token(&self, nonce: Uuid) -> WinnerResult<Option<MintAuthorizationToken>>;
    fn revoke_by_candidate_block(&mut self, candidate_block_id: i64) -> WinnerResult<usize>;
    fn is_nonce_consumed(&self, nonce: Uuid) -> bool;
    fn mark_nonce_consumed(&mut self, nonce: Uuid) -> WinnerResult<()>;
    fn get_token_by_candidate_block(
        &self,
        candidate_block_id: i64,
    ) -> WinnerResult<Option<MintAuthorizationToken>>;
}

/// Persistence boundary for `candidate_blocks`.
pub trait CandidateRepository: Send + Sync {
    fn insert_candidate(&mut self, block: CandidateBlockRecord) -> WinnerResult<i64>;
    fn update_status(
        &mut self,
        id: i64,
        status: crate::types::CandidateBlockStatus,
    ) -> WinnerResult<()>;
    fn get(&self, id: i64) -> WinnerResult<Option<CandidateBlockRecord>>;
    fn list_active(&self) -> WinnerResult<Vec<CandidateBlockRecord>>;
}
