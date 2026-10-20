RIGOR MAINNET SOLO mining guide  v0.2

Network: RIGOR Mainnet
This is CPU SOLO mining against your local node.
This is not a pool. There is no stratum+tcp:// address.
The miner talks only to the wallet/node on this computer.

Do not use a Testnet wallet or a Testnet trg1q address.

RIGOR Mainnet uses OpuHash:
SHA3-256 over the 8-byte tag "RGRPOW01" followed by the 80-byte header.
OpuHash is the name of the algorithm. rgrpow01 is the identifier you
pass to the miner with -a. They are the same thing, not two algorithms.

P2P port: 8760
Local RPC: 127.0.0.1:8761
P2P DNS seed: seed.rigorchain.com
Payout address prefix: rg1q
Genesis: e2ce92cfef740f92e8cc84a8541d8e760c776c668afd9b61740b3a4315241468

The miner connects to the local node at:
http://127.0.0.1:8761

Do NOT expose RPC port 8761 to the public Internet.

seed.rigorchain.com is for Mainnet P2P node discovery.
It is NOT the miner RPC address.

Requirements:
- RIGOR v0.2 Mainnet wallet on this computer
- RIGOR v0.2 Mainnet miner on this computer
- a rg1q receive address from your own wallet
- spare CPU cores (leave some for the wallet)

================================================
Step 1  Start the Mainnet wallet
================================================
1. Install or extract the Mainnet wallet, then run rigor-wallet.exe.
2. Confirm it is Mainnet. Receive addresses must start with rg1q.
3. Mainnet activation is 2026-10-25 06:00:00 UTC. Before mining, confirm
   the wallet is on Mainnet and that it has at least one peer. Compare
   block 0 and the tip with that peer. One peer is not enough if it is
   on a different chain. Do not add solomining=1 to mine height 1.
4. rigor.conf belongs only in the wallet data folder:
   same folder as rigor-wallet.exe -> open data -> rigor.conf
   Do not save it next to rigor-wallet.exe. The wallet will not read it.
5. If there is no data folder yet, start the wallet once. It creates the folder.

The official Mainnet wallet starts local RPC on 127.0.0.1:8761.
If you need to write rigor.conf yourself, put these lines in
<data>\rigor.conf (change YOUR_NAME and YOUR_PASSWORD):

server=1
rpcuser=YOUR_NAME
rpcpassword=YOUR_PASSWORD
rpcallowip=127.0.0.1

Do not add solomining=1. With no peers, getblocktemplate returns
"not connected". That flag skips the check and can mine a private
branch. Those blocks and their reward can be orphaned. There is no
designated first miner, no premine, and no founder allocation.

Save as All Files, name rigor.conf, not rigor.conf.txt.

================================================
Step 2  Fully quit the wallet
================================================
Do this if you created or changed rigor.conf. Do not skip it.

1. Close the Rigor Wallet window.
2. Check the system tray. If a Rigor icon is still there,
   right-click it -> Exit / Quit.
3. Open Task Manager and confirm there is no rigor-qt.exe,
   rigor-wallet.exe, or rigord.exe.

================================================
Step 3  Start the wallet again
================================================
1. Start the Mainnet wallet again.
2. Confirm receive addresses start with rg1q.
3. Confirm the wallet has at least one peer on this same chain before mining.

================================================
Step 4  Copy a Mainnet rg1q payout address
================================================
In Receive, copy an address that starts with rg1q.
Do not use a Testnet trg1q address.
Do not use an rg1p address for mining payout.
Use an address from a wallet you control.

================================================
Step 5  Start the miner (Windows)
================================================
Published files:
  rigor-v0.2-miner-windows-x86_64.exe
  rigor-v0.2-miner-windows-x86_64.zip

The mining program is rigor-miner.exe.
The intended launcher is rigor-miner.cmd in the miner folder.

1. Start the Mainnet wallet first so RPC is up.
2. Open the miner folder.
3. Prefer double-click rigor-miner.cmd.
   It uses http://127.0.0.1:8761 and loads local RPC credentials.
   It asks for your rg1q payout address unless COINBASE_ADDR is already set.
4. Threads are handled for you. The miner leaves one logical processor
   free for the wallet, and two from five processors up. It prints the
   count it picked on the first line, for example:
       Mining with 6 of 8 processors, 2 reserved for the wallet
   Only set MINER_THREADS if you want to override that. Doing so takes
   exactly that many threads, so MINER_THREADS equal to your processor
   count will compete with the wallet and node on the same PC.
   Task Manager -> Performance -> CPU shows Logical processors.
5. Leave the console window open. Hashrate lines mean it is mining.

Do not double-click rigor-miner.exe with no arguments.

If you run the program yourself:

rigor-miner.exe -a rgrpow01 -o "http://127.0.0.1:8761" -u "<user>" -p "<pass>" --coinbase-addr="rg1q<your-mainnet-payout-address>" -t <threads>

================================================
Linux
================================================
Published file:
  rigor-v0.2-miner-linux-x86_64.tar.gz

Start the Mainnet wallet/node on the same computer first.
Confirm addresses start with rg1q. Then:

  tar -xzf rigor-v0.2-miner-linux-x86_64.tar.gz
  ./bin/rigor-miner -a rgrpow01 -o http://127.0.0.1:8761 \
    -u RPCUSER -p RPCPASS \
    --coinbase-addr=rg1q<your-mainnet-payout-address>

Omit -t and the miner leaves a core for the wallet on its own.
Add -t N only to override; N is taken literally, minimum 1.
Use the RPC username and password from the wallet data folder.
To stop: Ctrl+C.

================================================
Troubleshooting
================================================
RPC connection failed
- Is the Mainnet wallet/node running on this computer?
- Is the URL 127.0.0.1:8761? Do not use port 8760.
- Are the RPC credentials correct?

Wrong payout address
- Use a Mainnet rg1q address.

Not connected / Cannot connect to local RIGOR node
- If getpeerinfo is empty, wait. Do not add solomining=1.
- Mining with no peers can build a private branch that is later orphaned.
- Confirm the peer, if any, is on Mainnet and the published genesis.

Node still syncing
- Wait until the wallet has the public tip before mining.

Miner starts but does not mine
- Check the local RPC connection, credentials, and peer count.

Blocks arrive much faster than 10 minutes at first
- This is expected and is not an emission bug. Genesis only seeds a
  difficulty. The retarget anchors at height 1 and then converges, and
  it may move the target by at most 12.5% per block. If the network
  brings more hashrate than the seed assumed, the first few dozen
  blocks come in quickly while difficulty catches up.

Several miners against one node
- Each miner holds one long-polling RPC connection. The node keeps two
  RPC workers free so a found block can always be submitted, and any
  long poll beyond that limit is answered right away instead of
  waiting. Those miners still mine; they just poll for new work rather
  than being pushed it. Raise -rpcthreads in rigor.conf if you want
  more of them pushed.

================================================
Verify the miner download
================================================
Compare SHA256 against SHA256SUMS.txt from the same release:

rigor-v0.2-miner-windows-x86_64.exe
395c97c739107abb1392973eed491b25ffb90ee3a7279b3f021c991f114f392b

rigor-v0.2-miner-windows-x86_64.zip
4744409419cbf26cf4287e7ef40419ac54dcd3ace8d106bb81aeba23af7afb08

rigor-v0.2-miner-linux-x86_64.tar.gz
875c3662233dc7c4616064b7b80ec6b920fda9d4c85be0b72171126ddf16c6fd

  sha256sum -c SHA256SUMS.txt

macOS mining is not included in this release.
