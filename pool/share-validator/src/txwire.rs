//! BIP141 transaction wire helpers for **reading** GBT `data` hex.
//!
//! The pool never constructs a RES mint from these routines. Encoding helpers
//! exist only under `cfg(test)` so unit tests can feed node-shaped fixtures.

use crate::error::{ValidationErrorCode, ValidatorError, ValidatorResult};

pub struct TxIn {
    pub prev_txid: [u8; 32],
    pub vout: u32,
    pub script_sig: Vec<u8>,
    pub sequence: u32,
}

pub struct TxOut {
    pub value: i64,
    pub script: Vec<u8>,
}

pub struct ParsedTx {
    pub version: i32,
    pub vin: Vec<TxIn>,
    pub vout: Vec<TxOut>,
    /// Per-input witness stacks; empty when the tx is non-segwit.
    pub witnesses: Vec<Vec<Vec<u8>>>,
    pub locktime: u32,
}

pub fn parse_tx(tx: &[u8]) -> ValidatorResult<ParsedTx> {
    if tx.len() < 10 {
        return Err(malformed("tx too short"));
    }
    let mut off = 0usize;
    let version = read_i32(tx, &mut off)?;
    let mut segwit = false;
    if off + 2 <= tx.len() && tx[off] == 0x00 && tx[off + 1] == 0x01 {
        segwit = true;
        off += 2;
    }

    let vin_count = read_varint(tx, &mut off)? as usize;
    let mut vin = Vec::with_capacity(vin_count);
    for _ in 0..vin_count {
        if off + 36 > tx.len() {
            return Err(malformed("truncated input"));
        }
        let mut prev_txid = [0u8; 32];
        prev_txid.copy_from_slice(&tx[off..off + 32]);
        off += 32;
        let vout = read_u32(tx, &mut off)?;
        let ss_len = read_varint(tx, &mut off)? as usize;
        if off + ss_len + 4 > tx.len() {
            return Err(malformed("scriptSig truncated"));
        }
        let script_sig = tx[off..off + ss_len].to_vec();
        off += ss_len;
        let sequence = read_u32(tx, &mut off)?;
        vin.push(TxIn {
            prev_txid,
            vout,
            script_sig,
            sequence,
        });
    }

    let vout_count = read_varint(tx, &mut off)? as usize;
    let mut vout = Vec::with_capacity(vout_count);
    for _ in 0..vout_count {
        if off + 8 > tx.len() {
            return Err(malformed("truncated output value"));
        }
        let value = read_i64(tx, &mut off)?;
        let script_len = read_varint(tx, &mut off)? as usize;
        if off + script_len > tx.len() {
            return Err(malformed("scriptPubKey truncated"));
        }
        let script = tx[off..off + script_len].to_vec();
        off += script_len;
        vout.push(TxOut { value, script });
    }

    let mut witnesses = vec![Vec::new(); vin.len()];
    if segwit {
        for w in witnesses.iter_mut() {
            let n = read_varint(tx, &mut off)? as usize;
            for _ in 0..n {
                let len = read_varint(tx, &mut off)? as usize;
                if off + len > tx.len() {
                    return Err(malformed("witness item truncated"));
                }
                w.push(tx[off..off + len].to_vec());
                off += len;
            }
        }
    }

    let locktime = read_u32(tx, &mut off)?;
    Ok(ParsedTx {
        version,
        vin,
        vout,
        witnesses,
        locktime,
    })
}

pub fn read_varint(buf: &[u8], off: &mut usize) -> ValidatorResult<u64> {
    if *off >= buf.len() {
        return Err(malformed("varint past end"));
    }
    let first = buf[*off];
    *off += 1;
    match first {
        0..=0xfc => Ok(first as u64),
        0xfd => Ok(u16::from_le_bytes(read_arr(buf, off)?) as u64),
        0xfe => Ok(u32::from_le_bytes(read_arr(buf, off)?) as u64),
        _ => Ok(u64::from_le_bytes(read_arr(buf, off)?)),
    }
}

fn read_i32(buf: &[u8], off: &mut usize) -> ValidatorResult<i32> {
    Ok(i32::from_le_bytes(read_arr(buf, off)?))
}

fn read_u32(buf: &[u8], off: &mut usize) -> ValidatorResult<u32> {
    Ok(u32::from_le_bytes(read_arr(buf, off)?))
}

fn read_i64(buf: &[u8], off: &mut usize) -> ValidatorResult<i64> {
    Ok(i64::from_le_bytes(read_arr(buf, off)?))
}

fn read_arr<const N: usize>(buf: &[u8], off: &mut usize) -> ValidatorResult<[u8; N]> {
    if *off + N > buf.len() {
        return Err(malformed("truncated integer"));
    }
    let mut a = [0u8; N];
    a.copy_from_slice(&buf[*off..*off + N]);
    *off += N;
    Ok(a)
}

fn malformed(msg: &str) -> ValidatorError {
    ValidatorError::validation(ValidationErrorCode::ErrMalformedSubmission, msg)
}

#[cfg(test)]
pub fn write_varint(buf: &mut Vec<u8>, n: usize) {
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

#[cfg(test)]
pub fn write_witness_stack(buf: &mut Vec<u8>, stack: &[Vec<u8>]) {
    write_varint(buf, stack.len());
    for item in stack {
        write_varint(buf, item.len());
        buf.extend_from_slice(item);
    }
}
