use std::path::PathBuf;

pub const SCHEMA_VERSION: &str = "1.0.0";

pub fn schema_version() -> &'static str {
    SCHEMA_VERSION
}

/// Runtime metrics root — `RIGOR_POOL_METRICS_DIR` or `.runtime/pool-metrics/`.
pub fn metrics_dir() -> PathBuf {
    std::env::var("RIGOR_POOL_METRICS_DIR")
        .map(PathBuf::from)
        .unwrap_or_else(|_| PathBuf::from(".runtime/pool-metrics"))
}

pub fn metrics_file(name: &str) -> PathBuf {
    metrics_dir().join(name)
}

pub fn ensure_metrics_dir() -> std::io::Result<PathBuf> {
    let dir = metrics_dir();
    std::fs::create_dir_all(&dir)?;
    Ok(dir)
}

pub fn file_in_metrics_dir(filename: &str) -> std::io::Result<PathBuf> {
    ensure_metrics_dir()?;
    Ok(metrics_file(filename))
}
