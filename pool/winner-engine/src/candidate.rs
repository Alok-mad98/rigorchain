use rigor_gbt::TemplateRecord;

use crate::error::{WinnerError, WinnerResult};
use crate::hash::{display_hex_to_native, parse_prev_nbits};
use crate::types::CandidateSet;

/// Extract the draw the template PAYS: the set its parent committed, exactly as the node
/// resolved it (SECURITY-001-A8).
///
/// The candidates are transactions of block h-1, so none of them is in this template's own
/// transaction list and none of their weights can be recovered from it. `convergence_txids`
/// is a different set entirely — the one this template commits for the next block — and using
/// it here would pay the wrong draw.
pub fn extract_parent_draw_candidates(template: &TemplateRecord) -> WinnerResult<CandidateSet> {
    let gbt = &template.gbt;
    let draw = gbt.convergence_draw.as_ref().ok_or_else(|| {
        WinnerError::Validation(
            "getblocktemplate carried no convergence_draw: the node cannot tell this pool which \
             draw the block pays, so no mint may be built"
                .into(),
        )
    })?;

    if draw.payout_height != gbt.height {
        return Err(WinnerError::Validation(format!(
            "convergence_draw.payout_height {} does not match template height {}",
            draw.payout_height, gbt.height
        )));
    }
    if draw.source_height + 1 != draw.payout_height {
        return Err(WinnerError::Validation(format!(
            "convergence_draw source_height {} is not one below payout_height {}",
            draw.source_height, draw.payout_height
        )));
    }
    if !draw.sigma_seed.eq_ignore_ascii_case(&draw.source_blockhash) {
        return Err(WinnerError::Validation(
            "convergence_draw.sigma_seed is not the block that committed the set".into(),
        ));
    }
    // The seed must be the block this template builds on, or the pool is replaying a draw for
    // a different chain tip.
    if !draw.sigma_seed.eq_ignore_ascii_case(&gbt.previousblockhash) {
        return Err(WinnerError::Validation(format!(
            "convergence_draw.sigma_seed {} is not the template's previousblockhash {}",
            draw.sigma_seed, gbt.previousblockhash
        )));
    }

    let mut txids_ordered = Vec::with_capacity(draw.candidates.len());
    let mut txids_display = Vec::with_capacity(draw.candidates.len());
    let mut weights = Vec::with_capacity(draw.candidates.len());
    let mut res_programs = Vec::with_capacity(draw.candidates.len());
    for candidate in &draw.candidates {
        txids_ordered.push(display_hex_to_native(&candidate.txid)?);
        txids_display.push(candidate.txid.clone());
        weights.push(candidate.weight);
        res_programs.push(parse_res_program(&candidate.res_program)?);
    }

    Ok(CandidateSet {
        txids_ordered,
        txids_display,
        weights,
        res_programs,
        source_template_id: template.record_id,
        parent_block_hash_display: draw.sigma_seed.clone(),
        sigma_seed: display_hex_to_native(&draw.sigma_seed)?,
        convergence_root: display_hex_to_native(&draw.convergence_root)?,
        target_prev: parse_prev_nbits(&draw.target_prev)?,
        source_height: draw.source_height,
        height: draw.payout_height,
        extracted_at: rigor_gbt::record::unix_now(),
    })
}

/// Validate GBT V-06: the node's two views of the set this template COMMITS must agree.
///
/// A-9-C replaced the old body-membership rule. The committed set now spans the whole draw
/// window, so most of its members are transactions of earlier blocks and are legitimately absent
/// from this template's `transactions` — checking for them would reject every valid template.
/// What survives is the check that catches the same class of node/pool mismatch: the root the
/// node published must be the root the txid list hashes to.
///
/// Says nothing about the draw being paid, whose members live in the parent's window.
pub fn validate_convergence_membership(template: &TemplateRecord) -> WinnerResult<()> {
    let Some(node_root) = template.gbt.convergence_root.as_deref() else {
        // Pre-A9C fixture. The list alone can no longer be checked against anything.
        return Ok(());
    };
    let local_root = rigor_convergence_root::native_to_hash_hex(
        &rigor_convergence_root::compute_convergence_root(&template.gbt.convergence_txids),
    );
    if !local_root.eq_ignore_ascii_case(node_root) {
        return Err(WinnerError::Validation(format!(
            "convergence_root {node_root} is not C over the {} committed txids (locally {local_root})",
            template.gbt.convergence_txids.len()
        )));
    }
    Ok(())
}

/// A-5 R2: the mint pays `OP_1 <program32>`, so the declaration must be exactly 32 bytes.
fn parse_res_program(hex_str: &str) -> WinnerResult<[u8; 32]> {
    let bytes = hex::decode(hex_str)
        .map_err(|e| WinnerError::Validation(format!("res_program is not hex: {e}")))?;
    if bytes.len() != 32 {
        return Err(WinnerError::Validation(format!(
            "res_program must be 32 bytes, got {}",
            bytes.len()
        )));
    }
    let mut out = [0u8; 32];
    out.copy_from_slice(&bytes);
    Ok(out)
}
