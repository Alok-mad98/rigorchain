RIGOR MAINNET SOLO mining guide  v0.2

Network: MAINNET
This is the Linux miner. It is not the wallet archive.
This is not a pool. There is no stratum+tcp:// address.
You fill in the RPC username, password, and rg1q payout address.

Do not use a TESTNET wallet, trg1q, 18760, or 18761.
Do not publish RPC port 8761. Do not mine on a project seed server.

The miner program in this archive is the previously published build.
Its --help text was checked on Linux. A SOLO mining run was not recorded.

================================================
Step 1  Start the mainnet wallet and find the data folder
================================================
1. Extract the RIGOR v0.2 MAINNET Linux wallet archive.
2. Start it with ./bin/rigor-wallet or ./bin/rigord from that directory.
   Do not run bin/internal/* directly.
3. The window title must not include [testnet].
4. rigor.conf belongs only in the wallet data folder created by those
   launchers:
   Correct: <wallet-dir>/data/rigor.conf
   Wrong:   <wallet-dir>/rigor.conf
   Wrong:   ~/.rigor/rigor.conf when you use ./bin/rigor-wallet or ./bin/rigord
   Those launchers always pass -datadir=<wallet-dir>/data.
5. If there is no data folder yet, start the wallet once. It creates the folder.
6. P2P is port 8760. RPC is 127.0.0.1:8761.

================================================
Step 2  Put the RPC username and password in that data folder
================================================
1. If <wallet-dir>/data/rpc-auth.conf already exists, open it.
   Use its rpcuser and rpcpassword in Step 6.
   Do not invent a second password.
2. If that file does not exist, create a new text file.
3. Paste these 5 lines. Change YOUR_NAME and YOUR_PASSWORD
   to values you choose. Do not leave CHANGE_ME:

server=1
rpcbind=127.0.0.1
rpcallowip=127.0.0.1
rpcuser=YOUR_NAME
rpcpassword=YOUR_PASSWORD

4. Save it in <wallet-dir>/data.
5. The file name must be:

   rigor.conf

   Do not let the editor save rigor.conf.txt. The wallet will not read it.

================================================
Step 3  Fully quit the wallet
================================================
Do this as its own step. Do not skip it.

1. Close the Rigor Wallet window.
2. If a tray icon is still shown, choose Exit / Quit.
3. Confirm no rigor-qt, rigor-wallet, or rigord process is still running.
4. Only after the wallet has fully quit, go to Step 4.
   If it is still running, the new rigor.conf will not load.

================================================
Step 4  Start the wallet again
================================================
1. Start ./bin/rigor-wallet or ./bin/rigord again.
2. Confirm the title does not include [testnet].
3. In Receive, addresses must start with rg1q.
4. Open the Nodes or Peers window and wait until at least one peer
   is listed. A peer row is a real connection.
   A seed name in the Seed column is that connection's current DNS name.
   No row means that seed is not connected.
5. Wait until the wallet is past initial download before mining.
   If the node has no peer, it answers getblocktemplate with
   "is not connected". Wait. Do not set -solomining.

================================================
Step 5  Copy an rg1q receive address
================================================
In Receive, copy an address that starts with rg1q.
Do not use rg1p. That is a RES address, not a mining payout.
Do not use trg1q or rgrt1q.
Never type a private key or seed phrase into the miner.

The example rg1qYOURADDRESS below is not a valid address.
The miner rejects it. Replace it with the address you copied.

================================================
Step 6  Edit the miner script, then run it
================================================
1. Extract rigor-v0.2-miner-linux-x86_64.tar.gz:

   tar -xzf rigor-v0.2-miner-linux-x86_64.tar.gz
   cd into the directory that contains rigor-miner.sh

2. Edit rigor-miner.sh in a text editor.
   Do not run it to change the values. Running it starts the miner.
3. Change only these lines so they match Step 2:

   RPC_USER
   RPC_PASS
   COINBASE_ADDR

   Set COINBASE_ADDR to the rg1q address from Step 5.
   You can instead leave COINBASE_ADDR and run:
   ./rigor-miner.sh rg1q...
   Replace rg1q... with your address. The first argument is the payout.
4. Save.
5. Run:

   ./rigor-miner.sh

   Do not run bin/rigor-miner until the three lines are set.
6. Leave the terminal open.

The script runs:

./bin/rigor-miner -a rgrpow01 -q --no-getwork --coinbase-addr="YOUR_ADDRESS" -o "http://127.0.0.1:8761" -u "YOUR_RPC_USER" -p "YOUR_RPC_PASSWORD"

-a rgrpow01 is the MAINNET hash. --no-getwork keeps getwork off; SOLO uses
getblocktemplate. -q hides the per-thread hashmeter. It does not stop mining.
The script does not print the password. The password is still an argument
of the miner process.

Threads: you do not add -t. The miner leaves one logical processor for the
wallet, or two when the computer has 5 or more. To override, set this
before the command:

MINER_THREADS=2 ./rigor-miner.sh

================================================
What the window means
================================================
"Mining with N of M processors" means the miner started and reserved cores
for the wallet.
A status board about every 15 seconds means it is searching.
"Block found!" and "Confirmed by node" mean the node accepted a block.
An authorization or connection error means the wallet is not running, or
RPC_USER and RPC_PASS do not match the data folder.
"is not connected" means the wallet has no usable peer.
An invalid address line means the payout is not a MAINNET rg1q address.
rg1qYOURADDRESS and rg1qCHANGE_ME are rejected on purpose.

Stop the miner with Ctrl+C in that terminal.

================================================
If something fails
================================================
RPC connection refused
- Start ./bin/rigor-wallet or ./bin/rigord and complete Step 3 and Step 4.

RPC authentication failed
- RPC_USER and RPC_PASS must be the values in
  <wallet-dir>/data/rpc-auth.conf, or in rigor.conf if you created it.
- The file must be named rigor.conf, in the data folder, not rigor.conf.txt.
- Quit the wallet completely, then start it again.

No peers, or no work
- Wait until the Nodes window lists a peer.
- Do not set -solomining. Do not point the miner at a public seed.

This package contains bin/rigor-miner, bin/minerd, rigor-miner.sh, and README.txt.
bin/minerd is a link to bin/rigor-miner.
It does not contain a dashboard. Python is not required.
