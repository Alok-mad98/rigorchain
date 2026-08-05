use thiserror::Error;

#[derive(Debug, Error)]
pub enum GbtError {
    #[error("RPC error: {0}")]
    Rpc(String),

    #[error("HTTP error: {0}")]
    Http(#[from] reqwest::Error),

    #[error("JSON error: {0}")]
    Json(#[from] serde_json::Error),

    #[error("validation failed ({rule}): {message}")]
    Validation { rule: &'static str, message: String },

    #[error("template rejected: {0}")]
    Rejected(String),

    #[error("cache error: {0}")]
    Cache(String),

    #[error("hex decode error: {0}")]
    Hex(#[from] hex::FromHexError),
}

pub type GbtResult<T> = Result<T, GbtError>;
