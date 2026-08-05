/// 512-bit unsigned integer for cross-multiplication (RFC-000 §13).
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub struct Uint512 {
    pub limbs: [u64; 8],
}

impl Uint512 {
    pub fn zero() -> Self {
        Self { limbs: [0; 8] }
    }

    pub fn from_uint256_le(native: &[u8; 32]) -> Self {
        let mut limbs = [0u64; 8];
        for i in 0..4 {
            let mut limb = 0u64;
            for j in 0..8 {
                limb |= (native[i * 8 + j] as u64) << (j * 8);
            }
            limbs[i] = limb;
        }
        Self { limbs }
    }

    pub fn from_u64(v: u64) -> Self {
        let mut limbs = [0u64; 8];
        limbs[0] = v;
        Self { limbs }
    }

    pub fn cmp(&self, other: &Self) -> std::cmp::Ordering {
        for i in (0..8).rev() {
            match self.limbs[i].cmp(&other.limbs[i]) {
                std::cmp::Ordering::Equal => continue,
                ord => return ord,
            }
        }
        std::cmp::Ordering::Equal
    }

    pub fn lt(&self, other: &Self) -> bool {
        self.cmp(other) == std::cmp::Ordering::Less
    }

    pub fn eq(&self, other: &Self) -> bool {
        self.cmp(other) == std::cmp::Ordering::Equal
    }
}

/// Full 256×256 → 512 multiply (little-endian limbs), matching `Mul256x256Full`.
pub fn mul256x256_full(a: &Uint512, b: &Uint512) -> Uint512 {
    let mut p = [0u64; 8];
    for i in 0..4 {
        for j in 0..4 {
            let prod = u128::from(a.limbs[i]) * u128::from(b.limbs[j]);
            let mut k = i + j;
            let mut acc = u128::from(p[k]) + (prod as u64 as u128);
            p[k] = acc as u64;
            let mut carry = (prod >> 64) + (acc >> 64);
            while carry != 0 {
                k += 1;
                assert!(k < 8, "uint512 overflow");
                acc = u128::from(p[k]) + carry;
                p[k] = acc as u64;
                carry = acc >> 64;
            }
        }
    }
    Uint512 { limbs: p }
}

/// `ConvergenceDrawInequalityHolds`: H < T · f (512-bit exact).
pub fn convergence_draw_inequality_holds(
    h_native: &[u8; 32],
    res_probability_target_native: &[u8; 32],
    weight: u64,
) -> bool {
    if weight < 1 {
        return false;
    }
    let t_zero = res_probability_target_native.iter().skip(1).all(|&b| b == 0);
    if res_probability_target_native[0] == 0 && t_zero {
        return false;
    }

    let left = Uint512::from_uint256_le(h_native);
    let t = Uint512::from_uint256_le(res_probability_target_native);
    let f = Uint512::from_u64(weight);
    let right = mul256x256_full(&t, &f);
    left.lt(&right)
}

/// Argmin tie-break: H_i * f_winner < H_winner * f_i, else smaller txid wins.
pub fn draw_precedes(
    h_i: &[u8; 32],
    f_i: u64,
    h_winner: &[u8; 32],
    f_winner: u64,
    txid_i: &[u8; 32],
    txid_winner: &[u8; 32],
) -> bool {
    let hi = Uint512::from_uint256_le(h_i);
    let hw = Uint512::from_uint256_le(h_winner);
    let lhs = mul256x256_full(&hi, &Uint512::from_u64(f_winner));
    let rhs = mul256x256_full(&hw, &Uint512::from_u64(f_i));
    if lhs.lt(&rhs) {
        return true;
    }
    if lhs.eq(&rhs) {
        return txid_i < txid_winner;
    }
    false
}
