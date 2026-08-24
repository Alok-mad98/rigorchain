use block_assembler::{compute_convergence_root, native_to_hash_hex};
use rigor_gbt::TemplateRecord;

use crate::error::{ValidationErrorCode, ValidatorError, ValidatorResult};

/// Recompute **C** over GBT `convergence_txids`.
///
/// A-9-C: F is the W=32 sliding window relative to the parent block. Most listed
/// txids are already confirmed in earlier blocks and are **not** in this template's
/// `transactions` (or mempool). Requiring membership there is the pre-A-9-C rule and
/// rejects every valid share once the window is non-empty.
///
/// Safety that remains (pool does not control RES):
/// - C over the listed txids must match the coinbase-committed root (caller).
/// - When GBT published `convergence_root`, it must be that same C (node/pool drift).
/// - RES mint stays opaque node GBT data; this crate never constructs or redirects it.
pub fn validate_convergence(template: &TemplateRecord) -> ValidatorResult<[u8; 32]> {
    for txid in &template.gbt.convergence_txids {
        if !is_display_txid32(txid) {
            return Err(ValidatorError::validation(
                ValidationErrorCode::ErrConvergenceMismatch,
                format!("convergence_txid {txid} is not a 32-byte display hex id"),
            ));
        }
    }

    let computed = compute_convergence_root(&template.gbt.convergence_txids);

    if let Some(published) = template.gbt.convergence_root.as_deref() {
        let local = native_to_hash_hex(&computed);
        if !local.eq_ignore_ascii_case(published) {
            return Err(ValidatorError::validation(
                ValidationErrorCode::ErrConvergenceMismatch,
                format!(
                    "GBT convergence_root {published} is not C over the {} window txids (locally {local})",
                    template.gbt.convergence_txids.len()
                ),
            ));
        }
    }

    Ok(computed)
}

pub fn validate_convergence_root_matches(
    template: &TemplateRecord,
    committed_root_hex: &str,
) -> ValidatorResult<()> {
    let computed = validate_convergence(template)?;
    let committed = hex::decode(committed_root_hex)
        .map_err(|e| ValidatorError::validation(ValidationErrorCode::ErrConvergenceMismatch, e.to_string()))?;
    if committed.len() != 32 {
        return Err(ValidatorError::validation(
            ValidationErrorCode::ErrConvergenceMismatch,
            "convergence_root must be 32 bytes",
        ));
    }
    if committed.as_slice() != computed {
        return Err(ValidatorError::validation(
            ValidationErrorCode::ErrConvergenceMismatch,
            "convergence_root recomputation mismatch",
        ));
    }
    Ok(())
}

fn is_display_txid32(s: &str) -> bool {
    s.len() == 64 && s.as_bytes().iter().all(|b| b.is_ascii_hexdigit())
}

/// One candidate from the on-chain 32-block window ending at the parent
/// (`getconvergenceresult(parent)` or equivalent).
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct WindowCandidate {
    pub txid: String,
    pub source_height: u64,
}

/// On-chain draw window of the parent block (A-9-C, W=32).
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct ParentWindow {
    pub parent_hash: String,
    pub window_first_height: u64,
    pub window_last_height: u64,
    pub candidates: Vec<WindowCandidate>,
}

/// Optional chain view used to stop node/pool drift on `convergence_txids`.
///
/// Default `ShareValidator` leaves this unset (unit tests, no RPC). Production
/// can inject a `getconvergenceresult(parent)` wrapper.
pub trait CommittedWindowSource: Send + Sync {
    fn window_ending_at_parent(&self, parent_hash: &str) -> Result<ParentWindow, String>;
}

/// Derive the F this template should commit from the parent's on-chain window.
///
/// A-9-C: the new window ending at payout height H is `[H-31, H]`. Relative to
/// the parent at H-1 that drops `source_height == H-32`, excludes the winner
/// this block pays, and adds any new CVG2 txs in this template's body.
pub fn expected_committed_window_txids(
    parent: &ParentWindow,
    payout_height: u64,
    paid_winner_txid: Option<&str>,
    new_body_cvg2_txids: &[String],
) -> Vec<String> {
    let new_first = payout_height.saturating_sub(31);
    let mut ids: Vec<String> = parent
        .candidates
        .iter()
        .filter(|c| c.source_height >= new_first)
        .filter(|c| {
            paid_winner_txid
                .map(|w| !c.txid.eq_ignore_ascii_case(w))
                .unwrap_or(true)
        })
        .map(|c| c.txid.to_ascii_lowercase())
        .collect();
    for txid in new_body_cvg2_txids {
        let lower = txid.to_ascii_lowercase();
        if !ids.iter().any(|e| e == &lower) {
            ids.push(lower);
        }
    }
    ids.sort();
    ids.dedup();
    ids
}

/// Cross-check GBT `convergence_txids` against the parent's on-chain W=32 window.
pub fn validate_convergence_window_against_parent(
    template: &TemplateRecord,
    parent: &ParentWindow,
) -> ValidatorResult<()> {
    if !parent.parent_hash.eq_ignore_ascii_case(&template.previousblockhash) {
        return Err(ValidatorError::validation(
            ValidationErrorCode::ErrConvergenceMismatch,
            format!(
                "chain window parent {} is not template previousblockhash {}",
                parent.parent_hash, template.previousblockhash
            ),
        ));
    }
    if parent.window_last_height + 1 != template.height {
        return Err(ValidatorError::validation(
            ValidationErrorCode::ErrConvergenceMismatch,
            format!(
                "parent window_last_height {} is not template.height-1 ({})",
                parent.window_last_height,
                template.height.saturating_sub(1)
            ),
        ));
    }

    let paid = template
        .gbt
        .convergence_draw
        .as_ref()
        .and_then(|d| d.winner.as_ref())
        .map(|w| w.txid.as_str());
    let body = crate::res_mint::body_cvg2_txids(template);
    let expected = expected_committed_window_txids(parent, template.height, paid, &body);

    let mut got: Vec<String> = template
        .gbt
        .convergence_txids
        .iter()
        .map(|s| s.to_ascii_lowercase())
        .collect();
    got.sort();
    got.dedup();

    if got != expected {
        return Err(ValidatorError::validation(
            ValidationErrorCode::ErrConvergenceMismatch,
            format!(
                "GBT convergence_txids do not match the on-chain 32-block window of the parent (gbt {} vs chain {})",
                got.len(),
                expected.len()
            ),
        ));
    }
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;
    use rigor_gbt::{GbtRecord, GbtTransaction, RecordId, TipHash};

    fn template_with_convergence(
        conv: Vec<String>,
        txs: Vec<GbtTransaction>,
        published_root: Option<String>,
    ) -> TemplateRecord {
        TemplateRecord {
            record_id: RecordId::new_v4(),
            parent_record_id: None,
            template_version: rigor_gbt::TemplateVersion::INITIAL,
            tip_hash: TipHash("aa".repeat(32)),
            previousblockhash: "00".repeat(32),
            longpollid: "lp".into(),
            height: 1,
            template_state: rigor_gbt::TemplateState::Active,
            gbt_payload_hash: [0u8; 32],
            mint_required: None,
            winner_job_id: None,
            job_id: None,
            candidate_block_id: None,
            created_at: 0,
            state_changed_at: 0,
            gbt: GbtRecord {
                record_id: RecordId::new_v4(),
                fetched_at: 0,
                tip_hash: TipHash("aa".repeat(32)),
                previousblockhash: "00".repeat(32),
                height: 1,
                longpollid: "lp".into(),
                bits: "207fffff".into(),
                prev_nbits: "1c00ff00".into(),
                target: None,
                mintime: None,
                curtime: None,
                coinbasevalue: 50,
                transactions: txs,
                convergence_txids: conv,
                convergence_root: published_root,
                convergence_draw: None,
                default_witness_commitment: None,
                default_convergence_commitment: None,
                default_res_mint: None,
                default_pcorb_carrier: None,
                weightlimit: Some(1_000_000),
                version: None,
                gbt_payload_hash: [0u8; 32],
                raw_json: "{}".into(),
            },
            job_snapshot: None,
        }
    }

    fn mint_like_tx(txid: String) -> GbtTransaction {
        GbtTransaction {
            data: "02".into(),
            txid,
            hash: None,
            depends: None,
            fee: None,
            sigops: None,
            weight: Some(681),
        }
    }

    /// A-9-C: historical window member + opaque node automint in `transactions`.
    #[test]
    fn accepts_historical_window_txid_not_in_current_transactions() {
        let historical = "aa".repeat(32);
        let mint = "cc".repeat(32);
        let tpl = template_with_convergence(
            vec![historical],
            vec![mint_like_tx(mint)],
            None,
        );
        let root = validate_convergence(&tpl).expect("historical window txid must be accepted");
        // |F| < 2 → consensus C is zero (same as a single buried candidate).
        assert_eq!(root, [0u8; 32]);
    }

    #[test]
    fn accepts_two_window_txids_absent_from_template_body() {
        let a = "aa".repeat(32);
        let b = "bb".repeat(32);
        let mint = "cc".repeat(32);
        let computed = compute_convergence_root(&[a.clone(), b.clone()]);
        let published = native_to_hash_hex(&computed);
        let tpl = template_with_convergence(
            vec![a, b],
            vec![mint_like_tx(mint)],
            Some(published),
        );
        let root = validate_convergence(&tpl).expect("window F must not require body membership");
        assert_eq!(root, computed);
    }

    #[test]
    fn rejects_malformed_convergence_txid() {
        let tpl = template_with_convergence(vec!["zz".into()], vec![], None);
        let err = validate_convergence(&tpl).unwrap_err();
        assert!(matches!(
            err,
            crate::error::ValidatorError::Validation {
                code: ValidationErrorCode::ErrConvergenceMismatch,
                ..
            }
        ));
        let msg = format!("{err}");
        assert!(msg.contains("not a 32-byte display hex id"), "{msg}");
    }

    #[test]
    fn rejects_gbt_published_root_drift() {
        let a = "aa".repeat(32);
        let b = "bb".repeat(32);
        let tpl = template_with_convergence(
            vec![a, b],
            vec![],
            Some("00".repeat(32)),
        );
        let err = validate_convergence(&tpl).unwrap_err();
        assert!(matches!(
            err,
            crate::error::ValidatorError::Validation {
                code: ValidationErrorCode::ErrConvergenceMismatch,
                ..
            }
        ));
        let msg = format!("{err}");
        assert!(msg.contains("GBT convergence_root"), "{msg}");
    }

    #[test]
    fn committed_window_drops_fallen_block_and_paid_winner() {
        let keep = "aa".repeat(32);
        let drop = "bb".repeat(32);
        let paid = "cc".repeat(32);
        let parent = ParentWindow {
            parent_hash: "00".repeat(32),
            window_first_height: 274,
            window_last_height: 305,
            candidates: vec![
                WindowCandidate {
                    txid: drop.clone(),
                    source_height: 274,
                },
                WindowCandidate {
                    txid: keep.clone(),
                    source_height: 299,
                },
                WindowCandidate {
                    txid: paid.clone(),
                    source_height: 300,
                },
            ],
        };
        let expected = expected_committed_window_txids(&parent, 306, Some(&paid), &[]);
        assert_eq!(expected, vec![keep]);
    }

    #[test]
    fn rejects_gbt_list_that_drifts_from_parent_window() {
        let keep = "aa".repeat(32);
        let mut tpl = template_with_convergence(vec!["dd".repeat(32)], vec![], None);
        tpl.height = 306;
        tpl.gbt.height = 306;
        tpl.previousblockhash = "00".repeat(32);
        tpl.gbt.previousblockhash = "00".repeat(32);
        let parent = ParentWindow {
            parent_hash: "00".repeat(32),
            window_first_height: 274,
            window_last_height: 305,
            candidates: vec![WindowCandidate {
                txid: keep,
                source_height: 299,
            }],
        };
        let err = validate_convergence_window_against_parent(&tpl, &parent).unwrap_err();
        assert!(matches!(
            err,
            crate::error::ValidatorError::Validation {
                code: ValidationErrorCode::ErrConvergenceMismatch,
                ..
            }
        ));
        assert!(format!("{err}").contains("on-chain 32-block window"));
    }

    #[test]
    fn accepts_gbt_list_matching_parent_window() {
        let keep = "aa".repeat(32);
        let mut tpl = template_with_convergence(vec![keep.clone()], vec![], None);
        tpl.height = 306;
        tpl.gbt.height = 306;
        let parent = ParentWindow {
            parent_hash: "00".repeat(32),
            window_first_height: 274,
            window_last_height: 305,
            candidates: vec![WindowCandidate {
                txid: keep,
                source_height: 299,
            }],
        };
        validate_convergence_window_against_parent(&tpl, &parent).unwrap();
    }
}
