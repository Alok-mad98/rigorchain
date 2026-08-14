/// Absolute Stratum target: `diff1_target / difficulty` (Bitcoin / mainnet convention).
pub fn share_target_from_difficulty(difficulty: f64) -> [u8; 32] {
    if difficulty <= 0.0 {
        return [0xff; 32];
    }
    let mut target = diff1_target_le();
    divide_target(&mut target, difficulty);
    target
}

/// Legacy helper: `block_target / difficulty` (not used for Alpha share scanning).
pub fn share_target_from_block(block_target: [u8; 32], difficulty: f64) -> [u8; 32] {
    if difficulty <= 0.0 {
        return block_target;
    }
    let mut target = block_target;
    divide_target(&mut target, difficulty);
    target
}

/// Pool-compatible share target for a job — absolute Stratum difficulty only.
///
/// Matches `SessionManager::share_target_for_session` / mainnet pool model.
/// The block compact target is unused for share PoW (kept for API stability).
pub fn share_target_for_bits(_block_target: [u8; 32], difficulty: f64) -> [u8; 32] {
    share_target_from_difficulty(difficulty)
}

fn diff1_target_le() -> [u8; 32] {
    let mut t = [0u8; 32];
    t[26] = 0xff;
    t[27] = 0xff;
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
    fn higher_absolute_difficulty_hardens_target() {
        let easy = share_target_from_difficulty(1e-5);
        let hard = share_target_from_difficulty(1024.0);
        let easy_gt = (0..32)
            .rev()
            .find(|&i| easy[i] != hard[i])
            .map(|i| easy[i] > hard[i]);
        assert_eq!(easy_gt, Some(true));
    }

    #[test]
    fn share_target_for_bits_ignores_easy_regtest_block() {
        let block = [0xff; 32];
        let share = share_target_for_bits(block, 1e-5);
        // Must not collapse to the all-ff block target.
        assert_ne!(share, block);
    }
}
