use thiserror::Error;

#[derive(Debug, Error)]
pub enum WinnerError {
    #[error("validation: {0}")]
    Validation(String),

    #[error("authorization forbidden: {0}")]
    Forbidden(String),

    #[error("hex decode: {0}")]
    Hex(#[from] hex::FromHexError),

    #[error("gbt: {0}")]
    Gbt(#[from] rigor_gbt::GbtError),

    #[error("persist: {0}")]
    Persist(String),
}

pub type WinnerResult<T> = Result<T, WinnerError>;
