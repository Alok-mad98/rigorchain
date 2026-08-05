use thiserror::Error;

#[derive(Debug, Error)]
pub enum MinerError {
    #[error("io: {0}")]
    Io(#[from] std::io::Error),

    #[error("json: {0}")]
    Json(#[from] serde_json::Error),

    #[error("stratum: {0}")]
    Stratum(String),

    #[error("job: {0}")]
    Job(String),

    #[error("config: {0}")]
    Config(String),
}

pub type MinerResult<T> = Result<T, MinerError>;
