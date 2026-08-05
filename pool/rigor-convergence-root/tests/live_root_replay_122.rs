//! 122-block live testnet root replay (TASK-MINING-019 corpus).

use rigor_convergence_root::{
    compute_convergence_root, compute_convergence_root_for_rigord_f, native_to_hash_hex,
};

#[derive(serde::Deserialize)]
struct Fixture {
    count: usize,
    blocks: Vec<BlockRow>,
}

#[derive(serde::Deserialize)]
struct BlockRow {
    height: u64,
    convergence_root_set_size: usize,
    rigord_root_hex: String,
    gbt_convergence_txids: Vec<String>,
}

fn load_fixture() -> Fixture {
    let path = concat!(
        env!("CARGO_MANIFEST_DIR"),
        "/tests/fixtures/testnet_root_replay_122.json"
    );
    let raw = std::fs::read_to_string(path).expect("fixture");
    serde_json::from_str(&raw).expect("parse fixture")
}

#[test]
fn assembler_winner_engine_rigord_root_122_blocks() {
    let fx = load_fixture();
    assert_eq!(fx.count, 122);
    assert_eq!(fx.blocks.len(), 122);

    for row in &fx.blocks {
        let rigord_f = row.convergence_root_set_size;
        let txids = &row.gbt_convergence_txids;

        let root_rigord_rule = compute_convergence_root_for_rigord_f(rigord_f, txids);
        let root_gbt_path = if rigord_f < 2 {
            [0u8; 32]
        } else {
            compute_convergence_root(txids)
        };

        assert_eq!(
            native_to_hash_hex(&root_rigord_rule),
            row.rigord_root_hex,
            "height {} rigord_rule",
            row.height
        );
        assert_eq!(
            native_to_hash_hex(&root_gbt_path),
            row.rigord_root_hex,
            "height {} gbt_path",
            row.height
        );
        assert_eq!(root_rigord_rule, root_gbt_path, "height {} unified", row.height);
    }
}
