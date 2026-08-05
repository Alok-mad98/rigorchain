use crate::error::{GbtError, GbtResult};
use serde::Deserialize;
use serde_json::{json, Value};

/// Minimal JSON-RPC client for `rigord` (Bitcoin Core compatible wire).
pub struct RigordRpc {
    client: reqwest::blocking::Client,
    url: String,
    auth: Option<(String, String)>,
}

#[derive(Debug, Deserialize)]
struct RpcResponse {
    result: Option<Value>,
    error: Option<RpcErrorObj>,
}

#[derive(Debug, Deserialize)]
struct RpcErrorObj {
    message: String,
    code: Option<i64>,
}

impl RigordRpc {
    pub fn new(url: impl Into<String>) -> Self {
        Self {
            client: reqwest::blocking::Client::new(),
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
        resp.result
            .ok_or_else(|| GbtError::Rpc("empty RPC result".into()))
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
}
