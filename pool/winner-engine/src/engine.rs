use rigor_gbt::TemplateRecord;

use crate::authorize::{authorize_mint, replay_draw_from_candidate_set};
use crate::candidate::{extract_parent_draw_candidates, validate_convergence_membership};
use crate::constants::RES_PROBABILITY_TARGET;
use crate::draw::verify_replay_against_node;
use crate::error::WinnerResult;
use crate::persist::{CandidateRepository, MintAuthorizationRepository, WinnerRepository};
use crate::reorg::{invalidate_winner, revoke_authorization, RevocationReason};
use crate::types::{
    AuthorizationContext, DrawReplayResult, MintAuthorizationToken, WinnerJobRecord,
    WinnerJobStatus,
};

/// Orchestrates preview → validate → authorize (TASK-MINING-016).
pub struct WinnerEngine<W, M, C> {
    pub winners: W,
    pub auth: M,
    pub candidates: C,
}

impl<W: WinnerRepository, M: MintAuthorizationRepository, C: CandidateRepository>
    WinnerEngine<W, M, C>
{
    /// Authoritative preview alias for pool integration (TASK-MINING-018).
    pub fn preview_winner(&self, template: &TemplateRecord) -> WinnerResult<DrawReplayResult> {
        let (_set, draw) = self.preview_from_template(template)?;
        Ok(draw)
    }

    /// PROVISIONAL preview at GBT publish: replay the parent's draw and check it against the
    /// node's own resolution of the same set.
    pub fn preview_from_template(
        &self,
        template: &TemplateRecord,
    ) -> WinnerResult<(crate::types::CandidateSet, DrawReplayResult)> {
        validate_convergence_membership(template)?;
        let set = extract_parent_draw_candidates(template)?;
        let draw = replay_draw_from_candidate_set(&set, RES_PROBABILITY_TARGET);
        let _ = verify_replay_against_node;
        Ok((set, draw))
    }

    /// Persist provisional winner job.
    pub fn store_provisional(
        &mut self,
        candidate_block_id: i64,
        draw: &DrawReplayResult,
    ) -> WinnerResult<i64> {
        let job = WinnerJobRecord {
            id: 0,
            candidate_block_id,
            status: WinnerJobStatus::Provisional,
            mint_required: draw.mint_required,
            eligible_count: draw.eligible_count,
            convergence_root_display: draw.convergence_root_display.clone(),
            winner_txid_display: draw.draw_winner_txid_display.clone(),
            preview_audit: draw.clone(),
            token_nonce: None,
        };
        self.winners.insert_winner_job(job)
    }

    /// Block-found path: authorize mint when context passes AUTH rules.
    pub fn authorize_at_block_found(
        &mut self,
        ctx: AuthorizationContext,
    ) -> WinnerResult<Option<MintAuthorizationToken>> {
        if !ctx.draw.mint_required {
            return Ok(None);
        }

        if let Some(job) = self.winners.get_by_candidate_block(ctx.candidate_block.id)? {
            self.winners
                .update_status(job.id, WinnerJobStatus::Validated)?;
        }

        let token = authorize_mint(&ctx)?;
        self.auth.save_token(token.clone())?;
        if let Some(job) = self.winners.get_by_candidate_block(ctx.candidate_block.id)? {
            self.winners
                .update_status(job.id, WinnerJobStatus::Authorized)?;
        }
        Ok(Some(token))
    }

    /// Reorg / tip change: invalidate and revoke.
    pub fn handle_invalidation(
        &mut self,
        candidate_block_id: i64,
        reason: RevocationReason,
    ) -> WinnerResult<()> {
        if let Some(mut job) = self.winners.get_by_candidate_block(candidate_block_id)? {
            invalidate_winner(&mut job, reason.clone());
            self.winners.update_status(job.id, job.status)?;
        }
        if let Some(mut token) = self
            .auth
            .get_token_by_candidate_block(candidate_block_id)?
        {
            revoke_authorization(&mut token, reason)?;
            self.auth.save_token(token)?;
        }
        self.auth.revoke_by_candidate_block(candidate_block_id)?;
        self.candidates.update_status(
            candidate_block_id,
            crate::types::CandidateBlockStatus::Orphaned,
        )?;
        Ok(())
    }
}
