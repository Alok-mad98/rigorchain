//! OpuHash PoW — port of `core/src/pow_opus.cpp` + `test_framework/opus_pow.py`.
//! Buffer: 64 KiB, rounds: 256, ChaCha20 seed = header_hash.

use sha2::{Digest, Sha256};

pub const OPUS_POW_BUFFER_SIZE: usize = 64 * 1024;
pub const OPUS_POW_MIX_ROUNDS: usize = 256;
pub const OPUS_POW_BUFFER_MASK: u32 = (OPUS_POW_BUFFER_SIZE - 1) as u32;

pub fn hash256(data: &[u8]) -> [u8; 32] {
    let first = Sha256::digest(data);
    let second = Sha256::digest(&first);
    let mut out = [0u8; 32];
    out.copy_from_slice(&second);
    out
}

fn ser_uint256_le(value: [u8; 32]) -> [u8; 32] {
    // `uint256` memory order (already LE bytes).
    value
}

fn read_le32(h: &[u8; 32]) -> u32 {
    u32::from_le_bytes([h[0], h[1], h[2], h[3]])
}

fn write_le64(buf: &mut [u8], val: u64) {
    buf.copy_from_slice(&val.to_le_bytes());
}

fn chacha20_keystream(key: &[u8; 32], nbytes: usize) -> Vec<u8> {
    fn rotl32(v: u32, c: u32) -> u32 {
        v.rotate_left(c)
    }

    fn quarter_round(s: &mut [u32; 16], a: usize, b: usize, c: usize, d: usize) {
        s[a] = s[a].wrapping_add(s[b]);
        s[d] = rotl32(s[d] ^ s[a], 16);
        s[c] = s[c].wrapping_add(s[d]);
        s[b] = rotl32(s[b] ^ s[c], 12);
        s[a] = s[a].wrapping_add(s[b]);
        s[d] = rotl32(s[d] ^ s[a], 8);
        s[c] = s[c].wrapping_add(s[d]);
        s[b] = rotl32(s[b] ^ s[c], 7);
    }

    let j4 = u32::from_le_bytes(key[0..4].try_into().unwrap());
    let j5 = u32::from_le_bytes(key[4..8].try_into().unwrap());
    let j6 = u32::from_le_bytes(key[8..12].try_into().unwrap());
    let j7 = u32::from_le_bytes(key[12..16].try_into().unwrap());
    let j8 = u32::from_le_bytes(key[16..20].try_into().unwrap());
    let j9 = u32::from_le_bytes(key[20..24].try_into().unwrap());
    let j10 = u32::from_le_bytes(key[24..28].try_into().unwrap());
    let j11 = u32::from_le_bytes(key[28..32].try_into().unwrap());
    let mut j12 = 0u32;
    let mut j13 = 0u32;
    let j14 = 0u32;
    let j15 = 0u32;

    let mut out = Vec::with_capacity(nbytes);
    while out.len() < nbytes {
        let (mut x0, mut x1, mut x2, mut x3) = (0x6170_7865, 0x3320_646e, 0x7962_2d32, 0x6b20_6574);
        let (mut x4, mut x5, mut x6, mut x7) = (j4, j5, j6, j7);
        let (mut x8, mut x9, mut x10, mut x11) = (j8, j9, j10, j11);
        let (mut x12, mut x13, mut x14, mut x15) = (j12, j13, j14, j15);

        for _ in 0..10 {
            let mut x = [
                x0, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15,
            ];
            quarter_round(&mut x, 0, 4, 8, 12);
            quarter_round(&mut x, 1, 5, 9, 13);
            quarter_round(&mut x, 2, 6, 10, 14);
            quarter_round(&mut x, 3, 7, 11, 15);
            quarter_round(&mut x, 0, 5, 10, 15);
            quarter_round(&mut x, 1, 6, 11, 12);
            quarter_round(&mut x, 2, 7, 8, 13);
            quarter_round(&mut x, 3, 4, 9, 14);
            (
                x0, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15,
            ) = (
                x[0], x[1], x[2], x[3], x[4], x[5], x[6], x[7], x[8], x[9], x[10], x[11],
                x[12], x[13], x[14], x[15],
            );
        }

        x0 = x0.wrapping_add(0x6170_7865);
        x1 = x1.wrapping_add(0x3320_646e);
        x2 = x2.wrapping_add(0x7962_2d32);
        x3 = x3.wrapping_add(0x6b20_6574);
        x4 = x4.wrapping_add(j4);
        x5 = x5.wrapping_add(j5);
        x6 = x6.wrapping_add(j6);
        x7 = x7.wrapping_add(j7);
        x8 = x8.wrapping_add(j8);
        x9 = x9.wrapping_add(j9);
        x10 = x10.wrapping_add(j10);
        x11 = x11.wrapping_add(j11);
        x12 = x12.wrapping_add(j12);
        x13 = x13.wrapping_add(j13);
        x14 = x14.wrapping_add(j14);
        x15 = x15.wrapping_add(j15);

        let words = [
            x0, x1, x2, x3, x4, x5, x6, x7, x8, x9, x10, x11, x12, x13, x14, x15,
        ];
        let mut block = Vec::with_capacity(64);
        for w in words {
            block.extend_from_slice(&w.to_le_bytes());
        }
        let take = (64).min(nbytes - out.len());
        out.extend_from_slice(&block[..take]);

        j12 = j12.wrapping_add(1);
        if j12 == 0 {
            j13 = j13.wrapping_add(1);
        }
    }
    out
}

fn compute_opus_mix(base_hash: &[u8; 32]) -> [u8; 32] {
    let mem = chacha20_keystream(base_hash, OPUS_POW_BUFFER_SIZE);
    let mut current = *base_hash;

    for _round in 0..OPUS_POW_MIX_ROUNDS {
        let val = read_le32(&current);
        let mut idx = (val & OPUS_POW_BUFFER_MASK) as usize;
        if idx > OPUS_POW_BUFFER_SIZE - 8 {
            idx = OPUS_POW_BUFFER_SIZE - 8;
        }
        let mem_val = u64::from_le_bytes(mem[idx..idx + 8].try_into().unwrap());

        let mut ser = Vec::with_capacity(40);
        ser.extend_from_slice(&ser_uint256_le(current));
        let mut le = [0u8; 8];
        write_le64(&mut le, mem_val);
        ser.extend_from_slice(&le);
        current = hash256(&ser);
    }
    current
}

/// `GetOpusHashFromBaseHash` — consensus OpuHash digest from `SHA256d(header)`.
pub fn get_opus_hash_from_base_hash(base_hash: &[u8; 32]) -> [u8; 32] {
    let mixed = compute_opus_mix(base_hash);
    let mut ser = Vec::with_capacity(64);
    ser.extend_from_slice(base_hash);
    ser.extend_from_slice(&mixed);
    hash256(&ser)
}

/// Verify share: `pow_hash <= share_target`.
pub fn verify_opuhash_share(base_hash: &[u8; 32], share_target: &[u8; 32]) -> ([u8; 32], bool) {
    let pow = get_opus_hash_from_base_hash(base_hash);
    (pow, uint256_le_cmp(&pow, share_target) != std::cmp::Ordering::Greater)
}

/// Verify block: `pow_hash <= block_target` (expanded compact `bits`).
pub fn verify_opuhash_block(base_hash: &[u8; 32], block_target: &[u8; 32]) -> ([u8; 32], bool) {
    verify_opuhash_share(base_hash, block_target)
}

/// Compare two LE `uint256` values (Bitcoin `arith_uint256` ordering).
pub fn uint256_le_cmp(a: &[u8; 32], b: &[u8; 32]) -> std::cmp::Ordering {
    for i in (0..32).rev() {
        match a[i].cmp(&b[i]) {
            std::cmp::Ordering::Equal => continue,
            other => return other,
        }
    }
    std::cmp::Ordering::Equal
}

/// Expand compact `bits` (8 hex chars, display order) to 32-byte LE target.
pub fn expand_compact_bits(bits_hex: &str) -> Result<[u8; 32], String> {
    let compact = u32::from_str_radix(bits_hex, 16).map_err(|e| e.to_string())?;
    expand_compact(compact)
}

/// `arith_uint256::SetCompact` — `target = mantissa * 256^(size-3)`.
///
/// SECURITY-001-E N-1: this previously wrote the mantissa at `32 - size` in
/// **big-endian** order while every consumer (`uint256_le_cmp`,
/// `get_opus_hash_from_base_hash`, `difficulty::diff1_target_le`) works in
/// little-endian. The resulting target was numerically tiny, so
/// `verify_opuhash_share` / `verify_opuhash_block` rejected essentially every
/// share and could never flag a real block candidate.
pub fn expand_compact(compact: u32) -> Result<[u8; 32], String> {
    let size = (compact >> 24) as usize;
    if size > 32 {
        return Err("invalid compact size".into());
    }
    let mut target = [0u8; 32];
    let mut word = compact & 0x007f_ffff;
    if size == 0 || word == 0 {
        return Ok(target);
    }
    if size <= 3 {
        word >>= 8 * (3 - size);
        target[..3].copy_from_slice(&word.to_le_bytes()[..3]);
        return Ok(target);
    }
    let shift = size - 3;
    if shift + 3 > 32 {
        return Err("compact target overflows uint256".into());
    }
    target[shift..shift + 3].copy_from_slice(&word.to_le_bytes()[..3]);
    Ok(target)
}

#[cfg(test)]
mod tests {
    use super::*;

    /// Bitcoin `uint256` display hex → internal LE bytes.
    fn display_hex_to_le(s: &str) -> [u8; 32] {
        let mut bytes = hex::decode(s).unwrap();
        assert_eq!(bytes.len(), 32);
        bytes.reverse();
        let mut out = [0u8; 32];
        out.copy_from_slice(&bytes);
        out
    }

    #[test]
    fn reference_vector_base_0x01() {
        let mut base = [0u8; 32];
        base[0] = 1;
        let pow = get_opus_hash_from_base_hash(&base);
        let expect = display_hex_to_le("0cc4ff55b1598a19689b97bd06cdb7c6abe8799bfd5e1554fbe44b606c265780");
        assert_eq!(pow, expect);
    }

    #[test]
    fn reference_vector_base_all_ff() {
        let base = [0xff; 32];
        let pow = get_opus_hash_from_base_hash(&base);
        let expect = display_hex_to_le("4e96acde7248b3483051a3233a261b7e25cbfc41851093d71a2fde437124d4f7");
        assert_eq!(pow, expect);
    }

    #[test]
    fn verify_share_rejects_high_pow() {
        let base = [0u8; 32];
        let target = [0u8; 32];
        let (_, ok) = verify_opuhash_share(&base, &target);
        assert!(!ok);
    }

    /// N-1 regression: expanded compact targets must be little-endian so that
    /// `uint256_le_cmp` against an LE pow hash is meaningful.
    #[test]
    fn expand_compact_is_little_endian() {
        // difficulty-1 target 0x00000000FFFF0000...0000
        let diff1 = expand_compact_bits("1d00ffff").unwrap();
        assert_eq!(
            diff1,
            display_hex_to_le("00000000FFFF0000000000000000000000000000000000000000000000000000")
        );

        // regtest 0x207fffff => 0x7fffff << 232
        let regtest = expand_compact_bits("207fffff").unwrap();
        assert_eq!(
            regtest,
            display_hex_to_le("7FFFFF0000000000000000000000000000000000000000000000000000000000")
        );
    }

    /// N-1 regression: a low-difficulty (regtest) target must be numerically
    /// larger than a high-difficulty (mainnet-like) one.
    #[test]
    fn easier_bits_expand_to_larger_target() {
        let easy = expand_compact_bits("207fffff").unwrap();
        let hard = expand_compact_bits("1d00ffff").unwrap();
        assert_eq!(uint256_le_cmp(&easy, &hard), std::cmp::Ordering::Greater);
    }

    /// N-1 regression: with a regtest target the validator must be able to accept
    /// work; the old big-endian encoding made this impossible.
    #[test]
    fn regtest_target_accepts_some_work() {
        let target = expand_compact_bits("207fffff").unwrap();
        let accepted = (0u8..64).any(|i| {
            let mut base = [0u8; 32];
            base[0] = i;
            verify_opuhash_share(&base, &target).1
        });
        assert!(accepted, "regtest target accepted no work at all");
    }

    #[test]
    fn small_and_degenerate_compact_sizes() {
        assert_eq!(expand_compact(0).unwrap(), [0u8; 32]);
        assert_eq!(expand_compact(0x0300_0000).unwrap(), [0u8; 32]);
        // size = 1 => mantissa >> 16
        let mut expect = [0u8; 32];
        expect[0] = 0x12;
        assert_eq!(expand_compact(0x0112_3456).unwrap(), expect);
        assert!(expand_compact(0x2100_ffff).is_err());
    }
}
