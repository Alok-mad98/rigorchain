//! Same construction as pool/share-validator/src/rgrpow.rs::rgrpow_hash80.
use sha3::{Digest, Sha3_256};

fn rgrpow_hash80(header: &[u8; 80]) -> [u8; 32] {
    let mut hasher = Sha3_256::new();
    hasher.update(b"RGRPOW01");
    hasher.update(header);
    hasher.finalize().into()
}

fn main() {
    let arg = std::env::args().nth(1).expect("header hex");
    let bytes = hex_decode(&arg);
    assert_eq!(bytes.len(), 80, "header must be 80 bytes");
    let mut header = [0u8; 80];
    header.copy_from_slice(&bytes);
    let digest = rgrpow_hash80(&header);
    for b in digest {
        print!("{b:02x}");
    }
    println!();
}

fn hex_decode(s: &str) -> Vec<u8> {
    assert!(s.len() % 2 == 0);
    (0..s.len())
        .step_by(2)
        .map(|i| u8::from_str_radix(&s[i..i + 2], 16).expect("hex"))
        .collect()
}
