use crate::constants::CONVERGENCE_DRAW_MIN_TXS_FOR_RES;
use crate::hash::{display_hex_to_native, hash_hex_to_native, native_to_display_hex};
use rigor_convergence_root::native_to_hash_hex;
use crate::math::{convergence_draw_inequality_holds, draw_precedes};
use rigor_convergence_root::compute_convergence_root_from_native;
use crate::sigma::compute_sigma_hash;
use crate::types::{CandidateAudit, DrawReplayResult, ValidationMethod};

/// Inputs for full draw replay (mirrors `ResolveConvergenceDraw`).
///
/// Everything here describes block h-1: its convergence set, and its own hash as the σ seed.
/// `payout_height` is h, the height of the block that pays the winner.
#[derive(Clone, Debug)]
pub struct DrawReplayInput {
    /// F in the parent block's enumeration order (uint256 native bytes).
    pub candidate_txids: Vec<[u8; 32]>,
    pub weights: Vec<u64>,
    /// B_{h-1}, native bytes.
    pub prev_block_hash: [u8; 32],
    pub payout_height: u32,
    pub res_probability_target: [u8; 32],
}

impl DrawReplayInput {
    pub fn convergence_root(&self) -> [u8; 32] {
        compute_convergence_root_from_native(&self.candidate_txids)
    }
}

/// Port of `ResolveConvergenceDraw`: σ per candidate, inequality filter, argmin by 512-bit
/// cross-multiplication with a txid tie-break.
pub fn replay_convergence_draw(input: &DrawReplayInput) -> DrawReplayResult {
    let convergence_root = input.convergence_root();
    let mut candidates = Vec::with_capacity(input.candidate_txids.len());

    for (i, txid) in input.candidate_txids.iter().enumerate() {
        let h = compute_sigma_hash(
            txid,
            &convergence_root,
            &input.prev_block_hash,
            input.payout_height,
        );
        let weight = input.weights.get(i).copied().unwrap_or(0);
        let draw_eligible = convergence_draw_inequality_holds(
            &h,
            &input.res_probability_target,
            weight,
        );
        candidates.push(CandidateAudit {
            txid: *txid,
            txid_display: native_to_display_hex(txid),
            convergence_amount: weight,
            h,
            h_display: native_to_display_hex(&h),
            draw_eligible,
        });
    }

    let mut winner_index: i32 = -1;
    if input.candidate_txids.len() >= CONVERGENCE_DRAW_MIN_TXS_FOR_RES {
        for (i, cand) in candidates.iter().enumerate() {
            if !cand.draw_eligible {
                continue;
            }
            if winner_index < 0 {
                winner_index = i as i32;
                continue;
            }
            let w = winner_index as usize;
            if draw_precedes(
                &candidates[i].h,
                candidates[i].convergence_amount,
                &candidates[w].h,
                candidates[w].convergence_amount,
                &candidates[i].txid,
                &candidates[w].txid,
            ) {
                winner_index = i as i32;
            }
        }
    }

    let draw_winner_txid = if winner_index >= 0 {
        Some(candidates[winner_index as usize].txid)
    } else {
        None
    };

    let eligible_count = candidates.iter().filter(|c| c.draw_eligible).count() as u32;
    let mint_required = draw_winner_txid.is_some();

    DrawReplayResult {
        convergence_root,
        convergence_root_display: native_to_hash_hex(&convergence_root),
        draw_winner_txid,
        draw_winner_txid_display: draw_winner_txid.map(|t| native_to_display_hex(&t)),
        winner_index,
        eligible_count,
        mint_required,
        candidates,
        validation_method: ValidationMethod::Replay,
    }
}

/// Cross-check a local replay against the draw the node published in `getblocktemplate`.
///
/// The pool and the node must agree on every value σ decides, because a block built on a
/// disagreement is a block the network rejects. Any divergence is therefore fatal rather than
/// something to fall back from: the node's answer is not trusted over the pool's, the run is
/// abandoned.
pub fn verify_replay_against_node(
    node_draw: &rigor_gbt::ConvergenceDraw,
    replay: &crate::types::DrawReplayResult,
) -> crate::error::WinnerResult<()> {
    use crate::error::WinnerError;

    let mismatch = |what: &str, node: String, pool: String| {
        WinnerError::Validation(format!(
            "consensus divergence in the parent draw at height {}: {what} — node says {node}, \
             pool replay says {pool}",
            node_draw.payout_height
        ))
    };

    let node_root = display_hex_to_native(&node_draw.convergence_root)?;
    if node_root != replay.convergence_root {
        return Err(mismatch(
            "convergence_root",
            node_draw.convergence_root.clone(),
            native_to_display_hex(&replay.convergence_root),
        ));
    }

    if node_draw.candidates.len() != replay.candidates.len() {
        return Err(mismatch(
            "candidate count",
            node_draw.candidates.len().to_string(),
            replay.candidates.len().to_string(),
        ));
    }

    for (i, node_candidate) in node_draw.candidates.iter().enumerate() {
        let pool_candidate = &replay.candidates[i];
        if !node_candidate
            .txid
            .eq_ignore_ascii_case(&pool_candidate.txid_display)
        {
            return Err(mismatch(
                &format!("candidate[{i}].txid"),
                node_candidate.txid.clone(),
                pool_candidate.txid_display.clone(),
            ));
        }
        if node_candidate.weight != pool_candidate.convergence_amount {
            return Err(mismatch(
                &format!("candidate[{i}].weight"),
                node_candidate.weight.to_string(),
                pool_candidate.convergence_amount.to_string(),
            ));
        }
        if let Some(node_sigma) = &node_candidate.sigma {
            if display_hex_to_native(node_sigma)? != pool_candidate.h {
                return Err(mismatch(
                    &format!("candidate[{i}].sigma"),
                    node_sigma.clone(),
                    native_to_display_hex(&pool_candidate.h),
                ));
            }
        }
        if node_candidate.eligible != pool_candidate.draw_eligible {
            return Err(mismatch(
                &format!("candidate[{i}].eligible"),
                node_candidate.eligible.to_string(),
                pool_candidate.draw_eligible.to_string(),
            ));
        }
    }

    match (&node_draw.winner, replay.winner_index) {
        (None, -1) => Ok(()),
        (None, i) => Err(mismatch(
            "winner",
            "none".into(),
            format!("index {i}"),
        )),
        (Some(w), -1) => Err(mismatch("winner", format!("index {}", w.index), "none".into())),
        (Some(w), i) => {
            if w.index as i32 != i {
                return Err(mismatch(
                    "winner index",
                    w.index.to_string(),
                    i.to_string(),
                ));
            }
            let pool_winner = &replay.candidates[i as usize];
            if !w.txid.eq_ignore_ascii_case(&pool_winner.txid_display) {
                return Err(mismatch(
                    "winner txid",
                    w.txid.clone(),
                    pool_winner.txid_display.clone(),
                ));
            }
            if w.weight != pool_winner.convergence_amount {
                return Err(mismatch(
                    "winner weight",
                    w.weight.to_string(),
                    pool_winner.convergence_amount.to_string(),
                ));
            }
            Ok(())
        }
    }
}

/// Build draw input from golden `RunSelection` vector fields.
pub fn draw_input_from_golden(
    txids_hex: &[String],
    fees: &[u64],
    payout_height: u32,
    prev_block_hash_hex: &str,
    t_hex: &str,
) -> crate::error::WinnerResult<DrawReplayInput> {
    let mut candidate_txids = Vec::with_capacity(txids_hex.len());
    for h in txids_hex {
        candidate_txids.push(hash_hex_to_native(h)?);
    }
    Ok(DrawReplayInput {
        candidate_txids,
        weights: fees.to_vec(),
        prev_block_hash: hash_hex_to_native(prev_block_hash_hex)?,
        payout_height,
        res_probability_target: hash_hex_to_native(t_hex)?,
    })
}
