use serde::{Deserialize, Serialize};
use serde_json::Value;

use crate::error::{MinerError, MinerResult};

pub const DEFAULT_AGENT: &str = "rigor-miner/1.0-rgrpow01";

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
pub struct StratumMessage {
    pub id: Option<Value>,
    pub method: Option<String>,
    pub params: Option<Value>,
    pub result: Option<Value>,
    pub error: Option<Value>,
}

impl StratumResponse {
    pub fn is_ok(&self) -> bool {
        self.error.is_none() && self.result.is_some()
    }

    pub fn error_message(&self) -> Option<String> {
        self.error.as_ref().and_then(|e| {
            e.as_array()
                .and_then(|a| a.get(1))
                .and_then(|v| v.as_str())
                .map(|s| s.to_string())
        })
    }
}

pub fn parse_line(line: &str) -> MinerResult<StratumMessage> {
    serde_json::from_str(line.trim()).map_err(MinerError::from)
}

pub fn build_request(id: u64, method: &str, params: Value) -> MinerResult<String> {
    let req = StratumRequest {
        id: Some(Value::from(id)),
        method: method.to_string(),
        params,
    };
    Ok(format!("{}\n", serde_json::to_string(&req)?))
}

pub fn params_as_strings(params: &Value) -> MinerResult<Vec<String>> {
    match params {
        Value::Array(arr) => arr
            .iter()
            .map(|v| match v {
                Value::String(s) => Ok(s.clone()),
                Value::Number(n) => Ok(n.to_string()),
                Value::Bool(b) => Ok(b.to_string()),
                Value::Null => Ok(String::new()),
                _ => Err(MinerError::Stratum("param must be string/number/bool".into())),
            })
            .collect(),
        _ => Err(MinerError::Stratum("params must be array".into())),
    }
}

pub fn requires_rgrpow01_agent(agent: &str) -> bool {
    agent.to_ascii_lowercase().contains("rgrpow01")
}

pub fn requires_opushash_agent(agent: &str) -> bool {
    agent.to_ascii_lowercase().contains("opushash")
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn agent_gate() {
        assert!(requires_rgrpow01_agent("rigor-miner/1.0-rgrpow01"));
        assert!(!requires_rgrpow01_agent("legacy-miner"));
        assert!(requires_opushash_agent("rigor-miner/1.0-opushash"));
    }
}
