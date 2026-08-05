use serde::{Deserialize, Serialize};
use serde_json::Value;

/// Raw GBT transaction entry from `getblocktemplate`.
#[derive(Clone, Debug, Serialize, Deserialize, PartialEq)]
pub struct GbtTransaction {
    pub data: String,
    pub txid: String,
    pub hash: Option<String>,
    pub depends: Option<Vec<String>>,
    pub fee: Option<i64>,
    pub sigops: Option<u32>,
    pub weight: Option<u32>,
}

/// One member of the parent block's convergence set.
#[derive(Clone, Debug, Serialize, Deserialize, PartialEq)]
pub struct ConvergenceDrawCandidate {
    /// Display-order txid, as `uint256::GetHex` writes it.
    pub txid: String,
    /// Draw weight f_i — the convergence amount in Tharns.
    pub weight: u64,
    /// The 32-byte witness v1 RES program this convergence declared (A-5 R1).
    pub res_program: String,
    /// σ_i as the node computed it; display order.
    #[serde(default)]
    pub sigma: Option<String>,
    /// Whether σ_i < T·f_i.
    #[serde(default)]
    pub eligible: bool,
}

/// The candidate the node resolved as winner. Absent when nobody was eligible.
#[derive(Clone, Debug, Serialize, Deserialize, PartialEq)]
pub struct ConvergenceDrawWinner {
    pub index: u32,
    pub txid: String,
    pub weight: u64,
    pub res_program: String,
}

/// The draw a template PAYS, resolved over the set its parent committed (SECURITY-001-A8).
///
/// Distinct from [`GbtTemplate::convergence_txids`], which is the set the template *commits*
/// for the next block to draw. Every field here is a function of the parent block alone, so
/// nothing the pool does while assembling can change any of it.
///
/// All uint256 values are RPC display order and must be reversed before entering the σ
/// preimage; use `rigor_convergence_root::display_hex_to_native`.
#[derive(Clone, Debug, Serialize, Deserialize, PartialEq)]
pub struct ConvergenceDraw {
    pub source_blockhash: String,
    pub source_height: u64,
    /// The σ seed — the parent block hash. Identical to `source_blockhash`; named for the role.
    pub sigma_seed: String,
    /// Height fed to σ and the height of this template: `source_height + 1`.
    pub payout_height: u64,
    pub convergence_root: String,
    /// Compact nBits of the parent header; the value a RES mint proof must report.
    pub target_prev: String,
    pub res_probability_target: String,
    /// Commitment over the eligible subset; this template's CVGE coinbase output carries it.
    pub eligible_root: String,
    #[serde(default)]
    pub candidates: Vec<ConvergenceDrawCandidate>,
    #[serde(default)]
    pub winner: Option<ConvergenceDrawWinner>,
}

/// Parsed `getblocktemplate` response (RIGOR-extended fields).
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
    /// The set this template COMMITS for the next block to draw — not the set it pays.
    ///
    /// A-9-C: spans the whole draw window, so most members are transactions of earlier blocks
    /// and are absent from `transactions`.
    #[serde(default)]
    pub convergence_txids: Vec<String>,
    /// C over `convergence_txids` — the exact root the coinbase scriptSig must carry.
    ///
    /// Served by the node because the window makes C unrecoverable from this template's body
    /// alone. Optional only so that pre-A9C fixtures still parse.
    #[serde(default)]
    pub convergence_root: Option<String>,
    /// The resolved parent draw this template PAYS. Optional only so that pre-A8 fixtures
    /// still parse; every consumer on the mint path must refuse to proceed without it.
    #[serde(default)]
    pub convergence_draw: Option<ConvergenceDraw>,
    pub default_convergence_commitment: Option<String>,
    pub prev_nbits: String,
}

impl GbtTemplate {
    pub fn from_json_value(value: &Value) -> Result<Self, serde_json::Error> {
        serde_json::from_value(value.clone())
    }
}
