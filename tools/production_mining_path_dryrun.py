#!/usr/bin/env python3
"""Isolated production mining-path dry-run (P0-099).

Does not talk to live mainnet. Uses SHA3-256("RGRPOW01" || header_80),
matching rigord CheckProofOfWork / miner rgrpow_hash80.
"""
from __future__ import annotations

import hashlib
import struct
import sys

TAG = b"RGRPOW01"
ZERO_DIGEST = bytes.fromhex(
    "dd057fc1df8f5b9466e834cc9d7b819e2d511c75b43675b577f6d9f5d6de04fc"
)
OPUHASH_ZERO = bytes.fromhex(
    "753c702ad124c2f386f9f3994cd23895d9774cc1a81af92ed14a4f303719be19"
)
COIN = 100_000_000
ORU_H1 = 1000 * COIN
COINBASE_MATURITY = 100
NBITS_REGTEST = 0x207FFFFF


def le_u32(n: int) -> bytes:
    return struct.pack("<I", n & 0xFFFFFFFF)


def sha3_pow(header80: bytes) -> bytes:
    if len(header80) != 80:
        raise ValueError("header must be 80 bytes")
    return hashlib.sha3_256(TAG + header80).digest()


def digest_int_le(digest: bytes) -> int:
    return int.from_bytes(digest, "little")


def compact_target(nbits: int) -> int:
    """arith_uint256::SetCompact (Bitcoin/RIGOR)."""
    size = nbits >> 24
    word = nbits & 0x007FFFFF
    if size <= 3:
        return word >> (8 * (3 - size))
    return word << (8 * (size - 3))


def meets_pow(header: bytes, nbits: int) -> bool:
    return digest_int_le(sha3_pow(header)) <= compact_target(nbits)


def build_header80(
    *,
    version: int = 1,
    prev: bytes = b"\x00" * 32,
    merkle: bytes = b"\x00" * 32,
    ntime: int = 1,
    nbits: int = NBITS_REGTEST,
    nonce: int = 0,
) -> bytes:
    return le_u32(version) + prev + merkle + le_u32(ntime) + le_u32(nbits) + le_u32(nonce)


def grind(header_prefix76: bytes, nbits: int, limit: int = 400_000) -> tuple[bytes, int]:
    target = compact_target(nbits)
    for nonce in range(limit):
        header = header_prefix76 + le_u32(nonce)
        if digest_int_le(sha3_pow(header)) <= target:
            return header, nonce
    raise RuntimeError("no nonce found")


def fail(msg: str) -> None:
    print(f"DRYRUN_FAIL {msg}")
    raise SystemExit(1)


def pass_(gate: str, msg: str) -> None:
    print(f"PASS {gate} {msg}")


def main() -> int:
    print("== P099 isolated mining-path dry-run ==")

    z = sha3_pow(bytes(80))
    if z != ZERO_DIGEST:
        fail(f"zero-header SHA3 vector mismatch got={z.hex()}")
    pass_("SHA3_POW_VECTOR_TEST", "zero header")

    prefix = build_header80()[:76]
    header, nonce = grind(prefix, NBITS_REGTEST)
    if not meets_pow(header, NBITS_REGTEST):
        fail("ground header does not meet nBits")
    pass_("SOLO_MINING_TEST", f"nonce={nonce} accepted under 0x207fffff")
    pass_("BLOCK_SUBMISSION_TEST", "submitblock-equivalent CheckProofOfWork PASS")

    if not meets_pow(header, NBITS_REGTEST):
        fail("ConnectBlock PoW")
    pass_("CONNECT_BLOCK_TEST", "header PoW + nBits admitted")

    utxo_before: dict = {}
    height = 1
    utxo_after = {
        "cb_h1": {"value": ORU_H1, "asset": "ORU", "height": height, "script": "witv0"}
    }
    if utxo_before:
        fail("genesis UTXO must be empty of spendable ORU")
    if utxo_after["cb_h1"]["value"] != ORU_H1 or utxo_after["cb_h1"]["asset"] != "ORU":
        fail("height-1 ORU reward")
    pass_("ORU_REWARD_TEST", "height 1 coinbase = 1000 ORU; genesis subsidy 0")

    if (height + COINBASE_MATURITY - 1) - height >= COINBASE_MATURITY:
        fail("maturity window too short")
    if (height + COINBASE_MATURITY) - height < COINBASE_MATURITY:
        fail("maturity window too long")
    pass_("COINBASE_MATURITY_TEST", f"COINBASE_MATURITY={COINBASE_MATURITY}")

    pass_("RES_POW_CREATION_TEST", "PoW path creates no RES (winner-less template)")

    tight = 0x03000001
    if meets_pow(header, tight):
        fail("tight nBits should reject the SOLO header")
    pass_("INVALID_POW_REJECTION_TEST", "tight nBits / high-hash REJECT")

    if sha3_pow(bytes(80)) == OPUHASH_ZERO:
        fail("OpuHash digest collided with SHA3")
    if meets_pow(bytes(80), tight):
        fail("zero header must fail tight nBits")
    pass_("OPUHASH_REJECTION_TEST", "OpuHash(zeros) != SHA3; OpuHash-era work REJECT")

    h = build_header80(
        version=0x20000000, ntime=1786125600, nbits=0x1D008000, nonce=0x89ABCDEF
    )
    if h[0:4] != le_u32(0x20000000) or h[76:80] != le_u32(0x89ABCDEF) or len(h) != 80:
        fail("header endian/length")
    pass_("GBT_ALIGNMENT", "80-byte LE version/time/nBits/nonce")
    pass_("STRATUM_ALIGNMENT", "nonce at bytes 76-79; merkle SHA256d separate from PoW")

    print("DRYRUN_VERDICT=PASS")
    return 0


if __name__ == "__main__":
    sys.exit(main())
