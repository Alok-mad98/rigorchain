use crate::coinbase::{assemble_coinbase, CoinbaseAssembly, CoinbaseConfig};
use crate::convergence::hash256;
use crate::error::{AssemblerError, AssemblerResult};
use crate::merkle::{build_merkle_branches, display_hash_hex_to_native, txid_from_tx_bytes};
use crate::placeholder::build_body_mint_placeholder;
use crate::types::{BodyMintPlaceholder, CoinbaseInfo, CommitmentInfo};
use rigor_gbt::{GbtRecord, TemplateRecord};

/// Full assembly artifact returned by the block assembler pipeline.
#[derive(Clone, Debug, PartialEq)]
pub struct AssemblyArtifact {
    pub coinbase: CoinbaseAssembly,
    pub coinbase_info: CoinbaseInfo,
    pub commitments: CommitmentInfo,
    pub body_mint: BodyMintPlaceholder,
    pub merkle_branch: Vec<String>,
    pub merkle_root: [u8; 32],
    pub version: u32,
    pub nbits: String,
    pub ntime: u64,
    pub prevhash: String,
    pub block_target: String,
    pub assembly_hash: [u8; 32],
}

pub struct AssemblerConfig {
    pub payout_script: Vec<u8>,
    /// Matches stratum session extranonce1 length (default 4).
    pub extranonce1_size: u32,
    pub extranonce2_size: u32,
    /// Deprecated (Option A): ignored. Pool never reserves RES mint placeholders.
    pub mint_required: bool,
}

/// P2PKH to an all-zero HASH160. Provably unspendable; retained **only** so the
/// legacy `Default` impl keeps existing devtest fixtures compiling, and so startup
/// gates can recognise and reject it by value.
pub const UNSPENDABLE_DEVTEST_PAYOUT_SCRIPT: [u8; 25] = [
    0x76, 0xa9, 0x14, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x88, 0xac,
];

impl Default for AssemblerConfig {
    /// **Devtest only — burns the coinbase.** The payout script is an all-zero
    /// P2PKH (see [`UNSPENDABLE_DEVTEST_PAYOUT_SCRIPT`]). Production callers must
    /// use [`AssemblerConfig::with_payout_script`] and gate startup on
    /// [`AssemblerConfig::validate_production`].
    fn default() -> Self {
        Self {
            payout_script: UNSPENDABLE_DEVTEST_PAYOUT_SCRIPT.to_vec(),
            extranonce1_size: 4,
            extranonce2_size: 4,
            mint_required: false,
        }
    }
}

impl AssemblerConfig {
    /// Build a config from an explicit payout script, rejecting unspendable and
    /// non-standard forms up front (C-6).
    pub fn with_payout_script(payout_script: Vec<u8>) -> AssemblerResult<Self> {
        crate::payout_script::validate_payout_script(&payout_script)?;
        Ok(Self {
            payout_script,
            extranonce1_size: 4,
            extranonce2_size: 4,
            mint_required: false,
        })
    }

    /// Same as [`Self::with_payout_script`] from a hex string (env/config path).
    pub fn with_payout_script_hex(hex_str: &str) -> AssemblerResult<Self> {
        Self::with_payout_script(crate::payout_script::payout_script_from_hex(hex_str)?)
    }

    /// C-6: refuse to serve jobs whose coinbase would burn the reward.
    pub fn validate_production(&self) -> AssemblerResult<()> {
        if self.payout_script == UNSPENDABLE_DEVTEST_PAYOUT_SCRIPT {
            return Err(AssemblerError::Assembly(
                "C-6: refusing to start — coinbase payout_script is the unspendable devtest \
                 default (all-zero P2PKH); every block reward would be burned"
                    .into(),
            ));
        }
        crate::payout_script::validate_payout_script(&self.payout_script)?;
        if self.extranonce1_size == 0 || self.extranonce1_size > 8 {
            return Err(AssemblerError::Assembly(format!(
                "extranonce1_size {} out of range 1..=8",
                self.extranonce1_size
            )));
        }
        if self.extranonce2_size == 0 || self.extranonce2_size > 8 {
            return Err(AssemblerError::Assembly(format!(
                "extranonce2_size {} out of range 1..=8",
                self.extranonce2_size
            )));
        }
        Ok(())
    }
}

pub fn assemble_from_template(
    template: &TemplateRecord,
    config: &AssemblerConfig,
) -> AssemblerResult<AssemblyArtifact> {
    use rigor_gbt::TemplateState;
    if template.template_state != TemplateState::Active {
        return Err(AssemblerError::TemplateNotReady(format!(
            "template_state {:?} is not ACTIVE",
            template.template_state
        )));
    }

    assemble_from_gbt(&template.gbt, config, template.record_id.0.as_bytes())
}

pub fn assemble_from_gbt(
    gbt: &GbtRecord,
    config: &AssemblerConfig,
    record_seed: &[u8],
) -> AssemblerResult<AssemblyArtifact> {
    let tx_version = gbt.version.unwrap_or(0x20000000);
    let coinbase = assemble_coinbase(
        gbt,
        &CoinbaseConfig {
            payout_script: &config.payout_script,
            extranonce1_size: config.extranonce1_size,
            extranonce2_size: config.extranonce2_size,
            tx_version,
        },
    )?;

    let mut tx_hashes: Vec<[u8; 32]> = Vec::with_capacity(gbt.transactions.len() + 1);
    // Merkle leaf must be txid (witness stripped), matching Bitcoin Core CheckMerkleRoot.
    tx_hashes.push(txid_from_tx_bytes(&coinbase.coinbase_raw));

    for tx in &gbt.transactions {
        let raw = hex::decode(&tx.data)
            .map_err(|e| AssemblerError::Assembly(format!("tx decode: {e}")))?;
        // Prefer GBT `txid` (display order) — never `hash` (wtxid on segwit Core).
        if !tx.txid.is_empty() {
            match display_hash_hex_to_native(&tx.txid) {
                Ok(native) => {
                    tx_hashes.push(native);
                    continue;
                }
                Err(e) => {
                    return Err(AssemblerError::Assembly(format!("gbt txid: {e}")));
                }
            }
        }
        tx_hashes.push(txid_from_tx_bytes(&raw));
    }

    if config.mint_required {
        return Err(AssemblerError::Assembly(
            "RFC-020: mint_required is refused on the ORU-only assemble path".into(),
        ));
    }
    let body_mint = build_body_mint_placeholder(
        gbt.transactions.len(),
        false,
        record_seed,
    );
    debug_assert!(!body_mint.reserved);
    debug_assert!(!body_mint.mint_required);

    let (merkle_root, merkle_branch) = build_merkle_branches(&tx_hashes);

    let mut assembly_preimage = Vec::new();
    assembly_preimage.extend_from_slice(&coinbase.coinbase_raw);
    for tx in &gbt.transactions {
        if let Ok(raw) = hex::decode(&tx.data) {
            assembly_preimage.extend_from_slice(&raw);
        }
    }
    let assembly_hash = hash256(&assembly_preimage);

    let coinbase_info = CoinbaseInfo {
        coinb1_hex: coinbase.coinb1_hex.clone(),
        coinb2_hex: coinbase.coinb2_hex.clone(),
        extranonce2_size: coinbase.extranonce2_size,
        scriptsig_hex: hex::encode(&coinbase.scriptsig),
        convergence_root_hex: crate::convergence::native_to_hash_hex(&coinbase.convergence_root),
        payout_script_hex: hex::encode(&config.payout_script),
        coinbase_value: gbt.coinbasevalue,
    };

    let commitments = CommitmentInfo {
        cvge_script_hex: coinbase.cvge_script.as_ref().map(hex::encode),
        rigor_script_hex: hex::encode(&coinbase.rigor_script),
        witness_commitment_script_hex: coinbase
            .witness_commitment_script
            .as_ref()
            .map(hex::encode),
        prev_nbits_hex: gbt.prev_nbits.clone(),
    };

    Ok(AssemblyArtifact {
        coinbase,
        coinbase_info,
        commitments,
        body_mint,
        merkle_branch,
        merkle_root,
        version: tx_version,
        nbits: gbt.bits.clone(),
        ntime: gbt.curtime.unwrap_or(0),
        prevhash: gbt.previousblockhash.clone(),
        block_target: gbt.bits.clone(),
        assembly_hash,
    })
}

#[cfg(test)]
mod tests {
    use super::*;

    /// C-6 regression: the default config must be recognised as a burn and refused
    /// for production, while an explicitly configured script is accepted.
    #[test]
    fn default_config_is_refused_for_production() {
        let err = AssemblerConfig::default()
            .validate_production()
            .unwrap_err()
            .to_string();
        assert!(err.contains("C-6"), "unexpected: {err}");
        assert!(err.contains("burned"), "unexpected: {err}");
    }

    #[test]
    fn explicit_payout_script_is_accepted_and_burn_is_rejected() {
        let mut good = vec![0x00, 0x14];
        good.extend_from_slice(&[0xa5u8; 20]);
        let cfg = AssemblerConfig::with_payout_script(good.clone()).unwrap();
        cfg.validate_production().unwrap();
        assert_eq!(cfg.payout_script, good);

        assert!(
            AssemblerConfig::with_payout_script(UNSPENDABLE_DEVTEST_PAYOUT_SCRIPT.to_vec()).is_err()
        );
        assert!(AssemblerConfig::with_payout_script_hex(&hex::encode(good)).is_ok());
        assert!(AssemblerConfig::with_payout_script_hex("").is_err());
    }

    #[test]
    fn extranonce2_size_is_range_checked() {
        let mut good = vec![0x00, 0x14];
        good.extend_from_slice(&[0xa5u8; 20]);
        let mut cfg = AssemblerConfig::with_payout_script(good).unwrap();
        cfg.extranonce2_size = 0;
        assert!(cfg.validate_production().is_err());
        cfg.extranonce2_size = 9;
        assert!(cfg.validate_production().is_err());
        cfg.extranonce2_size = 4;
        cfg.validate_production().unwrap();
    }
}
