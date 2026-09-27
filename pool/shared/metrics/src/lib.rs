//! Shared JSONL metrics stream for pool / miner operational telemetry.
//!
//! Schema authority: `docs/ops/MONITORING_METRICS_SPEC.md`

mod schema;
mod timestamp;
mod writer;

pub use schema::{file_in_metrics_dir, metrics_dir, metrics_file, schema_version, SCHEMA_VERSION};
pub use timestamp::{now_record_timestamps, RecordTimestamps};
pub use writer::{JsonlMetricsWriter, MetricsWriterError};
