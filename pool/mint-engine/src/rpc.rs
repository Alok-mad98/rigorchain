use serde::Deserialize;
use serde_json::{json, Value};

use crate::error::{MintError, MintResult};

#[derive(Clone, Debug)]
pub struct RigordRpcConfig {
    pub url: String,
    pub user: Option<String>,
    pub password: Option<String>,
}

pub struct RigordRpc {
    client: reqwest::blocking::Client,
    config: RigordRpcConfig,
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
    pub fn new(config: RigordRpcConfig) -> Self {
        Self {
            client: reqwest::blocking::Client::new(),
            config,
        }
    }

    pub fn call(&self, method: &str, params: Value) -> MintResult<Value> {
        let body = json!({
            "jsonrpc": "1.0",
            "id": "mint-engine",
            "method": method,
            "params": params,
        });

        let mut req = self.client.post(&self.config.url).json(&body);
        if let (Some(user), Some(pass)) = (&self.config.user, &self.config.password) {
            req = req.basic_auth(user, Some(pass));
        }

        let resp: RpcResponse = req
            .send()
            .map_err(|e| MintError::Rpc(e.to_string()))?
            .json()
            .map_err(|e| MintError::Rpc(e.to_string()))?;

        if let Some(err) = resp.error {
            return Err(MintError::Rpc(format!(
                "{} (code {:?})",
                err.message, err.code
            )));
        }
        resp.result
            .ok_or_else(|| MintError::Rpc("empty RPC result".into()))
    }

    pub fn submit_block(&self, block_hex: &str) -> MintResult<()> {
        let result = self.call("submitblock", json!([block_hex]))?;
        if result.is_null() {
            return Ok(());
        }
        if let Some(msg) = result.as_str() {
            if msg.is_empty() {
                return Ok(());
            }
            return Err(MintError::Rpc(format!("submitblock rejected: {msg}")));
        }
        Ok(())
    }

    /// `getblockhash` — used for chain-accurate ConvergenceDrawBindHashFromParent (M-06).
    pub fn get_block_hash(&self, height: u64) -> MintResult<String> {
        let result = self.call("getblockhash", json!([height]))?;
        result
            .as_str()
            .map(|s| s.to_string())
            .ok_or_else(|| MintError::Rpc("getblockhash: expected hex string".into()))
    }

    /// `testmempoolaccept` — M-07 mandatory mint reject-before-submit preflight.
    pub fn test_mempool_accept(
        &self,
        raw_tx_hex: &str,
    ) -> MintResult<crate::mempool_preflight::MempoolAcceptResult> {
        let result = self.call("testmempoolaccept", json!([[raw_tx_hex]]))?;
        crate::mempool_preflight::parse_testmempoolaccept_result(&result)
    }
}
