//! RES body mint — production path via `mint-engine` + Winner Engine auth (TASK-MINING-018).

pub use mint_engine::{
    BodyMintArtifact, MintBlockContext, MintEngine, MintEngineConfig, MintOutcome,
    MintWinnerContext, RES_REWARD_ATOMS,
};

use winner_engine::MintAuthorizationToken;

use crate::winner::WinnerPreview;

pub fn winner_context_from_preview(preview: &WinnerPreview) -> MintWinnerContext {
    MintWinnerContext {
        mint_required: preview.mint_required,
        winner_txid: preview.winner_txid.clone(),
        convergence_root_hex: preview.convergence_root_hex.clone(),
        draw_target_prev_nbits: preview.draw_target_prev_nbits.clone(),
        authorized_bind_hash: None,
        authorized_bind_hash_display: None,
    }
}

pub fn winner_context_from_token(token: &MintAuthorizationToken) -> MintWinnerContext {
    MintWinnerContext {
        mint_required: true,
        winner_txid: Some(token.draw_winner_txid_display.clone()),
        convergence_root_hex: token.convergence_root_display.clone(),
        draw_target_prev_nbits: format!("{:08x}", token.target_prev),
        authorized_bind_hash: Some(token.bind_hash),
        authorized_bind_hash_display: Some(token.bind_hash_display.clone()),
    }
}

pub fn validate_mint_integrity(mint: &BodyMintArtifact) -> bool {
    !mint.body_mint_txid.is_empty() && mint.amount_atoms == RES_REWARD_ATOMS
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::winner::WinnerPreview;
    use winner_engine::{WinnerJobStatus, RES_PROBABILITY_TARGET};
    use uuid::Uuid;

    #[test]
    fn winner_context_maps_preview() {
        let preview = WinnerPreview {
            mint_required: true,
            winner_txid: Some("aa".repeat(32)),
            convergence_root_hex: "11".repeat(64),
            draw_target_prev_nbits: "1c00ff00".into(),
        };
        let ctx = winner_context_from_preview(&preview);
        assert!(ctx.mint_required);
        assert_eq!(ctx.winner_txid.as_deref(), Some("aa".repeat(32).as_str()));
    }

    #[test]
    fn winner_context_maps_authorization_token() {
        let token = MintAuthorizationToken {
            winner_job_id: 1,
            candidate_block_id: 1,
            draw_winner_txid: [0xaa; 32],
            draw_winner_txid_display: "aa".repeat(64),
            convergence_root: [0x11; 32],
            convergence_root_display: "11".repeat(64),
            bind_hash: [0xbb; 32],
            bind_hash_display: "bb".repeat(64),
            target_prev: 0x1c00ff00,
            height: 1,
            winner_convergence_amount: 100_000_000,
            res_payout_program: [0xcc; 32],
            res_probability_target: RES_PROBABILITY_TARGET,
            authorized_at: 1,
            expires_at: 100,
            token_nonce: Uuid::new_v4(),
            replay_cert_tag: "tag".into(),
            status: WinnerJobStatus::Authorized,
        };
        let ctx = winner_context_from_token(&token);
        assert!(ctx.mint_required);
        assert_eq!(ctx.winner_txid.as_deref(), Some(token.draw_winner_txid_display.as_str()));
    }
}
