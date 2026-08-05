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
    let mut value = 0u128;
    for i in (16..32).rev() {
        value = (value << 8) | target[i] as u128;
    }
    let scaled = (value as f64 / difficulty).min(u128::MAX as f64) as u128;
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
