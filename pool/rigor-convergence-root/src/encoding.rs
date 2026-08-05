use thiserror::Error;

#[derive(Debug, Error)]
pub enum ConvergenceRootError {
    #[error("hex decode: {0}")]
    Hex(#[from] hex::FromHexError),
    #[error("{0}")]
    Validation(String),
}

pub type ConvergenceRootResult<T> = Result<T, ConvergenceRootError>;

/// How txid hex strings are decoded before memcmp sort.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum TxidEncoding {
    /// GBT / `uint256::GetHex` — reverse decoded bytes to uint256 native storage.
    Display,
    /// Golden ConvergenceRoot vectors — forward hash-output byte order.
    HashHex,
    /// Golden `encoding: sethex` — literal bytes without endian reversal.
    SetHex,
}

/// Parse display-order hex (Bitcoin `GetHex` / GBT `convergence_txids`) into uint256 native.
pub fn display_hex_to_native(hex_str: &str) -> ConvergenceRootResult<[u8; 32]> {
    let mut bytes = hex::decode(hex_str)?;
    if bytes.len() != 32 {
        return Err(ConvergenceRootError::Validation(format!(
            "expected 32 bytes, got {}",
            bytes.len()
        )));
    }
    bytes.reverse();
    let mut out = [0u8; 32];
    out.copy_from_slice(&bytes);
    Ok(out)
}

/// Serialize uint256 native bytes to display-order hex (GBT txid wire form).
pub fn native_to_display_hex(native: &[u8; 32]) -> String {
    let mut rev = *native;
    rev.reverse();
    hex::encode(rev)
}

/// Serialize native root/hash bytes to forward hex (job snapshot / golden CR format).
pub fn native_to_hash_hex(native: &[u8; 32]) -> String {
    hex::encode(native)
}

/// Parse forward hash-output hex (golden CR expected roots).
pub fn hash_hex_to_native(hex_str: &str) -> ConvergenceRootResult<[u8; 32]> {
    let bytes = hex::decode(hex_str)?;
    if bytes.len() != 32 {
        return Err(ConvergenceRootError::Validation(format!(
            "expected 32 bytes, got {}",
            bytes.len()
        )));
    }
    let mut out = [0u8; 32];
    out.copy_from_slice(&bytes);
    Ok(out)
}

/// Parse `sethex` encoding (CR-011).
pub fn sethex_to_native(hex_str: &str) -> ConvergenceRootResult<[u8; 32]> {
    hash_hex_to_native(hex_str)
}
