RIGOR TESTNET — official website download

Download source:
Official RIGOR website
http://www.rigorchain.com/downloads/testnet/

TESTNET ONLY. Test coins have no monetary value.
Do not use TESTNET software on Mainnet.
Do not use Mainnet software on TESTNET.

Network: TESTNET
Chain: test
Address: trg1q
P2P: 18760
Public peers (first start):
  seed1.testnet.rigor.network:18760
  seed2.testnet.rigor.network:18760
RPC: localhost only (127.0.0.1:18761)

Datadir: folder named data next to the wallet program
Config: that data folder / rigor.conf
Do not put rigor.conf next to the wallet exe.

This TESTNET package is a consensus reset. After install, delete the
old testnet3 data directory (or the whole data folder next to the
program) and let the wallet sync from genesis. Do not reuse a chain
built by an older TESTNET wallet. Mainnet ASERT is unchanged.

DO NOT USE rg1q
DO NOT CONNECT PORT 8760
DO NOT USE rigor-v0.2-* Mainnet packages

Verify:
 sha256sum -c SHA256SUMS

Windows wallet:
 Run rigor-wallet-v0.2-testnet-windows.exe
 Start rigor-wallet.exe or start-wallet.cmd
 Do not open files inside internal\

Linux wallet:
 Extract rigor-wallet-v0.2-testnet-linux-x86_64.tar.gz
 Use bin/rigord and bin/rigor-wallet
 Wrappers use -testnet and a portable data folder
 Do not run bin/internal/*

Miner:
 Payout must be trg1q
 Local RPC 127.0.0.1:18761
 English SOLO mining guide: MINER-TESTNET-README.txt
 Chinese SOLO mining guide: MINER-TESTNET-README-CN.txt
 Edit rigor-miner-testnet.cmd in Notepad. Do not double-click the .cmd file to edit it.
 Leave 1-2 CPU cores free: set -t to (cores minus 1 or 2). Do not omit -t.

Explorer:
 http://explorer.rigorchain.com
