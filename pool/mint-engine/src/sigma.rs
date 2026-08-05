use sha2::{Digest, Sha256};

pub fn sha256d(data: &[u8]) -> [u8; 32] {
    let first = Sha256::digest(data);
    let second = Sha256::digest(&first);
    let mut out = [0u8; 32];
    out.copy_from_slice(&second);
    out
}

/// Single SHA-256. Required by BIP141 `WitnessV0ScriptHash` (P2WSH program).
pub fn sha256(data: &[u8]) -> [u8; 32] {
    let mut out = [0u8; 32];
    out.copy_from_slice(&Sha256::digest(data));
    out
}

/// ``Consensus::ConvergeProofDigest`` / ``rigor::ComputeSigmaHash`` (100-byte buffer).
///
/// SECURITY-001-A8 (Sigma-PoW-01): slot `[64..95]` is the parent block hash verbatim
/// (`proof.bind_hash` == B_{h-1}). `target_prev` remains a PCORB witness field but does
/// not enter the digest — the parent header already commits its nBits.
pub fn converge_proof_digest(proof: &super::types::ConvergeProofFields) -> [u8; 32] {
    let height_le = (proof.height as u32).to_le_bytes();
    let mut buf = [0u8; 100];
    buf[0..32].copy_from_slice(&proof.convergence_txid);
    buf[32..64].copy_from_slice(&proof.convergence_root);
    buf[64..96].copy_from_slice(&proof.bind_hash);
    buf[96..100].copy_from_slice(&height_le);
    sha256d(&buf)
}

pub fn display_hex_to_le32(hex_str: &str) -> crate::error::MintResult<[u8; 32]> {
    let mut bytes = hex::decode(hex_str).map_err(|e| crate::error::MintError::Validation(e.to_string()))?;
    if bytes.len() != 32 {
        return Err(crate::error::MintError::Validation(format!(
            "expected 32 bytes, got {}",
            bytes.len()
        )));
    }
    bytes.reverse();
    let mut out = [0u8; 32];
    out.copy_from_slice(&bytes);
    Ok(out)
}

pub fn le32_to_display_hex(bytes: &[u8; 32]) -> String {
    let mut rev = *bytes;
    rev.reverse();
    hex::encode(rev)
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::types::{ConvergeProofFields, MIN_CONVERGENCE_AMOUNT_SAT};

    fn sample_proof() -> ConvergeProofFields {
        ConvergeProofFields {
            convergence_txid: [1u8; 32],
            convergence_amount_sat: MIN_CONVERGENCE_AMOUNT_SAT,
            bind_hash: [2u8; 32],
            height: 100,
            target_prev: 0x1c00ff00,
            convergence_root: [3u8; 32],
        }
    }

    #[test]
    fn digest_is_deterministic() {
        let proof = sample_proof();
        assert_eq!(converge_proof_digest(&proof), converge_proof_digest(&proof));
    }

    #[test]
    fn target_prev_does_not_enter_digest() {
        let mut a = sample_proof();
        let mut b = sample_proof();
        b.target_prev = a.target_prev ^ 0xffff;
        assert_eq!(converge_proof_digest(&a), converge_proof_digest(&b));
        b.bind_hash[0] ^= 1;
        assert_ne!(converge_proof_digest(&a), converge_proof_digest(&b));
    }

    #[test]
    fn parent_hash_is_not_folded() {
        let proof = sample_proof();
        let direct = converge_proof_digest(&proof);

        let mut folded_preimage = Vec::with_capacity(36);
        folded_preimage.extend_from_slice(&proof.bind_hash);
        folded_preimage.extend_from_slice(&proof.target_prev.to_le_bytes());
        let folded = sha256d(&folded_preimage);

        let mut old = proof.clone();
        old.bind_hash = folded;
        // Old rule put Hash256(B || LE32(nBits)) in the slot; A-8 puts B itself.
        assert_ne!(direct, {
            let height_le = (old.height as u32).to_le_bytes();
            let mut buf = [0u8; 100];
            buf[0..32].copy_from_slice(&old.convergence_txid);
            buf[32..64].copy_from_slice(&old.convergence_root);
            buf[64..96].copy_from_slice(&folded);
            buf[96..100].copy_from_slice(&height_le);
            sha256d(&buf)
        });
        let _ = direct;
    }
}
