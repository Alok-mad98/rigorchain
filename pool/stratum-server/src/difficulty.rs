/// Stratum difficulty → share target (LE uint256).
///
/// `share_target = diff1_target / difficulty` (Bitcoin-stratum convention).
pub fn share_target_from_difficulty(difficulty: f64) -> [u8; 32] {
    if difficulty <= 0.0 {
        return [0xff; 32];
    }
    let mut target = diff1_target_le();
    divide_target(&mut target, difficulty);
    target
}

/// `share_target = block_target / difficulty` (pool-relative to network target).
pub fn share_target_from_block(block_target: [u8; 32], difficulty: f64) -> [u8; 32] {
    if difficulty <= 0.0 {
        return block_target;
    }
    let mut target = block_target;
    divide_target(&mut target, difficulty);
    target
}

/// Multiply an expanded target by `ease` (>1 → easier PoW). Caps at all-`0xff`.
pub fn ease_target(target: &[u8; 32], ease: f64) -> [u8; 32] {
    if !(ease.is_finite() && ease > 1.0) {
        return *target;
    }
    let mut value = 0u128;
    for i in (16..32).rev() {
        value = (value << 8) | target[i] as u128;
    }
    let scaled_f = value as f64 * ease;
    if !scaled_f.is_finite() || scaled_f >= u128::MAX as f64 {
        return [0xff; 32];
    }
    let scaled = scaled_f as u128;
    let mut out = *target;
    for i in 16..32 {
        let shift = 8 * (i - 16);
        out[i] = ((scaled >> shift) & 0xff) as u8;
    }
    // If low half was zero but high half had bits, preserve a minimal ease via OR of top byte.
    if value == 0 {
        return *target;
    }
    out
}

fn diff1_target_le() -> [u8; 32] {
    // 0x00000000FFFF0000... display → internal LE bytes
    let mut t = [0u8; 32];
    t[26] = 0xff;
    t[27] = 0xff;
    t[28] = 0x00;
    t[29] = 0x00;
    t
}

fn divide_target(target: &mut [u8; 32], difficulty: f64) {
    if !(difficulty.is_finite() && difficulty > 0.0) {
        *target = [0xff; 32];
        return;
    }
    let mut value = 0u128;
    for i in (16..32).rev() {
        value = (value << 8) | target[i] as u128;
    }
    let scaled_f = value as f64 / difficulty;
    // f64 cannot round-trip u128::MAX; overflowing the high 128 bits means
    // "easier than we can represent" → accept essentially every hash.
    if !scaled_f.is_finite() || scaled_f >= u128::MAX as f64 {
        *target = [0xff; 32];
        return;
    }
    let scaled = scaled_f as u128;
    for i in 16..32 {
        let shift = 8 * (i - 16);
        target[i] = ((scaled >> shift) & 0xff) as u8;
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn higher_difficulty_means_lower_target() {
        let easy = share_target_from_difficulty(1.0);
        let hard = share_target_from_difficulty(100.0);
        let easy_gt = (0..32).rev().find(|&i| easy[i] != hard[i]).map(|i| easy[i] > hard[i]);
        assert_eq!(easy_gt, Some(true));
    }

}
