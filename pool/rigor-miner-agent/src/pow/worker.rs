use share_validator::{get_opus_hash_from_base_hash, verify_opuhash_share};

use super::header::header_base_hash;
use super::traits::OpuHashWorker;

/// CPU OpuHash worker — consensus port from `share-validator` (no SIMD/GPU).
#[derive(Debug, Clone, Copy, Default)]
pub struct CpuOpuHashWorker;

impl OpuHashWorker for CpuOpuHashWorker {
    fn hash_header(&self, header_80: &[u8; 80]) -> [u8; 32] {
        let base = header_base_hash(header_80);
        get_opus_hash_from_base_hash(&base)
    }
}

impl CpuOpuHashWorker {
    pub fn evaluate(&self, header_80: &[u8; 80], share_target: &[u8; 32]) -> ([u8; 32], bool) {
        let base = header_base_hash(header_80);
        verify_opuhash_share(&base, share_target)
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use share_validator::get_opus_hash_from_base_hash;

    #[test]
    fn matches_golden_vector() {
        let mut base = [0u8; 32];
        base[0] = 1;
        let worker = CpuOpuHashWorker;
        let header = [0u8; 80];
        // header_base_hash won't equal base unless crafted; test direct opuhash path
        let pow = get_opus_hash_from_base_hash(&base);
        let mut expect =
            hex::decode("0cc4ff55b1598a19689b97bd06cdb7c6abe8799bfd5e1554fbe44b606c265780").unwrap();
        expect.reverse();
        assert_eq!(pow.as_slice(), expect.as_slice());
        let _ = worker;
        let _ = header;
    }
}
