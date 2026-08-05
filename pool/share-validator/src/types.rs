use block_assembler::JobId;
use rigor_gbt::RecordId;
use serde::{Deserialize, Serialize};

use crate::error::ValidationErrorCode;

/// Miner share submission (Stratum `mining.submit` materialized).
#[derive(Clone, Debug, Serialize, Deserialize, PartialEq)]
pub struct ShareSubmission {
    pub share_id: String,
    pub job_id: JobId,
    pub record_id: Option<RecordId>,
    pub extranonce1: Vec<u8>,
    pub extranonce2: Vec<u8>,
    pub ntime: u32,
    pub nonce: u32,
    /// Pool share target (LE uint256); `pow_hash` must be `<= share_target`.
    pub share_target: [u8; 32],
}

/// Full block candidate for `submitblock` path.
#[derive(Clone, Debug, Serialize, Deserialize, PartialEq)]
pub struct BlockSubmission {
    pub job_id: JobId,
    pub record_id: Option<RecordId>,
    /// Serialized block hex (header + vtx).
    pub block_hex: String,
    /// Only `true` for Block Assembler output path; miner shares must leave this `false`.
    pub allow_res_body_mint: bool,
}

#[derive(Clone, Copy, Debug, PartialEq, Eq, Serialize, Deserialize)]
#[serde(rename_all = "SCREAMING_SNAKE_CASE")]
pub enum ValidationKind {
    Share,
    Block,
}

#[derive(Clone, Debug, PartialEq, Eq, Serialize, Deserialize)]
pub struct ValidationResult {
    pub accepted: bool,
    pub kind: ValidationKind,
    pub job_id: JobId,
    pub share_id: Option<String>,
    pub error_code: Option<ValidationErrorCode>,
    pub message: Option<String>,
    pub pow_hash: Option<[u8; 32]>,
    pub is_block_candidate: bool,
}

impl ValidationResult {
    pub fn accepted_share(job_id: JobId, share_id: String, pow_hash: [u8; 32]) -> Self {
        Self {
            accepted: true,
            kind: ValidationKind::Share,
            job_id,
            share_id: Some(share_id),
            error_code: None,
            message: None,
            pow_hash: Some(pow_hash),
            is_block_candidate: false,
        }
    }

    pub fn accepted_block(job_id: JobId, pow_hash: [u8; 32]) -> Self {
        Self {
            accepted: true,
            kind: ValidationKind::Block,
            job_id,
            share_id: None,
            error_code: None,
            message: None,
            pow_hash: Some(pow_hash),
            is_block_candidate: true,
        }
    }

    pub fn rejected(
        kind: ValidationKind,
        job_id: JobId,
        share_id: Option<String>,
        code: ValidationErrorCode,
        message: impl Into<String>,
    ) -> Self {
        Self {
            accepted: false,
            kind,
            job_id,
            share_id,
            error_code: Some(code),
            message: Some(message.into()),
            pow_hash: None,
            is_block_candidate: false,
        }
    }
}

#[derive(Clone, Debug, Serialize, Deserialize, PartialEq)]
pub struct JobValidationState {
    pub job_id: JobId,
    pub accepted_shares: u64,
    pub rejected_shares: u64,
    pub last_share_id: Option<String>,
    pub last_pow_hash: Option<String>,
}
