pub mod header;
pub mod r#loop;
pub mod scanner;
pub mod target;
pub mod traits;
pub mod worker;

pub use header::{build_header_bytes, header_base_hash};
pub use r#loop::{MiningController, MiningWork};
pub use scanner::{share_target_for_job, CpuNonceScanner, ScanConfig};
pub use target::{share_target_for_bits, share_target_from_block, share_target_from_difficulty};
pub use traits::{
    NonceScanner, OpuHashWorker, ShareCandidate, ShareProducer, StubOpuHashWorker,
    StubShareProducer,
};
pub use worker::CpuOpuHashWorker;
