//! M-07: `testmempoolaccept` preflight gate (reject-before-submit).

use serde_json::Value;

use crate::error::{MintError, MintResult};

/// Parsed `testmempoolaccept` single-tx result.
#[derive(Clone, Debug, PartialEq, Eq)]
pub struct MempoolAcceptResult {
    pub txid: String,
    pub allowed: bool,
    pub reject_reason: Option<String>,
}

/// Parse Bitcoin Core / rigord `testmempoolaccept` JSON array result.
pub fn parse_testmempoolaccept_result(value: &Value) -> MintResult<MempoolAcceptResult> {
    let arr = value
        .as_array()
        .ok_or_else(|| MintError::Rpc("testmempoolaccept: expected array result".into()))?;
    let first = arr
        .first()
        .ok_or_else(|| MintError::Rpc("testmempoolaccept: empty result array".into()))?;
    let txid = first
        .get("txid")
        .and_then(|v| v.as_str())
        .unwrap_or("")
        .to_string();
    let allowed = first
        .get("allowed")
        .and_then(|v| v.as_bool())
        .ok_or_else(|| MintError::Rpc("testmempoolaccept: missing allowed bool".into()))?;
    let reject_reason = first
        .get("reject-reason")
        .and_then(|v| v.as_str())
        .map(|s| s.to_string());
    Ok(MempoolAcceptResult {
        txid,
        allowed,
        reject_reason,
    })
}

/// Enforce M-07 gate: disallowed mint must not proceed to submitblock.
pub fn enforce_mempool_preflight(result: &MempoolAcceptResult) -> MintResult<()> {
    if result.allowed {
        return Ok(());
    }
    let reason = result
        .reject_reason
        .clone()
        .unwrap_or_else(|| "unknown".into());
    Err(MintError::Validation(format!(
        "M-07 testmempoolaccept rejected mint before submit (txid={}, reason={reason})",
        result.txid
    )))
}

#[cfg(test)]
mod tests {
    use super::*;
    use serde_json::json;

    #[test]
    fn invalid_mint_is_rejected() {
        let v = json!([{
            "txid": "ab".repeat(32),
            "allowed": false,
            "reject-reason": "mandatory-script-verify-flag-failed"
        }]);
        let r = parse_testmempoolaccept_result(&v).unwrap();
        assert!(!r.allowed);
        let err = enforce_mempool_preflight(&r).unwrap_err().to_string();
        assert!(err.contains("M-07"));
        assert!(err.contains("rejected"));
    }

    #[test]
    fn valid_mint_is_accepted() {
        let v = json!([{
            "txid": "cd".repeat(32),
            "allowed": true
        }]);
        let r = parse_testmempoolaccept_result(&v).unwrap();
        assert!(r.allowed);
        enforce_mempool_preflight(&r).unwrap();
    }

    #[test]
    fn valid_then_submit_gate_sequence() {
        // Simulates: accept → submit permitted; reject → submit blocked.
        let accept = parse_testmempoolaccept_result(&json!([{
            "txid": "11".repeat(32),
            "allowed": true
        }]))
        .unwrap();
        enforce_mempool_preflight(&accept).unwrap();
        let submit_allowed = true; // would call submitblock only after Ok(())
        assert!(submit_allowed);

        let reject = parse_testmempoolaccept_result(&json!([{
            "txid": "22".repeat(32),
            "allowed": false,
            "reject-reason": "bad-txns-inputs-missingorspent"
        }]))
        .unwrap();
        assert!(enforce_mempool_preflight(&reject).is_err());
    }
}
