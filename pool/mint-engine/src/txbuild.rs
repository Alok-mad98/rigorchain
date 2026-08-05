
use crate::error::{MintError, MintResult};
use crate::sigma::{le32_to_display_hex, sha256, sha256d};
use crate::types::{BodyMintArtifact, MintBuildParams, RES_REWARD_ATOMS};
use crate::witness::build_pcorb_witness_stack;

fn write_compact_size(buf: &mut Vec<u8>, n: usize) {
    if n < 0xfd {
        buf.push(n as u8);
    } else if n <= 0xffff {
        buf.push(0xfd);
        buf.extend_from_slice(&(n as u16).to_le_bytes());
    } else {
        buf.push(0xfe);
        buf.extend_from_slice(&(n as u32).to_le_bytes());
    }
}

fn serialize_outpoint(buf: &mut Vec<u8>, txid: &[u8; 32], vout: u32) {
    buf.extend_from_slice(txid);
    buf.extend_from_slice(&vout.to_le_bytes());
}

fn serialize_txout(buf: &mut Vec<u8>, value: i64, script: &[u8]) {
    buf.extend_from_slice(&value.to_le_bytes());
    write_compact_size(buf, script.len());
    buf.extend_from_slice(script);
}

fn serialize_witness_stack(buf: &mut Vec<u8>, stack: &[Vec<u8>]) {
    write_compact_size(buf, stack.len());
    for item in stack {
        write_compact_size(buf, item.len());
        buf.extend_from_slice(item);
    }
}

/// Witness v1 RES script: ``OP_1`` + 32-byte program.
pub fn witness_v1_res_script(program32: &[u8; 32]) -> Vec<u8> {
    let mut script = Vec::with_capacity(34);
    script.push(0x51); // OP_1
    script.push(0x20); // push 32 bytes
    script.extend_from_slice(program32);
    script
}

/// Devtest bootstrap ORU funding script (P2WSH OP_TRUE redeem path).
///
/// The witness program is a **single** SHA-256 of the redeem script, matching
/// BIP141 `WitnessV0ScriptHash` as used by `AutoResMintBootstrapScriptPubKey()`
/// in `core/src/node/auto_res_mint.cpp`. Using `sha256d` here produced a
/// scriptPubKey that does not exist on chain, so both the ORU change output and
/// the funding-input reference were unspendable.
pub fn bootstrap_p2wsh_scriptpubkey() -> Vec<u8> {
    p2wsh_scriptpubkey(&bootstrap_redeem_script())
}

/// BIP141 P2WSH: `OP_0 <32-byte sha256(redeem_script)>`.
pub fn p2wsh_scriptpubkey(redeem_script: &[u8]) -> Vec<u8> {
    let hash = sha256(redeem_script);
    let mut script = Vec::with_capacity(34);
    script.push(0x00); // OP_0
    script.push(0x20);
    script.extend_from_slice(&hash);
    script
}

pub fn bootstrap_redeem_script() -> Vec<u8> {
    let mut script = vec![0x75; crate::witness::PCORB_WITNESS_ITEMS]; // OP_DROP per proof push
    script.push(0x51); // OP_TRUE
    script
}

pub fn build_body_mint_tx(params: &MintBuildParams) -> MintResult<BodyMintArtifact> {
    if params.proof.convergence_amount_sat < crate::types::MIN_CONVERGENCE_AMOUNT_SAT {
        return Err(MintError::Validation(
            "convergence_amount_sat below MIN_CONVERGENCE_AMOUNT".into(),
        ));
    }

    let (mut witness_stack, digest) = build_pcorb_witness_stack(&params.proof)?;

    // Devtest/bootstrap: append redeem script for P2WSH spend authorization.
    witness_stack.push(bootstrap_redeem_script());

    let change_amt = params
        .funding
        .amount
        .saturating_sub(params.oru_fee)
        .max(0);

    let mut base = Vec::new();
    base.extend_from_slice(&2i32.to_le_bytes()); // nVersion = 2
    write_compact_size(&mut base, 1);
    serialize_outpoint(
        &mut base,
        &params.funding.prev_txid,
        params.funding.vout,
    );
    base.extend_from_slice(&[0xff, 0xff, 0xff, 0xff]); // nSequence

    write_compact_size(&mut base, 2);
    serialize_txout(&mut base, RES_REWARD_ATOMS, &params.res_payout_script);
    serialize_txout(&mut base, change_amt, &params.oru_change_script);

    base.extend_from_slice(&0u32.to_le_bytes()); // locktime

    let txid = le32_to_display_hex(&sha256d(&base));

    let mut raw = Vec::new();
    raw.extend_from_slice(&2i32.to_le_bytes());
    raw.push(0x00);
    raw.push(0x01);
    write_compact_size(&mut raw, 1);
    serialize_outpoint(&mut raw, &params.funding.prev_txid, params.funding.vout);
    raw.extend_from_slice(&[0xff, 0xff, 0xff, 0xff]);
    write_compact_size(&mut raw, 2);
    serialize_txout(&mut raw, RES_REWARD_ATOMS, &params.res_payout_script);
    serialize_txout(&mut raw, change_amt, &params.oru_change_script);
    write_compact_size(&mut raw, 1);
    serialize_witness_stack(&mut raw, &witness_stack);
    raw.extend_from_slice(&0u32.to_le_bytes());

    Ok(BodyMintArtifact {
        body_mint_txid: txid.clone(),
        raw_tx_hex: hex::encode(&raw),
        amount_atoms: RES_REWARD_ATOMS,
        winner_txid: params.mint_job.winner_txid.clone(),
        witness_item_count: witness_stack.len(),
        sigma_digest_hex: hex::encode(digest),
    })
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::sigma::display_hex_to_le32;
    use crate::types::{ConvergeProofFields, MintJobRef, OruFundingInput};
    use rigor_gbt::RecordId;

    fn sample_params() -> MintBuildParams {
        let program = [0x11u8; 32];
        MintBuildParams {
            mint_job: MintJobRef {
                winner_job_id: Some("job-winner".into()),
                job_id: "job-1".into(),
                record_id: RecordId::new_v4(),
                winner_txid: "aa".repeat(32),
            },
            proof: ConvergeProofFields {
                convergence_txid: display_hex_to_le32(&"aa".repeat(32)).unwrap(),
                convergence_amount_sat: crate::types::MIN_CONVERGENCE_AMOUNT_SAT,
                bind_hash: [0x22; 32],
                height: 200,
                target_prev: 0x207fffff,
                convergence_root: [0x33; 32],
            },
            res_payout_script: witness_v1_res_script(&program),
            oru_change_script: bootstrap_p2wsh_scriptpubkey(),
            funding: OruFundingInput {
                prev_txid: [0x44; 32],
                vout: 0,
                amount: 500_000_000,
                script_pubkey: bootstrap_p2wsh_scriptpubkey(),
            },
            oru_fee: 10_000,
        }
    }

    #[test]
    fn body_mint_txid_is_deterministic() {
        let p = sample_params();
        let a = build_body_mint_tx(&p).unwrap();
        let b = build_body_mint_tx(&p).unwrap();
        assert_eq!(a.body_mint_txid, b.body_mint_txid);
        assert_eq!(a.raw_tx_hex, b.raw_tx_hex);
        assert_eq!(a.amount_atoms, RES_REWARD_ATOMS);
    }

    #[test]
    fn body_mint_has_witness_items() {
        let artifact = build_body_mint_tx(&sample_params()).unwrap();
        // Six proof pushes plus the P2WSH redeem script.
        assert_eq!(
            artifact.witness_item_count,
            crate::witness::PCORB_WITNESS_ITEMS + 1
        );
        assert!(!artifact.raw_tx_hex.is_empty());
    }

    /// C-6d regression: P2WSH program must be single SHA-256 (BIP141
    /// `WitnessV0ScriptHash`), never SHA-256d, or the ORU change output and the
    /// bootstrap funding reference are unspendable.
    #[test]
    fn p2wsh_program_is_single_sha256_not_sha256d() {
        let redeem = bootstrap_redeem_script();
        let spk = bootstrap_p2wsh_scriptpubkey();
        assert_eq!(spk.len(), 34);
        assert_eq!(spk[0], 0x00);
        assert_eq!(spk[1], 0x20);
        assert_eq!(&spk[2..], &sha256(&redeem)[..]);
        assert_ne!(&spk[2..], &crate::sigma::sha256d(&redeem)[..]);
    }

    /// Pins the exact bootstrap scriptPubKey so a future primitive swap fails loudly.
    #[test]
    fn bootstrap_scriptpubkey_matches_witness_v0_script_hash_vector() {
        // redeem = OP_DROP x PCORB_WITNESS_ITEMS || OP_TRUE, per RedeemAutoResMintBootstrapScript().
        let redeem = bootstrap_redeem_script();
        assert_eq!(hex::encode(&redeem), "75757575757551");
        assert_eq!(
            hex::encode(bootstrap_p2wsh_scriptpubkey()),
            format!("0020{}", hex::encode(sha256(&redeem)))
        );
    }
}
