use crate::error::{GbtError, GbtResult};
use crate::gbt::GbtTemplate;
use sha2::{Digest, Sha256};

/// Lottery GBT keys that shipping rigord must not emit (RFC-020).
const LOTTERY_GBT_KEYS: &[&str] = &[
    "paid_winner_txid",
    "default_res_mint",
    "default_pcorb_carrier",
    "convergence_root",
    "convergence_draw",
    "convergence_txids",
    "default_convergence_commitment",
];

/// Validation gate V-01 through V-10 (RIGOR_GBT_ADAPTER_SPEC §9.3).
pub fn validate_template(template: &GbtTemplate, raw_json: &str) -> GbtResult<[u8; 32]> {
    v01_height(template)?;
    v02_previousblockhash(template)?;
    v03_bits_and_prev_nbits(template)?;
    v04_coinbasevalue(template)?;
    v05_transaction_data(template)?;
    v06_reject_lottery_fields(raw_json)?;
    v07_longpollid(template)?;
    v08_weightlimit(template)?;
    v09_bits_distinct(template)?;
    v10_payload_hash(raw_json)
}

fn v01_height(template: &GbtTemplate) -> GbtResult<()> {
    if template.height >= 1 {
        Ok(())
    } else {
        Err(GbtError::Validation {
            rule: "V-01",
            message: format!("height must be >= 1, got {}", template.height),
        })
    }
}

fn v02_previousblockhash(template: &GbtTemplate) -> GbtResult<()> {
    validate_hex32("previousblockhash", &template.previousblockhash, "V-02")
}

fn v03_bits_and_prev_nbits(template: &GbtTemplate) -> GbtResult<()> {
    validate_hex8("bits", &template.bits, "V-03")?;
    validate_hex8("prev_nbits", &template.prev_nbits, "V-03")
}

fn v04_coinbasevalue(template: &GbtTemplate) -> GbtResult<()> {
    if template.coinbasevalue >= 0 {
        Ok(())
    } else {
        Err(GbtError::Validation {
            rule: "V-04",
            message: format!(
                "coinbasevalue must be >= 0, got {}",
                template.coinbasevalue
            ),
        })
    }
}

fn v05_transaction_data(template: &GbtTemplate) -> GbtResult<()> {
    for (i, tx) in template.transactions.iter().enumerate() {
        if tx.data.is_empty() {
            return Err(GbtError::Validation {
                rule: "V-05",
                message: format!("transactions[{i}].data is empty"),
            });
        }
        hex::decode(&tx.data).map_err(|e| GbtError::Validation {
            rule: "V-05",
            message: format!("transactions[{i}].data does not decode: {e}"),
        })?;
    }
    Ok(())
}

/// V-06: shipping GBT must not carry leftover lottery fields.
///
/// Scans `raw_json` for keys that lottery-era nodes emitted. Any present and
/// non-null value fails the template — the pool must not mine on those fields.
fn v06_reject_lottery_fields(raw_json: &str) -> GbtResult<()> {
    let value: serde_json::Value = serde_json::from_str(raw_json).map_err(|e| GbtError::Validation {
        rule: "V-06",
        message: format!("raw GBT JSON does not parse: {e}"),
    })?;
    let Some(obj) = value.as_object() else {
        return Err(GbtError::Validation {
            rule: "V-06",
            message: "raw GBT JSON must be an object".into(),
        });
    };
    for key in LOTTERY_GBT_KEYS {
        if let Some(v) = obj.get(*key) {
            if !v.is_null() {
                return Err(GbtError::Validation {
                    rule: "V-06",
                    message: format!("lottery GBT field `{key}` is present and non-null"),
                });
            }
        }
    }
    Ok(())
}

fn v07_longpollid(template: &GbtTemplate) -> GbtResult<()> {
    if !template.longpollid.is_empty() {
        Ok(())
    } else {
        Err(GbtError::Validation {
            rule: "V-07",
            message: "longpollid must be non-empty".into(),
        })
    }
}

fn v08_weightlimit(template: &GbtTemplate) -> GbtResult<()> {
    if template.weightlimit.is_some() {
        Ok(())
    } else {
        Err(GbtError::Validation {
            rule: "V-08",
            message: "weightlimit must be present (segwit rules)".into(),
        })
    }
}

/// V-09: `prev_nbits` must not be confused with `bits` — both fields required for separate storage.
fn v09_bits_distinct(template: &GbtTemplate) -> GbtResult<()> {
    if template.bits.is_empty() {
        return Err(GbtError::Validation {
            rule: "V-09",
            message: "bits must be present (distinct from prev_nbits)".into(),
        });
    }
    if template.prev_nbits.is_empty() {
        return Err(GbtError::Validation {
            rule: "V-09",
            message: "prev_nbits must be present (RIGOR commitment field)".into(),
        });
    }
    Ok(())
}

/// V-10: SHA256 of raw JSON for audit trail.
fn v10_payload_hash(raw_json: &str) -> GbtResult<[u8; 32]> {
    let digest = Sha256::digest(raw_json.as_bytes());
    let mut out = [0u8; 32];
    out.copy_from_slice(&digest);
    Ok(out)
}

fn validate_hex32(field: &str, value: &str, rule: &'static str) -> GbtResult<()> {
    let lower = value.to_ascii_lowercase();
    if lower.len() != 64 {
        return Err(GbtError::Validation {
            rule,
            message: format!("{field} must be 32-byte hex (64 chars), got len {}", lower.len()),
        });
    }
    if !lower.chars().all(|c| c.is_ascii_hexdigit()) {
        return Err(GbtError::Validation {
            rule,
            message: format!("{field} contains non-hex characters"),
        });
    }
    Ok(())
}

fn validate_hex8(field: &str, value: &str, rule: &'static str) -> GbtResult<()> {
    let lower = value.to_ascii_lowercase();
    if lower.len() != 8 {
        return Err(GbtError::Validation {
            rule,
            message: format!("{field} must be 8 hex chars, got len {}", lower.len()),
        });
    }
    if !lower.chars().all(|c| c.is_ascii_hexdigit()) {
        return Err(GbtError::Validation {
            rule,
            message: format!("{field} contains non-hex characters"),
        });
    }
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;
    fn valid_template() -> (GbtTemplate, String) {
        let raw = r#"{
            "version": 536870912,
            "previousblockhash": "0000000000000000000123456789abcdef0123456789abcdef0123456789abcd",
            "transactions": [
                {
                    "data": "0100000001",
                    "txid": "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
                }
            ],
            "coinbasevalue": 12800000000,
            "longpollid": "0000000000000000000123456789abcdef0123456789abcdef0123456789abcd/1",
            "weightlimit": 1000000,
            "bits": "1d00ffff",
            "prev_nbits": "1c00ff00",
            "height": 100
        }"#;
        let template: GbtTemplate = serde_json::from_str(raw).unwrap();
        (template, raw.to_string())
    }

    #[test]
    fn all_rules_pass_on_valid_template() {
        let (t, raw) = valid_template();
        assert!(validate_template(&t, &raw).is_ok());
    }

    #[test]
    fn v01_rejects_zero_height() {
        let (mut t, raw) = valid_template();
        t.height = 0;
        let err = validate_template(&t, &raw).unwrap_err();
        assert!(matches!(err, GbtError::Validation { rule: "V-01", .. }));
    }

    #[test]
    fn v02_rejects_bad_prevhash() {
        let (mut t, raw) = valid_template();
        t.previousblockhash = "abc".into();
        let err = validate_template(&t, &raw).unwrap_err();
        assert!(matches!(err, GbtError::Validation { rule: "V-02", .. }));
    }

    #[test]
    fn v04_rejects_negative_coinbase() {
        let (mut t, raw) = valid_template();
        t.coinbasevalue = -1;
        let err = validate_template(&t, &raw).unwrap_err();
        assert!(matches!(err, GbtError::Validation { rule: "V-04", .. }));
    }

    #[test]
    fn v05_rejects_bad_tx_data() {
        let (mut t, raw) = valid_template();
        t.transactions[0].data = "zzzz".into();
        let err = validate_template(&t, &raw).unwrap_err();
        assert!(matches!(err, GbtError::Validation { rule: "V-05", .. }));
    }

    #[test]
    fn v06_rejects_lottery_convergence_root() {
        let (t, _) = valid_template();
        let raw = r#"{
            "previousblockhash": "0000000000000000000123456789abcdef0123456789abcdef0123456789abcd",
            "transactions": [],
            "coinbasevalue": 12800000000,
            "longpollid": "lp",
            "weightlimit": 1000000,
            "bits": "1d00ffff",
            "prev_nbits": "1c00ff00",
            "height": 100,
            "convergence_root": "cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc"
        }"#;
        let err = validate_template(&t, raw).unwrap_err();
        assert!(matches!(err, GbtError::Validation { rule: "V-06", .. }));
    }

    #[test]
    fn v06_rejects_lottery_convergence_txids() {
        let (t, _) = valid_template();
        let raw = r#"{
            "previousblockhash": "0000000000000000000123456789abcdef0123456789abcdef0123456789abcd",
            "transactions": [],
            "coinbasevalue": 12800000000,
            "longpollid": "lp",
            "weightlimit": 1000000,
            "bits": "1d00ffff",
            "prev_nbits": "1c00ff00",
            "height": 100,
            "convergence_txids": ["aa"]
        }"#;
        let err = validate_template(&t, raw).unwrap_err();
        assert!(matches!(err, GbtError::Validation { rule: "V-06", .. }));
    }

    #[test]
    fn v06_rejects_default_res_mint() {
        let (t, _) = valid_template();
        let raw = r#"{
            "previousblockhash": "0000000000000000000123456789abcdef0123456789abcdef0123456789abcd",
            "transactions": [],
            "coinbasevalue": 12800000000,
            "longpollid": "lp",
            "weightlimit": 1000000,
            "bits": "1d00ffff",
            "prev_nbits": "1c00ff00",
            "height": 100,
            "default_res_mint": {"value": 1, "scriptPubKey": "bb"}
        }"#;
        let err = validate_template(&t, raw).unwrap_err();
        assert!(matches!(err, GbtError::Validation { rule: "V-06", .. }));
    }

    #[test]
    fn v06_accepts_null_lottery_key() {
        let (t, _) = valid_template();
        let raw = r#"{
            "previousblockhash": "0000000000000000000123456789abcdef0123456789abcdef0123456789abcd",
            "transactions": [{"data": "0100000001", "txid": "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"}],
            "coinbasevalue": 12800000000,
            "longpollid": "lp/1",
            "weightlimit": 1000000,
            "bits": "1d00ffff",
            "prev_nbits": "1c00ff00",
            "height": 100,
            "convergence_draw": null
        }"#;
        assert!(validate_template(&t, raw).is_ok());
    }

    #[test]
    fn v07_rejects_empty_longpollid() {
        let (mut t, raw) = valid_template();
        t.longpollid = String::new();
        let err = validate_template(&t, &raw).unwrap_err();
        assert!(matches!(err, GbtError::Validation { rule: "V-07", .. }));
    }

    #[test]
    fn v08_requires_weightlimit() {
        let (mut t, raw) = valid_template();
        t.weightlimit = None;
        let err = validate_template(&t, &raw).unwrap_err();
        assert!(matches!(err, GbtError::Validation { rule: "V-08", .. }));
    }

    #[test]
    fn v09_requires_prev_nbits_field_distinct_from_bits() {
        let (t, raw) = valid_template();
        validate_template(&t, &raw).unwrap();
        assert_ne!(t.bits, t.prev_nbits);
    }

    #[test]
    fn v10_returns_sha256_of_raw_json() {
        let (t, raw) = valid_template();
        let hash = validate_template(&t, &raw).unwrap();
        let expected: [u8; 32] = Sha256::digest(raw.as_bytes()).into();
        assert_eq!(hash, expected);
    }
}
