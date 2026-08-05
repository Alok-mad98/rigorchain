use sha2::{Digest, Sha256};

use crate::error::{WinnerError, WinnerResult};

/// Hash256 = SHA256(SHA256(x)) per RIGOR consensus.
pub fn hash256(data: &[u8]) -> [u8; 32] {
    let first = Sha256::digest(data);
    let second = Sha256::digest(&first);
    let mut out = [0u8; 32];
    out.copy_from_slice(&second);
    out
}

/// Parse display-order hex (Bitcoin `GetHex` / GBT) into uint256 native bytes.
pub fn display_hex_to_native(hex_str: &str) -> WinnerResult<[u8; 32]> {
    rigor_convergence_root::display_hex_to_native(hex_str)
        .map_err(|e| WinnerError::Validation(e.to_string()))
}

/// Serialize uint256 native bytes to display-order hex.
pub fn native_to_display_hex(native: &[u8; 32]) -> String {
    rigor_convergence_root::native_to_display_hex(native)
}

/// Parse raw hash output bytes from forward-order hex (golden sigma expectations).
pub fn hash_hex_to_native(hex_str: &str) -> WinnerResult<[u8; 32]> {
    rigor_convergence_root::hash_hex_to_native(hex_str)
        .map_err(|e| WinnerError::Validation(e.to_string()))
}

/// Parse `sethex` encoding: literal byte values without endian reversal (CR-011).
pub fn sethex_to_native(hex_str: &str) -> WinnerResult<[u8; 32]> {
    rigor_convergence_root::sethex_to_native(hex_str)
        .map_err(|e| WinnerError::Validation(e.to_string()))
}

/// Compare uint256 native bytes (memcmp ascending).
pub fn uint256_cmp(a: &[u8; 32], b: &[u8; 32]) -> std::cmp::Ordering {
    a.cmp(b)
}

/// Parse compact `prev_nbits` from GBT hex string.
pub fn parse_prev_nbits(hex_str: &str) -> WinnerResult<u32> {
    let v = u32::from_str_radix(hex_str, 16)
        .map_err(|e| WinnerError::Validation(format!("prev_nbits: {e}")))?;
    Ok(v)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn display_roundtrip() {
        let display = "8cb9012517c817fead650287d61bdd9c68803b6bf9c64133dcab3e65b5a50cb9";
        let native = display_hex_to_native(display).unwrap();
        assert_eq!(native_to_display_hex(&native), display);
    }
}
