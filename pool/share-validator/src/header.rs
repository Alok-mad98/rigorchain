use crate::error::{ValidationErrorCode, ValidatorError, ValidatorResult};
use block_assembler::{txid_from_tx_bytes, PublishedJobSnapshot};

/// Reconstruct 80-byte block header from job + miner deltas.
pub fn build_header_bytes(
    job: &PublishedJobSnapshot,
    extranonce1: &[u8],
    extranonce2: &[u8],
    ntime: u32,
    nonce: u32,
) -> ValidatorResult<[u8; 80]> {
    let coinbase_raw = assemble_coinbase_raw(job, extranonce1, extranonce2)?;
    // Bitcoin block merkle uses txid (witness stripped), not the full witness blob hash.
    let coinbase_hash = txid_from_tx_bytes(&coinbase_raw);
    let merkle_root = compute_merkle_root(&coinbase_hash, &job.merkle_branch)?;

    let version = job.version;
    let nbits = parse_nbits(&job.nbits)?;
    let prevhash = decode_prevhash(&job.prevhash)?;

    let mut header = [0u8; 80];
    header[0..4].copy_from_slice(&version.to_le_bytes());
    header[4..36].copy_from_slice(&prevhash);
    header[36..68].copy_from_slice(&merkle_root);
    header[68..72].copy_from_slice(&ntime.to_le_bytes());
    header[72..76].copy_from_slice(&nbits);
    header[76..80].copy_from_slice(&nonce.to_le_bytes());
    Ok(header)
}

pub fn header_base_hash(header: &[u8; 80]) -> [u8; 32] {
    crate::opuhash::hash256(header)
}

pub(crate) fn assemble_coinbase_raw(
    job: &PublishedJobSnapshot,
    extranonce1: &[u8],
    extranonce2: &[u8],
) -> ValidatorResult<Vec<u8>> {
    block_assembler::assemble_solved_coinbase_raw(job, extranonce1, extranonce2).map_err(|e| {
        ValidatorError::validation(ValidationErrorCode::ErrMalformedSubmission, e.to_string())
    })
}

fn compute_merkle_root(coinbase_hash: &[u8; 32], branches: &[String]) -> ValidatorResult<[u8; 32]> {
    let mut hash = *coinbase_hash;
    let mut index = 0usize;
    for branch_hex in branches {
        let branch_bytes = hex::decode(branch_hex).map_err(|e| {
            ValidatorError::validation(ValidationErrorCode::ErrMalformedSubmission, e.to_string())
        })?;
        if branch_bytes.len() != 32 {
            return Err(ValidatorError::validation(
                ValidationErrorCode::ErrMalformedSubmission,
                "merkle branch must be 32 bytes",
            ));
        }
        let mut branch = [0u8; 32];
        branch.copy_from_slice(&branch_bytes);

        let mut pair = Vec::with_capacity(64);
        if index % 2 == 0 {
            pair.extend_from_slice(&hash);
            pair.extend_from_slice(&branch);
        } else {
            pair.extend_from_slice(&branch);
            pair.extend_from_slice(&hash);
        }
        hash = crate::opuhash::hash256(&pair);
        index += 1;
    }
    Ok(hash)
}

fn decode_prevhash(hex_str: &str) -> ValidatorResult<[u8; 32]> {
    let bytes = hex::decode(hex_str).map_err(|e| {
        ValidatorError::validation(ValidationErrorCode::ErrMalformedSubmission, e.to_string())
    })?;
    if bytes.len() != 32 {
        return Err(ValidatorError::validation(
            ValidationErrorCode::ErrMalformedSubmission,
            "prevhash must be 32 bytes",
        ));
    }
    let mut out = [0u8; 32];
    out.copy_from_slice(&bytes);
    out.reverse();
    Ok(out)
}

fn parse_nbits(bits_hex: &str) -> ValidatorResult<[u8; 4]> {
    let compact = u32::from_str_radix(bits_hex, 16).map_err(|e| {
        ValidatorError::validation(ValidationErrorCode::ErrMalformedSubmission, e.to_string())
    })?;
    Ok(compact.to_le_bytes())
}

pub fn coinbase_scriptsig_from_submission(
    job: &PublishedJobSnapshot,
    extranonce1: &[u8],
    extranonce2: &[u8],
) -> ValidatorResult<Vec<u8>> {
    let raw = assemble_coinbase_raw(job, extranonce1, extranonce2)?;
    extract_vin0_scriptsig(&raw)
}

fn extract_vin0_scriptsig(tx: &[u8]) -> ValidatorResult<Vec<u8>> {
    if tx.len() < 10 {
        return Err(ValidatorError::validation(
            ValidationErrorCode::ErrMalformedSubmission,
            "coinbase too short",
        ));
    }
    let mut off = 4usize;
    // BIP141: version | marker | flag | vin...
    if tx.len() >= 6 && tx[4] == 0x00 && tx[5] == 0x01 {
        off = 6;
    }
    if off >= tx.len() {
        return Err(ValidatorError::validation(
            ValidationErrorCode::ErrMalformedSubmission,
            "coinbase missing vin",
        ));
    }
    let vin_count = tx[off];
    off += 1;
    if vin_count != 1 {
        return Err(ValidatorError::validation(
            ValidationErrorCode::ErrMalformedSubmission,
            "expected single coinbase input",
        ));
    }
    if off + 36 > tx.len() {
        return Err(ValidatorError::validation(
            ValidationErrorCode::ErrMalformedSubmission,
            "coinbase prevout truncated",
        ));
    }
    off += 36;
    if off >= tx.len() {
        return Err(ValidatorError::validation(
            ValidationErrorCode::ErrMalformedSubmission,
            "scriptSig missing",
        ));
    }
    let script_len = tx[off] as usize;
    off += 1;
    if script_len >= 0xfd {
        return Err(ValidatorError::validation(
            ValidationErrorCode::ErrMalformedSubmission,
            "scriptSig length compact size not supported in extractor",
        ));
    }
    if off + script_len > tx.len() {
        return Err(ValidatorError::validation(
            ValidationErrorCode::ErrMalformedSubmission,
            "scriptSig out of range",
        ));
    }
    Ok(tx[off..off + script_len].to_vec())
}
