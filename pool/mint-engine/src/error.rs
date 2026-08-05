use thiserror::Error;

#[derive(Debug, Error)]
pub enum MintError {
    #[error("build error: {0}")]
    Build(String),

    #[error("state error: {0}")]
    State(String),

    #[error("rpc error: {0}")]
    Rpc(String),

    #[error("validation error: {0}")]
    Validation(String),
}

pub type MintResult<T> = Result<T, MintError>;
