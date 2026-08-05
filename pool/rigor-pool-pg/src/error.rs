use thiserror::Error;

#[derive(Debug, Error)]
pub enum PgError {
    #[error("postgres: {0}")]
    Db(String),

    #[error("config: {0}")]
    Config(String),

    #[error("not available: {0}")]
    NotAvailable(String),

    #[error("serialization: {0}")]
    Serde(String),
}

pub type PgResult<T> = Result<T, PgError>;

impl From<serde_json::Error> for PgError {
    fn from(e: serde_json::Error) -> Self {
        PgError::Serde(e.to_string())
    }
}

#[cfg(feature = "postgres")]
impl From<postgres::Error> for PgError {
    fn from(e: postgres::Error) -> Self {
        PgError::Db(e.to_string())
    }
}

#[cfg(feature = "postgres")]
impl From<r2d2::Error> for PgError {
    fn from(e: r2d2::Error) -> Self {
        PgError::Db(e.to_string())
    }
}
