use block_assembler::compute_convergence_root;
use rigor_gbt::TemplateRecord;

use crate::error::{ValidationErrorCode, ValidatorError, ValidatorResult};

/// Validate convergence_txids ⊆ mempool txids and recompute C.
pub fn validate_convergence(template: &TemplateRecord) -> ValidatorResult<[u8; 32]> {
    let txids: std::collections::HashSet<&str> = template
        .gbt
        .transactions
        .iter()
        .map(|t| t.txid.as_str())
        .collect();

    for txid in &template.gbt.convergence_txids {
        if !txids.contains(txid.as_str()) {
            return Err(ValidatorError::validation(
                ValidationErrorCode::ErrConvergenceMismatch,
                format!("convergence_txid {txid} not in mempool set"),
            ));
        }
    }

    Ok(compute_convergence_root(&template.gbt.convergence_txids))
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

#[cfg(test)]
mod tests {
    use super::*;
    use rigor_gbt::{GbtRecord, GbtTransaction, RecordId, TipHash};

    fn template_with_convergence(conv: Vec<String>, txs: Vec<GbtTransaction>) -> TemplateRecord {
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
                convergence_root: None,
                convergence_draw: None,
                default_witness_commitment: None,
                default_convergence_commitment: None,
                weightlimit: Some(1_000_000),
                version: None,
                gbt_payload_hash: [0u8; 32],
                raw_json: "{}".into(),
            },
            job_snapshot: None,
        }
    }

    #[test]
    fn rejects_convergence_txid_not_in_mempool() {
        let txid = "aa".repeat(32);
        let tpl = template_with_convergence(
            vec![txid.clone(), "bb".repeat(32)],
            vec![GbtTransaction {
                data: "01".into(),
                txid,
                hash: None,
                depends: None,
                fee: None,
                sigops: None,
                weight: None,
            }],
        );
        let err = validate_convergence(&tpl).unwrap_err();
        assert!(matches!(
            err,
            crate::error::ValidatorError::Validation {
                code: ValidationErrorCode::ErrConvergenceMismatch,
                ..
            }
        ));
    }
}
