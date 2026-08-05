//! Worker authentication (SECURITY-001-E / H-5a).
//!
//! The pool previously accepted the single hardcoded password `"x"` for every
//! worker, auto-registering any name that presented it, and never read or wrote
//! the `workers.password_hash` column that the schema already provides. Anyone who
//! could reach the Stratum port could mine as — and be credited as — any worker.
//!
//! This module provides:
//! - a stretched, salted password hash with constant-time verification,
//! - a credential store abstraction backed by `workers.password_hash`,
//! - a devtest shared-password authenticator that self-identifies as unsafe so
//!   startup gates can refuse to boot with it.
//!
//! Consensus impact: NONE — Stratum-level access control only. No block, header,
//! transaction or script bytes are affected.

use std::collections::HashMap;
use std::sync::RwLock;

use sha2::{Digest, Sha256};

/// Hash algorithm tag stored in `workers.password_hash`.
pub const HASH_SCHEME: &str = "rigor-sha256i";

/// Default stretching rounds. Iterated SHA-256 is not memory-hard; it is chosen
/// here because it needs no new dependency. See the task report for the argon2id
/// follow-up recommendation.
pub const DEFAULT_ROUNDS: u32 = 100_000;

/// Minimum accepted worker password length.
pub const MIN_PASSWORD_LEN: usize = 12;

/// Passwords that must never be accepted, regardless of length rules.
const FORBIDDEN_PASSWORDS: &[&str] = &["x", "password", "changeme", "rigor", "pool", "miner"];

#[derive(Debug, PartialEq, Eq)]
pub enum AuthError {
    UnknownWorker,
    BadPassword,
    WeakPassword(String),
    MalformedHash(String),
    Unavailable(String),
}

impl std::fmt::Display for AuthError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::UnknownWorker => write!(f, "unknown worker"),
            Self::BadPassword => write!(f, "bad password"),
            Self::WeakPassword(m) => write!(f, "weak password: {m}"),
            Self::MalformedHash(m) => write!(f, "malformed password hash: {m}"),
            Self::Unavailable(m) => write!(f, "credential store unavailable: {m}"),
        }
    }
}

impl std::error::Error for AuthError {}

/// Reject passwords that provide no real protection (H-5a).
pub fn validate_password_strength(password: &str) -> Result<(), AuthError> {
    if FORBIDDEN_PASSWORDS
        .iter()
        .any(|p| p.eq_ignore_ascii_case(password))
    {
        return Err(AuthError::WeakPassword(
            "password is on the forbidden-defaults list".into(),
        ));
    }
    if password.len() < MIN_PASSWORD_LEN {
        return Err(AuthError::WeakPassword(format!(
            "password shorter than {MIN_PASSWORD_LEN} characters"
        )));
    }
    Ok(())
}

/// Produce a `rigor-sha256i$<rounds>$<salt_hex>$<digest_hex>` record.
pub fn hash_password(password: &str, salt: &[u8], rounds: u32) -> String {
    let digest = stretch(password, salt, rounds);
    format!(
        "{HASH_SCHEME}${rounds}${}${}",
        hex::encode(salt),
        hex::encode(digest)
    )
}

/// Verify a password against a stored hash record in constant time.
pub fn verify_password(password: &str, stored: &str) -> Result<(), AuthError> {
    let mut parts = stored.split('$');
    let scheme = parts.next().unwrap_or("");
    if scheme != HASH_SCHEME {
        return Err(AuthError::MalformedHash(format!(
            "unsupported scheme {scheme:?}"
        )));
    }
    let rounds: u32 = parts
        .next()
        .ok_or_else(|| AuthError::MalformedHash("missing rounds".into()))?
        .parse()
        .map_err(|_| AuthError::MalformedHash("rounds not an integer".into()))?;
    if rounds == 0 {
        return Err(AuthError::MalformedHash("rounds must be > 0".into()));
    }
    let salt = hex::decode(
        parts
            .next()
            .ok_or_else(|| AuthError::MalformedHash("missing salt".into()))?,
    )
    .map_err(|e| AuthError::MalformedHash(e.to_string()))?;
    let expected = hex::decode(
        parts
            .next()
            .ok_or_else(|| AuthError::MalformedHash("missing digest".into()))?,
    )
    .map_err(|e| AuthError::MalformedHash(e.to_string()))?;
    if parts.next().is_some() {
        return Err(AuthError::MalformedHash("trailing fields".into()));
    }
    if expected.len() != 32 {
        return Err(AuthError::MalformedHash("digest must be 32 bytes".into()));
    }

    let actual = stretch(password, &salt, rounds);
    if constant_time_eq(&actual, &expected) {
        Ok(())
    } else {
        Err(AuthError::BadPassword)
    }
}

fn stretch(password: &str, salt: &[u8], rounds: u32) -> [u8; 32] {
    let mut h = Sha256::new();
    h.update(salt);
    h.update([0u8]);
    h.update(password.as_bytes());
    let mut acc: [u8; 32] = h.finalize().into();
    for _ in 0..rounds {
        let mut h = Sha256::new();
        h.update(acc);
        h.update(salt);
        h.update(password.as_bytes());
        acc = h.finalize().into();
    }
    acc
}

fn constant_time_eq(a: &[u8], b: &[u8]) -> bool {
    if a.len() != b.len() {
        return false;
    }
    let mut diff = 0u8;
    for (x, y) in a.iter().zip(b.iter()) {
        diff |= x ^ y;
    }
    diff == 0
}

/// Authenticates a Stratum worker. Implementations must not auto-register.
pub trait WorkerAuthenticator: Send + Sync {
    fn authenticate(&self, worker: &str, password: &str) -> Result<(), AuthError>;

    /// True when this authenticator is only fit for devtest. Startup gates refuse
    /// to boot a value-bearing pool while this returns true.
    fn is_devtest(&self) -> bool {
        false
    }

    fn describe(&self) -> String;
}

/// Production authenticator over pre-registered `workers.password_hash` records.
pub struct HashedCredentialAuthenticator {
    hashes: RwLock<HashMap<String, String>>,
}

impl HashedCredentialAuthenticator {
    pub fn new() -> Self {
        Self {
            hashes: RwLock::new(HashMap::new()),
        }
    }

    /// Load `worker -> password_hash` rows (e.g. from the `workers` table).
    pub fn from_hashes<I: IntoIterator<Item = (String, String)>>(
        rows: I,
    ) -> Result<Self, AuthError> {
        let store = Self::new();
        for (worker, hash) in rows {
            store.upsert_hash(&worker, &hash)?;
        }
        Ok(store)
    }

    /// Register a worker from a plaintext password, enforcing strength rules and
    /// storing only the stretched hash.
    pub fn register(&self, worker: &str, password: &str, salt: &[u8]) -> Result<String, AuthError> {
        validate_password_strength(password)?;
        if salt.len() < 16 {
            return Err(AuthError::WeakPassword("salt shorter than 16 bytes".into()));
        }
        let hash = hash_password(password, salt, DEFAULT_ROUNDS);
        self.upsert_hash(worker, &hash)?;
        Ok(hash)
    }

    /// Install an already-hashed credential, rejecting records we cannot verify.
    pub fn upsert_hash(&self, worker: &str, password_hash: &str) -> Result<(), AuthError> {
        if worker.trim().is_empty() {
            return Err(AuthError::MalformedHash("empty worker name".into()));
        }
        if password_hash.trim().is_empty() {
            return Err(AuthError::MalformedHash(format!(
                "worker {worker} has an empty password_hash; refusing to accept a \
                 credential-less worker"
            )));
        }
        // Reject anything we could not later verify (catches plaintext columns).
        if let Err(AuthError::MalformedHash(m)) = verify_password("\u{0}probe", password_hash) {
            return Err(AuthError::MalformedHash(format!("worker {worker}: {m}")));
        }
        self.hashes
            .write()
            .map_err(|_| AuthError::Unavailable("credential lock poisoned".into()))?
            .insert(worker.to_string(), password_hash.to_string());
        Ok(())
    }

    pub fn worker_count(&self) -> usize {
        self.hashes.read().map(|h| h.len()).unwrap_or(0)
    }
}

impl Default for HashedCredentialAuthenticator {
    fn default() -> Self {
        Self::new()
    }
}

impl WorkerAuthenticator for HashedCredentialAuthenticator {
    fn authenticate(&self, worker: &str, password: &str) -> Result<(), AuthError> {
        let stored = {
            let guard = self
                .hashes
                .read()
                .map_err(|_| AuthError::Unavailable("credential lock poisoned".into()))?;
            guard.get(worker).cloned()
        };
        // No auto-registration: an unknown worker is a failure, not a signup.
        let stored = stored.ok_or(AuthError::UnknownWorker)?;
        verify_password(password, &stored)
    }

    fn describe(&self) -> String {
        format!("hashed-credentials(workers={})", self.worker_count())
    }
}

/// **Devtest only.** Accepts any worker name presenting one shared password.
pub struct SharedPasswordAuthenticator {
    password: String,
}

impl SharedPasswordAuthenticator {
    pub fn new(password: impl Into<String>) -> Self {
        Self {
            password: password.into(),
        }
    }
}

impl WorkerAuthenticator for SharedPasswordAuthenticator {
    fn authenticate(&self, _worker: &str, password: &str) -> Result<(), AuthError> {
        if constant_time_eq(password.as_bytes(), self.password.as_bytes()) {
            Ok(())
        } else {
            Err(AuthError::BadPassword)
        }
    }

    fn is_devtest(&self) -> bool {
        true
    }

    fn describe(&self) -> String {
        "shared-password(DEVTEST)".into()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    const SALT: &[u8] = b"0123456789abcdef";

    #[test]
    fn hash_roundtrip_verifies() {
        let hash = hash_password("correct horse battery", SALT, 64);
        verify_password("correct horse battery", &hash).unwrap();
        assert_eq!(
            verify_password("wrong horse battery", &hash).unwrap_err(),
            AuthError::BadPassword
        );
    }

    #[test]
    fn hash_record_is_salted() {
        let a = hash_password("correct horse battery", b"aaaaaaaaaaaaaaaa", 32);
        let b = hash_password("correct horse battery", b"bbbbbbbbbbbbbbbb", 32);
        assert_ne!(a, b);
    }

    #[test]
    fn malformed_hashes_are_rejected() {
        for bad in [
            "",
            "plaintext",
            "rigor-sha256i$0$aa$bb",
            "rigor-sha256i$10$zz$bb",
            "bcrypt$10$aa$bb",
            &format!("rigor-sha256i$10${}${}", hex::encode(SALT), hex::encode([0u8; 8])),
        ] {
            assert!(
                matches!(
                    verify_password("pw", bad),
                    Err(AuthError::MalformedHash(_)) | Err(AuthError::BadPassword)
                ),
                "accepted {bad:?}"
            );
        }
    }

    /// H-5a regression: the historical hardcoded password `"x"` must be refused.
    #[test]
    fn forbidden_and_short_passwords_are_refused() {
        assert!(matches!(
            validate_password_strength("x"),
            Err(AuthError::WeakPassword(_))
        ));
        assert!(matches!(
            validate_password_strength("short"),
            Err(AuthError::WeakPassword(_))
        ));
        validate_password_strength("a-sufficiently-long-secret").unwrap();
    }

    /// H-5a regression: unknown workers must not be auto-registered.
    #[test]
    fn unknown_worker_is_not_auto_registered() {
        let auth = HashedCredentialAuthenticator::new();
        auth.register("alice.worker1", "a-sufficiently-long-secret", SALT)
            .unwrap();

        auth.authenticate("alice.worker1", "a-sufficiently-long-secret")
            .unwrap();
        assert_eq!(
            auth.authenticate("mallory.worker9", "a-sufficiently-long-secret")
                .unwrap_err(),
            AuthError::UnknownWorker
        );
        assert_eq!(auth.worker_count(), 1);
    }

    #[test]
    fn empty_or_plaintext_password_hash_column_is_rejected() {
        let auth = HashedCredentialAuthenticator::new();
        assert!(auth.upsert_hash("bob", "").is_err());
        assert!(auth.upsert_hash("bob", "x").is_err());
        assert!(auth.register("bob", "x", SALT).is_err());
        assert_eq!(auth.worker_count(), 0);
    }

    #[test]
    fn shared_password_authenticator_self_identifies_as_devtest() {
        let auth = SharedPasswordAuthenticator::new("x");
        assert!(auth.is_devtest());
        auth.authenticate("anyone", "x").unwrap();
        assert!(auth.authenticate("anyone", "y").is_err());
        assert!(!HashedCredentialAuthenticator::new().is_devtest());
    }
}
