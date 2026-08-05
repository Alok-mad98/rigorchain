use uuid::Uuid;

use crate::constants::{AUTHORIZATION_TTL_SECS, REPLAY_CERT_TAG, RES_PROBABILITY_TARGET};
use crate::draw::DrawReplayInput;
use crate::error::{WinnerError, WinnerResult};
use crate::hash::{native_to_display_hex, parse_prev_nbits};
use crate::types::{
    AuthorizationContext, CandidateBlockStatus, MintAuthorizationToken, WinnerJobStatus,
};

/// Evaluate AUTH-01..AUTH-15 and issue a single-use token when all pass.
pub fn authorize_mint(ctx: &AuthorizationContext) -> WinnerResult<MintAuthorizationToken> {
    validate_authorization(ctx)?;

    let winner_index = ctx.draw.winner_index;
    let winner = ctx
        .draw
        .candidates
        .get(winner_index as usize)
        .ok_or_else(|| WinnerError::Forbidden("winner index out of range".into()))?;
    let res_payout_program = *ctx
        .candidate_set
        .res_programs
        .get(winner_index as usize)
        .ok_or_else(|| {
            WinnerError::Forbidden("A-5: winner declared no RES payout program".into())
        })?;

    let now = ctx.now_unix;
    Ok(MintAuthorizationToken {
        winner_job_id: ctx.candidate_block.id,
        candidate_block_id: ctx.candidate_block.id,
        draw_winner_txid: winner.txid,
        draw_winner_txid_display: winner.txid_display.clone(),
        convergence_root: ctx.draw.convergence_root,
        convergence_root_display: ctx.draw.convergence_root_display.clone(),
        // The PCORB `bind_hash` is B_{h-1} itself; it needs no derivation and no RPC round
        // trip, so it is set here rather than attached by a later step that could omit it.
        bind_hash: ctx.candidate_set.sigma_seed,
        bind_hash_display: native_to_display_hex(&ctx.candidate_set.sigma_seed),
        target_prev: parse_prev_nbits(&ctx.template_prev_nbits)?,
        height: ctx.candidate_block.height as u32,
        winner_convergence_amount: winner.convergence_amount,
        res_payout_program,
        res_probability_target: RES_PROBABILITY_TARGET,
        authorized_at: now,
        expires_at: now + AUTHORIZATION_TTL_SECS,
        token_nonce: Uuid::new_v4(),
        replay_cert_tag: REPLAY_CERT_TAG.to_string(),
        status: WinnerJobStatus::Authorized,
    })
}

pub fn validate_authorization(ctx: &AuthorizationContext) -> WinnerResult<()> {
    // AUTH-01 / AUTH-02
    let winner_index = ctx.draw.winner_index;
    if winner_index < 0 {
        return Err(WinnerError::Forbidden("FORB-02: no eligible winner".into()));
    }
    let winner = ctx.draw.candidates.get(winner_index as usize).ok_or_else(|| {
        WinnerError::Forbidden("AUTH-01: winner index invalid".into())
    })?;
    if !winner.draw_eligible {
        return Err(WinnerError::Forbidden("AUTH-02: winner not eligible".into()));
    }
    if ctx.draw.draw_winner_txid.is_none() {
        return Err(WinnerError::Forbidden("AUTH-01: no draw_winner_txid".into()));
    }

    // AUTH-03: the replayed draw must be over the set the node says the parent committed.
    // The block's own coinbase root is a different set and is deliberately not compared here.
    if ctx.draw.convergence_root != ctx.node_draw_convergence_root {
        return Err(WinnerError::Forbidden("AUTH-03: parent draw root mismatch".into()));
    }

    // AUTH-04: the winner must be a member of the parent draw set, never of this block's body.
    if !ctx
        .candidate_set
        .txids_ordered
        .get(winner_index as usize)
        .is_some_and(|txid| *txid == winner.txid)
    {
        return Err(WinnerError::Forbidden(
            "AUTH-04: winner is not the candidate at that index in the parent draw set".into(),
        ));
    }

    // AUTH-05
    if ctx.candidate_block.tip_hash != ctx.current_tip {
        return Err(WinnerError::Forbidden("AUTH-05: tip mismatch".into()));
    }

    // AUTH-06
    if ctx.candidate_set.height != ctx.candidate_block.height {
        return Err(WinnerError::Forbidden("AUTH-06: height mismatch".into()));
    }

    // AUTH-07: `target_prev` is a witness field checked against the parent header, so the value
    // the pool would write must be the one the node read off that header. It no longer feeds σ.
    if parse_prev_nbits(&ctx.template_prev_nbits)? != ctx.candidate_set.target_prev {
        return Err(WinnerError::Forbidden(
            "AUTH-07: template prev_nbits disagrees with the parent header nBits".into(),
        ));
    }

    // AUTH-10
    if !ctx.candidate_block.mint_required || !ctx.candidate_block.body_mint_reserved {
        return Err(WinnerError::Forbidden("AUTH-10: mint slot not reserved".into()));
    }

    // AUTH-11
    if ctx.candidate_block.status != CandidateBlockStatus::Active {
        return Err(WinnerError::Forbidden("FORB-08: job not ACTIVE".into()));
    }

    // AUTH-12 checked at consume time via expires_at

    // AUTH-13
    if !ctx.treasury_available {
        return Err(WinnerError::Forbidden("FORB-09: treasury unavailable".into()));
    }

    // AUTH-14
    if ctx.paused_new_jobs {
        return Err(WinnerError::Forbidden("AUTH-14: PAUSED_NEW_JOBS".into()));
    }

    Ok(())
}

/// Consume token: reject if expired or nonce reused (AUTH-12, AUTH-15).
pub fn validate_token_consumption(
    token: &MintAuthorizationToken,
    now_unix: u64,
    consumed_nonces: &[Uuid],
) -> WinnerResult<()> {
    if token.status != WinnerJobStatus::Authorized {
        return Err(WinnerError::Forbidden("token not AUTHORIZED".into()));
    }
    if now_unix >= token.expires_at {
        return Err(WinnerError::Forbidden("AUTH-12: token expired".into()));
    }
    if consumed_nonces.contains(&token.token_nonce) {
        return Err(WinnerError::Forbidden("AUTH-15: token nonce reused".into()));
    }
    // No block above genesis has an all-zero parent, so a zero seed means the token was built
    // without a real draw.
    if token.bind_hash == [0u8; 32] {
        return Err(WinnerError::Forbidden("AUTH-08: bind_hash unset".into()));
    }
    if token.res_payout_program == [0u8; 32] {
        return Err(WinnerError::Forbidden("A-5: res_payout_program unset".into()));
    }
    Ok(())
}

/// Replay the parent's draw from an extracted candidate set.
pub fn replay_draw_from_candidate_set(
    set: &crate::types::CandidateSet,
    res_probability_target: [u8; 32],
) -> crate::types::DrawReplayResult {
    let input = DrawReplayInput {
        candidate_txids: set.txids_ordered.clone(),
        weights: set.weights.clone(),
        prev_block_hash: set.sigma_seed,
        payout_height: set.height as u32,
        res_probability_target,
    };
    crate::draw::replay_convergence_draw(&input)
}
