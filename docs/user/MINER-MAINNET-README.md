# RIGOR v0.2 Mainnet — SOLO mining guide

This document is for **RIGOR Mainnet**. Do not use Testnet addresses or Testnet node settings with this guide.

Mainnet mining is **CPU-based SOLO mining**. The miner obtains work from a local RIGOR node over **localhost RPC**, then submits a candidate with `submitblock`. It is not a pool. GPU mining, ASIC mining, Stratum, and public pool mining are not supported.

The miner program is network-neutral software. **This guide documents only the same-host setup:** `rigord` and `rigor-miner` on one machine, talking over loopback RPC.

Official release: [RIGOR v0.2 — Mainnet](https://github.com/rigorchain/rigorchain/releases/tag/v0.2-mainnet)

---

## Mainnet identity

| Item | Value | Role |
| --- | --- | --- |
| Network | RIGOR Mainnet | Chain identity |
| Genesis | `f8aceb50d974f3157c7bc2dfffab28dee0c97d99e4e3ec931b84e06862fdd717` | Chain identity |
| Address prefix | `rg1q` | Wallet / miner payout |
| Mainnet P2P port | `8760` | Node-to-node networking |
| Public DNS seed | `seed.rigorchain.com` | Node discovery only |
| Local RPC | `127.0.0.1:8761` | Same-host node-to-miner only |
| PoW | OpuHash | Consensus PoW name |
| PoW domain | `RGRPOW01` | Consensus identifier |
| Algorithm identifier | `rgrpow01` | Miner `-a` value |

`seed.rigorchain.com` is the public Mainnet P2P DNS seed used for node discovery. It is **not** the miner RPC endpoint.

The RIGOR Mainnet RPC interface is intended for local node-to-miner communication and listens on the loopback interface (`127.0.0.1:8761`). Do not expose RPC port 8761 to the public Internet.

---

## Network connectivity

Read this section even if you skip the rest.

| Surface | Address | Used by | Not used for |
| --- | --- | --- | --- |
| P2P | port `8760` | `rigord` talking to other Mainnet nodes | Miner `getblocktemplate` |
| DNS seed | `seed.rigorchain.com` | Node discovery so `rigord` can find peers | Miner RPC, Stratum, wallet RPC |
| RPC | `127.0.0.1:8761` | Miner on the **same host** as `rigord` | Public access |

- Do **not** point the miner at port `8760`.
- Do **not** point the miner at `seed.rigorchain.com`.
- Do **not** use the DNS seed as an RPC, mining, or Stratum server.

`127.0.0.1` means the miner and `rigord` are running on the same host.

### Same-machine setup (documented)

If `rigord` and `rigor-miner` run on the same computer:

```text
http://127.0.0.1:8761
```

is the intended RPC URL for `-o` / `--url`.

### Different-machine setup (not documented here)

This README documents local-node / local-miner only. Keep RPC restricted to localhost. This is not a remote-RPC configuration guide.

---

## Mining architecture

```text
RIGOR Wallet
     |
     | rg1q payout address
     v
Local RIGOR Mainnet Node
     |
     | localhost RPC :8761
     v
RIGOR CPU SOLO Miner
     |
     | getblocktemplate
     | OpuHash
     | submitblock
     v
Local RIGOR Node
     |
     | P2P :8760
     v
RIGOR Mainnet
```

The DNS seed is used by the **node** for P2P discovery. It is not part of the miner RPC path.

---

## What the miner does

The published `rigor-miner` / `rigor-miner.exe` is a PoW search client. The full node is the only consensus authority.

```text
getblocktemplate (rules: ["segwit"])
        ↓
node builds the canonical coinbase
        ↓
miner copies the 80-byte header and coinbase bytes
        ↓
CPU search: SHA3-256("RGRPOW01" || 80-byte Bitcoin V25-derived header)
        ↓
submitblock
        ↓
node validates the candidate
```

- Block identifier is `SHA256d(header)`.
- The miner does **not** rebuild subsidy, scriptSig, or a RES mint.
- `--coinbase-addr` is sent as GBT `coinbase_address`. The node assembles the coinbase; the miner copies `default_coinbase_tx`.
- Allowed grind fields: **nonce**, and the node's GBT time when the template marks time mutable.
- `getwork` is not a RIGOR SOLO path. Stratum URLs are refused for SOLO.
- CLI `-a rgrpow01` is the default algorithm. `-a sha256d` is an alias for the same SHA3-256 function. scrypt is refused.

Finding a block is not guaranteed in any fixed time. Do not treat mining as a promised reward schedule.

---

## Prerequisites

1. A **RIGOR Mainnet** wallet/node from the same release (Windows or Linux). Official wallet packages do not include the miner.
2. Before mining, allow the Mainnet node to synchronize with the current chain tip. A node still in Initial Block Download may not provide current mining work.
3. Same-host RPC at `127.0.0.1:8761`. Do not expose port **8761** to the public Internet.
4. A payout address from **your** Mainnet wallet. It must begin with `rg1q` (ORU witness v0). Do not use a `rg1p` RES address.
5. The Mainnet miner package from the release above.
6. Spare CPU cores. Leave cores for the wallet/node.

The miner must not use a Testnet node.

---

## RPC credentials

The miner authenticates with `-u` / `--user` and `-p` / `--pass` (or `-O` / `--userpass`). The node URL option is `-o` / `--url`.

The official Mainnet wallet starts the node with localhost RPC (`-rpcallowip=127.0.0.1`) and writes local credential files under the wallet **data** directory. Use those generated values. Do not invent a shared public password. Do not publish `rpcuser` / `rpcpassword`.

Typical locations:

- Official Windows wallet: `data` next to `rigor-wallet.exe` (portable datadir). Look for `rpc-auth.conf` or the sidecar files the miner launcher reads after the node has started once.
- Official Linux wallet: `<extract>/data/rpc-auth.conf` after the first `./bin/rigord` or `./bin/rigor-wallet` start.

If you write `rigor.conf` yourself, keep it **inside the data directory**, not next to the wallet executable. Include at least:

```text
server=1
rpcallowip=127.0.0.1
```

plus the same `rpcuser` / `rpcpassword` you will pass to the miner. Fully quit the wallet/node, then start it again so the file is loaded.

Do not put `testnet=1` in this file.

---

## Workflow

```text
RIGOR Mainnet wallet
        ↓
create/copy a rg1q receive address you control
        ↓
let the Mainnet node synchronize to tip
        ↓
confirm local RPC on 127.0.0.1:8761
        ↓
start rigor-miner with -o http://127.0.0.1:8761, -u, -p, --coinbase-addr
        ↓
miner requests getblocktemplate (segwit)
        ↓
CPU hashing (RGRPOW01)
        ↓
submitblock
        ↓
node accepts or rejects the candidate
        ↓
node announces the block on P2P port 8760
```

---

## Verify downloads

From [RIGOR v0.2 — Mainnet](https://github.com/rigorchain/rigorchain/releases/tag/v0.2-mainnet), download `SHA256SUMS.txt` and the miner archives. Expected miner checksums:

| File | SHA256 |
| --- | --- |
| `rigor-v0.2-miner-windows-x86_64.exe` | `395c97c739107abb1392973eed491b25ffb90ee3a7279b3f021c991f114f392b` |
| `rigor-v0.2-miner-windows-x86_64.zip` | `4744409419cbf26cf4287e7ef40419ac54dcd3ace8d106bb81aeba23af7afb08` |
| `rigor-v0.2-miner-linux-x86_64.tar.gz` | `875c3662233dc7c4616064b7b80ec6b920fda9d4c85be0b72171126ddf16c6fd` |

```bash
sha256sum -c SHA256SUMS.txt
```

---

## Windows x86_64

Published names: `rigor-v0.2-miner-windows-x86_64.exe` (installer) and `rigor-v0.2-miner-windows-x86_64.zip`.

1. Install or extract the miner package. The mining binary is `rigor-miner.exe`.
2. Start the **Mainnet** wallet first on the **same computer**. Wait until it is synchronized. Receive addresses must start with `rg1q`.
3. Copy a `rg1q` receive address from that wallet.
4. Prefer `rigor-miner.cmd` in the miner folder. It loads local RPC credentials, uses `http://127.0.0.1:8761`, and runs:

```text
rigor-miner.exe -a rgrpow01 -q -t <threads> --no-getwork --coinbase-addr="rg1q<your-mainnet-payout-address>" -o "http://127.0.0.1:8761" -u "<user>" -p "<pass>"
```

   You can set `COINBASE_ADDR` before launching to skip the prompt. Set `MINER_THREADS` (or pass `-t`) so the miner does not take every logical processor; without that, the launcher defaults to all cores.

5. If the launcher cannot find RPC credentials, open Command Prompt in the miner folder and run the same `rigor-miner.exe` line with `-u` / `-p` taken from the **wallet** `data` directory (portable datadir next to `rigor-wallet.exe`). Do **not** double-click `rigor-miner.exe` with no arguments (it requires `-o` and `--coinbase-addr`).
6. Leave threads below the number of logical processors so the wallet/node stay responsive. Example: 8 logical processors → `-t 6`.
7. Leave the console open. Hashrate lines mean the miner is hashing. Confirm `-o` is `http://127.0.0.1:8761` and the payout address begins with `rg1q`.

Optional: `start-miner-dashboard.cmd` opens a **localhost** read-only view at `http://127.0.0.1:18083/`. It does not mine or hold keys. Python 3 is required for that helper.

To stop: close the console window or press Ctrl+C.

---

## Linux x86_64

Published name: `rigor-v0.2-miner-linux-x86_64.tar.gz`.

```bash
tar -xzf rigor-v0.2-miner-linux-x86_64.tar.gz
cd <extracted-tree>
```

The tarball contains `README.txt` and `bin/rigor-miner` (plus `bin/minerd` as a name for the same program). The wrapper is already executable.

Start the official Mainnet wallet/node first on the **same host** (`./bin/rigord` or `./bin/rigor-wallet` from the **wallet** package). Confirm `chain=main` and a `rg1q` address. Read RPC credentials from the wallet data directory. Then:

```bash
./bin/rigor-miner -a rgrpow01 -o http://127.0.0.1:8761 \
  -u RPCUSER -p RPCPASS \
  --coinbase-addr=rg1q<your-mainnet-payout-address> \
  -t N
```

Replace `RPCUSER` / `RPCPASS` with the values from your local node credentials. Replace `N` with cores minus 1 or 2 (minimum 1). Do not omit `-t` if you want to keep cores free for the node.

`--no-getwork` is the default SOLO path and may be omitted.

To stop: Ctrl+C in the terminal (or `kill` the `rigor-miner` process). Hashrate / status output means it is hashing against the local URL you passed (`http://127.0.0.1:8761`).

---

## Useful options (actual CLI)

Verified from the miner help text (`-h` / `--help`):

| Option | Meaning in this guide |
| --- | --- |
| `-a rgrpow01` | SHA3-256(`RGRPOW01` \|\| 80-byte header) |
| `-o http://127.0.0.1:8761` | Local node RPC URL (`--url`) |
| `-u` / `-p` | RPC username / password |
| `-O user:pass` | Combined credentials |
| `--coinbase-addr=` | Mainnet payout address (`rg1q…`) |
| `-t N` | Thread count |
| `--no-getwork` | Default; getwork is not used |
| `-q` | Quieter hashmeter (Windows launcher uses this) |
| `-h` | Help |

`--coinbase-sig` is ignored. `--no-gbt` is refused.

---

## Troubleshooting

### RPC connection failure

Confirm `rigord` is running on the same host and listening on `127.0.0.1:8761`. Confirm `-u` / `-p` match the local credential files. The miner URL is `-o http://127.0.0.1:8761`, not P2P port `8760`, and not `seed.rigorchain.com`.

### Wrong payout address

`--coinbase-addr` must be a Mainnet ORU address starting with `rg1q` that you control. RES (`rg1p…`) is not a mining payout.

### Node still syncing

Wait until the node is out of Initial Block Download and at the current tip. `getblocktemplate` on a syncing node may return an initial-sync error.

### GBT unavailable

Check synchronization, RPC credentials, and that `server=1` is active. The miner requests `rules: ["segwit"]`.

### submitblock rejected

The node validates the candidate. Rejection is determined by consensus rules. Check miner and node logs. The miner only searched a nonce; it did not invent a new coinbase.

---

## Security

- The RIGOR Mainnet RPC interface is intended for local node-to-miner communication on `127.0.0.1:8761`. Do not expose RPC port 8761 to the public Internet.
- Do not publish RPC credentials.
- Do not paste private keys into the terminal or this document.
- Use a `rg1q` address from a wallet you control.
- Verify SHA256 of downloaded archives against `SHA256SUMS.txt`.
- Install wallet and miner only from the official RIGOR v0.2 Mainnet release.

---

## macOS

A macOS miner is not included in this release.
