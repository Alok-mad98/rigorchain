use block_assembler::PublishedJobSnapshot;

use crate::error::{ValidationErrorCode, ValidatorError, ValidatorResult};

/// CVGE + RIGOR script consistency against JobSnapshot commitments.
pub fn validate_cvge_rigor(job: &PublishedJobSnapshot, coinbase_raw: &[u8]) -> ValidatorResult<()> {
    let outputs = parse_tx_outputs(coinbase_raw)?;
    let scripts: Vec<Vec<u8>> = outputs.into_iter().map(|o| o.script).collect();

    let rigor = hex::decode(&job.commitments.rigor_script_hex).map_err(|e| {
        ValidatorError::validation(ValidationErrorCode::ErrConvergenceMismatch, e.to_string())
    })?;
    if !scripts.iter().any(|s| s == &rigor) {
        return Err(ValidatorError::validation(
            ValidationErrorCode::ErrConvergenceMismatch,
            "RIGOR commitment missing from coinbase",
        ));
    }

    if let Some(cvge_hex) = &job.commitments.cvge_script_hex {
        let cvge = hex::decode(cvge_hex).map_err(|e| {
            ValidatorError::validation(ValidationErrorCode::ErrConvergenceMismatch, e.to_string())
        })?;
        if !scripts.iter().any(|s| s == &cvge) {
            return Err(ValidatorError::validation(
                ValidationErrorCode::ErrConvergenceMismatch,
                "CVGE commitment missing from coinbase",
            ));
        }
    }

    Ok(())
}

struct TxOut {
    script: Vec<u8>,
}

fn parse_tx_outputs(tx: &[u8]) -> ValidatorResult<Vec<TxOut>> {
    if tx.len() < 10 {
        return Err(ValidatorError::validation(
            ValidationErrorCode::ErrMalformedSubmission,
            "tx too short for output scan",
        ));
    }
    let mut off = 4usize;
    // BIP141: version | marker | flag | vin | vout | witness | locktime
    if tx.len() >= 6 && tx[4] == 0x00 && tx[5] == 0x01 {
        off = 6;
    }

    let vin_count = read_varint(tx, &mut off)? as usize;
    for _ in 0..vin_count {
        if off + 36 > tx.len() {
            return Err(ValidatorError::validation(
                ValidationErrorCode::ErrMalformedSubmission,
                "tx too short for input scan",
            ));
        }
        off += 36;
        let ss_len = read_varint(tx, &mut off)? as usize;
        if off + ss_len + 4 > tx.len() {
            return Err(ValidatorError::validation(
                ValidationErrorCode::ErrMalformedSubmission,
                "scriptSig truncated",
            ));
        }
        off += ss_len + 4;
    }

    let out_count = read_varint(tx, &mut off)?;
    let mut outs = Vec::new();
    for _ in 0..out_count {
        if off + 8 > tx.len() {
            break;
        }
        off += 8;
        let script_len = read_varint(tx, &mut off)? as usize;
        if off + script_len > tx.len() {
            break;
        }
        outs.push(TxOut {
            script: tx[off..off + script_len].to_vec(),
        });
        off += script_len;
    }
    Ok(outs)
}

fn read_varint(tx: &[u8], off: &mut usize) -> ValidatorResult<u64> {
    if *off >= tx.len() {
        return Err(ValidatorError::validation(
            ValidationErrorCode::ErrMalformedSubmission,
            "varint past end",
        ));
    }
    let first = tx[*off];
    *off += 1;
    match first {
        0..=0xfc => Ok(first as u64),
        0xfd => {
            if *off + 2 > tx.len() {
                return Err(ValidatorError::validation(
                    ValidationErrorCode::ErrMalformedSubmission,
                    "varint fd truncated",
                ));
            }
            let v = u16::from_le_bytes(tx[*off..*off + 2].try_into().unwrap()) as u64;
            *off += 2;
            Ok(v)
        }
        0xfe => {
            if *off + 4 > tx.len() {
                return Err(ValidatorError::validation(
                    ValidationErrorCode::ErrMalformedSubmission,
                    "varint fe truncated",
                ));
            }
            let v = u32::from_le_bytes(tx[*off..*off + 4].try_into().unwrap()) as u64;
            *off += 4;
            Ok(v)
        }
        _ => {
            if *off + 8 > tx.len() {
                return Err(ValidatorError::validation(
                    ValidationErrorCode::ErrMalformedSubmission,
                    "varint ff truncated",
                ));
            }
            let v = u64::from_le_bytes(tx[*off..*off + 8].try_into().unwrap());
            *off += 8;
            Ok(v)
        }
    }
}
