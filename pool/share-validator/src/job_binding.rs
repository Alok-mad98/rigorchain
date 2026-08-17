use block_assembler::PublishedJobSnapshot;
use rigor_gbt::TemplateRecord;

use crate::error::{ValidationErrorCode, ValidatorError, ValidatorResult};

/// Confirm the share's job is the template it claims: height, parent, version, ids.
///
/// Already enforced elsewhere and restated here so a stale or swapped job cannot
/// sneak past as a live share:
/// - [`ShareValidator::resolve_job_and_template`] matches `job_id` → job → `record_id`
///   and rejects non-servable job state (`ErrStaleJob` / `ErrJobNotFound` / `ErrTemplateMismatch`).
/// - [`ReplayGuard::check_and_record_share`] also rejects non-ACTIVE jobs.
///
/// This function adds the remaining field-level equality so a cache mix-up of an
/// otherwise ACTIVE job is still `ErrStaleJob`.
pub fn validate_job_template_binding(
    job: &PublishedJobSnapshot,
    template: &TemplateRecord,
) -> ValidatorResult<()> {
    if !job.state.is_servable() {
        return Err(ValidatorError::validation(
            ValidationErrorCode::ErrStaleJob,
            format!("job {:?} is not ACTIVE", job.state),
        ));
    }
    if !template.template_state.is_servable() {
        return Err(ValidatorError::validation(
            ValidationErrorCode::ErrStaleJob,
            format!("template state {:?}", template.template_state),
        ));
    }
    if job.record_id != template.record_id {
        return Err(ValidatorError::validation(
            ValidationErrorCode::ErrTemplateMismatch,
            "job.record_id does not match template.record_id",
        ));
    }
    if job.height != template.height {
        return Err(ValidatorError::validation(
            ValidationErrorCode::ErrStaleJob,
            format!(
                "job height {} does not match template height {}",
                job.height, template.height
            ),
        ));
    }
    if !eq_hash(&job.previousblockhash, &template.previousblockhash)
        || !eq_hash(&job.prevhash, &template.previousblockhash)
        || !eq_hash(&job.prevhash, &template.gbt.previousblockhash)
    {
        return Err(ValidatorError::validation(
            ValidationErrorCode::ErrStaleJob,
            format!(
                "job prevhash {} does not match template previousblockhash {}",
                job.previousblockhash, template.previousblockhash
            ),
        ));
    }
    if job.template_version != template.template_version {
        return Err(ValidatorError::validation(
            ValidationErrorCode::ErrStaleJob,
            format!(
                "job template_version {:?} does not match template {:?}",
                job.template_version, template.template_version
            ),
        ));
    }
    if !eq_hash(job.tip_hash.as_str(), template.tip_hash.as_str()) {
        return Err(ValidatorError::validation(
            ValidationErrorCode::ErrStaleJob,
            format!(
                "job tip_hash {} does not match template tip_hash {}",
                job.tip_hash, template.tip_hash
            ),
        ));
    }
    if template.gbt.height != template.height {
        return Err(ValidatorError::validation(
            ValidationErrorCode::ErrTemplateMismatch,
            format!(
                "GbtRecord.height {} does not match TemplateRecord.height {}",
                template.gbt.height, template.height
            ),
        ));
    }
    Ok(())
}

fn eq_hash(a: &str, b: &str) -> bool {
    a.eq_ignore_ascii_case(b)
}

#[cfg(test)]
mod tests {
    use super::*;
    use block_assembler::{
        BodyMintPlaceholder, CandidateBlockId, CoinbaseInfo, CommitmentInfo, JobId, JobState,
    };
    use rigor_gbt::{GbtRecord, RecordId, TemplateVersion, TipHash};

    fn pair(height: u64, prev: &str, version: u32) -> (PublishedJobSnapshot, TemplateRecord) {
        let rid = RecordId::new_v4();
        let tip = TipHash("aa".repeat(32));
        let job = PublishedJobSnapshot {
            job_id: JobId("0000000000000001".into()),
            candidate_block_id: CandidateBlockId(1),
            record_id: rid,
            tip_hash: tip.clone(),
            template_version: TemplateVersion(version),
            previousblockhash: prev.to_string(),
            height,
            state: JobState::Active,
            coinbase: CoinbaseInfo {
                coinb1_hex: "01".into(),
                coinb2_hex: "02".into(),
                extranonce2_size: 4,
                scriptsig_hex: "03".into(),
                convergence_root_hex: "00".repeat(64),
                payout_script_hex: "76".into(),
                coinbase_value: 50,
            },
            commitments: CommitmentInfo {
                cvge_script_hex: None,
                rigor_script_hex: "6a08".into(),
                witness_commitment_script_hex: None,
                prev_nbits_hex: "1c00ff00".into(),
            },
            body_mint: BodyMintPlaceholder {
                slot_index: 1,
                reserved: false,
                mint_required: false,
                body_mint_txid: None,
                placeholder_txid: String::new(),
            },
            merkle_branch: vec![],
            version: 0x20000000,
            nbits: "207fffff".into(),
            ntime: 0,
            prevhash: prev.to_string(),
            block_target: "207fffff".into(),
            assembly_hash: [0u8; 32],
            created_at: 0,
            state_changed_at: 0,
        };
        let template = TemplateRecord {
            record_id: rid,
            parent_record_id: None,
            template_version: TemplateVersion(version),
            tip_hash: tip.clone(),
            previousblockhash: prev.to_string(),
            longpollid: "lp".into(),
            height,
            template_state: rigor_gbt::TemplateState::Active,
            gbt_payload_hash: [0u8; 32],
            mint_required: None,
            winner_job_id: None,
            job_id: None,
            candidate_block_id: None,
            created_at: 0,
            state_changed_at: 0,
            gbt: GbtRecord {
                record_id: rid,
                fetched_at: 0,
                tip_hash: tip,
                previousblockhash: prev.to_string(),
                height,
                longpollid: "lp".into(),
                bits: "207fffff".into(),
                prev_nbits: "1c00ff00".into(),
                target: None,
                mintime: None,
                curtime: None,
                coinbasevalue: 50,
                transactions: vec![],
                convergence_txids: vec![],
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
        };
        (job, template)
    }

    #[test]
    fn accepts_matching_job_and_template() {
        let (job, tpl) = pair(306, &"11".repeat(32), 1);
        validate_job_template_binding(&job, &tpl).unwrap();
    }

    #[test]
    fn rejects_height_mismatch() {
        let (mut job, tpl) = pair(306, &"11".repeat(32), 1);
        job.height = 305;
        let err = validate_job_template_binding(&job, &tpl).unwrap_err();
        assert!(matches!(
            err,
            ValidatorError::Validation {
                code: ValidationErrorCode::ErrStaleJob,
                ..
            }
        ));
        assert!(format!("{err}").contains("height"));
    }

    #[test]
    fn rejects_prevhash_mismatch() {
        let (mut job, tpl) = pair(306, &"11".repeat(32), 1);
        job.prevhash = "22".repeat(32);
        job.previousblockhash = "22".repeat(32);
        let err = validate_job_template_binding(&job, &tpl).unwrap_err();
        assert!(matches!(
            err,
            ValidatorError::Validation {
                code: ValidationErrorCode::ErrStaleJob,
                ..
            }
        ));
    }

    #[test]
    fn rejects_template_version_mismatch() {
        let (mut job, tpl) = pair(306, &"11".repeat(32), 1);
        job.template_version = rigor_gbt::TemplateVersion(9);
        let err = validate_job_template_binding(&job, &tpl).unwrap_err();
        assert!(matches!(
            err,
            ValidatorError::Validation {
                code: ValidationErrorCode::ErrStaleJob,
                ..
            }
        ));
    }

    #[test]
    fn rejects_non_active_template() {
        let (job, mut tpl) = pair(306, &"11".repeat(32), 1);
        tpl.template_state = rigor_gbt::TemplateState::Stale;
        let err = validate_job_template_binding(&job, &tpl).unwrap_err();
        assert!(matches!(
            err,
            ValidatorError::Validation {
                code: ValidationErrorCode::ErrStaleJob,
                ..
            }
        ));
    }
}
