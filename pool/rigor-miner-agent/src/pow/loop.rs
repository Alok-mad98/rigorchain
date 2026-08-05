use std::sync::atomic::{AtomicBool, Ordering};
use std::sync::{Arc, RwLock};
use std::thread::{self, JoinHandle};
use std::time::Duration;

use crate::job::JobSnapshot;
use crate::metrics::Metrics;

use super::scanner::{CpuNonceScanner, ScanConfig};
use super::traits::ShareCandidate;

/// Shared work unit for all mining threads.
#[derive(Clone, Debug)]
pub struct MiningWork {
    pub job: JobSnapshot,
    pub extranonce1: Vec<u8>,
    pub extranonce2_size: usize,
}

/// CPU mining controller — spawns `thread_count` scanners with partitioned nonce space.
pub struct MiningController {
    stop: Arc<AtomicBool>,
    work: Arc<RwLock<Option<MiningWork>>>,
    handles: Vec<JoinHandle<()>>,
}

impl MiningController {
    pub fn start(
        thread_count: u32,
        extranonce1: Vec<u8>,
        extranonce2_size: usize,
        metrics: Arc<Metrics>,
        share_tx: std::sync::mpsc::Sender<ShareCandidate>,
    ) -> Self {
        let stop = Arc::new(AtomicBool::new(false));
        let work = Arc::new(RwLock::new(None::<MiningWork>));
        let mut handles = Vec::with_capacity(thread_count as usize);

        for thread_id in 0..thread_count {
            let stop_t = stop.clone();
            let work_t = work.clone();
            let metrics_t = metrics.clone();
            let tx = share_tx.clone();
            let en1 = extranonce1.clone();
            let config = ScanConfig {
                thread_id,
                thread_count,
                extranonce1: en1,
                extranonce2_size,
                batch_size: 256,
            };

            handles.push(thread::spawn(move || {
                let mut scanner = CpuNonceScanner::new(config);
                while !stop_t.load(Ordering::Relaxed) {
                    let snapshot = work_t.read().ok().and_then(|g| g.clone());
                    match snapshot {
                        Some(w) if !w.job.stale => {
                            if let Some(share) = scanner.scan_batch(&w.job, &metrics_t) {
                                let _ = tx.send(share);
                            }
                        }
                        _ => thread::sleep(Duration::from_millis(50)),
                    }
                }
            }));
        }

        Self {
            stop,
            work,
            handles,
        }
    }

    pub fn set_work(&self, work: MiningWork) {
        if let Ok(mut guard) = self.work.write() {
            *guard = Some(work);
        }
    }

    pub fn clear_work(&self) {
        if let Ok(mut guard) = self.work.write() {
            *guard = None;
        }
    }
}

impl Drop for MiningController {
    fn drop(&mut self) {
        self.stop.store(true, Ordering::Relaxed);
        for h in self.handles.drain(..) {
            let _ = h.join();
        }
    }
}
