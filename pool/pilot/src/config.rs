//! TOML + env configuration for the Alpha ORU-only pool product.
//!
//! Consensus impact: NONE. RES mint is never configured here.

use std::path::Path;
use std::time::Duration;

use serde::Deserialize;

use crate::{PilotError, PilotResult, PoolPilotConfig};

#[derive(Clone, Debug, Deserialize)]
pub struct PoolFileConfig {
    #[serde(default)]
    pub node: NodeSection,
    #[serde(default)]
    pub stratum: StratumSection,
    #[serde(default)]
    pub payout: PayoutSection,
    #[serde(default)]
    pub database: DatabaseSection,
    #[serde(default)]
    pub security: SecuritySection,
    #[serde(default)]
    pub status: StatusSection,
}

#[derive(Clone, Debug, Deserialize)]
pub struct NodeSection {
    #[serde(default = "default_rpc_host")]
    pub rpc_host: String,
    #[serde(default = "default_rpc_port")]
    pub rpc_port: u16,
    #[serde(default)]
    pub rpc_user: Option<String>,
    #[serde(default)]
    pub rpc_password: Option<String>,
}

fn default_rpc_host() -> String {
    "127.0.0.1".into()
}
fn default_rpc_port() -> u16 {
    8332
}

impl Default for NodeSection {
    fn default() -> Self {
        Self {
            rpc_host: default_rpc_host(),
            rpc_port: default_rpc_port(),
            rpc_user: None,
            rpc_password: None,
        }
    }
}

#[derive(Clone, Debug, Deserialize)]
pub struct StratumSection {
    #[serde(default = "default_listen_address")]
    pub listen_address: String,
    #[serde(default = "default_listen_port")]
    pub listen_port: u16,
}

fn default_listen_address() -> String {
    "127.0.0.1".into()
}
fn default_listen_port() -> u16 {
    3333
}

impl Default for StratumSection {
    fn default() -> Self {
        Self {
            listen_address: default_listen_address(),
            listen_port: default_listen_port(),
        }
    }
}

#[derive(Clone, Debug, Deserialize)]
pub struct PayoutSection {
    /// Pool fee percent of coinbase ORU (0.0–100.0). Default 0.
    #[serde(default, alias = "ORU_fee_percent")]
    pub oru_fee_percent: f64,
    /// Operator settlement review interval (seconds).
    #[serde(default = "default_payout_interval")]
    pub payout_interval: u64,
    #[serde(default = "default_pplns_window")]
    pub pplns_window: u64,
    #[serde(default)]
    pub payout_script_hex: Option<String>,
}

fn default_payout_interval() -> u64 {
    3600
}
fn default_pplns_window() -> u64 {
    1
}

impl Default for PayoutSection {
    fn default() -> Self {
        Self {
            oru_fee_percent: 0.0,
            payout_interval: default_payout_interval(),
            pplns_window: default_pplns_window(),
            payout_script_hex: None,
        }
    }
}

#[derive(Clone, Debug, Deserialize)]
pub struct DatabaseSection {
    #[serde(default)]
    pub enabled: bool,
    #[serde(default = "default_db_host")]
    pub host: String,
    #[serde(default = "default_db_port")]
    pub port: u16,
    #[serde(default)]
    pub user: Option<String>,
    #[serde(default)]
    pub password: Option<String>,
    #[serde(default)]
    pub dbname: Option<String>,
    #[serde(default)]
    pub url: Option<String>,
}

fn default_db_host() -> String {
    "127.0.0.1".into()
}
fn default_db_port() -> u16 {
    5432
}

impl Default for DatabaseSection {
    fn default() -> Self {
        Self {
            enabled: false,
            host: default_db_host(),
            port: default_db_port(),
            user: None,
            password: None,
            dbname: None,
            url: None,
        }
    }
}

#[derive(Clone, Debug, Deserialize)]
pub struct SecuritySection {
    #[serde(default = "default_profile")]
    pub profile: String,
    #[serde(default)]
    pub worker_credentials: Option<String>,
}

fn default_profile() -> String {
    "production".into()
}

impl Default for SecuritySection {
    fn default() -> Self {
        Self {
            profile: default_profile(),
            worker_credentials: None,
        }
    }
}

#[derive(Clone, Debug, Deserialize)]
pub struct StatusSection {
    #[serde(default)]
    pub interval_secs: u64,
    #[serde(default = "default_status_file")]
    pub file: String,
}

fn default_status_file() -> String {
    ".runtime/pool-pilot/status.json".into()
}

impl Default for StatusSection {
    fn default() -> Self {
        Self {
            interval_secs: 0,
            file: default_status_file(),
        }
    }
}

impl PoolFileConfig {
    pub fn load_file(path: &Path) -> PilotResult<Self> {
        let raw = std::fs::read_to_string(path).map_err(|e| {
            PilotError::Config(format!("failed to read config {}: {e}", path.display()))
        })?;
        toml::from_str(&raw)
            .map_err(|e| PilotError::Config(format!("invalid TOML {}: {e}", path.display())))
    }

    pub fn apply_env_overrides(mut self) -> Self {
        if let Ok(url) = std::env::var("RIGOR_RPC_URL") {
            if let Some((host, port)) = parse_rpc_url_host_port(&url) {
                self.node.rpc_host = host;
                self.node.rpc_port = port;
            }
        }
        if let Ok(user) = std::env::var("RIGOR_RPC_USER") {
            self.node.rpc_user = Some(user);
        }
        if let Ok(pass) = std::env::var("RIGOR_RPC_PASSWORD") {
            self.node.rpc_password = Some(pass);
        }
        if let Ok(addr) = std::env::var("STRATUM_LISTEN") {
            if let Some((host, port)) = parse_listen(&addr) {
                self.stratum.listen_address = host;
                self.stratum.listen_port = port;
            }
        }
        if let Ok(w) = std::env::var("PPLNS_WINDOW") {
            if let Ok(v) = w.parse::<u64>() {
                self.payout.pplns_window = v;
            }
        }
        if let Ok(fee) = std::env::var("ORU_FEE_PERCENT") {
            if let Ok(v) = fee.parse::<f64>() {
                self.payout.oru_fee_percent = v;
            }
        }
        if let Ok(iv) = std::env::var("PAYOUT_INTERVAL") {
            if let Ok(v) = iv.parse::<u64>() {
                self.payout.payout_interval = v;
            }
        }
        if let Ok(hex) = std::env::var("RIGOR_POOL_PAYOUT_SCRIPT_HEX") {
            self.payout.payout_script_hex = Some(hex);
        }
        if let Ok(creds) = std::env::var("RIGOR_POOL_WORKER_CREDENTIALS") {
            self.security.worker_credentials = Some(creds);
        }
        if let Ok(profile) = std::env::var("RIGOR_POOL_PROFILE") {
            self.security.profile = profile;
        }
        if let Ok(url) = std::env::var("DATABASE_URL") {
            self.database.enabled = true;
            self.database.url = Some(url);
        }
        if let Ok(secs) = std::env::var("PILOT_STATUS_INTERVAL_SECS") {
            if let Ok(v) = secs.parse::<u64>() {
                self.status.interval_secs = v;
            }
        }
        if let Ok(file) = std::env::var("PILOT_STATUS_FILE") {
            self.status.file = file;
        }
        self
    }

    pub fn into_pilot_config(self) -> PilotResult<ResolvedPoolConfig> {
        if !(0.0..=100.0).contains(&self.payout.oru_fee_percent) {
            return Err(PilotError::Config(format!(
                "ORU_fee_percent must be in [0,100], got {}",
                self.payout.oru_fee_percent
            )));
        }

        let rpc_url = format!(
            "http://{}:{}/",
            self.node.rpc_host.trim(),
            self.node.rpc_port
        );
        let stratum_listen = format!(
            "{}:{}",
            self.stratum.listen_address.trim(),
            self.stratum.listen_port
        );

        let database_url = if self.database.enabled {
            if let Some(url) = self.database.url.clone().filter(|u| !u.trim().is_empty()) {
                Some(url)
            } else {
                let user = self.database.user.clone().unwrap_or_else(|| "rigor_pool".into());
                let pass = self.database.password.clone().unwrap_or_default();
                let db = self.database.dbname.clone().unwrap_or_else(|| "rigor_pool".into());
                Some(format!(
                    "postgres://{}:{}@{}:{}/{}",
                    user, pass, self.database.host, self.database.port, db
                ))
            }
        } else {
            self.database.url.clone().filter(|u| !u.trim().is_empty())
        };

        Ok(ResolvedPoolConfig {
            pilot: PoolPilotConfig {
                rpc_url,
                rpc_user: self.node.rpc_user,
                rpc_password: self.node.rpc_password,
                stratum_listen,
                gbt_poll_interval: Duration::from_secs(2),
                pplns_window: self.payout.pplns_window.max(1),
                oru_fee_percent: self.payout.oru_fee_percent,
                payout_interval_secs: self.payout.payout_interval,
            },
            profile: self.security.profile,
            payout_script_hex: self.payout.payout_script_hex,
            worker_credentials: self.security.worker_credentials,
            database_url,
            status_interval_secs: self.status.interval_secs,
            status_file: self.status.file,
        })
    }
}

#[derive(Clone, Debug)]
pub struct ResolvedPoolConfig {
    pub pilot: PoolPilotConfig,
    pub profile: String,
    pub payout_script_hex: Option<String>,
    pub worker_credentials: Option<String>,
    pub database_url: Option<String>,
    pub status_interval_secs: u64,
    pub status_file: String,
}

impl ResolvedPoolConfig {
    /// Export security secrets into process env for fail-closed production gate.
    pub fn export_security_env(&self) {
        if let Some(hex) = &self.payout_script_hex {
            if !hex.trim().is_empty() {
                std::env::set_var("RIGOR_POOL_PAYOUT_SCRIPT_HEX", hex);
            }
        }
        if let Some(creds) = &self.worker_credentials {
            if !creds.trim().is_empty() {
                std::env::set_var("RIGOR_POOL_WORKER_CREDENTIALS", creds);
            }
        }
        if !self.profile.trim().is_empty() {
            std::env::set_var("RIGOR_POOL_PROFILE", &self.profile);
        }
        if let Some(url) = &self.database_url {
            std::env::set_var("DATABASE_URL", url);
        }
        if self.status_interval_secs > 0 {
            std::env::set_var(
                "PILOT_STATUS_INTERVAL_SECS",
                self.status_interval_secs.to_string(),
            );
        }
        std::env::set_var("PILOT_STATUS_FILE", &self.status_file);
    }
}

pub fn load_resolved(config_path: Option<&Path>) -> PilotResult<ResolvedPoolConfig> {
    let file_cfg = if let Some(path) = config_path {
        PoolFileConfig::load_file(path)?
    } else {
        PoolFileConfig {
            node: NodeSection::default(),
            stratum: StratumSection::default(),
            payout: PayoutSection::default(),
            database: DatabaseSection::default(),
            security: SecuritySection::default(),
            status: StatusSection::default(),
        }
    };
    file_cfg.apply_env_overrides().into_pilot_config()
}

fn parse_rpc_url_host_port(url: &str) -> Option<(String, u16)> {
    let trimmed = url.trim().trim_end_matches('/');
    let without_scheme = trimmed
        .strip_prefix("http://")
        .or_else(|| trimmed.strip_prefix("https://"))
        .unwrap_or(trimmed);
    let hostport = without_scheme.split('/').next()?;
    parse_listen(hostport)
}

fn parse_listen(addr: &str) -> Option<(String, u16)> {
    let addr = addr.trim();
    if let Some((host, port)) = addr.rsplit_once(':') {
        let port: u16 = port.parse().ok()?;
        Some((host.to_string(), port))
    } else {
        None
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn example_toml_parses() {
        let raw = include_str!("../../config/example.toml");
        let cfg: PoolFileConfig = toml::from_str(raw).expect("example.toml");
        assert_eq!(cfg.node.rpc_port, 8332);
        assert_eq!(cfg.stratum.listen_port, 3333);
        assert!((cfg.payout.oru_fee_percent - 1.0).abs() < f64::EPSILON);
        assert_eq!(cfg.payout.payout_interval, 3600);
        assert!(!cfg.database.enabled);
    }

    #[test]
    fn fee_out_of_range_rejected() {
        let mut cfg = PoolFileConfig {
            node: NodeSection::default(),
            stratum: StratumSection::default(),
            payout: PayoutSection::default(),
            database: DatabaseSection::default(),
            security: SecuritySection::default(),
            status: StatusSection::default(),
        };
        cfg.payout.oru_fee_percent = 150.0;
        assert!(cfg.into_pilot_config().is_err());
    }
}
