RIGOR TESTNET SOLO mining guide  v0.2

Network: TESTNET ONLY
This is not a pool. There is no stratum+tcp:// address.
You fill in the RPC username, password, and trg1q payout address.

Do not use a Mainnet wallet, rg1q, 8760, or 8761.

================================================
Step 1  Start the testnet wallet and find the data folder
================================================
1. Start Rigor Wallet Testnet (run rigor-wallet.exe).
2. The window title must include [testnet].
3. rigor.conf belongs only in the wallet data folder:
   same folder as rigor-wallet.exe -> open data -> save rigor.conf there
   Do not save it next to rigor-wallet.exe. The wallet will not read it.
   Do not save it inside testnet3, and do not reuse a drive letter
   from an older install.
4. If there is no data folder yet, start the wallet once. It creates the folder.

================================================
Step 2  Create rigor.conf in Notepad
================================================
1. Start Windows Notepad.
2. Do not open an existing .txt file. Use File -> New.
3. Paste these 5 lines (change YOUR_NAME and YOUR_PASSWORD
   to values you choose; do not leave CHANGE_ME):

testnet=1
server=1
rpcuser=YOUR_NAME
rpcpassword=YOUR_PASSWORD
rpcallowip=127.0.0.1

4. File -> Save.
5. Browse to the data folder under the wallet install directory.
   Correct: <wallet-install>\data\rigor.conf
   Wrong:   <wallet-install>\rigor.conf
   (Next to rigor-wallet.exe the wallet ignores the file and mining auth fails.)
6. The file name must be:

   rigor.conf

7. Save as type must be:

   All Files

   Do not choose Text Documents.
   That creates rigor.conf.txt. The wallet will not read it.

8. After saving, confirm the name is rigor.conf, not rigor.conf.txt.
   If it is .txt, delete it and repeat this step.

================================================
Step 3  Fully quit the wallet
================================================
Do this as its own step. Do not skip it.

1. Close the Rigor Wallet Testnet window.
2. Check the system tray. If a Rigor icon is still there,
   right-click it -> Exit / Quit.
3. Open Task Manager and confirm there is no rigor-qt.exe,
   rigor-wallet.exe, or rigord.exe.
4. Only after the wallet has fully quit, go to Step 4.
   If it is still running, the new rigor.conf will not load.

================================================
Step 4  Start the wallet again
================================================
1. Start Rigor Wallet Testnet again.
2. Confirm the title includes [testnet] and receive addresses start with trg1q.
3. Wait until the wallet is ready to use.

================================================
Step 5  Copy a trg1q receive address
================================================
In Receive, copy an address that starts with trg1q.
Do not use rg1q.

================================================
Step 6  Edit the miner script in Notepad
================================================
1. Edit rigor-miner-testnet.cmd in Notepad.
   Do not double-click it to change the values. Double-clicking runs the miner.
   Right-click the file -> Edit
   or start Notepad first, then File -> Open and select this file.
2. First change only these three lines (they must match Step 2 rigor.conf):

   RPC_USER
   RPC_PASS
   COINBASE_ADDR

   Set COINBASE_ADDR to the trg1q address from Step 5.
3. Then limit miner threads. Do not use every CPU core.
   The same PC also runs the wallet and node.
   Set threads to CPU cores minus 1 or minus 2, minimum 1.
   Example: 8 cores -> -t 6; 4 cores -> -t 2.
   Task Manager -> Performance -> CPU shows Logical processors.
   Do one of the following:
   - If the script already has MINER_THREADS or -t, change that number only.
   - If neither is present, append -t N to the rigor-miner.exe line
     (replace N with the number above).
   Do not omit -t. Without -t the miner defaults to all CPU cores.

   Example: 8 logical processors, leave 2 cores for the wallet and node.
   If the rigor-miner.exe line has no -t yet, change that whole line to:

   rigor-miner.exe -a rgrpow01 --no-getwork -o "http://127.0.0.1:18761" -u "%RPC_USER%" -p "%RPC_PASS%" --coinbase-addr="%COINBASE_ADDR%" -t 6

   For 4 cores, use -t 2 at the end instead of -t 6.
4. Save.
5. Now double-click rigor-miner-testnet.cmd to run it.
6. Leave the console window open. Hashrate lines mean it is mining.

Do not double-click rigor-miner.exe.

================================================
Linux
================================================
Create rigor.conf in the data folder next to the wallet program.
Use the same contents as Step 2.
The file name must be rigor.conf, not rigor.conf.txt.

Fully quit the wallet process, start the wallet again, then edit
miner-testnet.sh in a text editor, or run (set N to cores minus 1 or 2,
minimum 1):

  ./bin/rigor-miner -a rgrpow01 -o http://127.0.0.1:18761 \
    -u YOUR_NAME -p YOUR_PASSWORD --coinbase-addr=trg1qYOURADDRESS \
    -t N
