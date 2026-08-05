use serde_json::Value;

use crate::error::{MinerError, MinerResult};
use crate::job::ConvergenceMetadata;
use crate::stratum::protocol::params_as_strings;

/// Parsed standard Stratum v1 `mining.notify` (9 params).
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct MiningNotify {
    pub job_id: String,
    pub prevhash: String,
    pub coinb1: String,
    pub coinb2: String,
    pub merkle_branch: Vec<String>,
    pub version: String,
    pub nbits: String,
    pub ntime: String,
    pub clean_jobs: bool,
}

impl MiningNotify {
    pub fn parse(params: &Value) -> MinerResult<Self> {
        let arr = params
            .as_array()
            .ok_or_else(|| MinerError::Stratum("mining.notify params must be array".into()))?;
        if arr.len() < 9 {
            return Err(MinerError::Stratum(format!(
                "mining.notify expects 9 params, got {}",
                arr.len()
            )));
        }
        let scalar = |i: usize| -> MinerResult<String> {
            match &arr[i] {
                Value::String(s) => Ok(s.clone()),
                Value::Number(n) => Ok(n.to_string()),
                Value::Bool(b) => Ok(b.to_string()),
                Value::Null => Ok(String::new()),
                _ => Err(MinerError::Stratum(format!("param {i} must be scalar"))),
            }
        };
        let merkle_branch = arr
            .get(4)
            .and_then(|v| v.as_array())
            .map(|a| {
                a.iter()
                    .filter_map(|x| x.as_str().map(|s| s.to_string()))
                    .collect()
            })
            .unwrap_or_default();
        let clean_jobs = match &arr[8] {
            Value::Bool(b) => *b,
            Value::String(s) => matches!(s.as_str(), "true" | "1"),
            Value::Number(n) => n.as_u64().unwrap_or(0) != 0,
            _ => false,
        };
        Ok(Self {
            job_id: scalar(0)?,
            prevhash: scalar(1)?,
            coinb1: scalar(2)?,
            coinb2: scalar(3)?,
            merkle_branch,
            version: scalar(5)?,
            nbits: scalar(6)?,
            ntime: scalar(7)?,
            clean_jobs,
        })
    }
}

/// Optional RIGOR extension (`mining.notify.rigor`) — display-only convergence fields.
#[derive(Clone, Debug, Default, PartialEq, Eq)]
pub struct MiningNotifyRigor {
    pub job_id: String,
    pub convergence: ConvergenceMetadata,
}

impl MiningNotifyRigor {
    pub fn parse(params: &Value) -> MinerResult<Self> {
        let obj = params
            .as_object()
            .ok_or_else(|| MinerError::Stratum("notify.rigor params must be object".into()))?;
        let job_id = obj
            .get("job_id")
            .and_then(|v| v.as_str())
            .unwrap_or("")
            .to_string();
        Ok(Self {
            job_id,
            convergence: ConvergenceMetadata {
                convergence_root_hex: obj
                    .get("convergence_root_hex")
                    .and_then(|v| v.as_str())
                    .map(|s| s.to_string()),
                preview_winner_txid: obj
                    .get("preview_winner_txid")
                    .and_then(|v| v.as_str())
                    .map(|s| s.to_string()),
                mint_required: obj
                    .get("mint_required")
                    .and_then(|v| v.as_bool())
                    .unwrap_or(false),
            },
        })
    }
}

#[derive(Clone, Debug, PartialEq)]
pub struct SetDifficulty {
    pub difficulty: f64,
}

impl SetDifficulty {
    pub fn parse(params: &Value) -> MinerResult<Self> {
        let p = params_as_strings(params)?;
        let difficulty = p
            .first()
            .ok_or_else(|| MinerError::Stratum("set_difficulty missing value".into()))?
            .parse()
            .map_err(|_| MinerError::Stratum("invalid difficulty".into()))?;
        Ok(Self { difficulty })
    }
}

#[derive(Clone, Debug, PartialEq, Eq)]
pub struct SubscribeResult {
    pub extranonce1: Vec<u8>,
    pub extranonce2_size: u32,
    pub subscription_id: String,
}

impl SubscribeResult {
    pub fn from_response(result: &Value) -> MinerResult<Self> {
        let arr = result
            .as_array()
            .ok_or_else(|| MinerError::Stratum("subscribe result must be array".into()))?;
        let details = arr
            .get(1)
            .and_then(|v| v.as_str())
            .ok_or_else(|| MinerError::Stratum("subscribe missing extranonce1".into()))?;
        let en1 = hex::decode(details)
            .map_err(|e| MinerError::Stratum(format!("extranonce1 hex: {e}")))?;
        let en2_size = arr
            .get(2)
            .and_then(|v| v.as_u64())
            .unwrap_or(4) as u32;
        let subscription_id = arr
            .get(0)
            .and_then(|v| v.as_array())
            .and_then(|a| a.first())
            .and_then(|x| x.as_array())
            .and_then(|b| b.get(1))
            .and_then(|v| v.as_str())
            .unwrap_or("")
            .to_string();
        Ok(Self {
            extranonce1: en1,
            extranonce2_size: en2_size,
            subscription_id,
        })
    }
}
