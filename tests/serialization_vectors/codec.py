"""C++-faithful block/tx codec for .

Matches:
  serialize.h ReadCompactSize / WriteCompactSize (canonical)
  transaction.h SerializeTransaction / UnserializeTransaction
  block.h CBlockHeader / CBlock SERIALIZE_METHODS

Not a consensus change. Not Coq extraction.
"""
from __future__ import annotations

import io
import struct
from dataclasses import dataclass, field
from typing import List, Optional, Tuple


class DecodeError(ValueError):
    pass


def write_compact_size(n: int) -> bytes:
    if n < 253:
        return struct.pack("B", n)
    if n <= 0xFFFF:
        return struct.pack("<BH", 253, n)
    if n <= 0xFFFFFFFF:
        return struct.pack("<BI", 254, n)
    return struct.pack("<BQ", 255, n)


def read_compact_size(buf: io.BytesIO) -> int:
    raw = buf.read(1)
    if len(raw) != 1:
        raise DecodeError("truncated CompactSize")
    ch = raw[0]
    if ch < 253:
        return ch
    if ch == 253:
        body = buf.read(2)
        if len(body) != 2:
            raise DecodeError("truncated CompactSize")
        n = struct.unpack("<H", body)[0]
        if n < 253:
            raise DecodeError("non-canonical ReadCompactSize()")
        return n
    if ch == 254:
        body = buf.read(4)
        if len(body) != 4:
            raise DecodeError("truncated CompactSize")
        n = struct.unpack("<I", body)[0]
        if n < 0x10000:
            raise DecodeError("non-canonical ReadCompactSize()")
        return n
    body = buf.read(8)
    if len(body) != 8:
        raise DecodeError("truncated CompactSize")
    n = struct.unpack("<Q", body)[0]
    if n < 0x100000000:
        raise DecodeError("non-canonical ReadCompactSize()")
    return n


def read_exact(buf: io.BytesIO, n: int, what: str) -> bytes:
    data = buf.read(n)
    if len(data) != n:
        raise DecodeError(f"truncated {what}")
    return data


def read_bytes(buf: io.BytesIO) -> bytes:
    n = read_compact_size(buf)
    return read_exact(buf, n, "byte vector")


def write_bytes(data: bytes) -> bytes:
    return write_compact_size(len(data)) + data


@dataclass
class COutPoint:
    txid: bytes  # 32 little-endian wire bytes
    n: int

    def encode(self) -> bytes:
        return self.txid + struct.pack("<I", self.n)

    @classmethod
    def decode(cls, buf: io.BytesIO) -> "COutPoint":
        return cls(read_exact(buf, 32, "outpoint hash"), struct.unpack("<I", read_exact(buf, 4, "outpoint n"))[0])


@dataclass
class CTxIn:
    prevout: COutPoint
    script_sig: bytes
    n_sequence: int
    witness_stack: List[bytes] = field(default_factory=list)

    def encode_base(self) -> bytes:
        return self.prevout.encode() + write_bytes(self.script_sig) + struct.pack("<I", self.n_sequence)

    @classmethod
    def decode_base(cls, buf: io.BytesIO) -> "CTxIn":
        prevout = COutPoint.decode(buf)
        script = read_bytes(buf)
        seq = struct.unpack("<I", read_exact(buf, 4, "nSequence"))[0]
        return cls(prevout, script, seq, [])


@dataclass
class CTxOut:
    n_value: int
    script_pubkey: bytes

    def encode(self) -> bytes:
        return struct.pack("<q", self.n_value) + write_bytes(self.script_pubkey)

    @classmethod
    def decode(cls, buf: io.BytesIO) -> "CTxOut":
        value = struct.unpack("<q", read_exact(buf, 8, "nValue"))[0]
        return cls(value, read_bytes(buf))


@dataclass
class CTransaction:
    n_version: int
    vin: List[CTxIn]
    vout: List[CTxOut]
    n_locktime: int

    def has_witness(self) -> bool:
        return any(txin.witness_stack for txin in self.vin)

    def encode(self) -> bytes:
        flags = 1 if self.has_witness() else 0
        out = struct.pack("<i", self.n_version)
        if flags:
            out += write_compact_size(0)
            out += struct.pack("B", flags)
        out += write_compact_size(len(self.vin))
        for txin in self.vin:
            out += txin.encode_base()
        out += write_compact_size(len(self.vout))
        for txout in self.vout:
            out += txout.encode()
        if flags & 1:
            for txin in self.vin:
                out += write_compact_size(len(txin.witness_stack))
                for item in txin.witness_stack:
                    out += write_bytes(item)
        out += struct.pack("<I", self.n_locktime)
        return out

    @classmethod
    def decode(cls, buf: io.BytesIO) -> "CTransaction":
        version = struct.unpack("<i", read_exact(buf, 4, "nVersion"))[0]
        vin_count = read_compact_size(buf)
        flags = 0
        vin: List[CTxIn] = []
        vout: List[CTxOut] = []
        if vin_count == 0:
            flag_b = buf.read(1)
            if len(flag_b) != 1:
                raise DecodeError("truncated witness flags")
            flags = flag_b[0]
            if flags != 0:
                vin_count = read_compact_size(buf)
                vin = [CTxIn.decode_base(buf) for _ in range(vin_count)]
                vout_count = read_compact_size(buf)
                vout = [CTxOut.decode(buf) for _ in range(vout_count)]
        else:
            vin = [CTxIn.decode_base(buf) for _ in range(vin_count)]
            vout_count = read_compact_size(buf)
            vout = [CTxOut.decode(buf) for _ in range(vout_count)]
        if flags & 1:
            flags ^= 1
            for txin in vin:
                n_stack = read_compact_size(buf)
                txin.witness_stack = [read_bytes(buf) for _ in range(n_stack)]
            if not any(txin.witness_stack for txin in vin):
                raise DecodeError("Superfluous witness record")
        if flags:
            raise DecodeError("Unknown transaction optional data")
        locktime = struct.unpack("<I", read_exact(buf, 4, "nLockTime"))[0]
        return cls(version, vin, vout, locktime)


@dataclass
class CBlockHeader:
    n_version: int
    hash_prev: bytes
    hash_merkle: bytes
    n_time: int
    n_bits: int
    n_nonce: int

    def encode(self) -> bytes:
        return (
            struct.pack("<i", self.n_version)
            + self.hash_prev
            + self.hash_merkle
            + struct.pack("<I", self.n_time)
            + struct.pack("<I", self.n_bits)
            + struct.pack("<I", self.n_nonce)
        )

    @classmethod
    def decode(cls, buf: io.BytesIO) -> "CBlockHeader":
        return cls(
            struct.unpack("<i", read_exact(buf, 4, "nVersion"))[0],
            read_exact(buf, 32, "hashPrevBlock"),
            read_exact(buf, 32, "hashMerkleRoot"),
            struct.unpack("<I", read_exact(buf, 4, "nTime"))[0],
            struct.unpack("<I", read_exact(buf, 4, "nBits"))[0],
            struct.unpack("<I", read_exact(buf, 4, "nNonce"))[0],
        )


@dataclass
class CBlock:
    header: CBlockHeader
    vtx: List[CTransaction]

    def encode(self) -> bytes:
        out = self.header.encode()
        out += write_compact_size(len(self.vtx))
        for tx in self.vtx:
            out += tx.encode()
        return out

    @classmethod
    def decode(cls, buf: io.BytesIO) -> "CBlock":
        header = CBlockHeader.decode(buf)
        n = read_compact_size(buf)
        vtx = [CTransaction.decode(buf) for _ in range(n)]
        return cls(header, vtx)


def decode_hex(hex_str: str) -> bytes:
    if len(hex_str) % 2:
        raise DecodeError("odd hex length")
    try:
        return bytes.fromhex(hex_str)
    except ValueError as exc:
        raise DecodeError("invalid hex") from exc


def try_decode_block(hex_str: str) -> Tuple[Optional[CBlock], Optional[str], int, Optional[bool]]:
    try:
        raw = decode_hex(hex_str)
    except DecodeError as exc:
        return None, str(exc), 0, None
    buf = io.BytesIO(raw)
    try:
        block = CBlock.decode(buf)
    except DecodeError as exc:
        return None, str(exc), 0, None
    leftover = len(raw) - buf.tell()
    encoded = block.encode()
    return block, None, leftover, encoded == raw


def try_decode_tx(hex_str: str) -> Tuple[Optional[CTransaction], Optional[str], int, Optional[bool]]:
    try:
        raw = decode_hex(hex_str)
    except DecodeError as exc:
        return None, str(exc), 0, None
    buf = io.BytesIO(raw)
    try:
        tx = CTransaction.decode(buf)
    except DecodeError as exc:
        return None, str(exc), 0, None
    leftover = len(raw) - buf.tell()
    encoded = tx.encode()
    return tx, None, leftover, encoded == raw


def header_summary(block: CBlock) -> dict:
    return {
        "nVersion": block.header.n_version,
        "hashPrevBlock": block.header.hash_prev.hex(),
        "hashMerkleRoot": block.header.hash_merkle.hex(),
        "nTime": block.header.n_time,
        "nBits": hex(block.header.n_bits),
        "nNonce": block.header.n_nonce,
        "tx_count": len(block.vtx),
        "has_witness": any(tx.has_witness() for tx in block.vtx),
    }


def tx_summary(tx: CTransaction) -> dict:
    return {
        "nVersion": tx.n_version,
        "nLockTime": tx.n_locktime,
        "vin": len(tx.vin),
        "vout": len(tx.vout),
        "has_witness": tx.has_witness(),
        "values": [out.n_value for out in tx.vout],
        "script_lens": [len(out.script_pubkey) for out in tx.vout],
    }


def noncanonical_vtx_count(block_hex: str) -> str:
    """Rewrite vtx CompactSize 1 as fd 01 00 (illegal in ReadCompactSize)."""
    raw = bytearray(bytes.fromhex(block_hex))
    if len(raw) < 81:
        raise ValueError("block too short")
    if raw[80] != 1:
        raise ValueError(f"expected vtx CompactSize 1, got {raw[80]}")
    return (bytes(raw[:80]) + b"\xfd\x01\x00" + bytes(raw[81:])).hex()


def superfluous_witness_tx() -> str:
    """nVersion=1, dummy vin, flags=1, empty vin/vout, empty witness, locktime=0."""
    return (
        struct.pack("<i", 1)
        + write_compact_size(0)
        + b"\x01"
        + write_compact_size(0)
        + write_compact_size(0)
        + struct.pack("<I", 0)
    ).hex()
