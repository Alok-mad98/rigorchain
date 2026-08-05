use std::sync::Arc;

use block_assembler::PublishedJobSnapshot;
use rigor_gbt::{record::unix_now, TemplateRecord};

use crate::block::{assemble_candidate_block, MintBlockContext};
use crate::error::{MintError, MintResult};
use crate::mempool_preflight::enforce_mempool_preflight;
use crate::rpc::{RigordRpc, RigordRpcConfig};
use crate::sigma::{display_hex_to_le32, sha256d};
use crate::state::MintStateStore;
use crate::txbuild::{bootstrap_p2wsh_scriptpubkey, build_body_mint_tx, witness_v1_res_script};
use crate::telemetry::MintTelemetry;
use crate::types::{
    BodyMintArtifact, CandidateBlockPackage, ConvergeProofFields, MintBuildParams, MintJobRef,
    MintRecord, MintWinnerContext, OruFundingInput, RES_REWARD_ATOMS,
};

#[derive(Clone, Debug)]
/// SECURITY-001-A / A-5: there is deliberately no `res_payout_program` here. RES belongs to the
/// owner of the winning convergence transaction, which the engine reads per-mint from that
/// transaction's declaration. The pool's income is ORU: the coinbase plus the fees that
/// convergence transactions must pay to be admitted to `F` at all.
///
/// SECURITY-001-A / A-7: nor is there an `ed25519_seed`. The mint witness carried a signature over
/// the σ digest whose verifying key came from the witness itself, so it authorized nothing and the
/// "mint key" was never a security boundary. The pool now holds no key on the mint path at all.
pub struct MintEngineConfig {
    pub funding: OruFundingInput,
    pub oru_fee: i64,
    pub rpc: Option<RigordRpcConfig>,
    pub auto_submit: bool,
}

/// Placeholder ORU funding outpoint txid shipped in [`MintEngineConfig::devtest`].
pub const DEVTEST_ORU_FUNDING_TXID: [u8; 32] = [0x99; 32];

impl MintEngineConfig {
    /// **Devtest only.** Every secret here is publicly derivable from the source
    /// tree. `validate_production` rejects this profile.
    pub fn devtest() -> Self {
        Self {
            funding: OruFundingInput {
                prev_txid: DEVTEST_ORU_FUNDING_TXID,
                vout: 0,
                amount: 500_000_000,
                script_pubkey: bootstrap_p2wsh_scriptpubkey(),
            },
            oru_fee: 10_000,
            rpc: None,
            auto_submit: false,
        }
    }

    /// C-6: refuse to run the RES mint path with publicly-known or placeholder
    /// material. Callers on any value-bearing chain MUST gate startup on this.
    ///
    /// Consensus impact: NONE — this only decides whether the pool process starts.
    pub fn validate_production(&self) -> MintResult<()> {
        // A-5 removed the res_payout_program check with the field: there is no pool-held RES
        // address left to misconfigure, and an uncustodied payout is no longer possible because
        // the target comes from the winner's own declaration. A-7 removed the Ed25519 seed checks
        // the same way — the pool no longer holds a mint key that could be weak or public.
        if self.funding.prev_txid == DEVTEST_ORU_FUNDING_TXID
            || self.funding.prev_txid == [0u8; 32]
        {
            return Err(MintError::Validation(
                "C-6: refusing to start — ORU funding outpoint is the devtest placeholder".into(),
            ));
        }
        if self.funding.script_pubkey == bootstrap_p2wsh_scriptpubkey() {
            return Err(MintError::Validation(
                "C-6: refusing to start — ORU funding scriptPubKey is the anyone-can-spend \
                 devtest bootstrap P2WSH"
                    .into(),
            ));
        }
        if self.funding.amount <= self.oru_fee {
            return Err(MintError::Validation(
                "C-6: refusing to start — ORU funding amount does not cover oru_fee".into(),
            ));
        }
        if self.auto_submit && self.rpc.is_none() {
            return Err(MintError::Validation(
                "C-6: refusing to start — auto_submit enabled without an RPC endpoint".into(),
            ));
        }
        Ok(())
    }
}

pub struct MintOutcome {
    pub artifact: BodyMintArtifact,
    pub block: CandidateBlockPackage,
    pub record: MintRecord,
    pub submitted: bool,
}

pub struct MintEngine {
    config: MintEngineConfig,
    state: Arc<MintStateStore>,
    rpc: Option<RigordRpc>,
}

impl MintEngine {
    pub fn new(config: MintEngineConfig) -> Self {
        let rpc = config.rpc.as_ref().map(|c| RigordRpc::new(c.clone()));
        Self {
            config,
            state: Arc::new(MintStateStore::new()),
            rpc,
        }
    }

    pub fn with_shared_state(config: MintEngineConfig, state: Arc<MintStateStore>) -> Self {
        let rpc = config.rpc.as_ref().map(|c| RigordRpc::new(c.clone()));
        Self {
            config,
            state,
            rpc,
        }
    }

    pub fn state(&self) -> Arc<MintStateStore> {
        self.state.clone()
    }

    /// Build and optionally submit RES body mint only when Winner Engine authorized (TASK-MINING-018).
    pub fn mint_for_authorized_winner(
        &self,
        token: &winner_engine::MintAuthorizationToken,
        template: &TemplateRecord,
        job: &PublishedJobSnapshot,
        block_ctx: &MintBlockContext,
    ) -> MintResult<Option<MintOutcome>> {
        let now = unix_now();
        winner_engine::validate_token_consumption(token, now, &[]).map_err(|e| {
            MintError::Validation(format!("winner authorization rejected: {e}"))
        })?;

        let preview = MintWinnerContext {
            mint_required: true,
            winner_txid: Some(token.draw_winner_txid_display.clone()),
            convergence_root_hex: token.convergence_root_display.clone(),
            draw_target_prev_nbits: format!("{:08x}", token.target_prev),
            authorized_bind_hash: Some(token.bind_hash),
            authorized_bind_hash_display: Some(token.bind_hash_display.clone()),
        };

        if preview.winner_txid.as_deref().unwrap_or("").is_empty() {
            return Err(MintError::Validation(
                "authorized mint requires non-empty draw_winner_txid".into(),
            ));
        }

        self.mint_for_winner(&preview, template, job, block_ctx)
    }

    /// Internal mint builder — callers must supply Winner-authorized preview context.
    pub fn mint_for_winner(
        &self,
        preview: &MintWinnerContext,
        template: &TemplateRecord,
        job: &PublishedJobSnapshot,
        block_ctx: &MintBlockContext,
    ) -> MintResult<Option<MintOutcome>> {
        if !preview.mint_required {
            return Ok(None);
        }
        let winner_hex = preview
            .winner_txid
            .as_ref()
            .ok_or_else(|| MintError::Validation("mint required but winner_txid missing".into()))?;

        MintTelemetry::record_mint_started(job, winner_hex);

        if template.gbt.bits == template.gbt.prev_nbits {
            return Err(MintError::Validation(
                "FVR-M: draw target must use prev_nbits, not template bits".into(),
            ));
        }

        let convergence_root = rigor_convergence_root::hash_hex_to_native(
            &preview.convergence_root_hex,
        )
        .map_err(|e| MintError::Validation(e.to_string()))?;
        let convergence_txid = display_hex_to_le32(winner_hex)?;
        let target_prev = u32::from_str_radix(&preview.draw_target_prev_nbits, 16)
            .map_err(|e| MintError::Validation(e.to_string()))?;
        let bind_hash = self.resolve_chain_accurate_bind_hash(preview, job)?;

        let convergence_amount_sat = template
            .gbt
            .coinbasevalue
            .max(crate::types::MIN_CONVERGENCE_AMOUNT_SAT as i64) as u64;

        let mint_job = MintJobRef {
            winner_job_id: template.winner_job_id.clone().or_else(|| {
                template
                    .job_id
                    .as_ref()
                    .map(|j| j.0.clone())
                    .or_else(|| Some(job.job_id.0.clone()))
            }),
            job_id: job.job_id.0.clone(),
            record_id: job.record_id,
            winner_txid: winner_hex.clone(),
        };

        let proof = ConvergeProofFields {
            convergence_txid,
            convergence_amount_sat,
            bind_hash,
            height: job.height,
            target_prev,
            convergence_root,
        };

        // SECURITY-001-A / A-5: the payout target is whatever the winning convergence declared.
        // The pool earns ORU (coinbase plus the convergence tx fees that admission to F
        // requires); the scarce asset goes to whoever burned the ORU to enter the draw.
        let owner_program = self.resolve_res_owner_program(template, winner_hex)?;

        let params = MintBuildParams {
            mint_job: mint_job.clone(),
            proof,
            res_payout_script: witness_v1_res_script(&owner_program),
            oru_change_script: bootstrap_p2wsh_scriptpubkey(),
            funding: self.config.funding.clone(),
            oru_fee: self.config.oru_fee,
        };

        let artifact = build_body_mint_tx(&params)?;
        if artifact.amount_atoms != RES_REWARD_ATOMS {
            return Err(MintError::Validation("vout[0] must be 100 RES".into()));
        }

        let block = assemble_candidate_block(
            template,
            job,
            &artifact.raw_tx_hex,
            &artifact.body_mint_txid,
            block_ctx,
        )?;

        MintTelemetry::record_mint_built(job, winner_hex, &artifact.body_mint_txid);

        let now = unix_now();
        let record = self.state.register_built(mint_job, artifact.clone(), now)?;

        let mut submitted = false;
        if self.config.auto_submit {
            if let Some(rpc) = &self.rpc {
                // M-07: mandatory testmempoolaccept before submitblock.
                let pre = rpc.test_mempool_accept(&artifact.raw_tx_hex)?;
                MintTelemetry::record_mempool_preflight(
                    job,
                    &artifact.body_mint_txid,
                    pre.allowed,
                    pre.reject_reason.as_deref(),
                );
                enforce_mempool_preflight(&pre)?;
                match rpc.submit_block(&block.block_hex) {
                    Ok(()) => {
                        self.state.mark_submitted(&job.job_id.0, unix_now())?;
                        if let Ok(Some(rec)) = self.state.get(&job.job_id.0) {
                            MintTelemetry::record_mint_submitted(job, &rec);
                        }
                        submitted = true;
                    }
                    Err(e) => {
                        return Err(e);
                    }
                }
            }
        }

        Ok(Some(MintOutcome {
            artifact,
            block,
            record,
            submitted,
        }))
    }

    pub fn revoke_for_jobs(&self, job_ids: &[String]) -> MintResult<Vec<MintRecord>> {
        let revoked = self.state.revoke_for_jobs(job_ids, unix_now())?;
        for rec in &revoked {
            MintTelemetry::record_mint_revoked(rec);
        }
        Ok(revoked)
    }

    pub fn orphan_submitted_for_jobs(&self, job_ids: &[String]) -> MintResult<Vec<MintRecord>> {
        self.state.orphan_for_jobs(job_ids, unix_now())
    }

    pub fn get_record(&self, job_id: &str) -> MintResult<Option<MintRecord>> {
        self.state.get(job_id)
    }

    /// SECURITY-001-A / A-5: read the RES payout owner the winning convergence declared.
    ///
    /// The winner is located in the template by display txid, the same set the draw was replayed
    /// over, and its declaration is parsed from the raw bytes the node handed us in GBT. There is
    /// deliberately no fallback to a pool-configured address: consensus would reject that block
    /// with `bad-rigor-res-owner-mismatch`, so failing here is strictly better than mining a
    /// block that cannot be accepted.
    fn resolve_res_owner_program(
        &self,
        template: &TemplateRecord,
        winner_hex: &str,
    ) -> MintResult<[u8; 32]> {
        let entry = template
            .gbt
            .transactions
            .iter()
            .find(|tx| tx.txid.eq_ignore_ascii_case(winner_hex))
            .ok_or_else(|| {
                MintError::Validation(format!(
                    "A-5: winning convergence tx {winner_hex} is absent from the template"
                ))
            })?;
        let raw = hex::decode(&entry.data).map_err(|e| {
            MintError::Validation(format!("A-5: winning convergence tx data is not hex: {e}"))
        })?;
        crate::res_owner::res_owner_program_from_raw_tx(&raw)
    }

    /// Resolve mint bind_hash using Consensus::ConvergenceDrawBindHashFromParent semantics (M-06).
    ///
    /// Prefer live `getblockhash(H-k)` when RPC is configured (authoritative chain fields).
    /// Otherwise use the authorization token bind_hash carried on `MintWinnerContext`.
    fn resolve_chain_accurate_bind_hash(
        &self,
        preview: &MintWinnerContext,
        job: &PublishedJobSnapshot,
    ) -> MintResult<[u8; 32]> {
        let token_bind = preview.authorized_bind_hash;
        let token_display = preview.authorized_bind_hash_display.as_deref().unwrap_or("");

        let (bind_hash, bind_height, source, matched) =
            if job.height < CONVERGENCE_DRAW_BIND_DEPTH {
                ([0u8; 32], -1i64, "rfc000_bootstrap_zero", true)
            } else if let Some(rpc) = &self.rpc {
                let bind_h = job.height - CONVERGENCE_DRAW_BIND_DEPTH;
                let ancestor_hex = rpc.get_block_hash(bind_h)?;
                let expected = derive_bind_hash_chain_accurate(&ancestor_hex, job.height)?;
                let token_matches = token_bind == Some(expected)
                    || (!token_display.is_empty()
                        && token_display.eq_ignore_ascii_case(&ancestor_hex));
                (
                    expected,
                    bind_h as i64,
                    if token_matches {
                        "rpc_getblockhash_H_minus_k"
                    } else {
                        "rpc_getblockhash_H_minus_k_override_infra_token"
                    },
                    true,
                )
            } else if let Some(b) = token_bind {
                if b == [0u8; 32] {
                    return Err(MintError::Validation(
                        "M-06: non-zero chain-accurate bind_hash required on authorization token when RPC unavailable"
                            .into(),
                    ));
                }
                (
                    b,
                    (job.height - CONVERGENCE_DRAW_BIND_DEPTH) as i64,
                    "authorization_token_bind_hash",
                    true,
                )
            } else {
                return Err(MintError::Validation(
                    "M-06: chain-accurate bind_hash required (RPC getblockhash or authorization token)"
                        .into(),
                ));
            };

        let bind_hex = hex::encode(bind_hash);
        MintTelemetry::record_bind_hash_preflight(job, &bind_hex, bind_height, source, matched);
        Ok(bind_hash)
    }
}

/// Consensus bind depth k (RFC-000 §5 / CONVERGENCE_DRAW_BIND_DEPTH).
pub const CONVERGENCE_DRAW_BIND_DEPTH: u64 = 10;

/// Chain-accurate bind hash matching `Consensus::ConvergenceDrawBindHashFromParent`.
///
/// For `height < 10`, returns the RFC-000 bootstrap zero preimage.
/// For `height >= 10`, `bind_ancestor_hash_hex` MUST be the block hash at height `height - 10`.
pub fn derive_bind_hash_chain_accurate(
    bind_ancestor_hash_hex: &str,
    height: u64,
) -> MintResult<[u8; 32]> {
    if height < CONVERGENCE_DRAW_BIND_DEPTH {
        return Ok([0u8; 32]);
    }
    let bytes = hex::decode(bind_ancestor_hash_hex.trim())
        .map_err(|e| MintError::Validation(format!("bind ancestor hex: {e}")))?;
    if bytes.len() != 32 {
        return Err(MintError::Validation(format!(
            "bind ancestor hash must be 32 bytes, got {}",
            bytes.len()
        )));
    }
    let mut out = [0u8; 32];
    out.copy_from_slice(&bytes);
    Ok(out)
}

/// Deprecated pool-local bind digest — NOT consensus-accurate.
/// Retained for legacy tests only; mint proof path must use `derive_bind_hash_chain_accurate`.
#[deprecated(note = "M-06: use derive_bind_hash_chain_accurate / RPC getblockhash(H-k)")]
pub fn derive_bind_hash_infra(tip_hash_hex: &str, height: u64) -> [u8; 32] {
    let tip = hex::decode(tip_hash_hex).unwrap_or_default();
    let mut preimage = Vec::with_capacity(16 + tip.len());
    preimage.extend_from_slice(b"RIGOR_POOL_BIND_V1");
    preimage.extend_from_slice(&tip);
    preimage.extend_from_slice(&height.to_le_bytes());
    sha256d(&preimage)
}

#[cfg(test)]
mod tests {
    use super::*;
    use block_assembler::{
        BodyMintPlaceholder, CandidateBlockId, CoinbaseInfo, CommitmentInfo, JobId, JobState,
    };
    use rigor_gbt::{GbtRecord, GbtTransaction, RecordId, TemplateVersion, TipHash};

    /// Owner program the fixture convergence tx declares; the mint must pay exactly this.
    const TEST_OWNER_PROGRAM: [u8; 32] = [0xa1; 32];

    /// Convergence-shaped raw tx: a CVG2 burn output plus one RESO owner declaration.
    fn convergence_tx_declaring(program32: &[u8; 32]) -> Vec<u8> {
        let mut cvg2_program = vec![0x43, 0x56, 0x47, 0x32]; // "CVG2"
        cvg2_program.extend_from_slice(&[0x00; 28]);
        let mut cvg2_script = vec![0x51, 0x20];
        cvg2_script.extend_from_slice(&cvg2_program);
        let declaration = crate::res_owner::res_owner_declaration_script(program32);

        let mut raw = Vec::new();
        raw.extend_from_slice(&2i32.to_le_bytes());
        raw.push(0x01); // one input
        raw.extend_from_slice(&[0x11; 32]);
        raw.extend_from_slice(&0u32.to_le_bytes());
        raw.push(0x00); // empty scriptSig
        raw.extend_from_slice(&0xffff_ffffu32.to_le_bytes());
        raw.push(0x02); // two outputs
        for (value, script) in [
            (crate::types::MIN_CONVERGENCE_AMOUNT_SAT as i64, cvg2_script),
            (0i64, declaration),
        ] {
            raw.extend_from_slice(&value.to_le_bytes());
            raw.push(script.len() as u8);
            raw.extend_from_slice(&script);
        }
        raw.extend_from_slice(&0u32.to_le_bytes());
        raw
    }

    fn sample_job(root_hex: &str) -> PublishedJobSnapshot {
        PublishedJobSnapshot {
            job_id: JobId("job-1".into()),
            candidate_block_id: CandidateBlockId(1),
            record_id: RecordId::new_v4(),
            tip_hash: TipHash("aa".repeat(32)),
            template_version: TemplateVersion::INITIAL,
            previousblockhash: "00".repeat(32),
            height: 300,
            state: JobState::Active,
            coinbase: CoinbaseInfo {
                coinb1_hex: "01000000010000000000".into(),
                coinb2_hex: "ffffffff".into(),
                extranonce2_size: 4,
                scriptsig_hex: "03".into(),
                convergence_root_hex: root_hex.into(),
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
                slot_index: 3,
                reserved: true,
                mint_required: true,
                body_mint_txid: None,
                placeholder_txid: "cc".repeat(32),
            },
            merkle_branch: vec![],
            version: 1,
            nbits: "207fffff".into(),
            ntime: 0,
            prevhash: "00".repeat(32),
            block_target: "207fffff".into(),
            assembly_hash: [0u8; 32],
            created_at: 0,
            state_changed_at: 0,
        }
    }

    fn golden_win002_fixtures() -> (TemplateRecord, PublishedJobSnapshot, winner_engine::DrawReplayResult) {
        use rigor_gbt::{ConvergenceDraw, ConvergenceDrawCandidate};
        use winner_engine::{
            draw_input_from_golden, replay_convergence_draw, GOLDEN_PREV_NBITS,
        };

        let txid = "a".repeat(64);
        let prev = "bb".repeat(32);
        let root_hex = block_assembler::native_to_hash_hex(
            &block_assembler::compute_convergence_root(&[txid.clone()]),
        );
        let draw_obj = ConvergenceDraw {
            source_blockhash: prev.clone(),
            source_height: 299,
            sigma_seed: prev.clone(),
            payout_height: 300,
            convergence_root: root_hex.clone(),
            target_prev: format!("{:08x}", GOLDEN_PREV_NBITS),
            res_probability_target: hex::encode(winner_engine::RES_PROBABILITY_TARGET),
            eligible_root: "dd".repeat(32),
            candidates: vec![ConvergenceDrawCandidate {
                txid: txid.clone(),
                weight: 1,
                res_program: hex::encode(TEST_OWNER_PROGRAM),
                sigma: None,
                eligible: true,
            }],
            winner: Some(rigor_gbt::ConvergenceDrawWinner {
                index: 0,
                txid: txid.clone(),
                weight: 1,
                res_program: hex::encode(TEST_OWNER_PROGRAM),
            }),
        };
        let template = rigor_gbt::build_template_record(
            GbtRecord {
                record_id: RecordId::new_v4(),
                fetched_at: 0,
                tip_hash: TipHash("aa".repeat(32)),
                previousblockhash: prev.clone(),
                height: 300,
                longpollid: "lp".into(),
                bits: "207fffff".into(),
                prev_nbits: format!("{:08x}", GOLDEN_PREV_NBITS),
                target: None,
                mintime: None,
                curtime: None,
                coinbasevalue: 500_000_000,
                transactions: vec![GbtTransaction {
                    data: hex::encode(convergence_tx_declaring(&TEST_OWNER_PROGRAM)),
                    txid: txid.clone(),
                    hash: None,
                    depends: None,
                    fee: Some(1),
                    sigops: None,
                    weight: None,
                }],
                convergence_txids: vec![txid.clone()],
                convergence_root: None,
                convergence_draw: Some(draw_obj),
                default_witness_commitment: None,
                default_convergence_commitment: None,
                weightlimit: None,
                version: None,
                gbt_payload_hash: [0u8; 32],
                raw_json: "{}".into(),
            },
            None,
            TemplateVersion::INITIAL,
            rigor_gbt::TemplateState::Active,
        );
        let mut job = sample_job(&root_hex);
        job.previousblockhash = prev.clone();
        let input = draw_input_from_golden(
            &[txid],
            &[1],
            300,
            &prev,
            "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff",
        )
        .unwrap();
        let mut draw = replay_convergence_draw(&input);
        draw.convergence_root_display = root_hex;
        (template, job, draw)
    }

    /// C-6 regression: the devtest profile must never be accepted for production.
    #[test]
    fn devtest_profile_is_rejected_for_production() {
        let err = MintEngineConfig::devtest()
            .validate_production()
            .unwrap_err()
            .to_string();
        assert!(err.contains("C-6"), "unexpected error: {err}");
        assert!(
            err.contains("devtest placeholder"),
            "unexpected error: {err}"
        );
    }

    /// C-6 regression: each unsafe field is rejected independently, so fixing one
    /// placeholder does not accidentally unlock the rest.
    #[test]
    fn production_validation_rejects_each_placeholder_field() {
        let good_funding = || OruFundingInput {
            prev_txid: [0x3c; 32],
            vout: 1,
            amount: 500_000_000,
            script_pubkey: vec![0x00, 0x20, 0x11],
        };
        let base = MintEngineConfig {
            funding: good_funding(),
            oru_fee: 10_000,
            rpc: None,
            auto_submit: false,
        };
        base.validate_production().expect("fully configured profile");

        let mut placeholder_outpoint = base.clone();
        placeholder_outpoint.funding.prev_txid = DEVTEST_ORU_FUNDING_TXID;
        assert!(placeholder_outpoint.validate_production().is_err());

        let mut bootstrap_funding = base.clone();
        bootstrap_funding.funding.script_pubkey = bootstrap_p2wsh_scriptpubkey();
        assert!(bootstrap_funding.validate_production().is_err());

        let mut underfunded = base.clone();
        underfunded.oru_fee = underfunded.funding.amount;
        assert!(underfunded.validate_production().is_err());

        let mut submit_without_rpc = base.clone();
        submit_without_rpc.auto_submit = true;
        assert!(submit_without_rpc.validate_production().is_err());
    }

    /// C-5 regression: a job with no reserved body-mint slot yields a submittable
    /// block with no mint tx; a job that reserved a slot must refuse, because its
    /// merkle branch commits to a placeholder leaf.
    #[test]
    fn mintless_block_assembles_only_when_no_slot_reserved() {
        let (template, mut job, _draw) = golden_win002_fixtures();
        let ctx = MintBlockContext {
            extranonce1: vec![0x01],
            extranonce2: vec![0x00, 0x00, 0x00, 0x01],
            ntime: 0x65000000,
            nonce: 7,
        };

        assert!(job.body_mint.reserved);
        let err = crate::block::assemble_block_without_mint(&template, &job, &ctx)
            .unwrap_err()
            .to_string();
        assert!(err.contains("merkle commitment"), "unexpected: {err}");

        job.body_mint.reserved = false;
        job.body_mint.mint_required = false;
        let pkg = crate::block::assemble_block_without_mint(&template, &job, &ctx).unwrap();
        assert_eq!(pkg.vtx_count, 1 + template.gbt.transactions.len());
        assert!(!pkg.block_hex.is_empty());
        assert!(pkg.body_mint_txid.is_empty());
    }

    #[test]
    fn end_to_end_mint_from_authorized_winner() {
        use winner_engine::{
            authorize_mint, extract_parent_draw_candidates, AuthorizationContext,
            CandidateBlockRecord, CandidateBlockStatus,
        };

        let (template, job, draw) = golden_win002_fixtures();
        let set = extract_parent_draw_candidates(&template).unwrap();
        assert!(draw.mint_required);
        let now = unix_now();
        let ctx = AuthorizationContext {
            draw: draw.clone(),
            node_draw_convergence_root: draw.convergence_root,
            candidate_set: set,
            candidate_block: CandidateBlockRecord {
                id: job.candidate_block_id.0 as i64,
                job_id: job.job_id.0.clone(),
                record_id: job.record_id,
                prev_block_hash: job.previousblockhash.clone(),
                tip_hash: job.tip_hash.0.clone(),
                height: job.height,
                convergence_root_display: draw.convergence_root_display.clone(),
                mint_required: draw.mint_required,
                status: CandidateBlockStatus::Active,
                body_mint_reserved: job.body_mint.reserved,
            },
            template_bits: template.gbt.bits.clone(),
            template_prev_nbits: template.gbt.prev_nbits.clone(),
            current_tip: job.tip_hash.0.clone(),
            paused_new_jobs: false,
            treasury_available: true,
            now_unix: now,
            consumed_token_nonces: vec![],
        };
        let token = authorize_mint(&ctx).unwrap();
        assert_ne!(token.bind_hash, [0u8; 32]);

        let engine = MintEngine::new(MintEngineConfig::devtest());
        let outcome = engine
            .mint_for_authorized_winner(
                &token,
                &template,
                &job,
                &MintBlockContext {
                    extranonce1: vec![0x01],
                    extranonce2: vec![0x00, 0x00, 0x00, 0x01],
                    ntime: 0x65000000,
                    nonce: 1,
                },
            )
            .unwrap()
            .unwrap();
        assert_eq!(outcome.artifact.amount_atoms, RES_REWARD_ATOMS);
        assert_eq!(outcome.block.vtx_count, 3);
        assert!(!outcome.submitted);
    }

    #[test]
    fn rejects_mint_when_bind_hash_cleared() {
        use winner_engine::{
            authorize_mint, extract_parent_draw_candidates, AuthorizationContext,
            CandidateBlockRecord, CandidateBlockStatus,
        };

        let engine = MintEngine::new(MintEngineConfig::devtest());
        let (template, job, draw) = golden_win002_fixtures();
        let set = extract_parent_draw_candidates(&template).unwrap();
        let now = unix_now();
        let mut token = authorize_mint(&AuthorizationContext {
            draw: draw.clone(),
            node_draw_convergence_root: draw.convergence_root,
            candidate_set: set,
            candidate_block: CandidateBlockRecord {
                id: job.candidate_block_id.0 as i64,
                job_id: job.job_id.0.clone(),
                record_id: job.record_id,
                prev_block_hash: job.previousblockhash.clone(),
                tip_hash: job.tip_hash.0.clone(),
                height: job.height,
                convergence_root_display: job.coinbase.convergence_root_hex.clone(),
                mint_required: true,
                status: CandidateBlockStatus::Active,
                body_mint_reserved: true,
            },
            template_bits: template.gbt.bits.clone(),
            template_prev_nbits: template.gbt.prev_nbits.clone(),
            current_tip: job.tip_hash.0.clone(),
            paused_new_jobs: false,
            treasury_available: true,
            now_unix: now,
            consumed_token_nonces: vec![],
        })
        .unwrap();
        token.bind_hash = [0u8; 32];
        let result = engine.mint_for_authorized_winner(
            &token,
            &template,
            &job,
            &MintBlockContext {
                extranonce1: vec![],
                extranonce2: vec![],
                ntime: 0,
                nonce: 0,
            },
        );
        assert!(result.is_err(), "cleared bind_hash must be rejected");
    }
}
