use rigor_gbt::TemplateRecord;

use crate::error::ValidatorResult;

/// Leftover helper. RFC-020 shipping GBT has no C-root; always returns zeros.
pub fn validate_convergence(_template: &TemplateRecord) -> ValidatorResult<[u8; 32]> {
    Ok([0u8; 32])
}

/// Leftover helper. RFC-020 share path does not bind a 32-byte C-root.
pub fn validate_convergence_root_matches(
    _template: &TemplateRecord,
    _committed_root_hex: &str,
) -> ValidatorResult<()> {
    Ok(())
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

/// Leftover W=32 helper. RFC-020 share path does not check a parent window.
pub fn validate_convergence_window_against_parent(
    _template: &TemplateRecord,
    _parent: &ParentWindow,
) -> ValidatorResult<()> {
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;
    use rigor_gbt::{GbtRecord, RecordId, TipHash};

    fn shipping_template() -> TemplateRecord {
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
                coinbasevalue: 12_800_000_000,
                transactions: vec![],
                default_witness_commitment: None,
                weightlimit: Some(1_000_000),
                version: None,
                gbt_payload_hash: [0u8; 32],
                raw_json: "{}".into(),
                default_coinbase_scriptsig: None,
                default_coinbase_tx: None,
                default_coinbase_txid: None,
                merkleroot: None,
                rigor_coinbase_immutable: None,
                coinbase_address: None,
            },
            job_snapshot: None,
        }
    }

    #[test]
    fn leftover_convergence_helpers_are_noops() {
        let tpl = shipping_template();
        assert_eq!(validate_convergence(&tpl).unwrap(), [0u8; 32]);
        validate_convergence_root_matches(&tpl, "ff").unwrap();
        let parent = ParentWindow {
            parent_hash: "00".repeat(32),
            window_first_height: 1,
            window_last_height: 1,
            candidates: vec![],
        };
        validate_convergence_window_against_parent(&tpl, &parent).unwrap();
    }

    #[test]
    fn leftover_window_helper_still_filters() {
        let keep = "aa".repeat(32);
        let drop = "bb".repeat(32);
        let paid = "cc".repeat(32);
        let parent = ParentWindow {
            parent_hash: "00".repeat(32),
            window_first_height: 274,
            window_last_height: 305,
            candidates: vec![
                WindowCandidate {
                    txid: drop,
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
}
