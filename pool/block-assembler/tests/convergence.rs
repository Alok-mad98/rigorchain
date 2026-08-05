use block_assembler::compute_convergence_root;

fn txid(byte: u8) -> String {
    // Display-order hex for a symmetric byte pattern (endian-agnostic sanity).
    hex::encode([byte; 32])
}

#[test]
fn zero_root_when_fewer_than_two_txids() {
    assert_eq!(compute_convergence_root(&[]), [0u8; 32]);
    assert_eq!(compute_convergence_root(&[txid(1)]), [0u8; 32]);
}

#[test]
fn root_is_order_independent() {
    let a = txid(0x02);
    let b = txid(0x01);
    let r1 = compute_convergence_root(&[a.clone(), b.clone()]);
    let r2 = compute_convergence_root(&[b, a]);
    assert_eq!(r1, r2);
    assert_ne!(r1, [0u8; 32]);
}
