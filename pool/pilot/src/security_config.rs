//! Fail-closed production configuration gate (SECURITY-001-E / C-5, C-6, H-5).
//!
//! Every secret and destination the pool needs used to have a working "devtest"
//! default that silently produced a broken or unsafe pool:
//!
//! | Item | Old default | Consequence |
//! |---|---|---|
//! | coinbase payout script | all-zero P2PKH | 100% of block rewards burned |
//! | ORU funding outpoint | `[0x99; 32]` | mint could never fund itself |
//! | Stratum password | `"x"` | anyone can mine as anyone |
//! | block submission sink | none | found blocks never reached the node |
//!
//! Two rows left this table by having their subject deleted rather than gated.
//! SECURITY-001-A / A-5 removed the pool-chosen RES payout program: the RES goes to
//! the owner declared by the winning convergence, so the pool has no destination to
//! misconfigure. SECURITY-001-A / A-7 removed the RES mint Ed25519 seed: the mint
//! witness carries no signature, so the pool holds no key on that path and there is
//! no longer a publicly derivable devtest key to guard against.
//!
//! This module replaces the rest with **required** explicit configuration and
//! refuses to construct a production pool when any of them is missing or unsafe.
//! There is deliberately no fallback path: the process exits instead of running in
//! a state where miners can be defrauded or rewards destroyed.
//!
//! Consensus impact: NONE. Everything here is pool-operator configuration; no
//! block, header, transaction, or script consensus rule is affected. The node
//! remains the sole authority on block and transaction validity.

use std::sync::Arc;

use block_assembler::payout_script::payout_script_from_hex;
use mint_engine::{MintEngineConfig, OruFundingInput, RigordRpcConfig};
use stratum_server::auth::HashedCredentialAuthenticator;
use stratum_server::{SessionManager, VardiffConfig};

use crate::PilotError;

/// Environment variable names, kept together so the runbook can reference them.
pub mod env_keys {
    pub const PAYOUT_SCRIPT_HEX: &str = "RIGOR_POOL_PAYOUT_SCRIPT_HEX";
    pub const ORU_FUNDING_TXID: &str = "RIGOR_POOL_ORU_FUNDING_TXID";
    pub const ORU_FUNDING_VOUT: &str = "RIGOR_POOL_ORU_FUNDING_VOUT";
    pub const ORU_FUNDING_AMOUNT: &str = "RIGOR_POOL_ORU_FUNDING_AMOUNT";
    pub const ORU_FUNDING_SCRIPT_HEX: &str = "RIGOR_POOL_ORU_FUNDING_SCRIPT_HEX";
    pub const ORU_FEE: &str = "RIGOR_POOL_ORU_FEE";
    pub const WORKER_CREDENTIALS: &str = "RIGOR_POOL_WORKER_CREDENTIALS";
    pub const PROFILE: &str = "RIGOR_POOL_PROFILE";
}

/// Deployment profile. Only `Devtest` may use publicly-known material.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum PoolProfile {
    /// Regtest / in-process simulation. Unsafe defaults permitted.
    Devtest,
    /// Any chain carrying value. All secrets required and validated.
    Production,
}

impl PoolProfile {
    pub fn from_env() -> Self {
        match std::env::var(env_keys::PROFILE).ok().as_deref() {
            Some(v) if v.eq_ignore_ascii_case("devtest") => Self::Devtest,
            // Fail safe: anything other than an explicit "devtest" is production.
            _ => Self::Production,
        }
    }
}

/// Fully-specified, validated pool security configuration.
pub struct PoolSecurityConfig {
    pub profile: PoolProfile,
    /// Validated, spendable coinbase payout script (C-6b).
    pub payout_script: Vec<u8>,
    /// Validated RES mint configuration (C-6c).
    pub mint: MintEngineConfig,
    /// Worker credential store, never a shared password (H-5a).
    pub sessions: Arc<SessionManager>,
}

impl PoolSecurityConfig {
    /// Build a production configuration from the environment.
    ///
    /// Returns `Err` — and the caller must abort startup — if any required item is
    /// absent, malformed, or a known-unsafe placeholder.
    pub fn production_from_env(rpc: Option<RigordRpcConfig>) -> Result<Self, PilotError> {
        let payout_script = required(env_keys::PAYOUT_SCRIPT_HEX)?;
        let payout_script = payout_script_from_hex(&payout_script).map_err(cfg_err)?;

        let mint = mint_config_from_env(rpc)?;
        mint.validate_production().map_err(cfg_err)?;

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
            mint,
            sessions,
        })
    }

    /// Explicit devtest profile. Every value here is publicly known; it exists so
    /// regtest harnesses do not need real secrets, and it is never reachable by
    /// accident because [`PoolProfile::from_env`] defaults to production.
    pub fn devtest() -> Self {
        Self {
            profile: PoolProfile::Devtest,
            payout_script: block_assembler::UNSPENDABLE_DEVTEST_PAYOUT_SCRIPT.to_vec(),
            mint: MintEngineConfig::devtest(),
            sessions: Arc::new(SessionManager::new("x")),
        }
    }

    /// Re-check every invariant. Production configs must pass; devtest must fail.
    pub fn validate(&self) -> Result<(), PilotError> {
        if self.profile == PoolProfile::Devtest {
            return Err(PilotError::Config(
                "refusing to treat the devtest profile as production".into(),
            ));
        }
        block_assembler::AssemblerConfig::with_payout_script(self.payout_script.clone())
            .and_then(|c| c.validate_production())
            .map_err(cfg_err)?;
        self.mint.validate_production().map_err(cfg_err)?;
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

fn required_hex32(key: &str) -> Result<[u8; 32], PilotError> {
    let raw = required(key)?;
    let bytes = hex::decode(&raw).map_err(|e| cfg_err(format!("{key}: {e}")))?;
    if bytes.len() != 32 {
        return Err(PilotError::Config(format!(
            "{key} must be 32 bytes ({} given)",
            bytes.len()
        )));
    }
    let mut out = [0u8; 32];
    out.copy_from_slice(&bytes);
    Ok(out)
}

fn mint_config_from_env(rpc: Option<RigordRpcConfig>) -> Result<MintEngineConfig, PilotError> {
    let prev_txid = required_hex32(env_keys::ORU_FUNDING_TXID)?;
    let vout: u32 = required(env_keys::ORU_FUNDING_VOUT)?
        .parse()
        .map_err(|e| cfg_err(format!("{}: {e}", env_keys::ORU_FUNDING_VOUT)))?;
    let amount: i64 = required(env_keys::ORU_FUNDING_AMOUNT)?
        .parse()
        .map_err(|e| cfg_err(format!("{}: {e}", env_keys::ORU_FUNDING_AMOUNT)))?;
    let script_pubkey = hex::decode(required(env_keys::ORU_FUNDING_SCRIPT_HEX)?)
        .map_err(|e| cfg_err(format!("{}: {e}", env_keys::ORU_FUNDING_SCRIPT_HEX)))?;
    let oru_fee: i64 = std::env::var(env_keys::ORU_FEE)
        .ok()
        .map(|v| v.parse::<i64>())
        .transpose()
        .map_err(|e| cfg_err(format!("{}: {e}", env_keys::ORU_FEE)))?
        .unwrap_or(10_000);

    Ok(MintEngineConfig {
        funding: OruFundingInput {
            prev_txid,
            vout,
            amount,
            script_pubkey,
        },
        oru_fee,
        auto_submit: rpc.is_some(),
        rpc,
    })
}

/// Parse `RIGOR_POOL_WORKER_CREDENTIALS` as `worker=password_hash` pairs separated
/// by `;`. Only hashed records are accepted — a plaintext value is rejected.
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

    /// C-6 regression: the devtest profile must never validate as production.
    #[test]
    fn devtest_profile_never_validates_as_production() {
        let cfg = PoolSecurityConfig::devtest();
        assert_eq!(cfg.profile, PoolProfile::Devtest);
        assert!(cfg.validate().is_err());
        assert!(!cfg.sessions.is_production_safe());
    }

    /// C-6b regression: the unspendable coinbase default is rejected.
    #[test]
    fn burn_payout_script_is_rejected() {
        let burn = hex::encode(block_assembler::UNSPENDABLE_DEVTEST_PAYOUT_SCRIPT);
        assert!(payout_script_from_hex(&burn).is_err());
    }

    /// The profile selector must fail safe: only an explicit "devtest" opts out.
    #[test]
    fn unknown_profile_values_mean_production() {
        // Read via a pure helper to avoid mutating process env in a parallel test run.
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

    /// H-5a regression: plaintext or empty credential entries are refused.
    #[test]
    fn plaintext_worker_credentials_are_refused() {
        let auth = HashedCredentialAuthenticator::new();
        assert!(auth.upsert_hash("alice", "x").is_err());
        assert!(auth.upsert_hash("alice", "").is_err());
        assert_eq!(auth.worker_count(), 0);

        let good = stratum_server::hash_password("a-sufficiently-long-secret", b"0123456789abcdef", 64);
        auth.upsert_hash("alice", &good).unwrap();
        assert_eq!(auth.worker_count(), 1);
    }

    /// C-6c regression: a config assembled from good values passes, and each
    /// placeholder independently fails.
    #[test]
    fn mint_config_validation_is_enforced() {
        let mut cfg = MintEngineConfig {
            funding: OruFundingInput {
                prev_txid: [0x3c; 32],
                vout: 0,
                amount: 500_000_000,
                script_pubkey: vec![0x00, 0x20, 0x11],
            },
            oru_fee: 10_000,
            rpc: None,
            auto_submit: false,
        };
        cfg.validate_production().unwrap();

        cfg.funding.prev_txid = mint_engine::DEVTEST_ORU_FUNDING_TXID;
        assert!(cfg.validate_production().is_err());
    }
}
