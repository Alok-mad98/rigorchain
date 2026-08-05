pub mod agent;
pub mod error;
pub mod job;
pub mod metrics;
pub mod pow;
pub mod stratum;
pub mod telemetry;

pub use agent::{AgentConfig, MinerAgent};
pub use error::{MinerError, MinerResult};
pub use job::{ConvergenceMetadata, JobManager, JobSnapshot, JobStatistics};
pub use metrics::{Metrics, MetricsSnapshot};
pub use pow::{
    build_header_bytes, header_base_hash, share_target_for_job, CpuNonceScanner,
    CpuOpuHashWorker, MiningController, MiningWork, NonceScanner, OpuHashWorker, ScanConfig,
    ShareCandidate, ShareProducer, StubOpuHashWorker, StubShareProducer,
};
pub use stratum::{
    MiningNotify, MiningNotifyRigor, PoolEndpoint, StratumClient, StratumEvent, DEFAULT_AGENT,
};
pub use stratum::protocol::parse_line;
pub use telemetry::{MinerShareTelemetry, MinerTelemetry, ShareResultEvent};
