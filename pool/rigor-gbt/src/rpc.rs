use crate::error::{GbtError, GbtResult};
use serde::Deserialize;
use serde_json::{json, Value};
use std::time::Duration;

/// Minimal JSON-RPC client for `rigord` (Bitcoin Core compatible wire).
pub struct RigordRpc {
    client: reqwest::blocking::Client,
    url: String,
    auth: Option<(String, String)>,
}

#[derive(Debug, Deserialize)]
struct RpcResponse {
    /// Core methods such as `submitblock` return JSON `null` on success.
    /// Do **not** use `Option<Value>` here: serde maps JSON null → `None`, which
    /// falsely becomes "empty RPC result".
    #[serde(default)]
    result: Value,
    error: Option<RpcErrorObj>,
}

#[derive(Debug, Deserialize)]
struct RpcErrorObj {
    message: String,
    code: Option<i64>,
}

impl RigordRpc {
    pub fn new(url: impl Into<String>) -> Self {
        let client = reqwest::blocking::Client::builder()
            .timeout(Duration::from_secs(30))
            .connect_timeout(Duration::from_secs(5))
            .build()
            .unwrap_or_else(|_| reqwest::blocking::Client::new());
        Self {
            client,
            url: url.into(),
            auth: None,
        }
    }

    pub fn with_auth(mut self, user: impl Into<String>, password: impl Into<String>) -> Self {
        self.auth = Some((user.into(), password.into()));
        self
    }

    pub fn call(&self, method: &str, params: Value) -> GbtResult<Value> {
        let body = json!({
            "jsonrpc": "1.0",
            "id": "rigor-gbt",
            "method": method,
            "params": params,
        });

        let mut req = self.client.post(&self.url).json(&body);
        if let Some((user, pass)) = &self.auth {
            req = req.basic_auth(user, Some(pass));
        }

        let resp: RpcResponse = req.send()?.json()?;
        if let Some(err) = resp.error {
            return Err(GbtError::Rpc(format!(
                "{} (code {:?})",
                err.message, err.code
            )));
        }
        Ok(resp.result)
    }

    pub fn get_best_block_hash(&self) -> GbtResult<String> {
        let result = self.call("getbestblockhash", json!([]))?;
        result
            .as_str()
            .map(|s| s.to_ascii_lowercase())
            .ok_or_else(|| GbtError::Rpc("getbestblockhash: expected string".into()))
    }

    pub fn get_blockchain_info(&self) -> GbtResult<BlockchainInfo> {
        let result = self.call("getblockchaininfo", json!([]))?;
        serde_json::from_value(result).map_err(GbtError::from)
    }

    pub fn get_block_template(&self, longpollid: Option<&str>) -> GbtResult<(Value, String)> {
        let mut params = json!([{"rules": ["segwit"]}]);
        if let Some(lp) = longpollid {
            params = json!([{"rules": ["segwit"], "longpollid": lp}]);
        }
        let result = self.call("getblocktemplate", params)?;
        let raw = serde_json::to_string(&result)?;
        Ok((result, raw))
    }

    /// Submit a fully assembled block hex (`submitblock`). Null/empty result means accepted.
    pub fn submit_block(&self, block_hex: &str) -> GbtResult<()> {
        let result = self.call("submitblock", json!([block_hex]))?;
        if result.is_null() {
            return Ok(());
        }
        if let Some(msg) = result.as_str() {
            if msg.is_empty() {
                return Ok(());
            }
            return Err(GbtError::Rpc(format!("submitblock rejected: {msg}")));
        }
        Ok(())
    }
}

#[derive(Debug, Clone, Deserialize)]
pub struct BlockchainInfo {
    pub blocks: u64,
    pub headers: u64,
    pub initialblockdownload: Option<bool>,
    pub chain: Option<String>,
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn rpc_client_constructs() {
        let _rpc = RigordRpc::new("http://127.0.0.1:8332");
    }

    #[test]
    fn submitblock_null_result_deserializes_as_json_null() {
        // Regression: Option<Value> would map null → None → "empty RPC result"
        // even though Core accepted the block.
        let raw = r#"{"result":null,"error":null,"id":"rigor-gbt"}"#;
        let resp: RpcResponse = serde_json::from_str(raw).unwrap();
        assert!(resp.error.is_none());
        assert!(resp.result.is_null());
    }

    #[test]
    fn submitblock_reject_string_is_preserved() {
        let raw = r#"{"result":"bad-txnmrklroot","error":null,"id":"rigor-gbt"}"#;
        let resp: RpcResponse = serde_json::from_str(raw).unwrap();
        assert_eq!(resp.result.as_str(), Some("bad-txnmrklroot"));
    }
}
