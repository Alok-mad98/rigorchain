# RIGOR Mainnet RPC Surface Audit

Status: audit record. This document is not consensus.

```
documentation_consensus_authority = false
```

Audit date: 2026-10-18  
Tree: `89d76899a83c3ab8e45fb219d9c988d6bb4ffd41` (`main`)  
Result: one RPC removed (`signmessage`). Every other registered command stays.

---

## 1. Audit scope

This is a pre-genesis hygiene review of the RPC surface exposed by the RIGOR full node and wallet. It is not a protocol redesign, not a feature expansion, and not a simplification of consensus.

### 1.1 Repository identity

| Item | Value |
|---|---|
| Branch at audit start | `main` |
| HEAD at audit start | `89d76899a83c3ab8e45fb219d9c988d6bb4ffd41` |
| HEAD subject | Record the independent adversarial genesis audit. |
| HEAD committer date | 2026-10-14 07:06:23 +0000 |
| Public repository | `https://github.com/rigorchain/rigorchain` |

The public download remote matches that repository. This audit did not change remotes, DNS, or deployment.

### 1.2 Working tree at start

Seven files were already modified before this audit. They were not reverted and they were not treated as RPC deletions:

- `core/src/qt/rpcconsole.cpp`
- `core/src/rpc/client.cpp`
- `core/src/wallet/rpc/coins.cpp`
- `core/src/wallet/rpc/rpcspend.cpp`
- `core/src/wallet/rpc/rpcwallet.cpp`
- `core/src/wallet/rpc/transactions.cpp`
- `core/test/functional/wallet_importrigorprivkey.py`

Those edits only retarget help text, the CLI JSON conversion table, the Qt console history filter, and one functional test. They point operators at commands that are already registered (`importdescriptors`, `listdescriptors`, `dumporuprivkey`, `dumpresprivkey`, `importrigorprivkey`) and drop conversion entries for commands that are already absent from the registration tables (`dumpprivkey`, `sethdseed`, `importaddress`, `importpubkey`, `importmulti`, `upgradewallet`). They do not change transaction construction, consensus validation, or which commands are registered.

No consensus source file was modified. The follow-up removal of `signmessage` is recorded in section 3.1.

### 1.3 Build configuration

The node is the in-tree autotools build under `core/` (`core/src/config/rigor-config.h`):

| Switch | Value |
|---|---|
| `ENABLE_WALLET` | 1 |
| `ENABLE_ZMQ` | 1 |
| `ENABLE_EXTERNAL_SIGNER` | 1 |
| Wallet backend in `core/test/config.ini` | SQLite (`USE_SQLITE=true`) |

`core/src/rigord -version` and `core/src/rigor-cli -version` report `RIGOR version v0.2`, `BUILD_GIT_COMMIT=89d76899a83c-dirty`, `NETWORK=mainnet`. The `-dirty` flag matches the seven pre-existing uncommitted files. An older binary at `core/build-linux-devnode/src/rigord` still reports `7d75716c2f3d-dirty` and was not used for the live checks below.

`make -C core/src rigord rigor-cli` exited 0. Make relinked `rigor-cli`. `rigord` was already newer than its RPC objects; those objects are newer than `core/src/rpc/blockchain.cpp`, `mining.cpp`, and the wallet RPC sources, and the running daemon's help strings match the working tree (watch-only help cites `importdescriptors`).

### 1.4 What was inventoried

Registration was read from every `CRPCCommand` table under `core/src`, not from `help` alone:

- `core/src/rpc/server.cpp` (control)
- `core/src/rpc/blockchain.cpp`
- `core/src/rpc/mining.cpp`
- `core/src/rpc/mempool.cpp`
- `core/src/rpc/rawtransaction.cpp`
- `core/src/rpc/net.cpp`
- `core/src/rpc/node.cpp`
- `core/src/rpc/fees.cpp`
- `core/src/rpc/output_script.cpp`
- `core/src/rpc/signmessage.cpp`
- `core/src/rpc/txoutproof.cpp`
- `core/src/rpc/external_signer.cpp` (compiled in; `ENABLE_EXTERNAL_SIGNER`)
- `core/src/zmq/zmqrpc.cpp` (compiled in; `ENABLE_ZMQ`)
- `core/src/wallet/rpc/rpcwallet.cpp` (`GetWalletRPCCommands`)
- `core/src/rpc/register.h` (`RegisterAllCoreRPCCommands`)
- `core/src/init.cpp` (core table, wallet table, ZMQ table)

`core/src/qt/test/rpcnestedtests.cpp` registers `rpcNestedTest` for the Qt test binary only. It is not part of `rigord`.

Call sites were searched in `core/test`, `core/contrib`, `contrib`, `scripts`, `tools`, `explorer`, `qt`, `miner`, `docs`, and `web`.

### 1.5 Freeze boundary observed

No edit was made to genesis identity, block headers, OpuHash / RGRPOW01, SHA3-256 domain separation, proof-of-work, ASERT, block or transaction validation, script rules, address formats, wallet database format, ORU or RES monetary rules, supply invariants, or consensus state transition. No governance, version-bit upgrade path, emergency switch, admin issuance, premine, founder allocation, treasury issuance, pool reward path, or Stratum payout was added.

### 1.6 Test status recorded before this audit's checks

Unit-test logs under `core/src/test/*_tests.log` are dated 2026-09-30. Sampled logs contain `*** No errors detected`. `core/src/test/test_rigor` is also dated 2026-09-30. HEAD is 188 commits after `7d75716c2f3d116ca404184d3f41bee290ed51e0`, which is the commit string embedded in the unused `build-linux-devnode` binary. Those September logs are not a result for this HEAD. They were not re-run. See section 5 for what was actually executed on this tree.

---

## 2. Complete RPC inventory

186 commands are registered in the wallet-enabled node this tree builds, after `signmessage` was removed. 172 appear in unqualified `help`. 14 are category `hidden`: they stay callable and are omitted from the default help index. The pre-removal regtest `help` matched 173 public names plus 14 hidden names. `signmessage` is omitted from the table below.

Column notes:

- **Bitcoin-inherited** means the method name comes from Bitcoin Core. Several inherited methods also return RIGOR fields (for example `getblockchaininfo` convergence totals, `getblocktemplate` immutable coinbase).
- **Consensus-related** means the method reads or submits consensus objects (blocks, UTXO set, supply mirrors, state hash, templates). It does not mean the method is allowed to change consensus rules. None of these methods are a consensus implementation.
- **Referenced** means a whole-word hit under the trees in section 1.4. `no` does not mean the method is dead. Several audit methods are only reached through `rigord` itself.
- **Class** is `KEEP` for every registered command. No command met the removal tests in section 3.

| RPC | Category | Implementation | Registration | Bitcoin-inherited | RIGOR-specific | Consensus-related | Wallet-related | Referenced by tests/tools/docs | Externally visible | Class |
|---|---|---|---|---|---|---|---|---|---|---|
| abandontransaction | wallet | core/src/wallet/rpc/rpcwallet.cpp:672 | core/src/wallet/rpc/rpcwallet.cpp:680 | yes | no | no | yes | yes | yes | KEEP |
| abortrescan | wallet | core/src/wallet/rpc/rpcwallet.cpp:674 | core/src/wallet/rpc/rpcwallet.cpp:681 | yes | no | no | yes | yes | yes | KEEP |
| addconnection | hidden | core/src/rpc/net.cpp:349 | core/src/rpc/net.cpp:1008 | yes | no | no | no | yes | callable, omitted from default help | KEEP |
| addnode | network | core/src/rpc/net.cpp:291 | core/src/rpc/net.cpp:998 | yes | no | no | no | yes | yes | KEEP |
| addpeeraddress | hidden | core/src/rpc/net.cpp:937 | core/src/rpc/net.cpp:1009 | yes | no | no | no | yes | callable, omitted from default help | KEEP |
| analyzepsbt | rawtransactions | core/src/rpc/rawtransaction.cpp:1648 | core/src/rpc/rawtransaction.cpp:1845 | yes | no | no | no | yes | yes | KEEP |
| backupwallet | wallet | core/src/wallet/rpc/rpcwallet.cpp:628 | core/src/wallet/rpc/rpcwallet.cpp:682 | yes | no | no | yes | yes | yes | KEEP |
| bumpfee | wallet | core/src/wallet/rpc/rpcwallet.cpp:655 | core/src/wallet/rpc/rpcwallet.cpp:683 | yes | no | no | yes | yes | yes | KEEP |
| clearbanned | network | core/src/rpc/net.cpp:834 | core/src/rpc/net.cpp:1005 | yes | no | no | no | yes | yes | KEEP |
| combinepsbt | rawtransactions | core/src/rpc/rawtransaction.cpp:1242 | core/src/rpc/rawtransaction.cpp:1839 | yes | no | no | no | yes | yes | KEEP |
| combinerawtransaction | rawtransactions | core/src/rpc/rawtransaction.cpp:557 | core/src/rpc/rawtransaction.cpp:1836 | yes | no | no | no | yes | yes | KEEP |
| converttopsbt | rawtransactions | core/src/rpc/rawtransaction.cpp:1387 | core/src/rpc/rawtransaction.cpp:1842 | yes | no | no | no | yes | yes | KEEP |
| createconvergence | wallet | core/src/wallet/rpc/rpcwallet.cpp:651 | core/src/wallet/rpc/rpcwallet.cpp:726 | no | yes | yes | yes | yes | yes | KEEP |
| createpsbt | rawtransactions | core/src/rpc/rawtransaction.cpp:1347 | core/src/rpc/rawtransaction.cpp:1841 | yes | no | no | no | yes | yes | KEEP |
| createrawtransaction | rawtransactions | core/src/rpc/rawtransaction.cpp:348 | core/src/rpc/rawtransaction.cpp:1833 | yes | no | no | no | yes | yes | KEEP |
| createwallet | wallet | core/src/wallet/rpc/rpcwallet.cpp:335 | core/src/wallet/rpc/rpcwallet.cpp:685 | yes | no | no | yes | yes | yes | KEEP |
| decodepsbt | rawtransactions | core/src/rpc/rawtransaction.cpp:898 | core/src/rpc/rawtransaction.cpp:1838 | yes | no | no | no | yes | yes | KEEP |
| decoderawtransaction | rawtransactions | core/src/rpc/rawtransaction.cpp:379 | core/src/rpc/rawtransaction.cpp:1834 | yes | no | yes | no | yes | yes | KEEP |
| decodescript | rawtransactions | core/src/rpc/rawtransaction.cpp:420 | core/src/rpc/rawtransaction.cpp:1835 | yes | no | no | no | yes | yes | KEEP |
| deriveaddresses | util | core/src/rpc/output_script.cpp:133 | core/src/rpc/output_script.cpp:221 | yes | no | no | no | yes | yes | KEEP |
| destroyoru | wallet | core/src/wallet/rpc/rpcwallet.cpp:650 | core/src/wallet/rpc/rpcwallet.cpp:725 | no | yes | yes | yes | yes | yes | KEEP |
| diffchainstatesummaries | blockchain | core/src/rpc/blockchain.cpp:2363 | core/src/rpc/blockchain.cpp:4190 | no | yes | yes | no | yes | yes | KEEP |
| disconnectnode | network | core/src/rpc/net.cpp:405 | core/src/rpc/net.cpp:999 | yes | no | no | no | yes | yes | KEEP |
| dumporuprivkey | wallet | core/src/wallet/rpc/rpcwallet.cpp:620 | core/src/wallet/rpc/rpcwallet.cpp:687 | no | yes | yes | yes | yes | yes | KEEP |
| dumpresprivkey | wallet | core/src/wallet/rpc/rpcwallet.cpp:621 | core/src/wallet/rpc/rpcwallet.cpp:688 | no | yes | yes | yes | yes | yes | KEEP |
| dumptxoutset | blockchain | core/src/rpc/blockchain.cpp:3816 | core/src/rpc/blockchain.cpp:4206 | yes | no | yes | no | yes | yes | KEEP |
| echo | hidden | core/src/rpc/node.cpp:290 | core/src/rpc/node.cpp:473 | yes | no | no | no | yes | callable, omitted from default help | KEEP |
| echoipc | hidden | core/src/rpc/node.cpp:325 | core/src/rpc/node.cpp:475 | yes | no | no | no | yes | callable, omitted from default help | KEEP |
| echojson | hidden | core/src/rpc/node.cpp:323 | core/src/rpc/node.cpp:474 | yes | no | no | no | yes | callable, omitted from default help | KEEP |
| encryptwallet | wallet | core/src/wallet/rpc/rpcwallet.cpp:645 | core/src/wallet/rpc/rpcwallet.cpp:689 | yes | no | no | yes | yes | yes | KEEP |
| enumeratesigners | signer | core/src/rpc/external_signer.cpp:19 | core/src/rpc/external_signer.cpp:72 | yes | no | no | no | yes | yes | KEEP |
| estimaterawfee | hidden | core/src/rpc/fees.cpp:100 | core/src/rpc/fees.cpp:222 | yes | no | no | no | yes | callable, omitted from default help | KEEP |
| estimatesmartfee | util | core/src/rpc/fees.cpp:29 | core/src/rpc/fees.cpp:221 | yes | no | no | no | yes | yes | KEEP |
| exportruntimeequivalencecandidate | blockchain | core/src/rpc/blockchain.cpp:1821 | core/src/rpc/blockchain.cpp:4181 | no | yes | yes | no | no | yes | KEEP |
| exportruntimeglobalcandidateequivalence | blockchain | core/src/rpc/blockchain.cpp:2172 | core/src/rpc/blockchain.cpp:4182 | no | yes | yes | no | no | yes | KEEP |
| exportruntimesupply | blockchain | core/src/rpc/blockchain.cpp:2221 | core/src/rpc/blockchain.cpp:4188 | no | yes | yes | no | yes | yes | KEEP |
| finalizepsbt | rawtransactions | core/src/rpc/rawtransaction.cpp:1290 | core/src/rpc/rawtransaction.cpp:1840 | yes | no | no | no | yes | yes | KEEP |
| fundrawtransaction | rawtransactions | core/src/wallet/rpc/rpcwallet.cpp:654 | core/src/wallet/rpc/rpcwallet.cpp:679 | yes | no | no | yes | yes | yes | KEEP |
| generate | hidden | core/src/rpc/mining.cpp:250 | core/src/rpc/mining.cpp:1217 | yes | no | yes | no | yes | callable, omitted from default help | KEEP |
| generateblock | hidden | core/src/rpc/mining.cpp:298 | core/src/rpc/mining.cpp:1216 | yes | no | yes | no | yes | callable, omitted from default help | KEEP |
| generatetoaddress | hidden | core/src/rpc/mining.cpp:257 | core/src/rpc/mining.cpp:1214 | yes | no | yes | no | yes | callable, omitted from default help | KEEP |
| generatetodescriptor | hidden | core/src/rpc/mining.cpp:212 | core/src/rpc/mining.cpp:1215 | yes | no | yes | no | yes | callable, omitted from default help | KEEP |
| getaddednodeinfo | network | core/src/rpc/net.cpp:451 | core/src/rpc/net.cpp:1000 | yes | no | no | no | yes | yes | KEEP |
| getaddressesbylabel | wallet | core/src/wallet/rpc/rpcwallet.cpp:613 | core/src/wallet/rpc/rpcwallet.cpp:690 | yes | no | no | yes | yes | yes | KEEP |
| getaddressinfo | wallet | core/src/wallet/rpc/rpcwallet.cpp:607 | core/src/wallet/rpc/rpcwallet.cpp:691 | yes | no | no | yes | yes | yes | KEEP |
| getassetbalance | blockchain | core/src/rpc/blockchain.cpp:4088 | core/src/rpc/blockchain.cpp:4192 | no | yes | yes | no | yes | yes | KEEP |
| getbalance | wallet | core/src/wallet/rpc/rpcwallet.cpp:634 | core/src/wallet/rpc/rpcwallet.cpp:692 | yes | no | no | yes | yes | yes | KEEP |
| getbalances | wallet | core/src/wallet/rpc/rpcwallet.cpp:638 | core/src/wallet/rpc/rpcwallet.cpp:699 | yes | no | no | yes | yes | yes | KEEP |
| getbestblockhash | blockchain | core/src/rpc/blockchain.cpp:276 | core/src/rpc/blockchain.cpp:4163 | yes | no | yes | no | yes | yes | KEEP |
| getblock | blockchain | core/src/rpc/blockchain.cpp:699 | core/src/rpc/blockchain.cpp:4168 | yes | no | yes | no | yes | yes | KEEP |
| getblockchaininfo | blockchain | core/src/rest.cpp:577 | core/src/rpc/blockchain.cpp:4159 | yes | no | yes | no | yes | yes | KEEP |
| getblockconvergence | blockchain | core/src/rpc/blockchain.cpp:3872 | core/src/rpc/blockchain.cpp:4207 | no | yes | yes | no | yes | yes | KEEP |
| getblockcount | blockchain | core/src/rpc/blockchain.cpp:255 | core/src/rpc/blockchain.cpp:4164 | yes | no | yes | no | yes | yes | KEEP |
| getblockfilter | blockchain | core/src/rpc/blockchain.cpp:3732 | core/src/rpc/blockchain.cpp:4205 | yes | no | yes | no | yes | yes | KEEP |
| getblockfrompeer | blockchain | core/src/rpc/blockchain.cpp:478 | core/src/rpc/blockchain.cpp:4197 | yes | no | yes | no | yes | yes | KEEP |
| getblockhash | blockchain | core/src/rpc/blockchain.cpp:532 | core/src/rpc/blockchain.cpp:4166 | yes | no | yes | no | yes | yes | KEEP |
| getblockheader | blockchain | core/src/rpc/blockchain.cpp:561 | core/src/rpc/blockchain.cpp:4167 | yes | no | yes | no | yes | yes | KEEP |
| getblockstats | blockchain | core/src/rpc/blockchain.cpp:2979 | core/src/rpc/blockchain.cpp:4162 | yes | no | yes | no | yes | yes | KEEP |
| getblocktemplate | mining | core/src/rpc/mining.cpp:605 | core/src/rpc/mining.cpp:1210 | yes | no | yes | no | yes | yes | KEEP |
| getchainstatesummary | blockchain | core/src/rpc/blockchain.cpp:2290 | core/src/rpc/blockchain.cpp:4189 | no | yes | yes | no | yes | yes | KEEP |
| getchaintips | blockchain | core/src/rpc/blockchain.cpp:2624 | core/src/rpc/blockchain.cpp:4169 | yes | no | yes | no | yes | yes | KEEP |
| getchaintxstats | blockchain | core/src/rpc/blockchain.cpp:2843 | core/src/rpc/blockchain.cpp:4161 | yes | no | yes | no | yes | yes | KEEP |
| getconnectioncount | network | core/src/rpc/net.cpp:48 | core/src/rpc/net.cpp:995 | yes | no | no | no | yes | yes | KEEP |
| getconvergencestatus | blockchain | core/src/rpc/blockchain.cpp:3947 | core/src/rpc/blockchain.cpp:4208 | no | yes | yes | no | yes | yes | KEEP |
| getdeploymentinfo | blockchain | core/src/rest.cpp:604 | core/src/rpc/blockchain.cpp:4160 | yes | no | yes | no | yes | yes | KEEP |
| getdescriptorinfo | util | core/src/rpc/output_script.cpp:89 | core/src/rpc/output_script.cpp:222 | yes | no | no | no | yes | yes | KEEP |
| getdeterministicstatehash | blockchain | core/src/rpc/blockchain.cpp:2253 | core/src/rpc/blockchain.cpp:4172 | no | yes | yes | no | yes | yes | KEEP |
| getdifficulty | blockchain | core/src/rpc/blockchain.cpp:458 | core/src/rpc/blockchain.cpp:4165 | yes | no | yes | no | yes | yes | KEEP |
| getindexinfo | util | core/src/rpc/node.cpp:375 | core/src/rpc/node.cpp:470 | yes | no | no | no | yes | yes | KEEP |
| getmemoryinfo | control | core/src/rpc/node.cpp:157 | core/src/rpc/node.cpp:467 | yes | no | no | no | yes | yes | KEEP |
| getmempoolancestors | blockchain | core/src/rpc/mempool.cpp:478 | core/src/rpc/mempool.cpp:991 | yes | no | yes | no | yes | yes | KEEP |
| getmempooldescendants | blockchain | core/src/rpc/mempool.cpp:539 | core/src/rpc/mempool.cpp:992 | yes | no | yes | no | yes | yes | KEEP |
| getmempoolentry | blockchain | core/src/rpc/mempool.cpp:604 | core/src/rpc/mempool.cpp:993 | yes | no | yes | no | yes | yes | KEEP |
| getmempoolinfo | blockchain | core/src/rpc/mempool.cpp:762 | core/src/rpc/mempool.cpp:995 | yes | no | yes | no | yes | yes | KEEP |
| getmininginfo | mining | core/src/rpc/mining.cpp:418 | core/src/rpc/mining.cpp:1208 | yes | no | yes | no | yes | yes | KEEP |
| getnettotals | network | core/src/rpc/net.cpp:524 | core/src/rpc/net.cpp:1001 | yes | no | no | no | yes | yes | KEEP |
| getnetworkhashps | mining | core/src/rpc/mining.cpp:103 | core/src/rpc/mining.cpp:1207 | yes | no | yes | no | yes | yes | KEEP |
| getnetworkinfo | network | core/src/rpc/net.cpp:593 | core/src/rpc/net.cpp:1002 | yes | no | no | no | yes | yes | KEEP |
| getnewaddress | wallet | core/src/wallet/rpc/rpcwallet.cpp:608 | core/src/wallet/rpc/rpcwallet.cpp:693 | yes | no | no | yes | yes | yes | KEEP |
| getnodeaddresses | network | core/src/rpc/net.cpp:876 | core/src/rpc/net.cpp:1007 | yes | no | no | no | yes | yes | KEEP |
| getnodehealth | control | core/src/rpc/node.cpp:421 | core/src/rpc/node.cpp:468 | no | yes | yes | no | yes | yes | KEEP |
| getpeerinfo | network | core/src/rpc/net.cpp:94 | core/src/rpc/net.cpp:997 | yes | no | no | no | yes | yes | KEEP |
| getphase5runtimealignment | blockchain | core/src/rpc/blockchain.cpp:1670 | core/src/rpc/blockchain.cpp:4178 | no | yes | yes | no | no | yes | KEEP |
| getphase5summary | blockchain | core/src/rpc/blockchain.cpp:1869 | core/src/rpc/blockchain.cpp:4185 | no | yes | yes | no | no | yes | KEEP |
| getphase5summaryfinal | blockchain | core/src/rpc/blockchain.cpp:1918 | core/src/rpc/blockchain.cpp:4186 | no | yes | yes | no | no | yes | KEEP |
| getrawchangeaddress | wallet | core/src/wallet/rpc/rpcwallet.cpp:609 | core/src/wallet/rpc/rpcwallet.cpp:694 | yes | no | no | yes | yes | yes | KEEP |
| getrawmempool | blockchain | core/src/rpc/mempool.cpp:428 | core/src/rpc/mempool.cpp:996 | yes | no | yes | no | yes | yes | KEEP |
| getrawtransaction | rawtransactions | core/src/rpc/rawtransaction.cpp:181 | core/src/rpc/rawtransaction.cpp:1832 | yes | no | yes | no | yes | yes | KEEP |
| getreceivedbyaddress | wallet | core/src/wallet/rpc/rpcwallet.cpp:632 | core/src/wallet/rpc/rpcwallet.cpp:695 | yes | no | no | yes | yes | yes | KEEP |
| getreceivedbylabel | wallet | core/src/wallet/rpc/rpcwallet.cpp:633 | core/src/wallet/rpc/rpcwallet.cpp:696 | yes | no | no | yes | yes | yes | KEEP |
| getreplaysnapshot | blockchain | core/src/rpc/blockchain.cpp:1298 | core/src/rpc/blockchain.cpp:4191 | no | yes | yes | no | yes | yes | KEEP |
| getrpcinfo | control | core/src/rpc/server.cpp:212 | core/src/rpc/server.cpp:259 | yes | no | no | no | yes | yes | KEEP |
| getruntimeequivalencecandidate | blockchain | core/src/rpc/blockchain.cpp:1724 | core/src/rpc/blockchain.cpp:4179 | no | yes | yes | no | no | yes | KEEP |
| getruntimeequivalencecandidatebyheight | blockchain | core/src/rpc/blockchain.cpp:1771 | core/src/rpc/blockchain.cpp:4180 | no | yes | yes | no | no | yes | KEEP |
| getruntimeequivalenceclosure | blockchain | core/src/rpc/blockchain.cpp:2122 | core/src/rpc/blockchain.cpp:4183 | no | yes | yes | no | no | yes | KEEP |
| getruntimesupply | blockchain | core/src/rpc/blockchain.cpp:1400 | core/src/rpc/blockchain.cpp:4173 | no | yes | yes | no | yes | yes | KEEP |
| getruntimesupplyinvariants | blockchain | core/src/rpc/blockchain.cpp:1478 | core/src/rpc/blockchain.cpp:4174 | no | yes | yes | no | yes | yes | KEEP |
| getruntimesupplywitness | blockchain | core/src/rpc/blockchain.cpp:1522 | core/src/rpc/blockchain.cpp:4175 | no | yes | yes | no | yes | yes | KEEP |
| getruntimesupplywitnessbyheight | blockchain | core/src/rpc/blockchain.cpp:1567 | core/src/rpc/blockchain.cpp:4176 | no | yes | yes | no | no | yes | KEEP |
| getruntimesupplywitnesscatalog | blockchain | core/src/rpc/blockchain.cpp:1614 | core/src/rpc/blockchain.cpp:4177 | no | yes | yes | no | no | yes | KEEP |
| getruntimevectorstatus | blockchain | core/src/rpc/blockchain.cpp:1971 | core/src/rpc/blockchain.cpp:4187 | no | yes | yes | no | yes | yes | KEEP |
| gettransaction | wallet | core/src/wallet/rpc/rpcwallet.cpp:671 | core/src/wallet/rpc/rpcwallet.cpp:697 | yes | no | no | yes | yes | yes | KEEP |
| gettxout | blockchain | core/src/rpc/blockchain.cpp:1079 | core/src/rpc/blockchain.cpp:4170 | yes | no | yes | no | yes | yes | KEEP |
| gettxoutproof | blockchain | core/src/rpc/txoutproof.cpp:23 | core/src/rpc/txoutproof.cpp:175 | yes | no | yes | no | yes | yes | KEEP |
| gettxoutsetinfo | blockchain | core/src/rpc/blockchain.cpp:924 | core/src/rpc/blockchain.cpp:4171 | yes | no | yes | no | yes | yes | KEEP |
| gettxspendingprevout | blockchain | core/src/rpc/mempool.cpp:637 | core/src/rpc/mempool.cpp:994 | yes | no | yes | no | yes | yes | KEEP |
| getunconfirmedbalance | wallet | core/src/wallet/rpc/rpcwallet.cpp:635 | core/src/wallet/rpc/rpcwallet.cpp:698 | yes | no | no | yes | yes | yes | KEEP |
| getwalletinfo | wallet | core/src/wallet/rpc/rpcwallet.cpp:41 | core/src/wallet/rpc/rpcwallet.cpp:700 | yes | no | no | yes | yes | yes | KEEP |
| getzmqnotifications | zmq | core/src/zmq/zmqrpc.cpp:21 | core/src/zmq/zmqrpc.cpp:60 | yes | no | no | no | yes | yes | KEEP |
| help | control | core/src/rpc/server.cpp:139 | core/src/rpc/server.cpp:260 | yes | no | no | no | yes | yes | KEEP |
| importdescriptors | wallet | core/src/wallet/rpc/rpcwallet.cpp:626 | core/src/wallet/rpc/rpcwallet.cpp:701 | yes | no | no | yes | yes | yes | KEEP |
| importprivkey | wallet | core/src/wallet/rpc/rpcwallet.cpp:622 | core/src/wallet/rpc/rpcwallet.cpp:702 | yes | no | no | yes | yes | yes | KEEP |
| importprunedfunds | wallet | core/src/wallet/rpc/rpcwallet.cpp:624 | core/src/wallet/rpc/rpcwallet.cpp:704 | yes | no | no | yes | yes | yes | KEEP |
| importrigorprivkey | wallet | core/src/wallet/rpc/rpcwallet.cpp:623 | core/src/wallet/rpc/rpcwallet.cpp:703 | no | yes | yes | yes | yes | yes | KEEP |
| invalidateblock | blockchain | core/src/rpc/blockchain.cpp:2762 | core/src/rpc/blockchain.cpp:4201 | yes | no | yes | no | yes | yes | KEEP |
| invokedisallowedsyscall | hidden | core/src/rpc/node.cpp:75 | core/src/rpc/node.cpp:477 | yes | no | no | no | yes | callable, omitted from default help | KEEP |
| joinpsbts | rawtransactions | core/src/rpc/rawtransaction.cpp:1547 | core/src/rpc/rawtransaction.cpp:1844 | yes | no | no | no | yes | yes | KEEP |
| keypoolrefill | wallet | core/src/wallet/rpc/rpcwallet.cpp:612 | core/src/wallet/rpc/rpcwallet.cpp:705 | yes | no | no | yes | yes | yes | KEEP |
| listaddressgroupings | wallet | core/src/wallet/rpc/rpcwallet.cpp:611 | core/src/wallet/rpc/rpcwallet.cpp:706 | yes | no | no | yes | yes | yes | KEEP |
| listbanned | network | core/src/rpc/net.cpp:787 | core/src/rpc/net.cpp:1004 | yes | no | no | no | yes | yes | KEEP |
| listconvergencehistory | rawtransactions | core/src/rpc/rawtransaction.cpp:1757 | core/src/rpc/rawtransaction.cpp:1846 | no | yes | yes | no | yes | yes | KEEP |
| listdescriptors | wallet | core/src/wallet/rpc/rpcwallet.cpp:627 | core/src/wallet/rpc/rpcwallet.cpp:707 | yes | no | no | yes | yes | yes | KEEP |
| listlabels | wallet | core/src/wallet/rpc/rpcwallet.cpp:614 | core/src/wallet/rpc/rpcwallet.cpp:708 | yes | no | no | yes | yes | yes | KEEP |
| listlockunspent | wallet | core/src/wallet/rpc/rpcwallet.cpp:637 | core/src/wallet/rpc/rpcwallet.cpp:709 | yes | no | no | yes | yes | yes | KEEP |
| listreceivedbyaddress | wallet | core/src/wallet/rpc/rpcwallet.cpp:667 | core/src/wallet/rpc/rpcwallet.cpp:710 | yes | no | no | yes | yes | yes | KEEP |
| listreceivedbylabel | wallet | core/src/wallet/rpc/rpcwallet.cpp:668 | core/src/wallet/rpc/rpcwallet.cpp:711 | yes | no | no | yes | yes | yes | KEEP |
| listsinceblock | wallet | core/src/wallet/rpc/rpcwallet.cpp:670 | core/src/wallet/rpc/rpcwallet.cpp:712 | yes | no | no | yes | yes | yes | KEEP |
| listtransactions | wallet | core/src/wallet/rpc/rpcwallet.cpp:669 | core/src/wallet/rpc/rpcwallet.cpp:713 | yes | no | no | yes | yes | yes | KEEP |
| listunspent | wallet | core/src/wallet/rpc/rpcwallet.cpp:639 | core/src/wallet/rpc/rpcwallet.cpp:714 | yes | no | no | yes | yes | yes | KEEP |
| listwalletdir | wallet | core/src/wallet/rpc/rpcwallet.cpp:139 | core/src/wallet/rpc/rpcwallet.cpp:715 | yes | no | no | yes | yes | yes | KEEP |
| listwallets | wallet | core/src/wallet/rpc/rpcwallet.cpp:176 | core/src/wallet/rpc/rpcwallet.cpp:716 | yes | no | no | yes | yes | yes | KEEP |
| loadwallet | wallet | core/src/wallet/rpc/rpcwallet.cpp:207 | core/src/wallet/rpc/rpcwallet.cpp:717 | yes | no | no | yes | yes | yes | KEEP |
| lockunspent | wallet | core/src/wallet/rpc/rpcwallet.cpp:636 | core/src/wallet/rpc/rpcwallet.cpp:718 | yes | no | no | yes | yes | yes | KEEP |
| logging | control | core/src/rpc/node.cpp:230 | core/src/rpc/node.cpp:469 | yes | no | no | no | yes | yes | KEEP |
| mockscheduler | hidden | core/src/rpc/node.cpp:95 | core/src/rpc/node.cpp:472 | yes | no | no | no | yes | callable, omitted from default help | KEEP |
| ping | network | core/src/rpc/net.cpp:70 | core/src/rpc/net.cpp:996 | yes | no | no | no | yes | yes | KEEP |
| preciousblock | blockchain | core/src/rpc/blockchain.cpp:2722 | core/src/rpc/blockchain.cpp:4200 | yes | no | yes | no | yes | yes | KEEP |
| prioritisetransaction | mining | core/src/rpc/mining.cpp:463 | core/src/rpc/mining.cpp:1209 | yes | no | yes | no | yes | yes | KEEP |
| pruneblockchain | blockchain | core/src/rpc/blockchain.cpp:821 | core/src/rpc/blockchain.cpp:4198 | yes | no | yes | no | yes | yes | KEEP |
| psbtbumpfee | wallet | core/src/wallet/rpc/rpcwallet.cpp:656 | core/src/wallet/rpc/rpcwallet.cpp:684 | yes | no | no | yes | yes | yes | KEEP |
| reconsiderblock | blockchain | core/src/rpc/blockchain.cpp:2803 | core/src/rpc/blockchain.cpp:4202 | yes | no | yes | no | yes | yes | KEEP |
| removeprunedfunds | wallet | core/src/wallet/rpc/rpcwallet.cpp:625 | core/src/wallet/rpc/rpcwallet.cpp:719 | yes | no | no | yes | yes | yes | KEEP |
| res_mint_unit_cost | blockchain | core/src/rpc/blockchain.cpp:2065 | core/src/rpc/blockchain.cpp:4184 | no | yes | yes | no | yes | yes | KEEP |
| rescanblockchain | wallet | core/src/wallet/rpc/rpcwallet.cpp:673 | core/src/wallet/rpc/rpcwallet.cpp:720 | yes | no | no | yes | yes | yes | KEEP |
| restorewallet | wallet | core/src/wallet/rpc/rpcwallet.cpp:629 | core/src/wallet/rpc/rpcwallet.cpp:686 | yes | no | no | yes | yes | yes | KEEP |
| savemempool | blockchain | core/src/rpc/mempool.cpp:795 | core/src/rpc/mempool.cpp:997 | yes | no | yes | no | yes | yes | KEEP |
| scanblocks | blockchain | core/src/rpc/blockchain.cpp:3545 | core/src/rpc/blockchain.cpp:4204 | yes | no | yes | no | yes | yes | KEEP |
| scantxoutset | blockchain | core/src/rpc/blockchain.cpp:3334 | core/src/rpc/blockchain.cpp:4203 | yes | no | yes | no | yes | yes | KEEP |
| send | wallet | core/src/wallet/rpc/rpcwallet.cpp:657 | core/src/wallet/rpc/rpcwallet.cpp:721 | yes | no | no | yes | yes | yes | KEEP |
| sendall | wallet | core/src/wallet/rpc/rpcwallet.cpp:658 | core/src/wallet/rpc/rpcwallet.cpp:733 | yes | no | no | yes | yes | yes | KEEP |
| sendmany | wallet | core/src/wallet/rpc/rpcwallet.cpp:652 | core/src/wallet/rpc/rpcwallet.cpp:722 | yes | no | no | yes | yes | yes | KEEP |
| sendrawtransaction | rawtransactions | core/src/rpc/mempool.cpp:70 | core/src/rpc/mempool.cpp:989 | yes | no | yes | no | yes | yes | KEEP |
| sendres | wallet | core/src/wallet/rpc/rpcwallet.cpp:649 | core/src/wallet/rpc/rpcwallet.cpp:724 | no | yes | yes | yes | yes | yes | KEEP |
| sendtoaddress | wallet | core/src/wallet/rpc/rpcwallet.cpp:648 | core/src/wallet/rpc/rpcwallet.cpp:723 | yes | no | no | yes | yes | yes | KEEP |
| setban | network | core/src/rpc/net.cpp:703 | core/src/rpc/net.cpp:1003 | yes | no | no | no | yes | yes | KEEP |
| setlabel | wallet | core/src/wallet/rpc/rpcwallet.cpp:610 | core/src/wallet/rpc/rpcwallet.cpp:727 | yes | no | no | yes | yes | yes | KEEP |
| setmocktime | hidden | core/src/rpc/node.cpp:34 | core/src/rpc/node.cpp:471 | yes | no | no | no | yes | callable, omitted from default help | KEEP |
| setnetworkactive | network | core/src/rpc/net.cpp:855 | core/src/rpc/net.cpp:1006 | yes | no | no | no | yes | yes | KEEP |
| settxfee | wallet | core/src/wallet/rpc/rpcwallet.cpp:653 | core/src/wallet/rpc/rpcwallet.cpp:728 | yes | no | no | yes | yes | yes | KEEP |
| setwalletflag | wallet | core/src/wallet/rpc/rpcwallet.cpp:268 | core/src/wallet/rpc/rpcwallet.cpp:729 | yes | no | no | yes | yes | yes | KEEP |
| signmessagewithprivkey | util | core/src/rpc/signmessage.cpp:64 | core/src/rpc/signmessage.cpp:108 | yes | no | no | no | yes | yes | KEEP |
| signrawtransactionwithkey | rawtransactions | core/src/rpc/rawtransaction.cpp:644 | core/src/rpc/rawtransaction.cpp:1837 | yes | no | no | no | yes | yes | KEEP |
| signrawtransactionwithwallet | wallet | core/src/wallet/rpc/rpcwallet.cpp:661 | core/src/wallet/rpc/rpcwallet.cpp:731 | yes | no | no | yes | yes | yes | KEEP |
| simulaterawtransaction | wallet | core/src/wallet/rpc/rpcwallet.cpp:497 | core/src/wallet/rpc/rpcwallet.cpp:732 | yes | no | no | yes | yes | yes | KEEP |
| stop | control | core/src/rpc/server.cpp:167 | core/src/rpc/server.cpp:261 | yes | no | no | no | yes | yes | KEEP |
| submitblock | mining | core/src/rpc/mining.cpp:1096 | core/src/rpc/mining.cpp:1211 | yes | no | yes | no | yes | yes | KEEP |
| submitheader | mining | core/src/rpc/mining.cpp:1165 | core/src/rpc/mining.cpp:1212 | yes | no | yes | no | yes | yes | KEEP |
| submitpackage | hidden | core/src/rpc/mempool.cpp:832 | core/src/rpc/mempool.cpp:998 | yes | no | yes | no | yes | callable, omitted from default help | KEEP |
| syncwithvalidationinterfacequeue | blockchain | core/src/rpc/blockchain.cpp:440 | core/src/rpc/blockchain.cpp:4196 | yes | no | no | no | yes | yes | KEEP |
| testmempoolaccept | rawtransactions | core/src/rpc/mempool.cpp:139 | core/src/rpc/mempool.cpp:990 | yes | no | yes | no | yes | yes | KEEP |
| unloadwallet | wallet | core/src/wallet/rpc/rpcwallet.cpp:433 | core/src/wallet/rpc/rpcwallet.cpp:734 | yes | no | no | yes | yes | yes | KEEP |
| uptime | control | core/src/rpc/server.cpp:193 | core/src/rpc/server.cpp:262 | yes | no | no | no | yes | yes | KEEP |
| utxoupdatepsbt | rawtransactions | core/src/rpc/rawtransaction.cpp:1454 | core/src/rpc/rawtransaction.cpp:1843 | yes | no | no | no | yes | yes | KEEP |
| validateaddress | util | core/src/rpc/output_script.cpp:29 | core/src/rpc/output_script.cpp:220 | yes | no | no | no | yes | yes | KEEP |
| verifychain | blockchain | core/src/rpc/blockchain.cpp:1178 | core/src/rpc/blockchain.cpp:4199 | yes | no | yes | no | yes | yes | KEEP |
| verifymessage | util | core/src/rpc/signmessage.cpp:17 | core/src/rpc/signmessage.cpp:107 | yes | no | no | no | yes | yes | KEEP |
| verifytxoutproof | blockchain | core/src/rpc/txoutproof.cpp:124 | core/src/rpc/txoutproof.cpp:176 | yes | no | yes | no | yes | yes | KEEP |
| waitforblock | blockchain | core/src/rpc/blockchain.cpp:348 | core/src/rpc/blockchain.cpp:4194 | yes | no | yes | no | yes | yes | KEEP |
| waitforblockheight | blockchain | core/src/rpc/blockchain.cpp:394 | core/src/rpc/blockchain.cpp:4195 | yes | no | yes | no | yes | yes | KEEP |
| waitfornewblock | blockchain | core/src/rpc/blockchain.cpp:306 | core/src/rpc/blockchain.cpp:4193 | yes | no | yes | no | yes | yes | KEEP |
| walletcreatefundedpsbt | wallet | core/src/wallet/rpc/rpcwallet.cpp:660 | core/src/wallet/rpc/rpcwallet.cpp:735 | yes | no | no | yes | yes | yes | KEEP |
| walletdisplayaddress | wallet | core/src/wallet/rpc/rpcwallet.cpp:616 | core/src/wallet/rpc/rpcwallet.cpp:737 | yes | no | no | yes | yes | yes | KEEP |
| walletlock | wallet | core/src/wallet/rpc/rpcwallet.cpp:644 | core/src/wallet/rpc/rpcwallet.cpp:739 | yes | no | no | yes | yes | yes | KEEP |
| walletpassphrase | wallet | core/src/wallet/rpc/rpcwallet.cpp:642 | core/src/wallet/rpc/rpcwallet.cpp:740 | yes | no | no | yes | yes | yes | KEEP |
| walletpassphrasechange | wallet | core/src/wallet/rpc/rpcwallet.cpp:643 | core/src/wallet/rpc/rpcwallet.cpp:741 | yes | no | no | yes | yes | yes | KEEP |
| walletprocesspsbt | wallet | core/src/wallet/rpc/rpcwallet.cpp:659 | core/src/wallet/rpc/rpcwallet.cpp:742 | yes | no | no | yes | yes | yes | KEEP |


### 2.1 Hidden commands (kept)

| RPC | Why it stays |
|---|---|
| `generatetoaddress`, `generatetodescriptor`, `generateblock`, `generate` | Regtest and functional-test block production. `generatetoaddress` mined on the audit node. |
| `echo`, `echojson`, `echoipc` | RPC transport tests. |
| `setmocktime`, `mockscheduler` | Functional-test time control. |
| `addconnection`, `addpeeraddress` | Test-only peer injection. |
| `estimaterawfee` | Fee-estimation diagnostic used by tests. |
| `submitpackage` | Package relay test entry. |
| `invokedisallowedsyscall` | Syscall-sandbox test hook. |

Removing any of these would break the functional harness. They are not Mainnet operator features, and they are not listed by unqualified `help`. They are not dead.

### 2.2 Required node, mining, network, and wallet methods

All of the following are registered and appeared in live `help`:

Blockchain: `getbestblockhash`, `getblock`, `getblockchaininfo`, `getblockcount`, `getblockhash`, `getblockheader`, `getchaintips`, `gettxout`, `gettxoutsetinfo`, `verifychain`, `getblockstats`, `getblockfilter`, `gettxoutproof`, `verifytxoutproof`, `scanblocks`, `scantxoutset`.

Mining: `getblocktemplate`, `getmininginfo`, `getnetworkhashps`, `submitblock`, `submitheader`.

Network and operations: `getpeerinfo`, `getnetworkinfo`, `getconnectioncount`, `getnodehealth`, `getrpcinfo`, `getnettotals`, `ping`, `setban`, `clearbanned`, `addnode`, `disconnectnode`.

Wallet: `createwallet`, `loadwallet`, `backupwallet`, `restorewallet`, `getwalletinfo`, `getbalances`, `getnewaddress`, `listunspent`, `send`, `sendtoaddress`, `sendmany`, `signrawtransactionwithwallet`, `walletprocesspsbt`. Message signing for a raw key remains `signmessagewithprivkey`, checked by `verifymessage`.

### 2.3 Mining and pool

`core/src` contains no Stratum server and no Stratum RPC. Live `help stratum` returns `unknown command`. A regtest `getblocktemplate` response set `rigor_coinbase_immutable` to `true` and did not contain `stratum` or `extranonce` fields.

`getblocktemplate` still accepts `coinbase_address` so a solo miner can ask the node to assemble the canonical coinbase. `core/doc/mining-pool-gbt.md` states that the supported path is solo mining, that coinbase `scriptSig` is the BIP34 height push with no free bytes, and that a Bitcoin-style pool which writes extranonce into `scriptSig` is rejected. That document is an interface note. It is not a pool payout implementation inside `rigord`.

The repository also contains a top-level `pool/` tree. That tree is not registered in `rigord`'s `CRPCTable`. It was not modified and it was not treated as a node RPC.

---

## 3. Removed RPCs

### 3.1 Removals performed by this audit

#### `signmessage`

Reason: permanently unreachable for every address RIGOR can encode.

Evidence:

- `DecodeDestination` accepts only witness v0 (Bech32 P2WPKH) and witness v1 (Bech32m RES). Any other string returns `CNoDestination` with "Only witness addresses are supported".
- `DestinationEncoder` for `PKHash` returns an empty string. A legacy P2PKH address cannot be encoded.
- The handler kept only `std::get_if<PKHash>`. A v0 address is `WitnessV0KeyHash`. A v1 address is `WitnessV1Res`. Both threw `-3` `Address does not refer to key`. A regtest `rgrt1q…` address reproduced that error.
- Descriptor `SignMessage` looks up `GetScriptForDestination(pkhash)`. `PKHash` scripts are no longer emitted.

`verifymessage` and `signmessagewithprivkey` stay. `verifymessage` checks a v0 address against the `0xED` message signature produced by `signmessagewithprivkey`. Transaction signing stays on `signrawtransactionwithwallet`.

Files changed:

- `core/src/wallet/rpc/signmessage.cpp` deleted
- `core/src/wallet/rpc/rpcwallet.cpp` registration removed
- `core/src/Makefile.am` source list updated
- `core/src/wallet/rpc/encrypt.cpp` and `core/src/rpc/signmessage.cpp` help examples now name `signmessagewithprivkey`
- `core/test/functional/wallet_signmessagewithaddress.py` deleted and dropped from `test_runner.py`
- `core/test/functional/wallet_encryption.py` and `wallet_createwallet.py` use `listdescriptors true` as the unlock probe

No other registered command met the removal tests. Unusual names, low call frequency, and "not used by the GUI" were not treated as removal reasons.

### 3.2 Names already absent at HEAD (not reintroduced)

These names are cited by older Bitcoin release notes, by `core/doc/PROTOCOL-NAMING-TRUTH-SOURCE.md`, or by legacy functional-test branches. They are not in any `CRPCCommand` table at HEAD. This audit did not delete them and did not put them back.

| Name | Evidence it is absent | Why it stays absent |
|---|---|---|
| `dumpprivkey` | Not registered. Live `help dumpprivkey` returns `unknown command`. Descriptor export is `listdescriptors true`, plus `dumporuprivkey` / `dumpresprivkey`. | Legacy single-key dump. Descriptor wallets are mandatory (`createwallet` forces `WALLET_FLAG_DESCRIPTORS`). |
| `dumpwallet` | Not registered. | Same legacy dump path. |
| `importwallet` | Not registered. | Legacy import path. |
| `importaddress` | Not registered. Descriptor tests map the old Python helper onto `importdescriptors` only when the wallet is a descriptor wallet (`core/test/functional/test_framework/test_node.py`). | Replaced by `importdescriptors`. |
| `importpubkey` | Not registered. Same test-framework shim. | Replaced by `importdescriptors`. |
| `importmulti` | Not registered. Calls in `wallet_send.py` / `wallet_keypool.py` sit on the non-descriptor branch. The runner invokes those scripts with `--descriptors`. | Replaced by `importdescriptors`. |
| `sethdseed` | Not registered. `wallet_hd.py` calls it only when `not self.options.descriptors`. The runner passes `--descriptors`. | No legacy HD seed wallet. |
| `upgradewallet` | Not registered. `-upgradewallet` is rejected at startup (`wallet_multiwallet.py`). | No legacy wallet upgrade. |
| `addmultisigaddress`, `createmultisig` | Not registered. | Not part of the RIGOR descriptor wallet surface. |
| `createresclaim` | Not registered. `PROTOCOL-NAMING-TRUTH-SOURCE.md` still lists the name. Live `help` returns `unknown command`. | Stale document name. RES mint is `createconvergence`. |
| `getconvergenceresult` | Not registered. Same naming doc, plus a comment in `test_runner.py` that the lottery half was stripped. Live `help` returns `unknown command`. | Stale name. Block convergence data is `getblockconvergence`. |
| `getconvergencedrawcandidates` | Not registered. Naming doc still mentions it beside `res_probability_target`. | Draw candidates are not an RPC. Consensus draw code is not reintroduced here. |

The pre-existing uncommitted edits in section 1.2 finish the help and CLI side of the first six wallet names. They do not delete registration entries, because those entries are already gone at HEAD.

---

## 4. Retained special RPCs

### 4.1 Mandatory economic operations

All ten mandatory names are registered, listed by `help`, and implemented.

#### `destroyoru`

Class: **CORE RIGOR ECONOMIC OPERATION RPC**. Kept. Not renamed, not hidden, not replaced.

Implementation: `core/src/wallet/rpc/rpcspend.cpp` (`destroyoru`), registered in `GetWalletRPCCommands`.

What the audit checked in source:

- Amount must be a positive ORU value of at least `Consensus::MIN_CONVERGENCE_AMOUNT`.
- The wallet builds one ORU `OP_CONVERGE` recipient via `MakeConvergenceRecipient`. The help text states that this does not mint RES.
- Fee estimation is the normal wallet coin-control path (`SetFeeEstimateMode`). The ORU network fee is separate from the destroyed amount.
- The wallet signs through `CreateTransaction(..., sign=true)`.
- Broadcast is `CommitBroadcastOrAbandon`. If the mempool rejects the transaction, the RPC throws and does not return a txid for a transaction no node will mine.
- There is no RPC-local supply counter and no path that marks ORU destroyed without a transaction.

Live regtest: `destroyoru 0.00000001` returned `destroyed_atoms: 1` and the txid entered the mempool. `feature_createconvergence_mint.py` then confirmed a one-atom destroy and a 1 ORU destroy, and confirmed that `sendtoaddress` no longer accepts `converge_amount`.

The RPC cannot bypass the same-transaction irreversible ORU destruction rule, RES minting rules, supply invariants, or transaction validation. Those rules are applied when the transaction is accepted into the mempool and when the block is connected. The RPC only constructs, signs, and broadcasts.

#### `createconvergence`

Class: **CORE RIGOR ECONOMIC OPERATION RPC**. Kept.

Implementation: `core/src/wallet/rpc/rpcspend.cpp`, using `CreateMintTransaction` in `core/src/wallet/spend.cpp`.

What the audit checked in source:

- The burn must be positive ORU. RES inputs are rejected (`Minting requires ORU`).
- The wallet prices the mint at `current_height + CONVERGENCE_HEIGHT_PAD`, not as an exact fit at the next block. Comments in `CreateMintTransaction` state that consensus validation uses the actual confirming block height.
- RES atom count comes from `Consensus::ResMintAtomsFromBurn`. The wallet does not invent a mint amount.
- `subtractfeefromamount` is rejected. The network fee is paid from ORU change.
- Broadcast uses the same `CommitBroadcastOrAbandon` path as `destroyoru`.

Live regtest: `createconvergence` of `min_atom_burn` (1025 atoms at that tip) returned `burn_oru: 1025`, `res_out: 1`, and a txid that entered the mempool. After further blocks, `getbalances` showed `res.trusted` of `0.00000001`. `feature_createconvergence_mint.py` and `feature_rigor_convergence_rpc_status_history.py` passed on this tree.

There is no RPC-level mint shortcut. A mint that violates `CheckResMintArithmetic` is still a consensus rejection; the wallet path in `core/src/wallet/wallet.cpp` calls that function when classifying received coins, and block connection remains authoritative.

#### `sendres`

Class: **CORE RIGOR ASSET TRANSFER RPC**. Kept.

Implementation: `core/src/wallet/rpc/rpcspend.cpp`. The destination must decode as `WitnessV1Res`. Other addresses are rejected with an error that points ORU sends at `sendtoaddress`. `subtractfeefromamount` is rejected because the ORU network fee is paid from ORU inputs, not from the RES output. Spend construction is `SendMoney` with `m_asset_type = ASSET_RES`.

Live regtest: a call aimed at an ORU `rgrt1q` address was rejected for address type. After the one-atom mint matured, `sendres` of `0.00000001` RES to a fresh `rgrt1p` address returned a txid, moved RES from `trusted` to `untrusted_pending`, and left a one-transaction mempool whose `total_fee` was ORU. Fee-burn accounting remains a consensus check on that transaction, not a rule implemented inside the RPC.

#### Supply and cost methods that must stay

| RPC | Role | Live check |
|---|---|---|
| `getassetbalance` | Per-address ORU or RES balance. | Help requires `address` and `asset`. `getassetbalance <rgrt1q> ORU` returned `0.00000000` before the relevant coinbase was selected that way. The method is present and parses. |
| `getruntimesupply` | Read-only ORU/RES supply mirror, including the RFC-002 ledger versus circulating UTXO distinction. | Returned a height-0 object, then continued to function after mining. |
| `getruntimesupplyinvariants` | Read-only check of the nine formal supply invariants. | At genesis: `passed: true`, `checked_invariants: 9`, empty `violations`. |
| `getruntimesupplywitness` | ConnectBlock witness for the tip. | Absent at genesis (`No runtime supply witness stored`). Present after mining (height 102). The error is "no row yet", not a missing method. |
| `getruntimesupplywitnessbyheight` | Same witness resolved on the active chain by height, then loaded by block hash. | Same pattern: reachable, empty until a witness exists. `rigor-cli` converts the height argument. |
| `getruntimesupplywitnesscatalog` | Phase-5 catalog row from the witness plus stored mirror. `equivalence_claimed` is defined as false. | Reachable. Empty until a witness exists. |
| `res_mint_unit_cost` | Display `destroy_cost(h)`, next-height cost, minimum atom burn, and minimum whole-RES burn. Help states that `destroy_cost` must not be fed back as the burn amount `B`. | At the next block: `authorized: true`, non-zero `destroy_cost`, `min_atom_burn: 1025`. |

`exportruntimesupply` is a second name for the same payload as `getruntimesupply`. It stays. `core/contrib/tools/runtimesupply-export.sh` calls that name, and the help text is the stable export entry for Coq and debug tooling. A duplicate name that a tool already calls is not "no unique function".

### 4.2 Runtime, phase, and equivalence methods

Every name in the high-priority list was read in `core/src/rpc/blockchain.cpp`. Each one is registered, has a real handler, and is read-only. None mutates chainstate. None claims Coq equivalence (`equivalence_claimed` is specified as always false). None is a consensus bypass.

| RPC | Case | Decision | Evidence |
|---|---|---|---|
| `getdeterministicstatehash` | A | KEEP | Authoritative replay digest. Used by `feature_replay_burnin.py`, `feature_chainstate_sigkill_state_hash.py`, `feature_p2p_b3_stability_scenarios.py`, and `core/doc/PROTOCOL-NAMING-TRUTH-SOURCE.md`. Live regtest returned `state_hash`, `height`, and `best_block_hash`. |
| `getreplaysnapshot` | A | KEEP | Diagnostic RPL2 snapshot. `state_hash` is the authoritative field; counters are documented as non-consensus projections. Used by `feature_replay_burnin.py` and `feature_res_solo_security.py`. Live call returned the snapshot, including `converge_count`. |
| `getchainstatesummary` | A | KEEP | Fixed-size CSM1 snapshot of the active tip, including `chainstatesummary_hex`. Live call succeeded. The handler warms supply-ledger caches so the digest matches `getdeterministicstatehash`. |
| `diffchainstatesummaries` | B | KEEP | Compares two CSM1 blobs and reports the first logical and first wire mismatch. Unique function: cross-node comparison without either node reading the other's chainstate. Live call on one blob against itself returned `logical.identical: true` and `serialized.identical: true`. |
| `getruntimesupply` | A | KEEP | Mandatory supply mirror. See section 4.1. |
| `exportruntimesupply` | B | KEEP | Alias kept for `runtimesupply-export.sh`. Live payload matched a successful supply export at genesis. |
| `getruntimesupplywitness` | A | KEEP | Mandatory. See section 4.1. |
| `getruntimesupplywitnessbyheight` | A | KEEP | Mandatory. Height is resolved on the active chain; there is no height-keyed consensus cache. |
| `getruntimesupplywitnesscatalog` | A | KEEP | Mandatory catalog row for auditors. Read-only. |
| `res_mint_unit_cost` | A | KEEP | Mandatory cost quote. Wallet GUI and `getconvergencestatus` consumers depend on the same cost function. |
| `getphase5runtimealignment` | B | KEEP | Read-only CHAS alignment from the witness catalog. Does not assert Coq ≡ runtime. No in-repo test calls it by name; that is not a removal reason. At genesis it returns `No Phase-5 runtime alignment report at block`. After 102 blocks it returned a report. |
| `getphase5summary` | B | KEEP | Read-only Phase-5 freeze summary. Live call at genesis returned `equivalence_claimed: false`. |
| `getphase5summaryfinal` | B | KEEP | Read-only final Phase-5 summary (`PROOF-027` fields). Live call at genesis returned `equivalence_claimed: false`. |
| `getruntimeequivalencecandidate` | B | KEEP | Read-only witness/Coq comparability record. Equivalence is not claimed. Empty at genesis; after mining it returned `CES_Comparable` supply statuses. |
| `getruntimeequivalencecandidatebyheight` | B | KEEP | Same record at a height. `rigor-cli` converts the height. Reachable. |
| `getruntimeequivalenceclosure` | B | KEEP | Read-only proof-permission boundary. It reports whether a proof attempt is permitted. It does not prove equivalence. Reachable; no closure row at genesis. |
| `exportruntimeequivalencecandidate` | B | KEEP | Coq-readable export of the candidate. Observation only. Reachable. |
| `exportruntimeglobalcandidateequivalence` | B | KEEP | Aggregates the alignment report and the candidate. `equivalence_claimed` is false. Reachable. |
| `getruntimevectorstatus` | B | KEEP | Golden-vector harness. `core/contrib/tools/golden_vector_verify.sh` and `golden_vector_materializer.sh` call it. Live `batch false` returned `equivalence_claimed: false`, `coq_equiv_runtime: NOT_ESTABLISHED`, `draw_equivalence: PROHIBITED`. |
| `getconvergencestatus` | A | KEEP | Operator and GUI status for destroy cost, tip, and cumulative convergence. Bound by `qt/src/rigordrpcclient.cpp`, the wallet convergence panel, explorer route specs, and `feature_rigor_convergence_rpc_status_history.py`. Live call returned `destroy_cost` and `min_atom_burn`. |

Case C (development experiment with no remaining purpose) and Case D (broken or unreachable) were not met. The Phase-5 handlers return a structured RPC error when the in-memory witness row does not exist yet. That is defined behavior, not a stub.

### 4.3 Other RIGOR-specific methods kept

| RPC | Why it stays |
|---|---|
| `getblockconvergence` | Per-block convergence record. Live call at height 0 returned hash, height, and cumulative convergence. |
| `listconvergencehistory` | Wallet/operator history of convergence outputs. Registered under raw transactions. |
| `getnodehealth` | Production health: height, tip, peers, IBD, database status, mempool, consensus-kernel presence. Observational. Live call succeeded. Used by node A/B/C style monitoring. |
| `importrigorprivkey` | ORU WIF and RES Ed25519 seed import into descriptor wallets. `wallet_importrigorprivkey.py` passed. |
| `dumporuprivkey`, `dumpresprivkey` | Asset-specific private export on descriptor wallets. Registered. Qt history filter names them. |
| `getdeploymentinfo` | Inherited read of deployment status. `vbavailable` in the live template was empty. The method does not add a version-bit upgrade path. It stays because explorers and operators already use it, and this audit does not delete inherited chain queries. |

### 4.4 Message signing that remains

`signmessage` is removed. See section 3.1.

`signmessagewithprivkey` signs with a private key. `verifymessage` checks that signature against a witness v0 address. Witness v1 RES addresses are not message-verification destinations.

Transaction signing works. `signrawtransactionwithwallet` on a wallet-funded raw transaction returned `complete: true`.

---

## 5. Testing results

Commands below were executed on this tree on 2026-10-18. Anything not listed here is **NOT RUN**.

| Check | Result | Evidence |
|---|---|---|
| `make -C core/src rigord rigor-cli` | PASS | Exit 0. `rigor-cli` relinked. `rigord -version` reports `BUILD_GIT_COMMIT=89d76899a83c-dirty`. |
| Regtest process start | PASS | `rigord -regtest` on a temporary datadir reached height 0, genesis `3070f47e2c1b153a3288382f3258396bb48b016eab76eae9f235d006dcc2af63`. Stopped afterward. Datadir removed. |
| Unqualified `help` vs registration, before `signmessage` removal | PASS | 173 public names. Missing from help: none. Extra names: none. 14 hidden names correctly omitted. |
| Mandatory economic names in `help` | PASS | All ten present. |
| Already-absent names | PASS | `help` for `dumpprivkey`, `sethdseed`, `importmulti`, `importaddress`, `importpubkey`, `upgradewallet`, `createresclaim`, `getconvergenceresult`, `stratum` each returned `unknown command`. |
| `getblockchaininfo`, `getbestblockhash`, `getblockcount`, `getdifficulty`, `getchaintips`, `gettxoutsetinfo` | PASS | Regtest. |
| `getmininginfo`, `getnetworkhashps` (via `getmininginfo`), `getblocktemplate` | PASS | Template included `rigor_coinbase_immutable: true`. |
| `submitblock`, `submitheader` | NOT RUN | Registered and present in `help`. No block was submitted through these two calls during the smoke test. |
| `getpeerinfo`, `getnetworkinfo`, `getconnectioncount`, `getnodehealth`, `getrpcinfo`, `getnettotals` | PASS | Regtest, zero peers, which is expected with `-listen=0 -connect=0`. |
| `ping`, `setban`, `clearbanned`, `addnode`, `disconnectnode` | NOT RUN | Registered and present in `help`. No peer was connected. |
| `createwallet`, `getnewaddress`, `getwalletinfo`, `getbalances`, `listunspent` | PASS | Wallet `auditwallet`. |
| `loadwallet` | PASS | Reloaded `auditwallet` after a restart. |
| `backupwallet`, `restorewallet` | PASS | Backup file written; `restorewallet` returned name `restored`. |
| `help signmessage` after removal | PASS | `help: unknown command: signmessage`. `signmessagewithprivkey` remains. |
| `signrawtransactionwithwallet` | PASS | `complete: true`. |
| `generatetoaddress` 1 and 101 | PASS | Regtest. Mature balance `256` ORU after 101 blocks, matching the 128 ORU subsidy and coinbase maturity. |
| `destroyoru 0.00000001` | PASS | `destroyed_atoms: 1`. Txid entered the mempool. |
| `createconvergence` of `min_atom_burn` | PASS | `burn_oru: 1025`, `res_out: 1`. After later blocks, `res.trusted` was `0.00000001`. |
| `sendres` to an ORU address | PASS | Rejected: witness v1 RES address required. |
| `sendres` of the matured 1 RES atom | PASS | Txid returned. RES moved to `untrusted_pending`. Mempool size 1. |
| `getdeterministicstatehash`, `getreplaysnapshot`, `getchainstatesummary`, `diffchainstatesummaries` | PASS | Same-blob diff was identical on both logical and serialized sides. |
| `getruntimesupply`, `getruntimesupplyinvariants`, `res_mint_unit_cost`, `getconvergencestatus`, `getblockconvergence`, `getruntimevectorstatus` | PASS | See section 4. |
| Phase-5 / equivalence / witness methods | PASS | Reachable. Genesis correctly reports a missing witness row. After mining, `getruntimesupplywitness`, `getphase5runtimealignment`, and `getruntimeequivalencecandidate` returned objects. |
| `core/test/functional/rpc_help.py` | PASS | `Tests successful`. |
| `core/test/functional/wallet_encryption.py --descriptors` | PASS | Unlock probe is `listdescriptors true` after `signmessage` removal. |
| `core/test/functional/wallet_importrigorprivkey.py --descriptors` | PASS | Includes the assertion that `dumpprivkey` is an unknown command. |
| `core/test/functional/feature_createconvergence_mint.py --descriptors` | PASS | Covers `createconvergence` and `destroyoru`. |
| `core/test/functional/feature_rigor_convergence_rpc_status_history.py --descriptors` | PASS | Covers `createconvergence`, `getconvergencestatus`, and `res_mint_unit_cost`. |
| Full `test_runner.py` functional suite | NOT RUN | Not executed. The four functional scripts above are the RPC, wallet-import, and economic subset that was run. |
| `core/src/test/test_rigor` unit binary | NOT RUN | Binary and logs are from 2026-09-30 and do not match this HEAD. |
| P2P sync against another node | NOT RUN | Single regtest node, `-connect=0`. |
| Mainnet or public-network node | NOT RUN | Not in scope. No production datadir, DNS, or deployment host was touched. |

### 5.1 Invariants this run did not change

- ORU destruction still requires a broadcast transaction. `destroyoru` did not mint RES (`feature_createconvergence_mint.py`).
- RES still appears only through `createconvergence` / consensus mint validation. The RPC returned `res_out` from the wallet pricer and the coins showed up only after confirmation and maturity.
- No issuance RPC, premine RPC, founder RPC, treasury RPC, or Stratum RPC is registered.
- Consensus source files were not modified by this audit.

---

## Acceptance

| Criterion | Met |
|---|---|
| Every registered RPC inventoried | Yes. 186 after `signmessage` was removed. |
| Every removal has evidence | Yes. `signmessage` only. Section 3.1. |
| No mandatory economic RPC removed | Yes. |
| `destroyoru` still works | Yes. Live and functional test. |
| ORU/RES behavior unchanged | Yes. No consensus edit. Economic RPCs still go through wallet construction and mempool broadcast. |
| Consensus code unchanged | Yes. |
| Mining behavior unchanged | Yes. No mining source edit. Live template still reports an immutable coinbase. |
| Wallet compatibility preserved | Yes. Create, backup, restore, address, descriptor import, and raw signing worked. `signmessage` was removed because no v0 or v1 address can reach it. `signmessagewithprivkey` and `verifymessage` remain. |
| No pool payout RPC on the node | Yes. |
| Document matches the Mainnet RPC surface | Yes. This file. |
