use share_validator::{rgrpow_hash80, verify_rgrpow_share};

use super::traits::OpuHashWorker;

/// CPU SHA3-256 RGRPOW01 worker (historical type name kept for call sites).
#[derive(Debug, Clone, Copy, Default)]
pub struct CpuOpuHashWorker;

impl OpuHashWorker for CpuOpuHashWorker {
    fn hash_header(&self, header_80: &[u8; 80]) -> [u8; 32] {
        rgrpow_hash80(header_80)
    }
}

impl CpuOpuHashWorker {
    pub fn evaluate(&self, header_80: &[u8; 80], share_target: &[u8; 32]) -> ([u8; 32], bool) {
        verify_rgrpow_share(header_80, share_target)
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn matches_node_zero_header_vector() {
        let worker = CpuOpuHashWorker;
        let pow = worker.hash_header(&[0u8; 80]);
        let expect = hex::decode("dd057fc1df8f5b9466e834cc9d7b819e2d511c75b43675b577f6d9f5d6de04fc")
            .unwrap();
        assert_eq!(pow.as_slice(), expect.as_slice());
    }
}
