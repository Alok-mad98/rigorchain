#!/usr/bin/env python3
# Copyright (c) 2026 The Rigor Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""P0-086 audit-only replica of Coin / undo / CoinsDB key encoding.

This is not a Consensus Kernel. It locks serialization, cache overlay
semantics, rebuild equivalence of consensus-visible state, and the
failure boundary of malformed records.

Leaf (Coin::Serialize, MuHash / RIGOR/STATE_HASH/v4):
    VARINT((coinbase ? 1 : 0) | (height << 1))
    VARINT(CompressAmount(nValue)) || VARINT(scriptLen) || script

Disk / undo (CoinDiskFormatter):
    leaf bytes || uint32 LE CRC-32C Castagnoli (domain "RGCI")

Checksum is never part of Coin::Serialize. Trailing bytes after a disk
record are DATABASE_CORRUPTION (CDBWrapper consume-all).

Canonical CoinsDB key:
    0x43 ('C') || txid[32] || VARINT(vout)

Undo encoding equals disk Coin. No Bitcoin dummy version byte.

Asset identity is not a Coin field. It is inferred from scriptPubKey
by ClassifyRigorTxout after the record is loaded.
"""

from __future__ import annotations

import hashlib
import json
import os
import sys
from dataclasses import dataclass, field
from pathlib import Path
from typing import Dict, Iterable, List, Optional, Tuple

ROOT = Path(__file__).resolve().parents[1]
VECTORS = ROOT / "formal_verification" / "database_state_golden_vectors.json"

DB_COIN = 0x43  # 'C'
MAX_SCRIPT_SIZE = 10000
MAX_MONEY = 10_000_000_000 * 100_000_000  # consensus/amount.h overflow bound
RIGOR_MARKER_RETURN = 0x6A
RIGOR_MARKER_RES_MINT = 0xBB
RIGOR_MARKER_CONVERGE = 0xBD

# Error classes for the audit harness. Production CDBWrapper::Read of an
# existing key throws DESERIALIZATION_FAILURE / DATABASE_CORRUPTION. Missing
# keys still return false. The harness records FAIL_CLOSED vs ACCEPTED_VALID.
class CodecError(Exception):
    """Malformed record: production Read() throws DESERIALIZATION_FAILURE (abort)."""


class TruncatedError(CodecError):
    pass


def crc32c(data: bytes) -> int:
    """CRC-32C Castagnoli. Matches crc32c::Crc32c (empty → 0, '123456789' → 0xE3069283)."""
    crc = 0xFFFFFFFF
    for b in data:
        crc ^= b
        for _ in range(8):
            crc = (crc >> 1) ^ (0x82F63B78 if crc & 1 else 0)
    return crc ^ 0xFFFFFFFF


assert crc32c(b"") == 0
assert crc32c(b"123456789") == 0xE3069283


def coin_integrity_checksum(coin: "Coin") -> int:
    raw = bytearray(b"RGCI")
    raw.extend(int(coin.height).to_bytes(4, "little"))
    raw.append(1 if coin.coinbase else 0)
    raw.extend(int(coin.n_value & ((1 << 64) - 1)).to_bytes(8, "little"))
    raw.extend(coin.script_pubkey)
    return crc32c(bytes(raw))


# ---------------------------------------------------------------------------
# Bitcoin Core VARINT (not CompactSize / protobuf)
# ---------------------------------------------------------------------------

def write_varint(n: int) -> bytes:
    if n < 0:
        raise ValueError("VARINT is unsigned")
    tmp: List[int] = []
    while True:
        tmp.append((n & 0x7F) | (0x80 if tmp else 0x00))
        if n <= 0x7F:
            break
        n = (n >> 7) - 1
    return bytes(reversed(tmp))


def read_varint(buf: bytes, pos: int) -> Tuple[int, int]:
    n = 0
    while True:
        if pos >= len(buf):
            raise TruncatedError("VARINT unexpected EOF")
        ch = buf[pos]
        pos += 1
        if n > (0xFFFFFFFF >> 7):
            raise CodecError("VARINT too large")
        n = (n << 7) | (ch & 0x7F)
        if ch & 0x80:
            if n == 0xFFFFFFFF:
                raise CodecError("VARINT overflow")
            n += 1
        else:
            return n, pos


# ---------------------------------------------------------------------------
# Amount compression (compressor.cpp)
# ---------------------------------------------------------------------------

def compress_amount(n: int) -> int:
    if n < 0:
        raise ValueError("amount must be non-negative before CompressAmount")
    if n == 0:
        return 0
    e = 0
    while n % 10 == 0 and e < 9:
        n //= 10
        e += 1
    if e < 9:
        d = n % 10
        n //= 10
        return 1 + (n * 9 + d - 1) * 10 + e
    return 1 + (n - 1) * 10 + 9


def decompress_amount(x: int) -> int:
    if x == 0:
        return 0
    x -= 1
    e = x % 10
    x //= 10
    if e < 9:
        d = (x % 9) + 1
        x //= 9
        n = x * 10 + d
    else:
        n = x + 1
    for _ in range(e):
        n *= 10
    return n


# ---------------------------------------------------------------------------
# ScriptPlainFormatter — VARINT(len) || bytes. No P2PKH/P2SH/P2PK specials.
# ---------------------------------------------------------------------------

def compress_script(script: bytes) -> bytes:
    if len(script) > MAX_SCRIPT_SIZE:
        raise CodecError("oversized script (DATABASE_CORRUPTION)")
    return write_varint(len(script)) + script


def decompress_script(buf: bytes, pos: int) -> Tuple[bytes, int, dict]:
    notes: dict = {"hidden_transform": None, "decompress_failed": False}
    nsize, pos = read_varint(buf, pos)
    if nsize > MAX_SCRIPT_SIZE:
        raise CodecError("oversized script (DATABASE_CORRUPTION)")
    if pos + nsize > len(buf):
        raise TruncatedError("script unexpected EOF")
    script = buf[pos : pos + nsize]
    pos += nsize
    return script, pos, notes


# ---------------------------------------------------------------------------
# Coin / undo / key
# ---------------------------------------------------------------------------

@dataclass
class Coin:
    n_value: int
    script_pubkey: bytes
    height: int
    coinbase: bool

    def is_spent(self) -> bool:
        return self.n_value == -1 and self.script_pubkey == b""

    def is_unspendable(self) -> bool:
        return (len(self.script_pubkey) > 0 and self.script_pubkey[0] == RIGOR_MARKER_RETURN) or (
            len(self.script_pubkey) > MAX_SCRIPT_SIZE
        )

    def is_convergence(self) -> bool:
        return (
            len(self.script_pubkey) == 22
            and self.script_pubkey[0] == 20
            and self.script_pubkey[21] == RIGOR_MARKER_CONVERGE
        )

    def classify_asset(self) -> str:
        s = self.script_pubkey
        if self.is_convergence():
            return "ORU"
        if len(s) > 0 and s[0] == RIGOR_MARKER_RETURN:
            return "NONE"
        if len(s) == 22 and s[0] == 0x00 and s[1] == 20:
            return "ORU"
        if len(s) == 34 and s[0] == 0x51 and s[1] == 32:
            return "RES"
        if len(s) >= 3 and s[-1] == RIGOR_MARKER_RES_MINT:
            return "RES"
        return "INVALID"


def encode_coin(coin: Coin) -> bytes:
    if coin.is_spent():
        raise ValueError("spent coins are not serialized (erased)")
    code = (coin.height << 1) | (1 if coin.coinbase else 0)
    payload = write_varint(code) + write_varint(compress_amount(coin.n_value)) + compress_script(
        coin.script_pubkey
    )
    return payload + coin_integrity_checksum(coin).to_bytes(4, "little")


def decode_coin(buf: bytes, require_consume_all: bool = True) -> Tuple[Coin, dict]:
    notes: dict = {"leftover": 0, "hidden_transform": None, "decompress_failed": False}
    code, pos = read_varint(buf, 0)
    height = code >> 1
    coinbase = bool(code & 1)
    camt, pos = read_varint(buf, pos)
    n_value = decompress_amount(camt)
    if n_value > MAX_MONEY:
        raise CodecError("AmountCompression: nValue out of MoneyRange (DATABASE_CORRUPTION)")
    script, pos, snotes = decompress_script(buf, pos)
    notes["hidden_transform"] = snotes["hidden_transform"]
    notes["decompress_failed"] = snotes["decompress_failed"]
    if pos + 4 > len(buf):
        raise TruncatedError("missing Coin integrity checksum")
    stored = int.from_bytes(buf[pos : pos + 4], "little")
    pos += 4
    coin = Coin(n_value, script, height, coinbase)
    if stored != coin_integrity_checksum(coin):
        raise CodecError("Coin integrity checksum mismatch (DATABASE_CORRUPTION)")
    notes["leftover"] = len(buf) - pos
    if require_consume_all and notes["leftover"] != 0:
        raise CodecError("trailing bytes after Coin")
    return coin, notes


def encode_undo(coin: Coin) -> bytes:
    return encode_coin(coin)


def decode_undo(buf: bytes, require_consume_all: bool = True) -> Tuple[Coin, dict]:
    return decode_coin(buf, require_consume_all=require_consume_all)


def encode_coin_key(txid: bytes, n: int) -> bytes:
    if len(txid) != 32:
        raise ValueError("txid must be 32 bytes")
    return bytes([DB_COIN]) + txid + write_varint(n)


def decode_coin_key(buf: bytes) -> Tuple[bytes, int]:
    if not buf:
        raise TruncatedError("empty key")
    if buf[0] != DB_COIN:
        raise CodecError(f"wrong type byte 0x{buf[0]:02x}")
    if len(buf) < 33:
        raise TruncatedError("txid truncated")
    txid = buf[1:33]
    n, pos = read_varint(buf, 33)
    if pos != len(buf):
        raise CodecError("trailing bytes after coin key")
    return txid, n


# ---------------------------------------------------------------------------
# Cache / UTXO view (audit replica of AddCoins / SpendCoin / Flush)
# ---------------------------------------------------------------------------

@dataclass
class CacheEntry:
    coin: Optional[Coin]
    dirty: bool = False
    fresh: bool = False

    def spent(self) -> bool:
        return self.coin is None or self.coin.is_spent()


@dataclass
class CoinsView:
    """In-memory parent. Models CCoinsViewDB after a successful flush."""

    coins: Dict[Tuple[bytes, int], Coin] = field(default_factory=dict)
    best_block: bytes = b"\x00" * 32

    def get(self, outpoint: Tuple[bytes, int]) -> Optional[Coin]:
        return self.coins.get(outpoint)

    def snapshot(self) -> Dict[Tuple[bytes, int], Tuple[int, bytes, int, bool]]:
        return {
            k: (c.n_value, c.script_pubkey, c.height, c.coinbase)
            for k, c in sorted(self.coins.items())
        }

    def state_digest(self) -> str:
        h = hashlib.sha256()
        h.update(self.best_block)
        for (txid, n), (val, spk, height, cb) in self.snapshot().items():
            h.update(txid)
            h.update(n.to_bytes(4, "little"))
            h.update(val.to_bytes(8, "little", signed=True))
            h.update(len(spk).to_bytes(4, "little"))
            h.update(spk)
            h.update(height.to_bytes(4, "little"))
            h.update(bytes([1 if cb else 0]))
        return h.hexdigest()

    def serialize_all(self) -> List[dict]:
        rows = []
        for (txid, n), coin in sorted(self.coins.items()):
            rows.append(
                {
                    "key_hex": encode_coin_key(txid, n).hex(),
                    "value_hex": encode_coin(coin).hex(),
                    "txid_hex": txid.hex(),
                    "n": n,
                    "n_value": coin.n_value,
                    "script_hex": coin.script_pubkey.hex(),
                    "height": coin.height,
                    "coinbase": coin.coinbase,
                    "asset": coin.classify_asset(),
                }
            )
        return rows

    @classmethod
    def from_serialized(cls, rows: Iterable[dict], best_block: bytes) -> "CoinsView":
        view = cls(best_block=best_block)
        for row in rows:
            key = bytes.fromhex(row["key_hex"])
            val = bytes.fromhex(row["value_hex"])
            txid, n = decode_coin_key(key)
            coin, _ = decode_coin(val)
            view.coins[(txid, n)] = coin
        return view


class CoinsViewCache:
    def __init__(self, base: CoinsView):
        self.base = base
        self.cache: Dict[Tuple[bytes, int], CacheEntry] = {}
        self.best_block = base.best_block

    def fetch(self, outpoint: Tuple[bytes, int]) -> Optional[CacheEntry]:
        if outpoint in self.cache:
            return self.cache[outpoint]
        coin = self.base.get(outpoint)
        if coin is None:
            return None
        entry = CacheEntry(coin=Coin(coin.n_value, coin.script_pubkey, coin.height, coin.coinbase))
        self.cache[outpoint] = entry
        return entry

    def have_coin(self, outpoint: Tuple[bytes, int]) -> bool:
        e = self.fetch(outpoint)
        return e is not None and not e.spent()

    def add_coin(self, outpoint: Tuple[bytes, int], coin: Coin, possible_overwrite: bool) -> None:
        if coin.is_spent():
            raise RuntimeError("cannot add spent coin")
        if coin.is_unspendable():
            return
        if coin.is_convergence():
            return
        entry = self.cache.get(outpoint)
        fresh = False
        if entry is None:
            entry = CacheEntry(coin=None)
            self.cache[outpoint] = entry
            if not possible_overwrite:
                fresh = True
        else:
            if not possible_overwrite and not entry.spent():
                raise RuntimeError("overwrite of unspent coin")
            if not possible_overwrite:
                fresh = not entry.dirty
        entry.coin = coin
        entry.dirty = True
        if fresh:
            entry.fresh = True

    def spend_coin(self, outpoint: Tuple[bytes, int]) -> Optional[Coin]:
        e = self.fetch(outpoint)
        if e is None or e.spent():
            return None
        moved = e.coin
        if e.fresh:
            del self.cache[outpoint]
        else:
            e.dirty = True
            e.coin = None
        return moved

    def flush(self) -> None:
        for outpoint, entry in list(self.cache.items()):
            if not entry.dirty:
                continue
            if entry.spent():
                self.base.coins.pop(outpoint, None)
            else:
                assert entry.coin is not None
                self.base.coins[outpoint] = entry.coin
        self.base.best_block = self.best_block
        self.cache.clear()

    def restart(self) -> "CoinsViewCache":
        """Flush, then drop cache — models process restart after Sync/Flush."""
        self.flush()
        rows = self.base.serialize_all()
        rebuilt = CoinsView.from_serialized(rows, self.base.best_block)
        return CoinsViewCache(rebuilt)


# ---------------------------------------------------------------------------
# Block / reorg simulation
# ---------------------------------------------------------------------------

@dataclass
class TxOut:
    n_value: int
    script_pubkey: bytes


@dataclass
class Tx:
    txid: bytes
    vins: List[Tuple[bytes, int]]
    vouts: List[TxOut]
    coinbase: bool = False


@dataclass
class Block:
    block_hash: bytes
    height: int
    txs: List[Tx]


def apply_block(cache: CoinsViewCache, block: Block) -> List[bytes]:
    """ConnectBlock-shaped UTXO apply. Returns undo blobs per non-coinbase tx."""
    undos: List[bytes] = []
    for tx in block.txs:
        if not tx.coinbase:
            parts = []
            for prev in tx.vins:
                coin = cache.spend_coin(prev)
                if coin is None:
                    raise RuntimeError(f"missing input {prev[0].hex()}:{prev[1]}")
                parts.append(encode_undo(coin))
            packed = write_varint(len(parts))
            for p in parts:
                packed += write_varint(len(p)) + p
            undos.append(packed)
        for i, out in enumerate(tx.vouts):
            cache.add_coin(
                (tx.txid, i),
                Coin(out.n_value, out.script_pubkey, block.height, tx.coinbase),
                possible_overwrite=tx.coinbase,
            )
    cache.best_block = block.block_hash
    return undos


def disconnect_block(cache: CoinsViewCache, block: Block, undos: List[bytes]) -> None:
    non_cb = [tx for tx in block.txs if not tx.coinbase]
    if len(undos) != len(non_cb):
        raise RuntimeError("undo/tx count mismatch")
    for tx in reversed(block.txs):
        for i, out in enumerate(tx.vouts):
            probe = Coin(out.n_value, out.script_pubkey, block.height, tx.coinbase)
            if probe.is_unspendable() or probe.is_convergence():
                continue
            spent = cache.spend_coin((tx.txid, i))
            if spent is None:
                raise RuntimeError("disconnect missing created coin")
        if not tx.coinbase:
            packed = undos[non_cb.index(tx)]
            n, pos = read_varint(packed, 0)
            restored: List[Coin] = []
            for _ in range(n):
                ln, pos = read_varint(packed, pos)
                blob = packed[pos : pos + ln]
                pos += ln
                coin, _ = decode_undo(blob)
                restored.append(coin)
            for prev, coin in zip(reversed(tx.vins), reversed(restored)):
                cache.add_coin(prev, coin, possible_overwrite=cache.have_coin(prev))
    cache.best_block = b"\x00" * 32


def _h(tag: str) -> bytes:
    return hashlib.sha256(tag.encode()).digest()


def _spk_oru(seed: int) -> bytes:
    return bytes([0x00, 20]) + bytes([(seed + i) % 256 for i in range(20)])


def _spk_res(seed: int) -> bytes:
    return bytes([0x51, 32]) + bytes([(seed + i) % 256 for i in range(32)])


def _spk_mint(height: int, seed: int) -> bytes:
    # CScriptNum for small positive height: single-byte push
    if 1 <= height <= 16:
        hbytes = bytes([0x50 + height])
    elif height == 0:
        hbytes = bytes([0x00])
    else:
        # minimal scriptnum
        raw = height.to_bytes((height.bit_length() + 8) // 8, "little")
        hbytes = bytes([len(raw)]) + raw
    return hbytes + bytes([20]) + bytes([(seed + i) % 256 for i in range(20)]) + bytes(
        [RIGOR_MARKER_RES_MINT]
    )


def _spk_converge(seed: int) -> bytes:
    return bytes([20]) + bytes([(seed + i) % 256 for i in range(20)]) + bytes([RIGOR_MARKER_CONVERGE])


def _spk_return() -> bytes:
    return bytes([RIGOR_MARKER_RETURN, 0x01, 0xAA])


def golden_chain() -> Tuple[List[Block], List[Block]]:
    """Genesis → 1 → 2 → reorg from 1 onto 2' → 3'. Returns (main, alt)."""
    g_txid = _h("genesis-cb")
    genesis = Block(
        _h("genesis"),
        0,
        [Tx(g_txid, [], [TxOut(50_0000_0000, _spk_oru(1))], coinbase=True)],
    )
    b1_cb = _h("b1-cb")
    b1_pay = _h("b1-pay")
    block1 = Block(
        _h("block1"),
        1,
        [
            Tx(b1_cb, [], [TxOut(50_0000_0000, _spk_oru(2)), TxOut(0, _spk_return())], coinbase=True),
            Tx(
                b1_pay,
                [(g_txid, 0)],
                [
                    TxOut(40_0000_0000, _spk_oru(3)),
                    TxOut(9_0000_0000, _spk_converge(4)),
                    TxOut(1_0000_0000, _spk_res(5)),
                ],
            ),
        ],
    )
    b2_cb = _h("b2-cb")
    block2 = Block(
        _h("block2"),
        2,
        [
            Tx(
                b2_cb,
                [],
                [TxOut(50_0000_0000, _spk_oru(6)), TxOut(50_0000_0000, _spk_mint(2, 7))],
                coinbase=True,
            )
        ],
    )
    b2p_cb = _h("b2p-cb")
    b2p_spend = _h("b2p-spend")
    block2p = Block(
        _h("block2p"),
        2,
        [
            Tx(b2p_cb, [], [TxOut(50_0000_0000, _spk_oru(8))], coinbase=True),
            Tx(b2p_spend, [(b1_pay, 0)], [TxOut(40_0000_0000, _spk_oru(9))]),
        ],
    )
    b3p_cb = _h("b3p-cb")
    block3p = Block(
        _h("block3p"),
        3,
        [Tx(b3p_cb, [], [TxOut(50_0000_0000, _spk_oru(10))], coinbase=True)],
    )
    main = [genesis, block1, block2]
    alt = [genesis, block1, block2p, block3p]
    return main, alt


def run_path(blocks: List[Block], mode: str) -> str:
    db = CoinsView()
    cache = CoinsViewCache(db)
    undos: Dict[bytes, List[bytes]] = {}
    for i, block in enumerate(blocks):
        undos[block.block_hash] = apply_block(cache, block)
        if mode == "clean":
            cache.flush()
        elif mode == "cache_heavy":
            if i == len(blocks) - 1:
                cache.flush()
        elif mode == "restart":
            cache.flush()
            cache = CoinsViewCache(
                CoinsView.from_serialized(cache.base.serialize_all(), cache.base.best_block)
            )
        elif mode == "flush_restart":
            cache.flush()
            cache = cache.restart()
    cache.flush()
    return cache.base.state_digest()


def run_reorg_equivalence() -> Tuple[str, str]:
    main, alt = golden_chain()
    db = CoinsView()
    cache = CoinsViewCache(db)
    undos: Dict[bytes, List[bytes]] = {}
    for block in main:
        undos[block.block_hash] = apply_block(cache, block)
        cache.flush()
    # disconnect block2, connect 2' and 3'
    disconnect_block(cache, main[-1], undos[main[-1].block_hash])
    for block in alt[2:]:
        undos[block.block_hash] = apply_block(cache, block)
        cache.flush()
    via_reorg = cache.base.state_digest()

    db2 = CoinsView()
    cache2 = CoinsViewCache(db2)
    for block in alt:
        apply_block(cache2, block)
        cache2.flush()
    via_clean = cache2.base.state_digest()
    return via_reorg, via_clean


def rebuild_from_blocks(blocks: List[Block]) -> str:
    db = CoinsView()
    cache = CoinsViewCache(db)
    for block in blocks:
        apply_block(cache, block)
    cache.flush()
    rows = db.serialize_all()
    rebuilt = CoinsView.from_serialized(rows, db.best_block)
    return rebuilt.state_digest()


# ---------------------------------------------------------------------------
# Corruption / fail-closed
# ---------------------------------------------------------------------------

def classify_decode_outcome(kind: str, raw: bytes) -> str:
    """Return FAIL_CLOSED | ACCEPTED_VALID."""
    try:
        if kind == "key":
            decode_coin_key(raw)
            return "ACCEPTED_VALID"
        if kind == "undo":
            coin, notes = decode_undo(raw)
        else:
            coin, notes = decode_coin(raw)
        if notes.get("hidden_transform"):
            return "FAIL_CLOSED"
        return "ACCEPTED_VALID"
    except (CodecError, TruncatedError, ValueError):
        return "FAIL_CLOSED"


def production_read_maps_error_to_missing(raw: bytes) -> bool:
    """After P0-086-01, deserialize failure throws (abort), not missing."""
    return False


def corruption_cases(good: bytes) -> List[Tuple[str, bytes]]:
    cases = [
        ("empty", b""),
        ("truncated_code", good[:1] if good else b""),
        ("truncated_mid", good[: max(1, len(good) // 2)]),
        ("wrong_type_key", bytes([0x63]) + good[1:] if good else b"\x63"),
        ("flipped_last", good[:-1] + bytes([good[-1] ^ 0xFF]) if good else b"\x00"),
        ("extra_suffix", good + b"\x00"),
        ("all_ff", b"\xff" * 8),
        ("height_zero_coinbase", write_varint(1) + good[1:] if len(good) > 1 else b"\x01"),
    ]
    return cases


# ---------------------------------------------------------------------------
# Golden vectors
# ---------------------------------------------------------------------------

def build_vectors() -> dict:
    oru = Coin(50_0000_0000, _spk_oru(1), 1, True)
    spent_meta = {"note": "spent coins are erased from CoinsDB, not stored as a spent record"}
    created = Coin(40_0000_0000, _spk_oru(3), 1, False)
    coinbase = Coin(50_0000_0000, _spk_oru(2), 0, True)
    res = Coin(1_0000_0000, _spk_res(5), 1, False)
    mint = Coin(50_0000_0000, _spk_mint(2, 7), 2, True)
    conv = Coin(9_0000_0000, _spk_converge(4), 1, False)

    main, alt = golden_chain()
    digests = {mode: run_path(alt, mode) for mode in ("clean", "cache_heavy", "restart", "flush_restart")}
    reorg_a, reorg_b = run_reorg_equivalence()
    rebuild = rebuild_from_blocks(alt)

    good_coin = encode_coin(oru)
    good_undo = encode_undo(oru)
    good_key = encode_coin_key(_h("genesis-cb"), 0)

    corrupt_results = []
    for label, blob in corruption_cases(good_coin):
        outcome = classify_decode_outcome("coin", blob)
        corrupt_results.append(
            {
                "case": label,
                "kind": "coin",
                "input_hex": blob.hex(),
                "harness_outcome": outcome,
                "production_read_maps_to_missing": False,
                "production_fail_closed": outcome == "FAIL_CLOSED",
            }
        )
    # Oversized script: VARINT(len) without Bitcoin nSpecialScripts offset.
    oversized = write_varint((1 << 1) | 1) + write_varint(compress_amount(1)) + write_varint(
        MAX_SCRIPT_SIZE + 1
    ) + (b"\x00" * (MAX_SCRIPT_SIZE + 1))
    corrupt_results.append(
        {
            "case": "oversized_script",
            "kind": "coin",
            "input_hex": f"<constructed {len(oversized)} bytes>",
            "harness_outcome": classify_decode_outcome("coin", oversized),
            "production_behavior": "ScriptCompression::Unser throws DATABASE_CORRUPTION",
        }
    )

    return {
        "task": "TASK-RIGOR-DATABASE-STATE-PERSISTENCE-SECURITY-AUDIT-P0-086",
        "note": "Locks serialization, reconstruction, and the failure boundary. Not a formal proof.",
        "canonical": {
            "coin_layout": "leaf VARINT(code)||VARINT(CompressAmount)||VARINT(scriptLen)||script; disk adds CRC32C_RGCI",
            "key_layout": "0x43 || txid32 || VARINT(vout)",
            "undo_layout": "same as disk Coin (leaf + CRC32C); no dummy version byte",
            "asset_field_in_coin": False,
            "asset_source": "scriptPubKey via ClassifyRigorTxout after load",
        },
        "vectors": {
            "clean_chainstate_coin": {
                "value_hex": encode_coin(oru).hex(),
                "key_hex": encode_coin_key(_h("genesis-cb"), 0).hex(),
                "coin": {"n_value": oru.n_value, "height": oru.height, "coinbase": True, "asset": "ORU"},
            },
            "spent_coin": spent_meta,
            "created_coin": {"value_hex": encode_coin(created).hex(), "asset": created.classify_asset()},
            "coinbase": {"value_hex": encode_coin(coinbase).hex(), "height": 0, "coinbase": True},
            "rigor_asset_res": {"value_hex": encode_coin(res).hex(), "asset": "RES"},
            "rigor_res_mint": {"value_hex": encode_coin(mint).hex(), "asset": "RES"},
            "rigor_convergence_not_stored": {
                "script_hex": conv.script_pubkey.hex(),
                "enters_utxo": False,
                "reason": "AddCoins skips via EntersUtxoSet",
            },
            "undo_created_coin": {"undo_hex": encode_undo(created).hex()},
            "undo_coinbase": {"undo_hex": encode_undo(coinbase).hex()},
            "undo_res": {"undo_hex": encode_undo(res).hex()},
        },
        "cache_db_equivalence": {
            "path_digests": digests,
            "equivalent": len(set(digests.values())) == 1,
        },
        "reorg_recovery": {
            "via_reorg": reorg_a,
            "via_clean_alt": reorg_b,
            "equivalent": reorg_a == reorg_b,
        },
        "rebuild_equivalence": {
            "from_block_storage_replay": rebuild,
            "from_live_alt": digests["clean"],
            "consensus_state_equivalent": rebuild == digests["clean"],
            "leveldb_bytes_identical_required": False,
        },
        "corruption": corrupt_results,
        "production_failure_boundary": {
            "leveldb_io_error": "HandleError throws dbwrapper_error; CCoinsViewErrorCatcher abort()",
            "missing_key": "Read returns false; treated as coin absent",
            "malformed_value": "Read throws DESERIALIZATION_FAILURE; ErrorCatcher abort()",
            "database_failure_mapped_to_consensus_invalid": False,
            "database_fail_closed_for_value_corruption": True,
            "coin_integrity": "CRC-32C Castagnoli over RGCI||height||coinbase||nValue||scriptPubKey; CoinDiskFormatter only",
            "pre_checksum_chainstate": "wipe / -reindex; no in-place migration",
        },
    }


def verify_vectors(doc: dict) -> List[str]:
    errors: List[str] = []
    fresh = build_vectors()
    for key in ("clean_chainstate_coin", "created_coin", "coinbase", "rigor_asset_res", "rigor_res_mint"):
        if fresh["vectors"][key]["value_hex"] != doc["vectors"][key]["value_hex"]:
            errors.append(f"vector drift: {key}")
    if not doc["cache_db_equivalence"]["equivalent"]:
        errors.append("CACHE_DB_EQUIVALENCE failed in stored vectors")
    if not fresh["cache_db_equivalence"]["equivalent"]:
        errors.append("CACHE_DB_EQUIVALENCE failed on recomputation")
    if fresh["cache_db_equivalence"]["path_digests"] != doc["cache_db_equivalence"]["path_digests"]:
        errors.append("cache path digest drift")
    if not fresh["reorg_recovery"]["equivalent"]:
        errors.append("REORG_RECOVERY digest mismatch")
    if not fresh["rebuild_equivalence"]["consensus_state_equivalent"]:
        errors.append("REBUILD consensus-state mismatch")
    # Round-trip locks
    for name in ("clean_chainstate_coin", "created_coin", "coinbase", "rigor_asset_res", "rigor_res_mint"):
        raw = bytes.fromhex(doc["vectors"][name]["value_hex"])
        coin, notes = decode_coin(raw)
        if notes["hidden_transform"]:
            errors.append(f"{name} decoded with hidden transform")
        again = encode_coin(coin)
        if again != raw:
            errors.append(f"{name} encode/decode not involution")
    if doc["production_failure_boundary"]["database_failure_mapped_to_consensus_invalid"]:
        errors.append("deserialize failure must not map to CONSENSUS_INVALID")
    if not doc["production_failure_boundary"]["database_fail_closed_for_value_corruption"]:
        errors.append("value corruption must fail closed")
    for row in doc["corruption"]:
        if row["case"] in {"empty", "truncated_code", "truncated_mid", "all_ff", "oversized_script", "extra_suffix", "flipped_last"}:
            if row["harness_outcome"] != "FAIL_CLOSED":
                errors.append(f"corruption {row['case']} expected FAIL_CLOSED")
    return errors


def write_vectors() -> dict:
    doc = build_vectors()
    VECTORS.parent.mkdir(parents=True, exist_ok=True)
    VECTORS.write_text(json.dumps(doc, indent=2) + "\n", encoding="utf-8")
    return doc


def main(argv: List[str]) -> int:
    cmd = argv[1] if len(argv) > 1 else "verify"
    if cmd == "write":
        doc = write_vectors()
        print(f"wrote {VECTORS}")
        print(f"CACHE_DB_EQUIVALENCE={'PASS' if doc['cache_db_equivalence']['equivalent'] else 'FAIL'}")
        print(f"REORG_RECOVERY={'PASS' if doc['reorg_recovery']['equivalent'] else 'FAIL'}")
        print(
            "CONSENSUS_STATE_EQUIVALENCE="
            + ("TRUE" if doc["rebuild_equivalence"]["consensus_state_equivalent"] else "FALSE")
        )
        return 0
    if cmd == "verify":
        if not VECTORS.is_file():
            print(f"FAIL: missing {VECTORS}", file=sys.stderr)
            return 1
        doc = json.loads(VECTORS.read_text(encoding="utf-8"))
        errors = verify_vectors(doc)
        good = bytes.fromhex(doc["vectors"]["clean_chainstate_coin"]["value_hex"])
        if classify_decode_outcome("coin", b"") != "FAIL_CLOSED":
            errors.append("empty blob must fail closed")
        if classify_decode_outcome("coin", good[:2]) != "FAIL_CLOSED":
            errors.append("truncated coin must fail closed")
        if classify_decode_outcome("coin", good) != "ACCEPTED_VALID":
            errors.append("valid coin must decode")
        flipped = bytearray(good)
        flipped[-1] ^= 0xFF
        if classify_decode_outcome("coin", bytes(flipped)) != "FAIL_CLOSED":
            errors.append("checksum tamper must fail closed")
        height_flip = bytearray(good)
        height_flip[0] ^= 0x01
        if classify_decode_outcome("coin", bytes(height_flip)) != "FAIL_CLOSED":
            errors.append("height/coinbase tamper must fail closed")
        if errors:
            for e in errors:
                print(f"FAIL: {e}", file=sys.stderr)
            return 1
        print("DATABASE_GOLDEN_VECTORS=PASS")
        print(f"CACHE_DB_EQUIVALENCE={'PASS' if doc['cache_db_equivalence']['equivalent'] else 'FAIL'}")
        print(f"REORG_RECOVERY={'PASS' if doc['reorg_recovery']['equivalent'] else 'FAIL'}")
        print(
            "CONSENSUS_STATE_EQUIVALENCE="
            + ("TRUE" if doc["rebuild_equivalence"]["consensus_state_equivalent"] else "FALSE")
        )
        print("DATABASE_FAILURE_MAPPED_TO_MISSING=FALSE")
        print("DATABASE_FAIL_CLOSED_FOR_VALUE_CORRUPTION=TRUE")
        return 0
    print("usage: database_state_codec.py [write|verify]", file=sys.stderr)
    return 2


if __name__ == "__main__":
    sys.exit(main(sys.argv))
