# rigor miner (minerd / rigor-miner.exe)

The miner is a **PoW search client**. The Full Node is the only consensus
authority.

## RIGOR SOLO mining

Algorithm:

```
SHA3-256("RGRPOW01" || header_80)
```

`header_80` is the canonical 80-byte header (little-endian version, time,
bits, nonce; 32-byte prev/merkle in internal byte order). No padding, no
midstate, no OpuHash mix, no fallback.

Supported SOLO path:

```
rigord getblocktemplate → copy canonical material → RGRPOW01 nonce search → submitblock
```

`getwork` is not a RIGOR SOLO path. Stratum is quarantined and is refused
as a SOLO URL.

| Surface | Function |
|---------|----------|
| Node `CheckProofOfWork` | SHA3-256(`"RGRPOW01" \|\| header_80`) |
| This `minerd` grind path | Same SHA3-256 `RGRPOW01` digest |

CLI: `-a rgrpow01` (default path). `-a sha256d` is an alias for the same
SHA3-256 function. scrypt is refused.

## Canonical GBT copy-bytes

The node GBT always includes (names from `core/src/rpc/mining.cpp`):

| Field | Role |
| --- | --- |
| `default_coinbase_scriptsig` | Exact vin[0].scriptSig: BIP34 height plus OP_0 padding |
| `default_coinbase_tx` | Witness-serialized canonical coinbase; miner copies these bytes |
| `default_coinbase_txid` / `merkleroot` | Cross-check only |
| `default_witness_commitment` | BIP141 commitment for the unmodified tx set |
| `rigor_coinbase_immutable` | Must be `true` |
| `transactions[]` | Consensus tx set |
| `coinbasevalue` | ORU vout[0] only: RFC-020 subsidy (0 at height 0; **128 ORU** for every `h ≥ 1`; no halving) plus claimable ORU fees. RES is not a coinbase prize |
| `version`, `previousblockhash`, `curtime`, `bits`, `target`, `height` | Header / job |

`--coinbase-addr` is sent as GBT `coinbase_address`. The node assembles the
canonical coinbase. The miner copies `default_coinbase_tx` and does not
rebuild subsidy, scriptSig, or a RES mint. The miner never applies a
Bitcoin-style or three-halving schedule. Lottery-era fields
(`paid_winner_txid`, `convergence_root`, `default_res_mint`) are refused.

Allowed grind fields: **nonce**, and GBT `curtime` as supplied (mutable:
`["time"]` on the node). `nBits`, prev hash, and version are copied.

## Building

```bash
cd miner
./autogen.sh
./configure
make
make check          # test_gbt_solo including SHA3 PoW vectors
```

Windows installer names it `rigor-miner.exe`. The Windows miner package
(`scripts/alpha/package_windows_miner.sh`) also ships the local Miner Dashboard
(`start-miner-dashboard.cmd` → `http://127.0.0.1:18083/`). Official wallet
installers do **not** include the miner.

## Running (SOLO)

Mainnet RPC port is **8761**. Address HRP is **`rg`** (regtest `rgrt`).

```bash
./minerd -a rgrpow01 -o http://127.0.0.1:8761 \
  -u RPCUSER -p RPCPASS \
  --coinbase-addr=rg1q...
```

`--coinbase-addr` is required and must be ORU witness v0 (`rg1q…` /
`rgrt1q…`). `--coinbase-sig` is ignored. `--no-gbt` is refused.

Without `-t` the miner reserves one logical processor for the node it
depends on, two from five processors up, and logs the count it chose.
`-t N` is taken literally, so `-t $(nproc)` will contend with `rigord`.

The node keeps `GBT_LONGPOLL_WORKER_RESERVE` RPC workers out of reach of
longpoll so `submitblock` always has one, and caps a single longpoll at
`-rpclongpolltimeout` seconds. Miners past that limit get the current
template immediately and poll instead of being pushed new work.

## Tests

- `miner/test_gbt_solo.c` — GBT parse, scriptSig/mint/witness/header, SHA3 vectors vs node `GetRigorPowHash`, OpuHash digest rejected
- `miner/fuzz_gbt_solo.c` — fail-closed malformed JSON
- `core/test/functional/mining_solo_gbt_copy_bytes.py` — live GBT → mine → `submitblock`

Facts: [`docs/audit/CODE_SSOT_PROTOCOL_FACTS_P0_097.md`](../docs/audit/CODE_SSOT_PROTOCOL_FACTS_P0_097.md).
Alignment: [`docs/security/RIGOR_SOLO_MINER_POW_ALIGNMENT_P0_098.md`](../docs/security/RIGOR_SOLO_MINER_POW_ALIGNMENT_P0_098.md).
