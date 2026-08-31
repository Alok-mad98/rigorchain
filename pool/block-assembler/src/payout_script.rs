//! Coinbase payout script classification and safety validation (SECURITY-001-E / C-6).
//!
//! The pool's coinbase output is where every miner's reward lands. The historical
//! default was a P2PKH to an all-zero HASH160 — a provably unspendable burn — and
//! it was reachable through `AssemblerConfig::default()` with no override path, so
//! a misconfigured pool destroyed 100% of its coinbase.
//!
//! This module makes an unspendable or non-standard payout script a **startup
//! error** instead of a silent burn.
//!
//! Allowed forms match consensus `IsAllowedScript` spendable coinbase destinations:
//! Witness v0/20 (ORU P2WPKH) and native Witness v1/32 (RES, neither CVG2 nor RES2).
//! P2PKH, P2SH, P2WSH, CVG2, and RES2 are rejected. Asset class is still derived
//! from `scriptPubKey` bytes (`AssetTypeFromScriptPubKey`); this module does not
//! store an asset tag.
//!
//! Consensus impact: NONE. This validates a script the pool chooses to pay itself
//! to before the script is embedded in a coinbase.

use crate::error::{AssemblerError, AssemblerResult};

const CVG2_PREFIX: &[u8] = b"CVG2";
const RES2_PREFIX: &[u8] = b"RES2";

/// Recognized consensus-legal payout script forms.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum PayoutScriptKind {
    /// `OP_0 <20>` — ORU P2WPKH
    P2wpkh,
    /// `OP_1 <32>` — native RES program (the Ed25519 pubkey itself)
    WitnessV1,
}

/// Classify and safety-check a coinbase payout script.
///
/// Rejects: empty scripts, unknown/non-standard forms, retired prefixes, and
/// any all-zero hash/program (which is unspendable by construction).
pub fn validate_payout_script(script: &[u8]) -> AssemblerResult<PayoutScriptKind> {
    let (kind, payload) = classify(script).ok_or_else(|| {
        AssemblerError::Assembly(format!(
            "C-6: payout_script is not a consensus P2WPKH/native-v1 form (len={}, hex={})",
            script.len(),
            hex::encode(script)
        ))
    })?;

    if payload.iter().all(|b| *b == 0) {
        return Err(AssemblerError::Assembly(format!(
            "C-6: payout_script {kind:?} has an all-zero hash/program — coinbase funds \
             would be permanently unspendable (burn)"
        )));
    }
    Ok(kind)
}

/// True when the script is a known-unspendable placeholder rather than a real
/// destination. Used by startup gates to refuse booting a value-bearing pool.
pub fn is_unspendable_placeholder(script: &[u8]) -> bool {
    validate_payout_script(script).is_err()
}

fn classify(script: &[u8]) -> Option<(PayoutScriptKind, &[u8])> {
    match script {
        [0x00, 0x14, rest @ ..] if rest.len() == 20 => Some((PayoutScriptKind::P2wpkh, rest)),
        [0x51, 0x20, rest @ ..]
            if rest.len() == 32 && !rest.starts_with(CVG2_PREFIX) && !rest.starts_with(RES2_PREFIX) =>
        {
            Some((PayoutScriptKind::WitnessV1, rest))
        }
        _ => None,
    }
}

/// Parse and validate a hex-encoded payout script (config / env ingestion path).
pub fn payout_script_from_hex(hex_str: &str) -> AssemblerResult<Vec<u8>> {
    let trimmed = hex_str.trim();
    if trimmed.is_empty() {
        return Err(AssemblerError::Assembly(
            "C-6: payout_script hex is empty".into(),
        ));
    }
    let script = hex::decode(trimmed)?;
    validate_payout_script(&script)?;
    Ok(script)
}

#[cfg(test)]
mod tests {
    use super::*;

    fn p2pkh(h: [u8; 20]) -> Vec<u8> {
        let mut s = vec![0x76, 0xa9, 0x14];
        s.extend_from_slice(&h);
        s.extend_from_slice(&[0x88, 0xac]);
        s
    }

    fn p2wpkh(h: [u8; 20]) -> Vec<u8> {
        let mut s = vec![0x00, 0x14];
        s.extend_from_slice(&h);
        s
    }

    fn v1(program: [u8; 32]) -> Vec<u8> {
        let mut s = vec![0x51, 0x20];
        s.extend_from_slice(&program);
        s
    }

    /// C-6 regression: the historical default coinbase script was a burn.
    #[test]
    fn all_zero_p2pkh_burn_is_rejected() {
        let err = validate_payout_script(&p2pkh([0u8; 20]))
            .unwrap_err()
            .to_string();
        assert!(err.contains("C-6"), "unexpected: {err}");
        assert!(is_unspendable_placeholder(&p2pkh([0u8; 20])));
    }

    #[test]
    fn consensus_forms_are_accepted() {
        assert_eq!(
            validate_payout_script(&p2wpkh([0x33; 20])).unwrap(),
            PayoutScriptKind::P2wpkh
        );

        assert_eq!(
            validate_payout_script(&v1([0x55; 32])).unwrap(),
            PayoutScriptKind::WitnessV1
        );
    }

    #[test]
    fn legacy_and_retired_forms_are_rejected() {
        assert!(validate_payout_script(&p2pkh([0x11; 20])).is_err());

        let mut p2sh = vec![0xa9, 0x14];
        p2sh.extend_from_slice(&[0x22u8; 20]);
        p2sh.push(0x87);
        assert!(validate_payout_script(&p2sh).is_err());

        let mut p2wsh = vec![0x00, 0x20];
        p2wsh.extend_from_slice(&[0x44u8; 32]);
        assert!(validate_payout_script(&p2wsh).is_err());

        let mut cvg2 = vec![0x51, 0x20];
        cvg2.extend_from_slice(b"CVG2");
        cvg2.extend_from_slice(&[0x11u8; 28]);
        assert!(validate_payout_script(&cvg2).is_err());

        let mut res2 = vec![0x51, 0x20];
        res2.extend_from_slice(b"RES2");
        res2.extend_from_slice(&[0x11u8; 28]);
        assert!(validate_payout_script(&res2).is_err());
    }

    #[test]
    fn empty_and_nonstandard_are_rejected() {
        assert!(validate_payout_script(&[]).is_err());
        assert!(validate_payout_script(&[0x6a, 0x01, 0x00]).is_err()); // OP_RETURN
        assert!(validate_payout_script(&[0x00, 0x20, 0x01]).is_err()); // truncated program
        let mut v2 = vec![0x52, 0x20];
        v2.extend_from_slice(&[0x66u8; 32]);
        assert!(validate_payout_script(&v2).is_err());
    }

    #[test]
    fn all_zero_witness_programs_are_rejected() {
        assert!(validate_payout_script(&p2wpkh([0u8; 20])).is_err());
        assert!(validate_payout_script(&v1([0u8; 32])).is_err());
    }

    #[test]
    fn hex_ingestion_validates() {
        assert!(payout_script_from_hex("").is_err());
        assert!(payout_script_from_hex("zz").is_err());
        assert!(payout_script_from_hex(&hex::encode(p2pkh([0u8; 20]))).is_err());
        let good = hex::encode(p2wpkh([0x77; 20]));
        assert_eq!(payout_script_from_hex(&good).unwrap(), p2wpkh([0x77; 20]));
    }
}
