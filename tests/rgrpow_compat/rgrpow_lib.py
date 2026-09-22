"""Shared RGRPOW01 helpers for the compatibility suite."""
from __future__ import annotations

import hashlib
import json
import os
import struct
import subprocess
import sys
from pathlib import Path
from typing import Any

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
VECTORS_PATH = HERE / "vectors" / "rgrpow_vectors.json"
NATIVE = HERE / "native"
BUILD = HERE / ".build"

DOMAIN_TAG = b"RGRPOW01"
HEADER_LEN = 80
PREIMAGE_LEN = 88


def load_vectors() -> dict[str, Any]:
    return json.loads(VECTORS_PATH.read_text())


def vector_by_id(vid: str) -> dict[str, Any]:
    for v in load_vectors()["vectors"]:
        if v["id"] == vid:
            return v
    raise KeyError(vid)


def opuhash(header80: bytes) -> bytes:
    if len(header80) != HEADER_LEN:
        raise ValueError(f"header must be {HEADER_LEN} bytes")
    return hashlib.sha3_256(DOMAIN_TAG + header80).digest()


def sha3_256(msg: bytes) -> bytes:
    return hashlib.sha3_256(msg).digest()


def rpc_hex_to_internal(hex64: str) -> bytes:
    raw = bytes.fromhex(hex64)
    if len(raw) != 32:
        raise ValueError("expected 32-byte hash")
    return raw[::-1]


def serialize_header(
    version: int,
    prev_internal: bytes,
    merkle_internal: bytes,
    ntime: int,
    nbits: int,
    nonce: int,
) -> bytes:
    if len(prev_internal) != 32 or len(merkle_internal) != 32:
        raise ValueError("prev/merkle must be 32 bytes")
    return (
        struct.pack("<I", version)
        + prev_internal
        + merkle_internal
        + struct.pack("<I", ntime)
        + struct.pack("<I", nbits)
        + struct.pack("<I", nonce)
    )


def set_compact(nbits: int) -> bytes:
    """Bitcoin SetCompact → 32-byte little-endian uint256."""
    nsize = nbits >> 24
    nword = nbits & 0x007FFFFF
    target = bytearray(32)
    if nsize <= 3:
        nword >>= 8 * (3 - nsize)
        target[0:4] = struct.pack("<I", nword)
    else:
        shift = nsize - 3
        raw = struct.pack("<I", nword) + bytes(28)
        out = bytearray(32)
        for i, b in enumerate(raw):
            dest = i + shift
            if dest < 32:
                out[dest] = b
        target[:] = out
    return bytes(target)


def uint256_le(digest: bytes) -> int:
    return int.from_bytes(digest, "little")


def meets_target(digest: bytes, target: bytes) -> bool:
    return uint256_le(digest) <= uint256_le(target)


def _run(cmd: list[str]) -> str:
    r = subprocess.run(cmd, check=True, capture_output=True, text=True)
    return r.stdout.strip().lower()


def ensure_native_bins() -> dict[str, Path]:
    BUILD.mkdir(exist_ok=True)
    c_bin = BUILD / "hash80_c"
    cpp_bin = BUILD / "hash80_cpp"
    miner_c = ROOT / "miner" / "rgrpow.c"
    keccak_cpp = ROOT / "core" / "src" / "rigor_kernel" / "keccak.cpp"
    core_inc = ROOT / "core" / "src"

    if not c_bin.exists() or c_bin.stat().st_mtime < max(
        miner_c.stat().st_mtime, (NATIVE / "hash80_c.c").stat().st_mtime
    ):
        subprocess.run(
            [
                "gcc",
                "-O2",
                "-o",
                str(c_bin),
                str(NATIVE / "hash80_c.c"),
                str(miner_c),
                "-I",
                str(ROOT / "miner"),
            ],
            check=True,
        )
    if not cpp_bin.exists() or cpp_bin.stat().st_mtime < max(
        keccak_cpp.stat().st_mtime, (NATIVE / "hash80_cpp.cpp").stat().st_mtime
    ):
        subprocess.run(
            [
                "g++",
                "-O2",
                "-std=c++17",
                "-o",
                str(cpp_bin),
                str(NATIVE / "hash80_cpp.cpp"),
                str(keccak_cpp),
                "-I",
                str(core_inc),
            ],
            check=True,
        )
    return {"c": c_bin, "cpp": cpp_bin}


def hash_c(header80: bytes) -> bytes:
    bins = ensure_native_bins()
    return bytes.fromhex(_run([str(bins["c"]), header80.hex()]))


def hash_cpp(header80: bytes) -> bytes:
    bins = ensure_native_bins()
    return bytes.fromhex(_run([str(bins["cpp"]), "rgrpow", header80.hex()]))


def sha3_cpp(msg: bytes) -> bytes:
    bins = ensure_native_bins()
    return bytes.fromhex(_run([str(bins["cpp"]), "sha3", msg.hex() if msg else ""]))


def keccak256_cpp(msg: bytes) -> bytes:
    bins = ensure_native_bins()
    hex_in = msg.hex() if msg else ""
    # empty hex is valid for keccak256("")
    args = [str(bins["cpp"]), "keccak256", hex_in if hex_in else ""]
    if not hex_in:
        # parse_hex("") yields empty vector
        args = [str(bins["cpp"]), "keccak256", ""]
    return bytes.fromhex(_run(args))


def hash_rust(header80: bytes) -> bytes | None:
    """Same Sha3_256 construction as share-validator::rgrpow_hash80."""
    src = NATIVE / "rust_cli"
    if not (src / "Cargo.toml").is_file():
        return None
    env = os.environ.copy()
    env["CARGO_TARGET_DIR"] = str(BUILD / "rust-target")
    try:
        built = subprocess.run(
            ["cargo", "build", "--quiet", "--release", "--manifest-path", str(src / "Cargo.toml")],
            check=True,
            capture_output=True,
            text=True,
            env=env,
            timeout=180,
        )
    except FileNotFoundError:
        return None
    except (subprocess.CalledProcessError, subprocess.TimeoutExpired) as exc:
        err = getattr(exc, "stderr", None) or str(exc)
        raise RuntimeError(f"rust hash80_rust build failed:\n{err}") from exc
    bin_path = BUILD / "rust-target" / "release" / "hash80_rust"
    if not bin_path.is_file():
        raise RuntimeError("rust hash80_rust binary missing after cargo build")
    _ = built
    return bytes.fromhex(_run([str(bin_path), header80.hex()]))
