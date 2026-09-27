//! GBT display encoding vs legacy literal decode (remediation).

use rigor_convergence_root::{
    compute_convergence_root, compute_convergence_root_with_encoding, TxidEncoding,
};

fn legacy_literal_root(txid_hex: &[String]) -> [u8; 32] {
    compute_convergence_root_with_encoding(txid_hex, TxidEncoding::HashHex)
}

#[test]
fn gbt_display_differs_from_legacy_literal_on_live_txids() {
    let txids = vec![
        "d74106f6d63f519754105a31e2cde838dcb84a6935460bd47538bbe18b4909a4".into(),
        "da949e156da6ecef781d751c3dced8500400e4b393c6bf8fa318b89bd769ba1d".into(),
    ];
    let canonical = compute_convergence_root(&txids);
    let legacy = legacy_literal_root(&txids);
    assert_ne!(canonical, legacy, "display-native must replace literal decode");
}

#[test]
fn golden_cr002_uses_hashhex_corpus_encoding() {
    let txids = vec![
        "8cb9012517c817fead650287d61bdd9c68803b6bf9c64133dcab3e65b5a50cb9".into(),
        "41f758f2e5cc078d3795b4fc0cb60c2d735fa92cc020572bdc982dd2d564d11b".into(),
    ];
    let root =
        compute_convergence_root_with_encoding(&txids, TxidEncoding::HashHex);
    assert_eq!(
        hex::encode(root),
        "2b6d935d9f5e35a64b0ef4907aeb835c4e6f25ddf031a96f80785235507ed35c"
    );
}
