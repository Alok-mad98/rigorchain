# Rigor miner (minerd / rigor-miner.exe)

The miner is a **PoW search client**. The Full Node is the only consensus
authority. The supported SOLO path is:

```
rigord getblocktemplate → copy canonical material → OpuHash nonce search → submitblock
```

`getwork` is not a RIGOR SOLO path. Stratum is quarantined for a later pool
task and is refused as a SOLO URL.

## Canonical GBT copy-bytes (P041)

The node GBT always includes (names from `core/src/rpc/mining.cpp`):

| Field | Role |
| --- | --- |
| `default_coinbase_scriptsig` | Exact vin[0].scriptSig: BIP34 height ‖ OP_PUSHBYTES_32(C) ‖ OP_PUSHBYTES_32(paid-winner) |
| `paid_winner_txid` | Third scriptSig push as RPC hex; zeros when this template does not mint |
| `default_coinbase_tx` | Witness-serialized canonical coinbase; miner copies these bytes |
| `default_coinbase_txid` / `merkleroot` | Cross-check only |
| `default_res_mint` | Optional `{value, scriptPubKey}` already inside `default_coinbase_tx` when a winner exists |
| `default_witness_commitment` | BIP141 commitment for the unmodified tx set (coinbase wtxid = 0) |
| `rigor_coinbase_immutable` | Must be `true` |
| `transactions[]` | Consensus tx set (`data` / `txid` / `hash`) |
| `coinbasevalue` | ORU vout[0] only |
| `version`, `previousblockhash`, `curtime`, `bits`, `target`, `height` | Header / job |

`--coinbase-addr` is sent as GBT `coinbase_address`. The node assembles the
canonical coinbase. The miner copies `default_coinbase_tx` and does not
rebuild mint, winner, subsidy, or scriptSig.

Missing a required field, a 2-push scriptSig, OP_n mint encoding (e.g. `OP_5`
instead of `OP_PUSHBYTES_1 0x05`), or a P2PKH/v1 payout **refuses the job**.
There is no Bitcoin or legacy-RIGR fallback.

Allowed grind fields: **nonce**, and GBT `curtime` as supplied (mutable:
`["time"]` on the node). `nBits`, prev hash, and version are copied.

CLI name `-a sha256d` means: hash the 80-byte header with SHA256d, then the
frozen OpuHash mix. scrypt is disabled for SOLO.

## Building

```bash
cd miner
./autogen.sh
./configure
make
make check          # test_gbt_solo
```

Binary: `./minerd` (Windows installer names it `rigor-miner.exe`).

## Running (SOLO)

```bash
./minerd -a sha256d -o http://127.0.0.1:8761 \
  -u RPCUSER -p RPCPASS \
  --coinbase-addr=rgrt1q... \
  -t 4
```

`--coinbase-addr` is required and must be ORU witness v0 (`rg1q…` / `rgrt1q…`).
`--coinbase-sig` is ignored. `--no-gbt` is refused.

## Tests

- `miner/test_gbt_solo.c` — GBT parse, scriptSig/mint/witness/header/OpuHash
- `miner/fuzz_gbt_solo.c` — fail-closed malformed JSON
- `core/test/functional/mining_solo_gbt_copy_bytes.py` — live GBT → mine → `submitblock`
