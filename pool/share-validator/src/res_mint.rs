use block_assembler::PublishedJobSnapshot;

use crate::error::{ValidationErrorCode, ValidatorError, ValidatorResult};

/// ORU-only share gate (TASK-POOL-RES-BOUNDARY-IMPLEMENT-003).
///
/// Jobs must not reserve a RES merkle placeholder or mark mint_required. The pool
/// never synthesizes RES body mints; any mint in GBT remains opaque node data.
pub fn validate_res_mint_placeholder(job: &PublishedJobSnapshot) -> ValidatorResult<()> {
    if job.body_mint.reserved {
        return Err(ValidatorError::validation(
            ValidationErrorCode::ErrResMintInvalid,
            "ORU-only pool: body mint merkle placeholder must not be reserved",
        ));
    }
    if job.body_mint.mint_required {
        return Err(ValidatorError::validation(
            ValidationErrorCode::ErrResMintInvalid,
            "ORU-only pool: job must not mark mint_required",
        ));
    }
    if job.body_mint.body_mint_txid.is_some() {
        return Err(ValidatorError::validation(
            ValidationErrorCode::ErrResMintInvalid,
            "ORU-only pool: job must not carry a pool-built body_mint_txid",
        ));
    }
    Ok(())
}

/// Block hex advisory check: pool must not have attached a synthesized mint leaf
/// beyond the opaque GBT package already committed in the job merkle.
///
/// When `allow_res_body_mint` is true (node may have included automint in GBT),
/// vtx count may equal coinbase + GBT txs; we do not invent additional mint txs.
pub fn validate_block_res_mint(
    job: &PublishedJobSnapshot,
    block_hex: &str,
    allow_res_body_mint: bool,
) -> ValidatorResult<()> {
    let _ = allow_res_body_mint;

    if let Some(ref mint_txid) = job.body_mint.body_mint_txid {
        let lower = block_hex.to_ascii_lowercase();
        if lower.contains(&mint_txid.to_ascii_lowercase()) {
            return Err(ValidatorError::validation(
                ValidationErrorCode::ErrResMintInvalid,
                "unauthorized pool-built RES body mint tx in block submission",
            ));
        }
    }

    let block = hex::decode(block_hex).map_err(|e| {
        ValidatorError::validation(ValidationErrorCode::ErrMalformedSubmission, e.to_string())
    })?;
    let vtx_count = count_block_transactions(&block)?;
    // slot_index historically meant "index of reserved mint leaf"; under ORU-only
    // it is gbt_tx_count + 1, so max vtx = coinbase + gbt = slot_index.
    let expected_max = job.body_mint.slot_index;
    if vtx_count > expected_max {
        return Err(ValidatorError::validation(
            ValidationErrorCode::ErrResMintInvalid,
            format!(
                "block has {vtx_count} txs; ORU-only max is {expected_max} (coinbase + opaque GBT)"
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
                ValidatorError::validation(ValidationErrorCode::ErrMalformedSubmission, "truncate")
            })?);
            *off += 8;
            Ok(v)
        }
    }
}
