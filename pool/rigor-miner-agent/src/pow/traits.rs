use crate::job::JobSnapshot;

/// OpuHash PoW worker — implementation provided by future crate / cpuminer FFI.
pub trait OpuHashWorker {
    fn hash_header(&self, header_80: &[u8; 80]) -> [u8; 32];
}

/// Candidate share produced by scanning nonces.
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct ShareCandidate {
    pub job_id: String,
    pub extranonce2: Vec<u8>,
    pub ntime: u32,
    pub nonce: u32,
}

impl ShareCandidate {
    pub fn extranonce2_hex(&self) -> String {
        hex::encode(&self.extranonce2)
    }

    pub fn ntime_hex(&self) -> String {
        format!("{:08x}", self.ntime)
    }

    pub fn nonce_hex(&self) -> String {
        format!("{:08x}", self.nonce)
    }
}

/// Produces share candidates for submission (scaffold — no optimization).
pub trait ShareProducer {
    fn next_share(&mut self, job: &JobSnapshot) -> Option<ShareCandidate>;
}

/// Scans nonce space using an OpuHash worker (CPU — no SIMD/GPU).
pub trait NonceScanner {
    fn scan_once(&mut self, job: &JobSnapshot, worker: &dyn OpuHashWorker) -> Option<ShareCandidate>;
}

/// No-op worker for tests and CLI scaffold.
pub struct StubOpuHashWorker;

impl OpuHashWorker for StubOpuHashWorker {
    fn hash_header(&self, header_80: &[u8; 80]) -> [u8; 32] {
        let mut out = [0u8; 32];
        out.copy_from_slice(&header_80[..32.min(32)]);
        out
    }
}

/// Stub producer returning a fixed candidate once per job.
pub struct StubShareProducer {
    emitted: std::collections::HashSet<String>,
}

impl StubShareProducer {
    pub fn new() -> Self {
        Self {
            emitted: std::collections::HashSet::new(),
        }
    }
}

impl Default for StubShareProducer {
    fn default() -> Self {
        Self::new()
    }
}

impl ShareProducer for StubShareProducer {
    fn next_share(&mut self, job: &JobSnapshot) -> Option<ShareCandidate> {
        if self.emitted.insert(job.job_id().to_string()) {
            Some(ShareCandidate {
                job_id: job.job_id().to_string(),
                extranonce2: vec![0, 0, 0, 1],
                ntime: u32::from_str_radix(&job.notify.ntime, 16).unwrap_or(0),
                nonce: 1,
            })
        } else {
            None
        }
    }
}
