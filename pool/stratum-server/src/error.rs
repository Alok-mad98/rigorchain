use thiserror::Error;

#[derive(Debug, Error)]
pub enum StratumError {
    #[error("protocol error: {0}")]
    Protocol(String),

    #[error("unauthorized: {0}")]
    Unauthorized(String),

    #[error("job error: {0}")]
    Job(String),

    #[error("validation error: {0}")]
    Validation(String),

    #[error("internal error: {0}")]
    Internal(String),
}

pub type StratumResult<T> = Result<T, StratumError>;
