/// Share target from block compact `bits` and pool difficulty (stratum-server convention).
pub fn share_target_from_block(block_target: [u8; 32], difficulty: f64) -> [u8; 32] {
    if difficulty <= 0.0 {
        return block_target;
    }
    let mut target = block_target;
    divide_target(&mut target, difficulty);
    target
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
    fn easier_difficulty_raises_target_bytes() {
        // diff1-style target: significant magnitude in bytes 16..32 (divide_target scope).
        let mut block = [0u8; 32];
        block[26] = 0xff;
        block[27] = 0xff;
        let easy = share_target_from_block(block, 1.0);
        let hard = share_target_from_block(block, 100.0);
        let easy_gt = (0..32)
            .rev()
            .find(|&i| easy[i] != hard[i])
            .map(|i| easy[i] > hard[i]);
        assert_eq!(easy_gt, Some(true));
    }
}
