//! Fail-closed production configuration gate (SECURITY-001-E / C-5, C-6, H-5).
//!
//! TASK-POOL-RES-BOUNDARY-IMPLEMENT-003 (Option A): pool no longer configures RES
//! mint funding. RES body mint, when required, is assembled by `rigord`
//! (`auto_res_mint` / `-miningresfeeaddr` / `getautomintfundaddress`).
//!
//! Remaining pool secrets / destinations:
//!
//! | Item | Consequence if misconfigured |
//! |---|---|
//! | coinbase payout script | block rewards burned / unpaid |
//! | Stratum password | anyone can mine as anyone |
//! | block submission sink | found blocks never reached the node |
//!
//! Consensus impact: NONE.

use std::sync::Arc;

use block_assembler::payout_script::payout_script_from_hex;
use stratum_server::auth::HashedCredentialAuthenticator;
use stratum_server::{SessionManager, VardiffConfig};

use crate::PilotError;

pub mod env_keys {
    pub const PAYOUT_SCRIPT_HEX: &str = "RIGOR_POOL_PAYOUT_SCRIPT_HEX";
    pub const WORKER_CREDENTIALS: &str = "RIGOR_POOL_WORKER_CREDENTIALS";
    pub const PROFILE: &str = "RIGOR_POOL_PROFILE";
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum PoolProfile {
    Devtest,
    Production,
}

impl PoolProfile {
    pub fn from_env() -> Self {
        match std::env::var(env_keys::PROFILE).ok().as_deref() {
            Some(v) if v.eq_ignore_ascii_case("devtest") => Self::Devtest,
            _ => Self::Production,
        }
    }
}

pub struct PoolSecurityConfig {
    pub profile: PoolProfile,
    pub payout_script: Vec<u8>,
    pub sessions: Arc<SessionManager>,
}

impl PoolSecurityConfig {
    pub fn production_from_env() -> Result<Self, PilotError> {
        let payout_script = required(env_keys::PAYOUT_SCRIPT_HEX)?;
        let payout_script = payout_script_from_hex(&payout_script).map_err(cfg_err)?;

        let sessions = sessions_from_env()?;
        if !sessions.is_production_safe() {
            return Err(PilotError::Config(
                "H-5a: refusing to start — session manager is using a devtest shared password"
                    .into(),
            ));
        }

        Ok(Self {
            profile: PoolProfile::Production,
            payout_script,
            sessions,
        })
    }

    pub fn devtest() -> Self {
        Self {
            profile: PoolProfile::Devtest,
            payout_script: block_assembler::UNSPENDABLE_DEVTEST_PAYOUT_SCRIPT.to_vec(),
            sessions: Arc::new(SessionManager::new("x")),
        }
    }

    pub fn validate(&self) -> Result<(), PilotError> {
        if self.profile == PoolProfile::Devtest {
            return Err(PilotError::Config(
                "refusing to treat the devtest profile as production".into(),
            ));
        }
        block_assembler::AssemblerConfig::with_payout_script(self.payout_script.clone())
            .and_then(|c| c.validate_production())
            .map_err(cfg_err)?;
        if !self.sessions.is_production_safe() {
            return Err(PilotError::Config(
                "H-5a: devtest authenticator installed".into(),
            ));
        }
        Ok(())
    }
}

fn cfg_err<E: std::fmt::Display>(e: E) -> PilotError {
    PilotError::Config(e.to_string())
}

fn required(key: &str) -> Result<String, PilotError> {
    match std::env::var(key) {
        Ok(v) if !v.trim().is_empty() => Ok(v.trim().to_string()),
        _ => Err(PilotError::Config(format!(
            "refusing to start — required setting {key} is unset; there is no safe default"
        ))),
    }
}

fn sessions_from_env() -> Result<Arc<SessionManager>, PilotError> {
    let raw = required(env_keys::WORKER_CREDENTIALS)?;
    let auth = HashedCredentialAuthenticator::new();
    for entry in raw.split(';').map(str::trim).filter(|e| !e.is_empty()) {
        let (worker, hash) = entry.split_once('=').ok_or_else(|| {
            PilotError::Config(format!(
                "{}: entry {entry:?} must be worker=password_hash",
                env_keys::WORKER_CREDENTIALS
            ))
        })?;
        auth.upsert_hash(worker.trim(), hash.trim()).map_err(cfg_err)?;
    }
    if auth.worker_count() == 0 {
        return Err(PilotError::Config(format!(
            "{} contained no usable worker credentials",
            env_keys::WORKER_CREDENTIALS
        )));
    }
    let vardiff = VardiffConfig::default();
    vardiff.validate().map_err(cfg_err)?;
    Ok(Arc::new(SessionManager::with_authenticator_and_vardiff(
        Arc::new(auth),
        vardiff,
    )))
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn devtest_profile_never_validates_as_production() {
        let cfg = PoolSecurityConfig::devtest();
        assert_eq!(cfg.profile, PoolProfile::Devtest);
        assert!(cfg.validate().is_err());
        assert!(!cfg.sessions.is_production_safe());
    }

    #[test]
    fn burn_payout_script_is_rejected() {
        let burn = hex::encode(block_assembler::UNSPENDABLE_DEVTEST_PAYOUT_SCRIPT);
        assert!(payout_script_from_hex(&burn).is_err());
    }

    #[test]
    fn unknown_profile_values_mean_production() {
        fn classify(v: Option<&str>) -> PoolProfile {
            match v {
                Some(v) if v.eq_ignore_ascii_case("devtest") => PoolProfile::Devtest,
                _ => PoolProfile::Production,
            }
        }
        assert_eq!(classify(Some("devtest")), PoolProfile::Devtest);
        assert_eq!(classify(Some("DevTest")), PoolProfile::Devtest);
        assert_eq!(classify(Some("staging")), PoolProfile::Production);
        assert_eq!(classify(Some("")), PoolProfile::Production);
        assert_eq!(classify(None), PoolProfile::Production);
    }

    #[test]
    fn plaintext_worker_credentials_are_refused() {
        let auth = HashedCredentialAuthenticator::new();
        assert!(auth.upsert_hash("alice", "x").is_err());
        assert!(auth.upsert_hash("alice", "").is_err());
        assert_eq!(auth.worker_count(), 0);

        let good =
            stratum_server::hash_password("a-sufficiently-long-secret", b"0123456789abcdef", 64);
        auth.upsert_hash("alice", &good).unwrap();
        assert_eq!(auth.worker_count(), 1);
    }
}
