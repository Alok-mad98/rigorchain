use serde::Deserialize;
use serde_json::Value;

/// Raw GBT transaction entry from `getblocktemplate`.
#[derive(Clone, Debug, serde::Serialize, Deserialize, PartialEq)]
pub struct GbtTransaction {
    pub data: String,
    pub txid: String,
    pub hash: Option<String>,
    pub depends: Option<Vec<String>>,
    pub fee: Option<i64>,
    pub sigops: Option<u32>,
    pub weight: Option<u32>,
}

/// Parsed shipping `getblocktemplate` response (RFC-020 ORU-only).
///
/// Lottery keys (`paid_winner_txid`, `default_res_mint`, `default_pcorb_carrier`,
/// `convergence_root`, `convergence_draw`, `convergence_txids`,
/// `default_convergence_commitment`) are **not** fields here. If the node emits
/// any of them non-null, validation V-06 rejects the raw JSON.
#[derive(Clone, Debug, Deserialize, PartialEq)]
pub struct GbtTemplate {
    pub version: Option<u32>,
    pub previousblockhash: String,
    pub transactions: Vec<GbtTransaction>,
    pub coinbaseaux: Option<Value>,
    pub coinbasevalue: i64,
    pub longpollid: String,
    pub target: Option<String>,
    pub mintime: Option<u64>,
    pub mutable: Option<Vec<String>>,
    pub noncerange: Option<String>,
    pub sigoplimit: Option<u64>,
    pub sizelimit: Option<u64>,
    pub weightlimit: Option<u64>,
    pub curtime: Option<u64>,
    pub bits: String,
    pub height: u64,
    pub default_witness_commitment: Option<String>,
    /// Compact nBits of the parent header (node still emits this).
    pub prev_nbits: String,
    /// Exact coinbase vin[0].scriptSig bytes (BIP34 height + OP_0). Always emitted by shipping rigord.
    #[serde(default)]
    pub default_coinbase_scriptsig: Option<String>,
    /// Full coinbase tx hex when `coinbase_address` was given to GBT.
    #[serde(default)]
    pub default_coinbase_tx: Option<String>,
    #[serde(default)]
    pub default_coinbase_txid: Option<String>,
    #[serde(default)]
    pub merkleroot: Option<String>,
    #[serde(default)]
    pub rigor_coinbase_immutable: Option<bool>,
    #[serde(default)]
    pub coinbase_address: Option<String>,
}

impl GbtTemplate {
    pub fn from_json_value(value: &Value) -> Result<Self, serde_json::Error> {
        serde_json::from_value(value.clone())
    }
}
