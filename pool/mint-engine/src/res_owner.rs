//! SECURITY-001-A / A-5 — read the RES payout owner declared by a convergence transaction.
//!
//! Consensus pays the RES minted for a convergence to the owner that convergence declared,
//! not to whoever assembled the block (`Consensus::CheckResRewardOwner`,
//! `core/src/consensus/res_owner.cpp`). The pool therefore cannot mint to an address of its
//! own: it must read the winner's declaration out of the raw transaction and pay that.
//!
//! This is a byte-for-byte mirror of the C++ recogniser. Any divergence means the pool builds
//! a block the network rejects with `bad-rigor-res-owner-mismatch`, so the shape is pinned
//! here as tightly as it is there: exact payload size, exact magic, exact version, canonical
//! push encoding, and **exactly one** declaration per transaction.

use crate::error::{MintError, MintResult};

/// ASCII `RESO`.
const RESO_MAGIC: [u8; 4] = [0x52, 0x45, 0x53, 0x4f];
const RES_OWNER_VERSION: u8 = 0x01;

/// `Consensus::RES_OWNER_PAYLOAD_SIZE` — magic + version + 32-byte program.
pub const RES_OWNER_PAYLOAD_SIZE: usize = 4 + 1 + 32;

/// `OP_RETURN` + minimal direct push + payload.
const DECLARATION_SCRIPT_SIZE: usize = 2 + RES_OWNER_PAYLOAD_SIZE;

const OP_RETURN: u8 = 0x6a;

/// Parse one `OP_RETURN` payload as a declaration, returning the 32-byte RES program.
fn parse_payload(data: &[u8]) -> Option<[u8; 32]> {
    if data.len() != RES_OWNER_PAYLOAD_SIZE {
        return None;
    }
    if data[0..4] != RESO_MAGIC {
        return None;
    }
    if data[4] != RES_OWNER_VERSION {
        return None;
    }
    let mut program = [0u8; 32];
    program.copy_from_slice(&data[5..]);
    Some(program)
}

/// True when `script` is exactly `OP_RETURN <push 37> <payload>`; returns the program.
fn program_from_script(script: &[u8]) -> Option<[u8; 32]> {
    if script.len() != DECLARATION_SCRIPT_SIZE {
        return None;
    }
    if script[0] != OP_RETURN {
        return None;
    }
    if script[1] != RES_OWNER_PAYLOAD_SIZE as u8 {
        return None;
    }
    parse_payload(&script[2..])
}

/// The RES payout program declared by `raw_tx`, which must declare **exactly one**.
///
/// Zero or several is not "pick one": consensus treats it as no declaration at all, which
/// keeps the transaction out of the candidate set `F`. Reaching this function with such a
/// transaction means the pool's winner selection and the node's disagree, so it is an error
/// rather than a fallback.
pub fn res_owner_program_from_raw_tx(raw_tx: &[u8]) -> MintResult<[u8; 32]> {
    let scripts = output_scripts(raw_tx).ok_or_else(|| {
        MintError::Validation("A-5: cannot parse winning convergence tx outputs".into())
    })?;

    let mut found: Option<[u8; 32]> = None;
    for script in &scripts {
        if let Some(program) = program_from_script(script) {
            if found.is_some() {
                return Err(MintError::Validation(
                    "A-5: winning convergence tx carries several RES owner declarations; \
                     consensus treats this as none and excludes it from F"
                        .into(),
                ));
            }
            found = Some(program);
        }
    }

    found.ok_or_else(|| {
        MintError::Validation(
            "A-5: winning convergence tx declares no RES owner; it cannot be in F".into(),
        )
    })
}

/// Output scripts of a raw transaction, tolerating the SegWit marker/flag.
///
/// Outputs precede the witness, so parsing stops once they are read.
fn output_scripts(raw: &[u8]) -> Option<Vec<Vec<u8>>> {
    let mut off = 4usize; // version
    if raw.len() < 10 {
        return None;
    }

    // SegWit marker 0x00 + flag. A real input count is never zero, so the marker is
    // unambiguous.
    if raw[off] == 0x00 {
        let flag = *raw.get(off + 1)?;
        if flag == 0x00 {
            return None;
        }
        off += 2;
    }

    let vin_count = read_compact_size(raw, &mut off)?;
    for _ in 0..vin_count {
        off = off.checked_add(36)?; // outpoint
        let script_sig_len = read_compact_size(raw, &mut off)? as usize;
        off = off.checked_add(script_sig_len)?.checked_add(4)?; // scriptSig + sequence
        if off > raw.len() {
            return None;
        }
    }

    let vout_count = read_compact_size(raw, &mut off)?;
    let mut scripts = Vec::new();
    for _ in 0..vout_count {
        off = off.checked_add(8)?; // value
        let script_len = read_compact_size(raw, &mut off)? as usize;
        let end = off.checked_add(script_len)?;
        if end > raw.len() {
            return None;
        }
        scripts.push(raw[off..end].to_vec());
        off = end;
    }
    Some(scripts)
}

fn read_compact_size(raw: &[u8], off: &mut usize) -> Option<u64> {
    let first = *raw.get(*off)?;
    *off += 1;
    let value = match first {
        0xfd => {
            let bytes = raw.get(*off..*off + 2)?;
            *off += 2;
            u16::from_le_bytes([bytes[0], bytes[1]]) as u64
        }
        0xfe => {
            let bytes = raw.get(*off..*off + 4)?;
            *off += 4;
            u32::from_le_bytes([bytes[0], bytes[1], bytes[2], bytes[3]]) as u64
        }
        0xff => {
            let bytes = raw.get(*off..*off + 8)?;
            *off += 8;
            u64::from_le_bytes(bytes.try_into().ok()?)
        }
        n => n as u64,
    };
    Some(value)
}

/// Build a declaration script, for tests and tooling that need to emit one.
pub fn res_owner_declaration_script(program32: &[u8; 32]) -> Vec<u8> {
    let mut script = Vec::with_capacity(DECLARATION_SCRIPT_SIZE);
    script.push(OP_RETURN);
    script.push(RES_OWNER_PAYLOAD_SIZE as u8);
    script.extend_from_slice(&RESO_MAGIC);
    script.push(RES_OWNER_VERSION);
    script.extend_from_slice(program32);
    script
}

#[cfg(test)]
mod tests {
    use super::*;

    /// Minimal convergence-shaped tx: one input, the given output scripts, no witness.
    fn raw_tx_with_output_scripts(scripts: &[Vec<u8>]) -> Vec<u8> {
        let mut raw = Vec::new();
        raw.extend_from_slice(&2i32.to_le_bytes());
        raw.push(0x01); // vin count
        raw.extend_from_slice(&[0x11; 32]); // prev txid
        raw.extend_from_slice(&0u32.to_le_bytes()); // prev vout
        raw.push(0x00); // empty scriptSig
        raw.extend_from_slice(&0xffff_ffffu32.to_le_bytes()); // sequence
        raw.push(scripts.len() as u8);
        for script in scripts {
            raw.extend_from_slice(&0i64.to_le_bytes());
            raw.push(script.len() as u8);
            raw.extend_from_slice(script);
        }
        raw.extend_from_slice(&0u32.to_le_bytes()); // locktime
        raw
    }

    fn segwit_raw_tx_with_output_scripts(scripts: &[Vec<u8>]) -> Vec<u8> {
        let mut raw = Vec::new();
        raw.extend_from_slice(&2i32.to_le_bytes());
        raw.push(0x00); // marker
        raw.push(0x01); // flag
        raw.push(0x01); // vin count
        raw.extend_from_slice(&[0x11; 32]);
        raw.extend_from_slice(&0u32.to_le_bytes());
        raw.push(0x00);
        raw.extend_from_slice(&0xffff_ffffu32.to_le_bytes());
        raw.push(scripts.len() as u8);
        for script in scripts {
            raw.extend_from_slice(&0i64.to_le_bytes());
            raw.push(script.len() as u8);
            raw.extend_from_slice(script);
        }
        raw.push(0x01); // one witness item
        raw.push(0x02);
        raw.extend_from_slice(&[0xaa, 0xbb]);
        raw.extend_from_slice(&0u32.to_le_bytes());
        raw
    }

    fn cvg2_script() -> Vec<u8> {
        let mut program = vec![0x43, 0x56, 0x47, 0x32]; // "CVG2"
        program.extend_from_slice(&[0x00; 28]);
        let mut script = vec![0x51, 0x20];
        script.extend_from_slice(&program);
        script
    }

    #[test]
    fn reads_declared_owner_program() {
        let program = [0xa1u8; 32];
        let raw = raw_tx_with_output_scripts(&[
            cvg2_script(),
            res_owner_declaration_script(&program),
        ]);
        assert_eq!(res_owner_program_from_raw_tx(&raw).unwrap(), program);
    }

    #[test]
    fn reads_declared_owner_program_from_segwit_tx() {
        let program = [0xb2u8; 32];
        let raw = segwit_raw_tx_with_output_scripts(&[
            cvg2_script(),
            res_owner_declaration_script(&program),
        ]);
        assert_eq!(res_owner_program_from_raw_tx(&raw).unwrap(), program);
    }

    #[test]
    fn undeclared_tx_is_an_error_not_a_fallback() {
        let raw = raw_tx_with_output_scripts(&[cvg2_script()]);
        assert!(res_owner_program_from_raw_tx(&raw).is_err());
    }

    #[test]
    fn several_declarations_are_rejected_like_consensus_does() {
        let raw = raw_tx_with_output_scripts(&[
            res_owner_declaration_script(&[0xa1; 32]),
            res_owner_declaration_script(&[0xb2; 32]),
        ]);
        assert!(res_owner_program_from_raw_tx(&raw).is_err());
    }

    #[test]
    fn non_canonical_push_encoding_is_not_a_declaration() {
        // OP_PUSHDATA1 carries the same 37 bytes but consensus does not recognise it, so the
        // pool must not either: paying that program would be paying nobody consensus agrees on.
        let program = [0xa1u8; 32];
        let canonical = res_owner_declaration_script(&program);
        let mut pushdata1 = vec![OP_RETURN, 0x4c, RES_OWNER_PAYLOAD_SIZE as u8];
        pushdata1.extend_from_slice(&canonical[2..]);
        let raw = raw_tx_with_output_scripts(&[pushdata1]);
        assert!(res_owner_program_from_raw_tx(&raw).is_err());
    }

    #[test]
    fn wrong_magic_or_version_is_not_a_declaration() {
        let program = [0xa1u8; 32];
        let mut wrong_magic = res_owner_declaration_script(&program);
        wrong_magic[2] = b'X';
        assert!(res_owner_program_from_raw_tx(&raw_tx_with_output_scripts(&[wrong_magic])).is_err());

        let mut wrong_version = res_owner_declaration_script(&program);
        wrong_version[6] = 0x02;
        assert!(
            res_owner_program_from_raw_tx(&raw_tx_with_output_scripts(&[wrong_version])).is_err()
        );
    }

    #[test]
    fn declaration_script_matches_consensus_layout() {
        let script = res_owner_declaration_script(&[0x5a; 32]);
        assert_eq!(script.len(), DECLARATION_SCRIPT_SIZE);
        assert_eq!(script[0], OP_RETURN);
        assert_eq!(script[1], RES_OWNER_PAYLOAD_SIZE as u8);
        assert_eq!(&script[2..6], b"RESO");
        assert_eq!(script[6], RES_OWNER_VERSION);
    }

    #[test]
    fn truncated_tx_does_not_panic() {
        let raw = raw_tx_with_output_scripts(&[res_owner_declaration_script(&[0xa1; 32])]);
        for cut in 0..raw.len() {
            let _ = res_owner_program_from_raw_tx(&raw[..cut]);
        }
    }
}
