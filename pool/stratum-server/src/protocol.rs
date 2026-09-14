use serde::{Deserialize, Serialize};
use serde_json::Value;

use crate::error::{StratumError, StratumResult};

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct StratumRequest {
    pub id: Option<Value>,
    pub method: String,
    pub params: Value,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct StratumResponse {
    pub id: Option<Value>,
    pub result: Option<Value>,
    pub error: Option<Value>,
}

#[derive(Debug, Clone, Serialize, Deserialize)]
pub struct StratumNotify {
    pub id: Option<Value>,
    pub method: String,
    pub params: Value,
}

impl StratumResponse {
    pub fn ok(id: Option<Value>, result: Value) -> Self {
        Self {
            id,
            result: Some(result),
            error: None,
        }
    }

    pub fn err(id: Option<Value>, code: i32, message: &str) -> Self {
        Self {
            id,
            result: None,
            error: Some(serde_json::json!([code, message, null])),
        }
    }
}

pub fn parse_request(line: &str) -> StratumResult<StratumRequest> {
    serde_json::from_str(line.trim())
        .map_err(|e| StratumError::Protocol(format!("invalid JSON: {e}")))
}

pub fn serialize_response(resp: &StratumResponse) -> StratumResult<String> {
    Ok(format!("{}\n", serde_json::to_string(resp).map_err(|e| StratumError::Protocol(e.to_string()))?))
}

pub fn build_notify(
    job_id: &str,
    prevhash: &str,
    coinb1: &str,
    coinb2: &str,
    merkle_branch: &[String],
    version: &str,
    nbits: &str,
    ntime: &str,
    clean_jobs: bool,
) -> StratumNotify {
    StratumNotify {
        id: None,
        method: "mining.notify".into(),
        params: serde_json::json!([
            job_id,
            prevhash,
            coinb1,
            coinb2,
            merkle_branch,
            version,
            nbits,
            ntime,
            clean_jobs
        ]),
    }
}

pub fn build_set_difficulty(difficulty: f64) -> StratumNotify {
    StratumNotify {
        id: None,
        method: "mining.set_difficulty".into(),
        params: serde_json::json!([difficulty]),
    }
}

/// Serialize a `mining.set_difficulty` notification for the wire (H-5d).
pub fn serialize_set_difficulty(difficulty: f64) -> StratumResult<String> {
    Ok(format!(
        "{}\n",
        serde_json::to_string(&build_set_difficulty(difficulty))
            .map_err(|e| StratumError::Protocol(e.to_string()))?
    ))
}

pub fn params_as_strings(params: &Value) -> StratumResult<Vec<String>> {
    match params {
        Value::Array(arr) => arr
            .iter()
            .map(|v| match v {
                Value::String(s) => Ok(s.clone()),
                Value::Number(n) => Ok(n.to_string()),
                Value::Bool(b) => Ok(b.to_string()),
                Value::Null => Ok(String::new()),
                _ => Err(StratumError::Protocol("param must be string/number/null".into())),
            })
            .collect(),
        _ => Err(StratumError::Protocol("params must be array".into())),
    }
}

/// Production Stratum agents must declare SHA3-256 RGRPOW01.
pub fn requires_rgrpow01_agent(agent: &str) -> bool {
    agent.to_ascii_lowercase().contains("rgrpow01")
}

/// Historical OpuHash agent gate. Production rejects these (wrong algorithm).
pub fn requires_opushash_agent(agent: &str) -> bool {
    agent.to_ascii_lowercase().contains("opushash")
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn parses_subscribe_request() {
        let line = r#"{"id":1,"method":"mining.subscribe","params":["rigor-miner/1.0-rgrpow01",null]}"#;
        let req = parse_request(line).unwrap();
        assert_eq!(req.method, "mining.subscribe");
    }

    #[test]
    fn agent_gate() {
        assert!(requires_rgrpow01_agent("rigor-miner/1.0-rgrpow01"));
        assert!(!requires_rgrpow01_agent("legacy-miner"));
        assert!(!requires_rgrpow01_agent("rigor-miner/1.0-opushash"));
        assert!(requires_opushash_agent("rigor-miner/1.0-opushash"));
    }
}
