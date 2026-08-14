use crate::job::JobSnapshot;
use crate::metrics::Metrics;

use super::header::build_header_bytes;
use super::target::share_target_for_bits;
use super::traits::{NonceScanner, OpuHashWorker, ShareCandidate, ShareProducer};
use super::worker::CpuOpuHashWorker;
use share_validator::expand_compact_bits;

/// Scan configuration for one worker thread.
#[derive(Clone, Debug)]
pub struct ScanConfig {
    pub thread_id: u32,
    pub thread_count: u32,
    pub extranonce1: Vec<u8>,
    pub extranonce2_size: usize,
    pub batch_size: u32,
}

impl ScanConfig {
    pub fn extranonce2_for_thread(&self) -> Vec<u8> {
        let mut en2 = vec![0u8; self.extranonce2_size];
        if !en2.is_empty() {
            en2[self.extranonce2_size - 1] = self.thread_id as u8;
        }
        en2
    }
}

/// CPU nonce scanner — strides nonce space per thread (`nonce % threads == thread_id`).
pub struct CpuNonceScanner {
    worker: CpuOpuHashWorker,
    pub(crate) next_nonce: u32,
    config: ScanConfig,
}

impl CpuNonceScanner {
    pub fn new(config: ScanConfig) -> Self {
        let start = config.thread_id;
        Self {
            worker: CpuOpuHashWorker,
            next_nonce: start,
            config,
        }
    }

    pub fn scan_batch(
        &mut self,
        job: &JobSnapshot,
        metrics: &Metrics,
    ) -> Option<ShareCandidate> {
        let share_target = share_target_for_job(job)?;
        let ntime = u32::from_str_radix(&job.notify.ntime, 16).unwrap_or(0);
        let extranonce2 = self.config.extranonce2_for_thread();
        let job_id = job.job_id().to_string();

        for _ in 0..self.config.batch_size {
            let nonce = self.next_nonce;
            self.next_nonce = self.next_nonce.wrapping_add(self.config.thread_count);

            let header = match build_header_bytes(
                &job.notify,
                &self.config.extranonce1,
                &extranonce2,
                ntime,
                nonce,
            ) {
                Ok(h) => h,
                Err(_) => continue,
            };

            metrics.record_hash();
            let (_, ok) = self.worker.evaluate(&header, &share_target);
            if ok {
                metrics.record_share_found();
                return Some(ShareCandidate {
                    job_id,
                    extranonce2,
                    ntime,
                    nonce,
                });
            }
        }
        None
    }
}

impl ShareProducer for CpuNonceScanner {
    fn next_share(&mut self, job: &JobSnapshot) -> Option<ShareCandidate> {
        self.scan_batch(job, &Metrics::new())
    }
}

impl NonceScanner for CpuNonceScanner {
    fn scan_once(&mut self, job: &JobSnapshot, worker: &dyn OpuHashWorker) -> Option<ShareCandidate> {
        let _ = worker;
        self.scan_batch(job, &Metrics::new())
    }
}

pub fn share_target_for_job(job: &JobSnapshot) -> Option<[u8; 32]> {
    let block_target = expand_compact_bits(&job.notify.nbits).ok()?;
    Some(share_target_for_bits(block_target, job.difficulty))
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::job::JobSnapshot;
    use std::time::Instant;

    fn easy_job() -> JobSnapshot {
        JobSnapshot {
            notify: crate::stratum::messages::MiningNotify {
                job_id: "easy".into(),
                prevhash: "00".repeat(32),
                coinb1: "01".into(),
                coinb2: "02".into(),
                merkle_branch: vec![],
                version: "20000000".into(),
                nbits: "207fffff".into(),
                ntime: "65000000".into(),
                clean_jobs: false,
            },
            convergence: Default::default(),
            difficulty: 1e-5,
            received_at: Instant::now(),
            stale: false,
        }
    }

    #[test]
    fn batch_advances_nonce_stride() {
        let config = ScanConfig {
            thread_id: 0,
            thread_count: 4,
            extranonce1: vec![0],
            extranonce2_size: 4,
            batch_size: 2,
        };
        let mut scanner = CpuNonceScanner::new(config);
        let before = scanner.next_nonce;
        scanner.scan_batch(&easy_job(), &Metrics::new());
        assert_eq!(scanner.next_nonce, before + 8);
    }
}
