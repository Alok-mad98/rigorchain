use crate::assembly::{assemble_from_template, AssemblerConfig, AssemblyArtifact};
use crate::cache::JobCache;
use crate::error::AssemblerResult;
use crate::types::{JobState, PublishedJobSnapshot};
use rigor_gbt::TemplateRecord;
use std::sync::Arc;

/// Block assembler + JobSnapshot publisher (TASK-MINING-007).
pub struct JobPublisher {
    cache: Arc<JobCache>,
    config: AssemblerConfig,
}

impl JobPublisher {
    pub fn new(cache: Arc<JobCache>, config: AssemblerConfig) -> Self {
        Self { cache, config }
    }

    /// **Devtest only.** Uses [`AssemblerConfig::default`], whose payout script is
    /// an unspendable all-zero P2PKH (C-6). Production callers must use
    /// [`Self::with_payout_script`].
    pub fn with_defaults(cache: Arc<JobCache>) -> Self {
        Self::new(cache, AssemblerConfig::default())
    }

    /// Publisher bound to a validated, spendable coinbase payout script (C-6).
    pub fn with_payout_script(
        cache: Arc<JobCache>,
        payout_script: Vec<u8>,
    ) -> AssemblerResult<Self> {
        let config = AssemblerConfig::with_payout_script(payout_script)?;
        config.validate_production()?;
        Ok(Self::new(cache, config))
    }

    pub fn cache(&self) -> &Arc<JobCache> {
        &self.cache
    }

    pub fn config(&self) -> &AssemblerConfig {
        &self.config
    }

    /// Run assembly pipeline and publish an ACTIVE `JobSnapshot`.
    pub fn publish_from_template(
        &self,
        template: &TemplateRecord,
    ) -> AssemblerResult<Arc<PublishedJobSnapshot>> {
        let artifact = assemble_from_template(template, &self.config)?;
        let (job_id, candidate_block_id) = self.cache.allocate_ids()?;
        let job = self.build_snapshot(template, artifact, job_id, candidate_block_id);
        self.cache.publish_active(job)
    }

    /// Assembly only (no cache write) — useful for tests and preview.
    pub fn assemble_preview(
        &self,
        template: &TemplateRecord,
    ) -> AssemblerResult<AssemblyArtifact> {
        assemble_from_template(template, &self.config)
    }

    pub fn on_tip_changed(
        &self,
        new_tip: rigor_gbt::TipHash,
    ) -> AssemblerResult<Vec<crate::types::JobId>> {
        self.cache.handle_tip_changed(new_tip)
    }

    fn build_snapshot(
        &self,
        template: &TemplateRecord,
        artifact: AssemblyArtifact,
        job_id: crate::types::JobId,
        candidate_block_id: crate::types::CandidateBlockId,
    ) -> PublishedJobSnapshot {
        PublishedJobSnapshot {
            job_id,
            candidate_block_id,
            record_id: template.record_id,
            tip_hash: template.tip_hash.clone(),
            template_version: template.template_version,
            previousblockhash: template.previousblockhash.clone(),
            height: template.height,
            state: JobState::Created,
            coinbase: artifact.coinbase_info,
            commitments: artifact.commitments,
            body_mint: artifact.body_mint,
            merkle_branch: artifact.merkle_branch,
            version: artifact.version,
            nbits: artifact.nbits,
            ntime: artifact.ntime,
            prevhash: artifact.prevhash,
            block_target: artifact.block_target,
            assembly_hash: artifact.assembly_hash,
            created_at: 0,
            state_changed_at: 0,
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use rigor_gbt::{
        build_template_record, normalize_gbt_record, GbtTemplate, TemplateState,
        TemplateVersion, TipHash,
    };

    fn active_template() -> TemplateRecord {
        let raw = format!(
            r#"{{
            "previousblockhash": "{prev}",
            "transactions": [
                {{
                    "data": "0100000000010000000001",
                    "txid": "{txa}",
                    "hash": "{txa}"
                }},
                {{
                    "data": "0100000000020000000001",
                    "txid": "{txb}",
                    "hash": "{txb}"
                }}
            ],
            "coinbasevalue": 12800000000,
            "longpollid": "lp/asm/1",
            "weightlimit": 1000000,
            "bits": "1d00ffff",
            "prev_nbits": "1c00ff00",
            "height": 120,
            "version": 536870912,
            "curtime": 1700000000,
            "default_witness_commitment": "6a24aa21a9ed"
        }}"#,
            prev = "00".repeat(32),
            txa = "aa".repeat(32),
            txb = "bb".repeat(32),
        );
        let gbt_tpl: GbtTemplate = serde_json::from_str(&raw).unwrap();
        let tip = TipHash("ff".repeat(32));
        let payload = rigor_gbt::validate_template(&gbt_tpl, &raw).unwrap();
        let gbt = normalize_gbt_record(gbt_tpl, raw, tip, payload);
        build_template_record(gbt, None, TemplateVersion::INITIAL, TemplateState::Active)
    }

    #[test]
    fn publish_assigns_unique_job_and_candidate_ids() {
        let cache = Arc::new(JobCache::new());
        let publisher = JobPublisher::with_defaults(cache.clone());
        let template = active_template();
        let job = publisher.publish_from_template(&template).unwrap();
        assert_eq!(job.candidate_block_id.0, 1);
        assert_eq!(job.job_id.0, "0000000000000001");
        assert_eq!(job.state, JobState::Active);
        assert_eq!(job.record_id, template.record_id);
        assert!(!job.body_mint.reserved);
        assert!(!job.body_mint.mint_required);
        assert!(!job.coinbase.coinb1_hex.is_empty());
    }
}
