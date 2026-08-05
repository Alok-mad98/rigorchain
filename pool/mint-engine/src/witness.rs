use crate::error::MintResult;
use crate::sigma::converge_proof_digest;
use crate::types::ConvergeProofFields;

/// Pushes in a PCORB witness: exactly the six proof fields.
///
/// Consensus carried five more until SECURITY-001-A / A-7: an Ed25519 signature over the σ digest
/// and the key that verified it, plus a leaf script and two reserved-zero placeholders. The
/// signature verified under a key the witness supplied itself, over a digest every other field
/// already pins to the block, so any keypair satisfied it; the remaining three were only read by a
/// RES spending path that was removed for accepting signatures that did not cover the spending
/// transaction. Funding inputs that carry this witness must drop exactly this many pushes.
pub const PCORB_WITNESS_ITEMS: usize = 6;

/// PCORB witness stack (bottom → top) per RES_BODY_MINT_SCHEMA.md §Witness stack.
///
/// Returns the stack and the σ digest. σ is unchanged and still decides the draw; nothing signs it.
pub fn build_pcorb_witness_stack(
    proof: &ConvergeProofFields,
) -> MintResult<(Vec<Vec<u8>>, [u8; 32])> {
    let digest = converge_proof_digest(proof);

    let mut stack = Vec::with_capacity(PCORB_WITNESS_ITEMS);
    stack.push(proof.convergence_txid.to_vec());
    stack.push(proof.convergence_amount_sat.to_le_bytes().to_vec());
    stack.push(proof.bind_hash.to_vec());
    stack.push(proof.height.to_le_bytes().to_vec());
    stack.push(proof.target_prev.to_le_bytes().to_vec());
    stack.push(proof.convergence_root.to_vec());

    Ok((stack, digest))
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::types::{ConvergeProofFields, MIN_CONVERGENCE_AMOUNT_SAT};

    fn fields() -> ConvergeProofFields {
        ConvergeProofFields {
            convergence_txid: [0xab; 32],
            convergence_amount_sat: MIN_CONVERGENCE_AMOUNT_SAT,
            bind_hash: [0xcd; 32],
            height: 42,
            target_prev: 0x207fffff,
            convergence_root: [0xef; 32],
        }
    }

    #[test]
    fn pcorb_stack_is_the_six_proof_fields() {
        let (stack, _) = build_pcorb_witness_stack(&fields()).unwrap();
        assert_eq!(stack.len(), PCORB_WITNESS_ITEMS);
        assert_eq!(stack[0].len(), 32);
        assert_eq!(stack[1].len(), 8);
        assert_eq!(stack[2].len(), 32);
        assert_eq!(stack[3].len(), 8);
        assert_eq!(stack[4].len(), 4);
        assert_eq!(stack[5].len(), 32);
    }

    // The pool holds no key on the mint path. Guards against a signature push creeping back in,
    // which would desynchronise the witness from `ParseConvergeProof` and break every mint.
    #[test]
    fn pcorb_stack_carries_no_signature_or_key() {
        let (stack, digest) = build_pcorb_witness_stack(&fields()).unwrap();
        assert!(stack.iter().all(|item| item.len() != 64));
        assert_eq!(digest, crate::sigma::converge_proof_digest(&fields()));
    }
}
