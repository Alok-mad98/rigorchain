use block_assembler::PublishedJobSnapshot;

use crate::error::{ValidationErrorCode, ValidatorError, ValidatorResult};

/// RES body mint safety gate (TASK-MINING-008 §6).
pub fn validate_res_mint_placeholder(job: &PublishedJobSnapshot) -> ValidatorResult<()> {
    if job.body_mint.body_mint_txid.is_some() {
        return Err(ValidatorError::validation(
            ValidationErrorCode::ErrResMintInvalid,
            "share path: body_mint_txid must be None (placeholder only)",
        ));
    }
    if !job.body_mint.reserved {
        return Err(ValidatorError::validation(
            ValidationErrorCode::ErrResMintInvalid,
            "body mint slot must remain reserved",
        ));
    }
    Ok(())
}

/// Block hex must not contain a real RES body mint unless explicitly allowed.
pub fn validate_block_res_mint(
    job: &PublishedJobSnapshot,
    block_hex: &str,
    allow_res_body_mint: bool,
) -> ValidatorResult<()> {
    if allow_res_body_mint {
        return Ok(());
    }

    let lower = block_hex.to_ascii_lowercase();
    if let Some(ref mint_txid) = job.body_mint.body_mint_txid {
        if lower.contains(&mint_txid.to_ascii_lowercase()) {
            return Err(ValidatorError::validation(
                ValidationErrorCode::ErrResMintInvalid,
                "unauthorized RES body mint tx in block submission",
            ));
        }
    }

    // Heuristic: reject blocks that include more vtx than GBT package + coinbase + placeholder slot.
    let block = hex::decode(block_hex)
        .map_err(|e| ValidatorError::validation(ValidationErrorCode::ErrMalformedSubmission, e.to_string()))?;
    let vtx_count = count_block_transactions(&block)?;
    let expected_max = 1 + job.body_mint.slot_index;
    if vtx_count > expected_max {
        return Err(ValidatorError::validation(
            ValidationErrorCode::ErrResMintInvalid,
            format!(
                "block has {vtx_count} txs; max allowed without mint engine is {expected_max}"
            ),
        ));
    }
    Ok(())
}

fn count_block_transactions(block: &[u8]) -> ValidatorResult<usize> {
    if block.len() < 84 {
        return Err(ValidatorError::validation(
            ValidationErrorCode::ErrMalformedSubmission,
            "block too short",
        ));
    }
    let mut off = 80usize;
    let tx_count = read_varint(block, &mut off)?;
    Ok(tx_count as usize)
}

fn read_varint(buf: &[u8], off: &mut usize) -> ValidatorResult<u64> {
    if *off >= buf.len() {
        return Err(ValidatorError::validation(
            ValidationErrorCode::ErrMalformedSubmission,
            "varint past end",
        ));
    }
    let first = buf[*off];
    *off += 1;
    match first {
        0..=0xfc => Ok(first as u64),
        0xfd => {
            let v = u16::from_le_bytes(buf[*off..*off + 2].try_into().map_err(|_| {
                ValidatorError::validation(ValidationErrorCode::ErrMalformedSubmission, "truncated")
            })?) as u64;
            *off += 2;
            Ok(v)
        }
        0xfe => {
            let v = u32::from_le_bytes(buf[*off..*off + 4].try_into().map_err(|_| {
                ValidatorError::validation(ValidationErrorCode::ErrMalformedSubmission, "truncated")
            })?) as u64;
            *off += 4;
            Ok(v)
        }
        _ => {
            let v = u64::from_le_bytes(buf[*off..*off + 8].try_into().map_err(|_| {
                ValidatorError::validation(ValidationErrorCode::ErrMalformedSubmission, "truncated")
            })?);
            *off += 8;
            Ok(v)
        }
    }
}
