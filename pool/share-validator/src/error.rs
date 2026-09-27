use thiserror::Error;

/// Explicit failure modes ( §9).
#[derive(Clone, Copy, Debug, PartialEq, Eq, Hash, serde::Serialize, serde::Deserialize)]
#[serde(rename_all = "SCREAMING_SNAKE_CASE")]
pub enum ValidationErrorCode {
    ErrInvalidOpouhash,
    ErrConvergenceMismatch,
    ErrReplayShare,
    ErrStaleJob,
    ErrResMintInvalid,
    ErrTemplateMismatch,
    ErrJobNotFound,
    ErrMalformedSubmission,
}

#[derive(Debug, Error)]
pub enum ValidatorError {
    #[error("{code:?}: {message}")]
    Validation {
        code: ValidationErrorCode,
        message: String,
    },

    #[error("internal error: {0}")]
    Internal(String),
}

pub type ValidatorResult<T> = Result<T, ValidatorError>;

impl ValidatorError {
    pub fn validation(code: ValidationErrorCode, message: impl Into<String>) -> Self {
        Self::Validation {
            code,
            message: message.into(),
        }
    }
}
