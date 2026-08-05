/// Encode BIP34 block height for coinbase `scriptSig` push #1.
pub fn encode_bip34_height(height: u64) -> Vec<u8> {
    if height == 0 {
        return vec![0x00];
    }
    if (1..=16).contains(&height) {
        return vec![0x50 + height as u8];
    }
    let mut magnitude = Vec::new();
    let mut n = height;
    while n > 0 {
        magnitude.push((n & 0xff) as u8);
        n >>= 8;
    }
    if magnitude.last().map(|b| b & 0x80 != 0).unwrap_or(false) {
        magnitude.push(0);
    }
    let mut out = vec![magnitude.len() as u8];
    out.extend(magnitude);
    out
}

/// Build coinbase `scriptSig`: height ‖ ConvergenceRoot (exactly two pushes).
pub fn build_coinbase_scriptsig(height: u64, convergence_root: &[u8; 32]) -> Vec<u8> {
    let height_push = encode_bip34_height(height);
    let mut script = Vec::with_capacity(height_push.len() + 33);
    script.extend_from_slice(&height_push);
    script.push(0x20);
    script.extend_from_slice(convergence_root);
    script
}

/// RIGOR draw-target commitment: OP_RETURN + 8-byte payload (RIGOR tag + prev_nbits LE32).
pub fn build_rigor_commitment_script(prev_nbits: u32) -> Vec<u8> {
    let mut script = Vec::with_capacity(10);
    script.push(0x6a); // OP_RETURN
    script.push(0x08); // push 8 bytes
    script.extend_from_slice(b"RIGR");
    script.push((prev_nbits & 0xff) as u8);
    script.push(((prev_nbits >> 8) & 0xff) as u8);
    script.push(((prev_nbits >> 16) & 0xff) as u8);
    script.push(((prev_nbits >> 24) & 0xff) as u8);
    script
}

/// Parse 8-char compact `bits` / `prev_nbits` hex (same as cpuminer `%x` parse).
pub fn parse_compact_hex(hex_str: &str) -> Result<u32, String> {
    if hex_str.len() != 8 {
        return Err(format!("expected 8 hex chars, got len {}", hex_str.len()));
    }
    u32::from_str_radix(hex_str, 16).map_err(|e| e.to_string())
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn bip34_small_height_uses_opcodes() {
        assert_eq!(encode_bip34_height(1), vec![0x51]);
        assert_eq!(encode_bip34_height(16), vec![0x60]);
    }

    #[test]
    fn rigor_script_is_ten_bytes() {
        let script = build_rigor_commitment_script(0x1d00ffff);
        assert_eq!(script.len(), 10);
        assert_eq!(&script[0..2], &[0x6a, 0x08]);
        assert_eq!(&script[2..6], b"RIGR");
    }
}
