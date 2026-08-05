/// Minimum convergence txs in F before draw may run (RFC-015 freeze).
pub const CONVERGENCE_DRAW_MIN_TXS_FOR_RES: usize = 1;

/// Minimum ORU convergence amount per candidate (1.00 ORU = COIN atoms).
pub const MIN_CONVERGENCE_AMOUNT_SAT: u64 = 100_000_000;

/// Constitutional inequality RHS (RFC-014 v1.1 / A-6): `floor(2^240 / MIN_CONVERGENCE_AMOUNT)`,
/// uint256 native little-endian bytes.
pub const RES_PROBABILITY_TARGET: [u8; 32] = [
    0x3c, 0x1a, 0x1c, 0x5f, 0x5a, 0x58, 0x60, 0x5d, 0x4f, 0x53, 0x4f, 0x0f, 0x9f, 0xae, 0xcd, 0x4a, 0x83, 0x70, 0x3f, 0xbf, 0x73, 0x18, 0x61, 0xc4, 0x1d, 0xf3, 0x2a, 0x00, 0x00, 0x00, 0x00, 0x00
];

/// `prev_nbits` used by golden fixtures. Since Sigma-PoW-01 the parent's compact target is no
/// longer a σ input — the parent header commits it — so this only populates witness and
/// template fields.
pub const GOLDEN_PREV_NBITS: u32 = 0x1d00ffff;

/// Golden suite tag (P-03 certification pending).
pub const REPLAY_CERT_TAG: &str = "mainnet-freeze-rfc015-2026-05-26";

/// Authorization token TTL after block find (seconds).
pub const AUTHORIZATION_TTL_SECS: u64 = 30;

/// `equivalence_claimed` remains false until P-03 golden certification passes.
pub const EQUIVALENCE_CLAIMED: bool = false;
