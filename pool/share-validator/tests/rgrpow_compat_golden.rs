use serde::Deserialize;
use share_validator::rgrpow_hash80;
use std::path::PathBuf;

#[derive(Deserialize)]
struct File {
    vectors: Vec<Vector>,
}

#[derive(Deserialize)]
struct Vector {
    id: String,
    header_hex: String,
    digest_hex: String,
}

#[test]
fn golden_vectors_match_share_validator() {
    let path = PathBuf::from(env!("CARGO_MANIFEST_DIR"))
        .join("../../tests/rgrpow_compat/vectors/rgrpow_vectors.json");
    let raw = std::fs::read_to_string(&path).unwrap_or_else(|e| panic!("{}: {e}", path.display()));
    let file: File = serde_json::from_str(&raw).expect("vectors json");
    for v in file.vectors {
        let header_vec = hex::decode(&v.header_hex).expect("header hex");
        let mut header = [0u8; 80];
        header.copy_from_slice(&header_vec);
        let got = rgrpow_hash80(&header);
        let want = hex::decode(&v.digest_hex).expect("digest hex");
        assert_eq!(
            got.as_slice(),
            want.as_slice(),
            "share-validator mismatch on vector {}",
            v.id
        );
    }
}
