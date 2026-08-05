use crate::error::{WinnerError, WinnerResult};
use crate::types::{CandidateBlockStatus, MintAuthorizationToken, WinnerJobRecord, WinnerJobStatus};

#[derive(Clone, Debug, PartialEq, Eq)]
pub enum RevocationReason {
    Reorg,
    TemplateInvalidation,
    TipMismatch,
    WinnerMismatch,
    Timeout,
    Manual,
}

/// Mark winner job stale and forbid further authorization.
pub fn invalidate_winner(job: &mut WinnerJobRecord, reason: RevocationReason) {
    job.status = match reason {
        RevocationReason::WinnerMismatch => WinnerJobStatus::Forbidden,
        _ => WinnerJobStatus::Stale,
    };
}

/// Revoke an issued authorization token.
pub fn revoke_authorization(
    token: &mut MintAuthorizationToken,
    reason: RevocationReason,
) -> WinnerResult<()> {
    if token.status != WinnerJobStatus::Authorized {
        return Err(WinnerError::Validation(format!(
            "cannot revoke token in status {:?}",
            token.status
        )));
    }
    token.status = WinnerJobStatus::Revoked;
    let _ = reason;
    Ok(())
}

/// Orphan active candidate blocks on tip change.
pub fn orphan_candidates_on_reorg(
    blocks: &mut [crate::types::CandidateBlockRecord],
    old_tip_prefix: &str,
) -> Vec<i64> {
    let mut orphaned = Vec::new();
    for block in blocks.iter_mut() {
        if block.status == CandidateBlockStatus::Active
            && (block.prev_block_hash.starts_with(old_tip_prefix)
                || block.tip_hash.starts_with(old_tip_prefix))
        {
            block.status = CandidateBlockStatus::Orphaned;
            orphaned.push(block.id);
        }
    }
    orphaned
}

/// Revoke all authorized tokens matching orphaned jobs.
pub fn revoke_tokens_for_jobs(
    tokens: &mut [MintAuthorizationToken],
    job_ids: &[i64],
) -> usize {
    let mut count = 0;
    for token in tokens.iter_mut() {
        if token.status == WinnerJobStatus::Authorized && job_ids.contains(&token.candidate_block_id)
        {
            token.status = WinnerJobStatus::Revoked;
            count += 1;
        }
    }
    count
}

/// Detect winner drift between replay and expected txid.
pub fn check_winner_mismatch(
    replay_winner_display: Option<&str>,
    expected_display: &str,
) -> bool {
    match replay_winner_display {
        Some(w) => !w.eq_ignore_ascii_case(expected_display),
        None => true,
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::types::{DrawReplayResult, ValidationMethod};
    use uuid::Uuid;

    fn sample_job() -> WinnerJobRecord {
        WinnerJobRecord {
            id: 1,
            candidate_block_id: 1,
            status: WinnerJobStatus::Provisional,
            mint_required: true,
            eligible_count: 1,
            convergence_root_display: "aa".repeat(64),
            winner_txid_display: Some("bb".repeat(64)),
            preview_audit: DrawReplayResult {
                convergence_root: [0; 32],
                convergence_root_display: String::new(),
                draw_winner_txid: None,
                draw_winner_txid_display: None,
                winner_index: -1,
                eligible_count: 0,
                mint_required: false,
                candidates: vec![],
                validation_method: ValidationMethod::Replay,
            },
            token_nonce: None,
        }
    }

    #[test]
    fn invalidate_sets_stale() {
        let mut job = sample_job();
        invalidate_winner(&mut job, RevocationReason::Reorg);
        assert_eq!(job.status, WinnerJobStatus::Stale);
    }

    #[test]
    fn revoke_authorized_token() {
        let mut token = MintAuthorizationToken {
            winner_job_id: 1,
            candidate_block_id: 1,
            draw_winner_txid: [1; 32],
            draw_winner_txid_display: "aa".repeat(64),
            convergence_root: [2; 32],
            convergence_root_display: "bb".repeat(64),
            bind_hash: [3; 32],
            bind_hash_display: "cc".repeat(64),
            target_prev: 0x1d00ffff,
            height: 10,
            winner_convergence_amount: 100,
            res_payout_program: [4; 32],
            res_probability_target: [0; 32],
            authorized_at: 0,
            expires_at: 100,
            token_nonce: Uuid::new_v4(),
            replay_cert_tag: "test".into(),
            status: WinnerJobStatus::Authorized,
        };
        revoke_authorization(&mut token, RevocationReason::Reorg).unwrap();
        assert_eq!(token.status, WinnerJobStatus::Revoked);
    }
}
