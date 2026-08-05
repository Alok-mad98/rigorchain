//! P-03 ConvergenceRoot golden vectors (CR-000..CR-011).

use rigor_convergence_root::{
    compute_convergence_root_with_encoding, native_to_hash_hex, TxidEncoding,
};

fn load_vectors() -> serde_json::Value {
    let path = concat!(
        env!("CARGO_MANIFEST_DIR"),
        "/../../formal_verification/phase3_golden_vectors.json"
    );
    let raw = std::fs::read_to_string(path).expect("phase3_golden_vectors.json");
    serde_json::from_str(&raw).expect("parse golden json")
}

#[test]
fn convergence_root_golden_vectors() {
    let data = load_vectors();
    for v in data["vectors"].as_array().expect("vectors") {
        if v["module"] != "ConvergenceRoot" {
            continue;
        }
        let txids: Vec<String> = v["input"]["txids_hex"]
            .as_array()
            .unwrap_or(&vec![])
            .iter()
            .map(|x| x.as_str().unwrap().to_string())
            .collect();
        let encoding = if v["input"].get("encoding").and_then(|e| e.as_str()) == Some("sethex") {
            TxidEncoding::SetHex
        } else {
            TxidEncoding::HashHex
        };
        let root = compute_convergence_root_with_encoding(&txids, encoding);
        let expected = v["expected"]["convergence_root"].as_str().unwrap();
        assert_eq!(native_to_hash_hex(&root), expected, "vector {}", v["id"]);
    }
}
