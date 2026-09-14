use block_assembler::{JobSnapshotApi, PublishedJobSnapshot};
use rigor_gbt::{RecordId, TemplateCacheApi};

use crate::error::{StratumError, StratumResult};
use crate::protocol::build_notify;

#[derive(Clone, Debug)]
pub struct StratumJobEnvelope {
    pub job_id: String,
    pub template_id: RecordId,
    pub notify: crate::protocol::StratumNotify,
    pub convergence_root_hex: String,
    pub prev_nbits_hex: String,
    pub block_target: String,
    /// Leftover wire field; RFC-020 never previews a lottery winner.
    pub preview_winner_txid: Option<String>,
    /// Always false on the RFC-020 ORU-only path (kept for wire compat).
    pub mint_required: bool,
}

pub struct JobAssignmentEngine {
    jobs: JobSnapshotApi,
    templates: TemplateCacheApi,
}

impl JobAssignmentEngine {
    pub fn new(jobs: JobSnapshotApi, templates: TemplateCacheApi) -> Self {
        Self { jobs, templates }
    }

    pub fn active_jobs(&self) -> StratumResult<Vec<StratumJobEnvelope>> {
        let active = self
            .jobs
            .get_active_jobs()
            .map_err(|e| StratumError::Internal(e.to_string()))?;
        active
            .iter()
            .map(|j| self.envelope_from_job(j))
            .collect()
    }

    pub fn envelope_for_job_id(&self, job_id: &str) -> StratumResult<StratumJobEnvelope> {
        let id = block_assembler::JobId(job_id.to_string());
        let job = self
            .jobs
            .get_job_by_id(&id)
            .map_err(|e| StratumError::Internal(e.to_string()))?
            .ok_or_else(|| StratumError::Job(format!("unknown job_id {job_id}")))?;
        self.envelope_from_job(&job)
    }

    pub fn envelope_from_job_with_preview(
        &self,
        job: &PublishedJobSnapshot,
        preview_winner_txid: Option<String>,
        mint_required: bool,
    ) -> StratumResult<StratumJobEnvelope> {
        self.envelope_from_job_inner(job, preview_winner_txid, mint_required)
    }

    fn envelope_from_job(&self, job: &PublishedJobSnapshot) -> StratumResult<StratumJobEnvelope> {
        self.envelope_from_job_inner(job, None, false)
    }

    fn envelope_from_job_inner(
        &self,
        job: &PublishedJobSnapshot,
        preview_winner_txid: Option<String>,
        _mint_required: bool,
    ) -> StratumResult<StratumJobEnvelope> {
        if !job.state.is_servable() {
            return Err(StratumError::Job(format!("job {:?} not active", job.state)));
        }

        let prevhash = reverse_hex(&job.prevhash);
        let notify = build_notify(
            &job.job_id.0,
            &prevhash,
            &job.coinbase.coinb1_hex,
            &job.coinbase.coinb2_hex,
            &job.merkle_branch,
            &format!("{:08x}", job.version),
            &job.nbits,
            &format!("{:08x}", job.ntime),
            true,
        );

        Ok(StratumJobEnvelope {
            job_id: job.job_id.0.clone(),
            template_id: job.record_id,
            notify,
            convergence_root_hex: job.coinbase.convergence_root_hex.clone(),
            prev_nbits_hex: job.commitments.prev_nbits_hex.clone(),
            block_target: job.block_target.clone(),
            preview_winner_txid,
            mint_required: false,
        })
    }

    pub fn template_for_job(&self, record_id: RecordId) -> StratumResult<rigor_gbt::TemplateRecord> {
        self.templates
            .get_template_by_id(record_id)
            .map_err(|e| StratumError::Internal(e.to_string()))?
            .map(|arc| (*arc).clone())
            .ok_or_else(|| StratumError::Job("template missing".into()))
    }
}

fn reverse_hex(hex_str: &str) -> String {
    let mut bytes = hex::decode(hex_str).unwrap_or_default();
    if bytes.len() != 32 {
        return hex_str.to_string();
    }
    bytes.reverse();
    hex::encode(bytes)
}
