use block_assembler::{JobSnapshotApi, PublishedJobSnapshot};
use rigor_gbt::{TemplateCacheApi, TemplateRecord};
use std::sync::Arc;

use crate::commitments::validate_cvge_rigor;
use crate::convergence::{
    validate_convergence_root_matches, validate_convergence_window_against_parent,
    CommittedWindowSource,
};
use crate::error::{ValidationErrorCode, ValidatorError, ValidatorResult};
use crate::header::{
    assemble_coinbase_raw, build_header_bytes, coinbase_scriptsig_from_submission, header_base_hash,
};
use crate::job_binding::validate_job_template_binding;
use crate::opuhash::{expand_compact_bits, get_opus_hash_from_base_hash, verify_opuhash_block, verify_opuhash_share};
use crate::replay::{ReplayGuard, SharedReplayGuard};
use crate::res_mint::{
    validate_block_res_mint, validate_node_supplied_automint, validate_res_mint_placeholder,
};
use crate::types::{
    BlockSubmission, JobValidationState, ShareSubmission, ValidationKind, ValidationResult,
};
/// Share validation enforcement boundary (TASK-MINING-008).
pub struct ShareValidator {
    jobs: JobSnapshotApi,
    templates: TemplateCacheApi,
    replay: SharedReplayGuard,
    /// Optional `getconvergenceresult(parent)` view of the W=32 window.
    window_source: Option<Arc<dyn CommittedWindowSource>>,
}

impl ShareValidator {
    pub fn new(
        jobs: JobSnapshotApi,
        templates: TemplateCacheApi,
        replay: SharedReplayGuard,
    ) -> Self {
        Self {
            jobs,
            templates,
            replay,
            window_source: None,
        }
    }

    pub fn with_defaults(jobs: Arc<block_assembler::JobCache>, templates: Arc<rigor_gbt::TemplateCache>) -> Self {
        Self::new(
            JobSnapshotApi::new(jobs),
            TemplateCacheApi::new(templates),
            Arc::new(ReplayGuard::new()),
        )
    }

    /// Cross-check GBT `convergence_txids` against the parent's on-chain 32-block window.
    pub fn with_window_source(mut self, source: Arc<dyn CommittedWindowSource>) -> Self {
        self.window_source = Some(source);
        self
    }

    pub fn validate_share(&self, share: ShareSubmission) -> ValidationResult {
        match self.validate_share_inner(&share) {
            Ok((pow, is_block)) => {
                self.replay
                    .record_accepted_share(&share.share_id, &share.job_id, &pow);
                let mut result = ValidationResult::accepted_share(share.job_id.clone(), share.share_id, pow);
                result.is_block_candidate = is_block;
                result
            }
            Err(e) => {
                self.replay.record_rejected_share(&share.job_id);
                self.to_validation_result_share(&share, e)
            }
        }
    }

    pub fn validate_block(&self, block: BlockSubmission) -> ValidationResult {
        match self.validate_block_inner(&block) {
            Ok(pow) => ValidationResult::accepted_block(block.job_id.clone(), pow),
            Err(e) => self.to_validation_result_block(&block, e),
        }
    }

    pub fn get_validation_state(&self, job_id: &block_assembler::JobId) -> Option<JobValidationState> {
        self.replay.get_validation_state(job_id)
    }

    fn validate_share_inner(&self, share: &ShareSubmission) -> ValidatorResult<([u8; 32], bool)> {
        let (job, template) = self.resolve_job_and_template(&share.job_id, share.record_id)?;

        self.replay.check_and_record_share(&share.share_id, &job)?;

        validate_job_template_binding(&job, &template)?;
        validate_res_mint_placeholder(&job)?;
        validate_node_supplied_automint(&template)?;
        self.check_parent_window(&template)?;

        // TASK-012 H-5: nTime rolling would pair a new timestamp with a frozen
        // job nBits. Until/unless the node scores nBits from parent time only
        // *and* the pool recomputes bits, require an exact match with the job.
        if u64::from(share.ntime) != job.ntime {
            return Err(ValidatorError::validation(
                ValidationErrorCode::ErrMalformedSubmission,
                format!(
                    "share ntime {:08x} does not match job ntime {:08x}",
                    share.ntime, job.ntime
                ),
            ));
        }

        validate_convergence_root_matches(&template, &job.coinbase.convergence_root_hex)?;

        let coinbase_raw = assemble_coinbase_raw(&job, &share.extranonce1, &share.extranonce2)?;
        validate_cvge_rigor(&job, &coinbase_raw)?;

        // Job snapshot stores consensus-exact scriptSig (height ‖ ConvergenceRoot).
        // Extranonces live in an OP_RETURN output, not scriptSig.
        let scriptsig =
            coinbase_scriptsig_from_submission(&job, &share.extranonce1, &share.extranonce2)?;
        let expected_ss = hex::decode(&job.coinbase.scriptsig_hex).map_err(|e| {
            ValidatorError::validation(ValidationErrorCode::ErrConvergenceMismatch, e.to_string())
        })?;
        if scriptsig != expected_ss {
            return Err(ValidatorError::validation(
                ValidationErrorCode::ErrConvergenceMismatch,
                "coinbase scriptSig does not match assembled job",
            ));
        }

        let header = build_header_bytes(&job, &share.extranonce1, &share.extranonce2, share.ntime, share.nonce)?;
        let base = header_base_hash(&header);
        let (pow, share_ok) = verify_opuhash_share(&base, &share.share_target);
        if !share_ok {
            return Err(ValidatorError::validation(
                ValidationErrorCode::ErrInvalidOpouhash,
                "pow_hash exceeds share_target",
            ));
        }

        let block_target = expand_compact_bits(&job.block_target)
            .map_err(|e| ValidatorError::validation(ValidationErrorCode::ErrInvalidOpouhash, e.to_string()))?;
        let is_block = verify_opuhash_block(&base, &block_target).1;
        Ok((pow, is_block))
    }

    fn validate_block_inner(&self, block: &BlockSubmission) -> ValidatorResult<[u8; 32]> {
        let (job, template) = self.resolve_job_and_template(&block.job_id, block.record_id)?;

        if !job.state.is_servable() {
            return Err(ValidatorError::validation(
                ValidationErrorCode::ErrStaleJob,
                format!("job state {:?}", job.state),
            ));
        }

        validate_job_template_binding(&job, &template)?;
        validate_convergence_root_matches(&template, &job.coinbase.convergence_root_hex)?;
        self.check_parent_window(&template)?;
        validate_block_res_mint(&job, &template, &block.block_hex, block.allow_res_body_mint)?;

        let block_bytes = hex::decode(&block.block_hex)
            .map_err(|e| ValidatorError::validation(ValidationErrorCode::ErrMalformedSubmission, e.to_string()))?;
        if block_bytes.len() < 80 {
            return Err(ValidatorError::validation(
                ValidationErrorCode::ErrMalformedSubmission,
                "block shorter than 80-byte header",
            ));
        }
        let mut header = [0u8; 80];
        header.copy_from_slice(&block_bytes[..80]);
        let base = header_base_hash(&header);
        let block_target = expand_compact_bits(&job.block_target)
            .map_err(|e| ValidatorError::validation(ValidationErrorCode::ErrInvalidOpouhash, e.to_string()))?;
        let (pow, ok) = verify_opuhash_block(&base, &block_target);
        if !ok {
            return Err(ValidatorError::validation(
                ValidationErrorCode::ErrInvalidOpouhash,
                "block pow_hash exceeds network target",
            ));
        }
        Ok(pow)
    }

    fn check_parent_window(&self, template: &TemplateRecord) -> ValidatorResult<()> {
        let Some(source) = &self.window_source else {
            return Ok(());
        };
        let parent = source
            .window_ending_at_parent(&template.previousblockhash)
            .map_err(|e| {
                ValidatorError::validation(
                    ValidationErrorCode::ErrConvergenceMismatch,
                    format!("parent 32-block window fetch failed: {e}"),
                )
            })?;
        validate_convergence_window_against_parent(template, &parent)
    }

    fn resolve_job_and_template(
        &self,
        job_id: &block_assembler::JobId,
        record_id: Option<rigor_gbt::RecordId>,
    ) -> ValidatorResult<(Arc<PublishedJobSnapshot>, Arc<TemplateRecord>)> {
        let job = self
            .jobs
            .get_job_by_id(job_id)
            .map_err(|e| ValidatorError::Internal(e.to_string()))?
            .ok_or_else(|| {
                ValidatorError::validation(
                    ValidationErrorCode::ErrJobNotFound,
                    format!("job_id {job_id:?} not found"),
                )
            })?;

        if !job.state.is_servable() {
            return Err(ValidatorError::validation(
                ValidationErrorCode::ErrStaleJob,
                format!("job {:?} not servable", job.state),
            ));
        }

        let template = self
            .templates
            .get_template_by_id(job.record_id)
            .map_err(|e| ValidatorError::Internal(e.to_string()))?
            .ok_or_else(|| {
                ValidatorError::validation(
                    ValidationErrorCode::ErrTemplateMismatch,
                    "template record missing for job",
                )
            })?;

        if let Some(rid) = record_id {
            if rid != job.record_id {
                return Err(ValidatorError::validation(
                    ValidationErrorCode::ErrTemplateMismatch,
                    "record_id does not match job",
                ));
            }
        }

        if template.record_id != job.record_id {
            return Err(ValidatorError::validation(
                ValidationErrorCode::ErrTemplateMismatch,
                "template record_id mismatch",
            ));
        }

        Ok((job, template))
    }

    fn to_validation_result_share(&self, share: &ShareSubmission, err: ValidatorError) -> ValidationResult {
        let (code, message) = match err {
            ValidatorError::Validation { code, message } => (code, message),
            ValidatorError::Internal(msg) => (ValidationErrorCode::ErrMalformedSubmission, msg),
        };
        ValidationResult::rejected(
            ValidationKind::Share,
            share.job_id.clone(),
            Some(share.share_id.clone()),
            code,
            message,
        )
    }

    fn to_validation_result_block(&self, block: &BlockSubmission, err: ValidatorError) -> ValidationResult {
        let (code, message) = match err {
            ValidatorError::Validation { code, message } => (code, message),
            ValidatorError::Internal(msg) => (ValidationErrorCode::ErrMalformedSubmission, msg),
        };
        ValidationResult::rejected(
            ValidationKind::Block,
            block.job_id.clone(),
            None,
            code,
            message,
        )
    }
}

// Re-export assemble helper for header module tests
impl ShareValidator {
    pub fn verify_opuhash_from_base(&self, base: &[u8; 32], target: &[u8; 32]) -> ([u8; 32], bool) {
        verify_opuhash_share(base, target)
    }

    pub fn compute_opuhash(&self, base: &[u8; 32]) -> [u8; 32] {
        get_opus_hash_from_base_hash(base)
    }
}
