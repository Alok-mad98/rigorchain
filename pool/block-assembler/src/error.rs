use thiserror::Error;

#[derive(Debug, Error)]
pub enum AssemblerError {
    #[error("assembly error: {0}")]
    Assembly(String),

    #[error("publish error: {0}")]
    Publish(String),

    #[error("cache error: {0}")]
    Cache(String),

    #[error("template not ready: {0}")]
    TemplateNotReady(String),

    #[error("hex error: {0}")]
    Hex(#[from] hex::FromHexError),
}

pub type AssemblerResult<T> = Result<T, AssemblerError>;
