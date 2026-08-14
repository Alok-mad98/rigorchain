//! Winner preview / confirmation via Winner Engine (TASK-MINING-018).

use block_assembler::PublishedJobSnapshot;
use rigor_gbt::TemplateRecord;

use crate::error::{StratumError, StratumResult};
use crate::winner_service::SharedWinnerService;

/// Frozen RES reward: 100.00 RES = 10000 atoms (`RES_COIN` = 100).
pub const RES_REWARD_ATOMS: i64 = 10_000;

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct WinnerPreview {
    pub mint_required: bool,
    pub winner_txid: Option<String>,
    pub convergence_root_hex: String,
    pub draw_target_prev_nbits: String,
}

/// Authoritative preview using full convergence draw replay (replaces min-txid heuristic).
pub fn preview_winner(
    winner: &SharedWinnerService,
    template: &TemplateRecord,
    job: &PublishedJobSnapshot,
) -> StratumResult<WinnerPreview> {
    winner.preview_winner(template, job)
}

pub fn confirm_winner_for_block(
    winner: &SharedWinnerService,
    template: &TemplateRecord,
    job: &PublishedJobSnapshot,
) -> StratumResult<WinnerPreview> {
    let preview = preview_winner(winner, template, job)?;
    if preview.mint_required && preview.winner_txid.is_none() {
        return Err(StratumError::Validation(
            "mint required but no authoritative draw_winner_txid".into(),
        ));
    }
    Ok(preview)
}

#[cfg(test)]
mod tests {
    use super::*;
    use block_assembler::{
        BodyMintPlaceholder, CandidateBlockId, CoinbaseInfo, CommitmentInfo, JobId, JobState,
    };
    use rigor_gbt::{GbtRecord, GbtTransaction, RecordId, TemplateVersion, TipHash};
    use winner_engine::WinnerRuntime;

    fn job_with_root(root_hex: &str) -> PublishedJobSnapshot {
        PublishedJobSnapshot {
            job_id: JobId("1".into()),
            candidate_block_id: CandidateBlockId(1),
            record_id: RecordId::new_v4(),
            tip_hash: TipHash("aa".repeat(32)),
            template_version: TemplateVersion::INITIAL,
            previousblockhash: "ab".repeat(32),
            height: 1,
            state: JobState::Active,
            coinbase: CoinbaseInfo {
                coinb1_hex: "01".into(),
                coinb2_hex: "02".into(),
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
                slot_index: 2,
                reserved: true,
                mint_required: false,
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


    fn parent_draw_two_tx() -> rigor_gbt::ConvergenceDraw {
        use rigor_gbt::{ConvergenceDraw, ConvergenceDrawCandidate, ConvergenceDrawWinner};
        use winner_engine::{
            display_hex_to_native, native_to_display_hex, replay_convergence_draw, DrawReplayInput,
            RES_PROBABILITY_TARGET,
        };
        let a = "aa".repeat(32);
        let b = "bb".repeat(32);
        let prev = "ab".repeat(32);
        let weights = vec![100_000_000 * 65536, 100_000_000 * 65536];
        let replay = replay_convergence_draw(&DrawReplayInput {
            candidate_txids: vec![
                display_hex_to_native(&a).unwrap(),
                display_hex_to_native(&b).unwrap(),
            ],
            weights: weights.clone(),
            prev_block_hash: display_hex_to_native(&prev).unwrap(),
            payout_height: 1,
            res_probability_target: RES_PROBABILITY_TARGET,
        });
        let candidates = vec![
            ConvergenceDrawCandidate {
                txid: a.clone(),
                weight: weights[0],
                res_program: "11".repeat(32),
                sigma: None,
                eligible: replay.candidates[0].draw_eligible,
            },
            ConvergenceDrawCandidate {
                txid: b.clone(),
                weight: weights[1],
                res_program: "11".repeat(32),
                sigma: None,
                eligible: replay.candidates[1].draw_eligible,
            },
        ];
        let winner = if replay.winner_index >= 0 {
            let i = replay.winner_index as usize;
            Some(ConvergenceDrawWinner {
                index: i as u32,
                txid: if i == 0 { a } else { b },
                weight: weights[i],
                res_program: "11".repeat(32),
            })
        } else {
            None
        };
        ConvergenceDraw {
            source_blockhash: prev.clone(),
            source_height: 0,
            sigma_seed: prev,
            payout_height: 1,
            convergence_root: native_to_display_hex(&replay.convergence_root),
            target_prev: "1c00ff00".into(),
            res_probability_target: hex::encode(RES_PROBABILITY_TARGET),
            eligible_root: "dd".repeat(32),
            candidates,
            winner,
        }
    }

    fn template_two_tx() -> rigor_gbt::TemplateRecord {
        rigor_gbt::build_template_record(
            GbtRecord {
                record_id: RecordId::new_v4(),
                fetched_at: 0,
                tip_hash: TipHash("aa".repeat(32)),
                previousblockhash: "ab".repeat(32),
                height: 1,
                longpollid: "lp".into(),
                bits: "207fffff".into(),
                prev_nbits: "1c00ff00".into(),
                target: None,
                mintime: None,
                curtime: None,
                coinbasevalue: 50,
                transactions: vec![
                    GbtTransaction {
                        data: "01".into(),
                        txid: "aa".repeat(32),
                        hash: None,
                        depends: None,
                        fee: Some(100_000_000),
                        sigops: None,
                        weight: None,
                    },
                    GbtTransaction {
                        data: "02".into(),
                        txid: "bb".repeat(32),
                        hash: None,
                        depends: None,
                        fee: Some(100_000_000),
                        sigops: None,
                        weight: None,
                    },
                ],
                convergence_txids: vec!["aa".repeat(32), "bb".repeat(32)],
                convergence_root: None,
                convergence_draw: Some(parent_draw_two_tx()),
                default_witness_commitment: None,
                default_convergence_commitment: None,
                weightlimit: Some(1_000_000),
                version: None,
                gbt_payload_hash: [0u8; 32],
                raw_json: "{}".into(),
            },
            None,
            TemplateVersion::INITIAL,
            rigor_gbt::TemplateState::Active,
        )
    }

    fn matching_root_job(tpl: &rigor_gbt::TemplateRecord) -> PublishedJobSnapshot {
        let root_hex = block_assembler::native_to_hash_hex(
            &block_assembler::compute_convergence_root(&tpl.gbt.convergence_txids),
        );
        job_with_root(&root_hex)
    }

    #[test]
    fn authoritative_preview_not_min_txid_sort() {
        let winner_svc = SharedWinnerService::memory();
        let tpl = template_two_tx();
        let job = matching_root_job(&tpl);
        let preview = preview_winner(&winner_svc, &tpl, &job).unwrap();
        assert!(preview.mint_required);
        assert!(preview.winner_txid.is_some());
        // min-txid heuristic would always pick "aa..."; draw replay may differ.
        let runtime = WinnerRuntime::memory();
        let draw = runtime.preview_winner(&tpl, &job).unwrap();
        assert_eq!(preview.winner_txid, draw.winner_txid_display);
    }

    #[test]
    fn rejects_root_mismatch() {
        let winner_svc = SharedWinnerService::memory();
        let tpl = template_two_tx();
        let err = preview_winner(&winner_svc, &tpl, &job_with_root("ff".repeat(64).as_str()))
            .unwrap_err();
        assert!(matches!(err, StratumError::Validation(_)));
    }
}
