# RIGOR_EXTERNAL_STACK_GAPS

**Task:** `TASK-RIGOR-EXTERNAL-STACK-CURRENT-STATE-INVENTORY-P0-040`  
**Mode:** READ-ONLY inventory. No production consensus / RPC / miner / pool / UI change in this task.  
**Authority for current rules:** `core/src/` (GBT: `rpc/mining.cpp`, coinbase: `node/miner.cpp`, scriptSig: `consensus/convergence_root.cpp`, PoW: `pow_opus.cpp`).

---

## Current product stack (from this tree, not the assumed diagram)

The assumed `Miner → SOLO Pool → Full Node` path is **not** what ships.

```
                         RIGOR Full Node (core/  rigord)
                                  |
              +-------------------+-------------------+
              |                   |                   |
            RPC                  P2P            GBT + submitblock
              |                                       |
     +--------+--------+                    +---------+---------+
     |                 |                    |                   |
 Explorer           Website           SOLO cpuminer         Alpha pool
 (explorer/)     (web/rigorchain-site)  (miner/ minerd       (pool/pilot
  indexer+SSR     static + optional      = rigor-miner.exe)   rigor-pool)
  + REST API      RPC proxy / explorer         |                   |
     |              client              local GBT only        Stratum v1
     +--------+--------+                       |                   |
              |                         Windows dashboard    rigor-miner-agent
         observation only               :18083 read-only     (pool miner)
                                        (not a gateway)

Broadcast: whoever calls submitblock → rigord ProcessNewBlock → P2P.
Pool is quarantined from the SOLO release (pool/README.md).
yiimp (pool/stratum, pool/web, pool/sql) is ABANDONED.
mint-engine is LEGACY / not on the runtime path.
```

**Shipped SOLO path:** `installer/windows/rigor-miner.cmd` → `rigor-miner.exe -a sha256d --no-getwork --coinbase-addr=rg1q…` → `getblocktemplate {rules:[segwit]}` on local `rigord` (`-solomining=1` when peers=0) → miner rebuilds block → `submitblock`.

**Not in that path:** Stratum, SOLO pool gateway, `rigor-miner-agent`.

---

## Gap table

| Component | Old assumption | Current RIGOR | Status | Next Task |
|-----------|----------------|---------------|--------|-----------|
| Miner work source | Bitcoin getwork / Stratum / pool | Local `getblocktemplate` + `submitblock` against `rigord`. Stratum still compiled in `cpu-miner.c` / `util.c` but installer uses `--no-getwork` HTTP GBT. Node has **no** `getwork` RPC. | PARTIAL | P041 |
| Miner GBT decode | BIP22 `coinbasetxn` / `mutable: coinbase/append` | Node emits **no** `coinbasetxn`. `mutable` is **`["time"]` only**. Miner still parses append / `coinbasetxn`. | STALE | P041 |
| Miner coinbase scriptSig | 2-push: BIP34 height ‖ C. `gbt_write_coinbase_scriptsig` | 3-push: height ‖ C ‖ paid-winner. Copy `default_coinbase_scriptsig`. Miner **never reads** that field. | STALE | P041 |
| Miner Convergence C | `compute_convergence_root`: `count < 2` → zeros | Empty F → zeros; **n≥1 hashes** (singleton hashes). Prefer GBT `convergence_root` (miner does if present). Fallback is wrong. | STALE | P041 |
| Miner RES mint | Ignore / Bitcoin single-asset coinbase | Copy `default_res_mint` when parent draw named a winner. Miner **never reads** `default_res_mint` / `paid_winner_txid`. | STALE | P041 |
| Miner RIGR OP_RETURN | Required; fail if GBT `prev_nbits` missing | `CheckRigorDrawTargetCommitment` is a no-op. Parent nBits live in the header. Extra OP_RETURN is leftover, not required. | STALE | P041 |
| Miner CVGE | Append `default_convergence_commitment` | Node **does not emit** that field. CVGE is not consensus. | STALE | P041 |
| Miner witness coinbase | Local assembly: version + vin, no BIP141 marker/flag, witness commitment as vout only | Coinbase witness reserved value = **32 zero bytes**. Copy node coinbase. | STALE | P041 |
| Miner tx set / merkle | SHA256d merkle over GBT `transactions[].data` / `txid` | Same Bitcoin-shaped merkle for header; RIGOR dual-asset txs still in `data`. Header 80-byte layout unchanged. | CURRENT | — |
| Miner OpuHash | CLI name `sha256d` / default algo `scrypt` | Header SHA256d → OpuHash mix (`opus_hash.cpp` ≡ `pow_opus.cpp`: 64KiB, 256 rounds, ChaCha20). Installer passes `-a sha256d`. Default binary algo is still `scrypt`. | PARTIAL | P041 |
| Miner difficulty | `SetCompact(nBits)` + `fulltest` (Bitcoin arith) | Same compact nBits / ASERT target. Byte order of digest vs target was already patched in `scanhash_sha256d`. | CURRENT | — |
| Miner RES/ORU reward | Display `coinbasevalue` as “satoshis” (`cpu-miner.c` work struct) | `coinbasevalue` = ORU vout[0] only (subsidy+claimable ORU fees). Genesis subsidy **0**; h≥1 ORU schedule; RES mint **not** in `coinbasevalue`. Miner does not implement subsidy; it copies GBT value. | PARTIAL | P041 |
| Miner maturity / fee burn / convergence draw | Not implemented | Maturity=100, fee burn, RES schedule are node consensus. Miner must not invent them. | CURRENT | — |
| Pool template source | Bitcoin GBT / yiimp | `pool/rigor-gbt`: `getblocktemplate` + long-poll against local `rigord`. | CURRENT | P042 |
| Pool GBT fields | `default_convergence_commitment`, `default_pcorb_carrier` required | Node emits `default_coinbase_scriptsig`, `paid_winner_txid`, `rigor_coinbase_immutable`, `default_res_mint` (optional), `convergence_root`, `convergence_draw`, `prev_nbits`. **No** PCORB carrier field, **no** CVGE field. Pool `GbtTemplate` still deserializes both obsolete fields. | STALE | P042 |
| Pool coinbase scriptSig | 2-push height ‖ C (`block-assembler/src/script.rs::build_coinbase_scriptsig`) | 3-push including paid-winner. Must copy `default_coinbase_scriptsig`. | STALE | P042 |
| Pool extranonce | Bitcoin scriptSig extranonce | scriptSig has **zero free bytes**. Pool moved extranonce into a coinbase **OP_RETURN output** (`build_extranonce_op_return`). Extra OP_RETURN is leftover-allowed, but pool still **assembles** coinbase instead of copying node bytes. | ADAPTER | P042 |
| Pool RIGR / CVGE / PCORB outputs | Pool rebuilds RIGR; copies CVGE + PCORB + mint | RIGR not consensus. CVGE/PCORB fields absent on node GBT. Mint must be **copied** from `default_res_mint`, not invented. | STALE | P042 |
| Pool job wire | Stratum `mining.notify` = job_id, prevhash, coinb1, coinb2, merkle, version, nbits, ntime, clean | Same Bitcoin Stratum v1 shape. RIGOR extras (`convergence_root`, `prev_nbits`, mint flag) stay off the notify params (`jobs.rs`). `mining.notify.rigor` is documented on the miner-agent, **not implemented** on stratum-server. | PARTIAL | P042 |
| Pool share | Bitcoin share = SHA256d vs share diff | OpuHash vs `share_target`; network vs `bits`; codes: stale (`ErrStaleJob`), duplicate (`ErrReplayShare`), low PoW (`ErrInvalidOpouhash`), bad header/ntime (`ErrMalformedSubmission`), bad block (`validate_block`). nTime must **exact-match** job (no time roll). | CURRENT | P042 |
| Pool block submit | Pool `submitblock` | `RpcBlockSubmitter` → `rigord submitblock` → `ProcessNewBlock` → **node** broadcasts P2P. Unconfigured submitter fail-closed. | CURRENT | — |
| Pool payout | Bitcoin pool payout / BTC | **PPLNS ORU**, not SOLO. Coinbase pays **pool payout script**; workers credited off-chain. Pool fee % of ORU. Does **not** mint RES; `mint-engine` not linked. Product says pool must not construct RES — but assembler still **rebuilds** coinbase (boundary tension). | STALE vs SOLO | P042 |
| Pool identity | SOLO gateway | Alpha **ORU coordination pool**, quarantined from SOLO release. Not the test-phase unique mining path. | QUARANTINED | P042 |
| Pool dashboard | yiimp PHP (`pool/web`) | Abandoned. Live status: `rigor-pool status` JSON (hashrate proxy, workers, shares, blocks, ORU payout, RES=NONE). Windows `:18083` dashboard is a **read-only observer** (explorer + optional pool status + wallet RPC). Lifecycle copy still says Miner→**Pool**→Node. | STALE copy | P042 |
| Browser mint | PCORB body-tx proof witness (`importer.ts`, `witnessPanel.ts`) | RES issuance = coinbase `OP_RES_MINT` (script classifier already knows this). Body PCORB mint is retired. | STALE | P043 |
| Browser assets | BTC / satoshi amounts | ORU 1e8, RES 1e6 (`amount.ts`). Comment still says “sat atoms”. | PARTIAL | P043 |
| Browser address | Bitcoin address | Bech32 `rg1q` ORU / Bech32m `rg1p` RES. P2PKH/P2SH not consensus outputs. | CURRENT | — |
| Browser script | Bitcoin script zoo | v0/20 ORU, v1/32 RES, OP_CONVERGE, OP_RES_MINT; else INVALID. Retired CVG2/RES2 labeled. | CURRENT | — |
| Browser RPC | Bitcoin Core fields | `rigor-cli` + RIGOR RPCs (`getconvergenceresult`, `getruntimesupply`, `opus_hash`). Historical RPC keys (e.g. `arda_*` on node) are not explorer primary. Difficulty shown as node `difficulty` labeled ASERT/OpuHash. | PARTIAL | P043 |
| Browser 10 min / 210000 | Bitcoin subsidy epoch | Display uses node RPC / indexed data. Maturity display **100** (valid). | CURRENT | — |
| Website protocol | Website defines rules | Observation only. Live data: explorer API + optional read-only RPC proxy. Write RPCs blocked. | CURRENT | — |
| Website mining UX | Miner → pool → node | SOLO GBT is the shipped miner path. Site `/mining` still offers pool dashboard + “optional pool”. `/mining/pool` is a placeholder. | STALE | P044 |
| Website 210000 | Bitcoin 210000 halving | `RES_REWARD_EPOCH = 210000` is **current RIGOR**. Not stale. | CURRENT | — |
| Website 50 BTC / 10 min hardcoded | Bitcoin subsidy / spacing | No 50 BTC. Mainnet `nPowTargetSpacing = 10*60` is current RIGOR. ORU subsidy copy is 1000→500→250→125. | CURRENT | — |
| Website 100 blocks | Bitcoin maturity | Maturity=100 is current RIGOR (`WalletTopicPage`). | CURRENT | — |
| Docs drift | `docs/architecture/ARCHITECTURE.md` still cites `default_pcorb_carrier` | Node GBT removed PCORB carrier. `core/doc/mining-pool-gbt.md` is the live GBT contract. | STALE | P044 |

### Miner FILE / FUNCTION (Bitcoin-shaped logic still in tree)

| FILE | FUNCTION | OLD ASSUMPTION | CURRENT RIGOR RULE | REQUIRED ACTION |
|------|----------|----------------|--------------------|-----------------|
| `miner/cpu-miner.c` | `gbt_write_coinbase_scriptsig` | height ‖ 32-byte C | height ‖ C ‖ paid-winner | Copy `default_coinbase_scriptsig`; do not re-encode |
| `miner/cpu-miner.c` | `compute_convergence_root` | n<2 → C=0 | n=0 → 0; n≥1 → Hash256(sort) | Use GBT `convergence_root` only; delete n<2 shortcut |
| `miner/cpu-miner.c` | `gbt_work_decode` | Rebuild coinbase; RIGR; CVGE | Immutable coinbase; copy mint | Read `default_res_mint`; stop RIGR/CVGE as required |
| `miner/cpu-miner.c` | `gbt_work_decode` | Require `prev_nbits` or abort | `prev_nbits` informational | Do not fail GBT if only RIGR needs it |
| `miner/cpu-miner.c` | `get_upstream_work` | Fallback `getwork` | RPC removed | Keep `--no-getwork`; delete getwork path |
| `miner/cpu-miner.c` | `stratum_thread` / `stratum_gen_work` | Bitcoin Stratum extranonce in coinbase | scriptSig immutable | Do not use this path for RIGOR SOLO |
| `miner/cpu-miner.c` | `opt_algo` default | `ALGO_SCRYPT` | OpuHash via `-a sha256d` | Fail closed unless OpuHash path |
| `miner/example-cfg.json` | `algo` | `"scrypt"` | OpuHash | Fix in P041 |
| `pool/block-assembler/src/script.rs` | `build_coinbase_scriptsig` | 2 pushes | 3 pushes | Copy node bytes |
| `pool/rigor-gbt/src/gbt.rs` | `GbtTemplate` | `default_pcorb_carrier`, `default_convergence_commitment` | Fields gone | Drop; require `default_coinbase_scriptsig` |
| `explorer/src/importer.ts` | mint detection | PCORB body witness | Coinbase `OP_RES_MINT` | Index coinbase mint output |

---

## Field ownership (GBT / job)

| Field | Node | Pool (if used) | SOLO miner (intended) |
|-------|------|----------------|------------------------|
| prev_hash, height, version, nbits, curtime, tx set, C, paid-winner, RES mint, witness commitment, coinbasevalue | **decides** | must copy | must copy |
| nTime | template + `[mintime, maxtime]` | currently frozen to job ntime | **may roll** (`mutable: ["time"]`) |
| nNonce | grind | grind | **grind** |
| coinbase scriptSig / mint / C | **immutable** | currently rebuilds | currently rebuilds (**wrong**) |
| extranonce | forbidden in scriptSig | OP_RETURN splice | unused in SOLO |
| payout script (ORU vout[0]) | miner/pool `scriptPubKeyIn` | pool payout script (PPLNS) | `--coinbase-addr` |

---

## Required conclusions

### MINER_STATUS
**STALE vs current node coinbase contract. OpuHash / header / target ALIGNED.**  
Shipped SOLO client (`miner/` cpuminer) pulls GBT from local `rigord` and submits `submitblock`. It still **constructs** a Bitcoin-GBT coinbase (2-push scriptSig, optional CVGE, required RIGR, no `default_res_mint`). That will fail `convergence-root-cb-missing-paid-winner-push` on current `ConnectBlock`. PoW path (`sha2.c::scanhash_sha256d` → `opus_hash`) matches `pow_opus.cpp`.

### POOL_STATUS
**QUARANTINED Alpha ORU Stratum pool, not a SOLO gateway. Assembler STALE on scriptSig.**  
Template from node GBT. Jobs are Bitcoin Stratum notify + local coinb1/coinb2. Shares are RIGOR (OpuHash, stale/dup/low-diff codes). Found blocks: pool `submitblock`, node broadcasts. Payout is **PPLNS ORU**, not “finder takes all”. RES mint construction is policy-forbidden; coinbase bytes are still pool-assembled.

### BROWSER_STATUS
**MOSTLY CURRENT as ORU/RES observer; mint path STALE (PCORB body).**  
Indexer/SSR/API talk to `rigor-cli`, classify native scripts, display ORU/RES, convergence, fee burn, maturity 100, OpuHash. Mint lineage still assumes PCORB body witness.

### WEBSITE_STATUS
**CURRENT as non-authority display; mining journey STALE (pool-first).**  
No 50 BTC / Bitcoin RPC field hardcodes. `210000` and 10-minute mainnet spacing are live RIGOR params. `/mining` still describes Miner→Pool.

### MINER_PROTOCOL_DEPENDENCIES
`getblocktemplate{rules:[segwit]}`, `prev_nbits`, `convergence_root` / `convergence_txids`, `bits`, `transactions`, `coinbasevalue`, `submitblock`, OpuHash(header80). **Missing deps in code:** `default_coinbase_scriptsig`, `default_res_mint`, `paid_winner_txid`, `rigor_coinbase_immutable`.

### POOL_PROTOCOL_DEPENDENCIES
Same GBT + `submitblock` + Stratum v1 (`subscribe/authorize/notify/submit/set_difficulty`) + OpuHash share/block targets. Extra assumed fields: `default_pcorb_carrier`, `default_convergence_commitment` (node no longer sends). Does not consume `default_coinbase_scriptsig`.

### BROWSER_PROTOCOL_DEPENDENCIES
Read-only: `getblock` / `getrawtransaction` / `getblockchaininfo` / `getconvergenceresult` / `getconvergencestats` / `getruntimesupply` / `getnetworkinfo`. Explorer HTTP `/api/{status,block,tx,address,res/mints,lineage}`.

### WEBSITE_PROTOCOL_DEPENDENCIES
Optional `VITE_EXPLORER_API_URL` + `VITE_RPC_PROXY_URL` (`getblockchaininfo`, `getnetworkinfo`, `getconvergencestatus`, …). No consensus writes.

### STALE_BITCOIN_ASSUMPTIONS
GBT coinbase reconstruction; 2-push scriptSig; scriptSig extranonce (pool moved it to OP_RETURN); getwork; default scrypt; RIGR/CVGE/PCORB as required template objects; `coinbasevalue` as total reward; “satoshis”; PCORB body mint; Stratum-as-SOLO; yiimp dashboard; website Miner→Pool primary journey.

### CONSENSUS_BOUNDARY_BREACHES
(Inventory only — not changed here.)

1. **SOLO miner constructs coinbase** (scriptSig, outputs, RIGR) instead of copying node bytes. Violates `rigor_coinbase_immutable`.
2. **Pool assembler constructs coinbase** (scriptSig, mint/PCORB/CVGE/RIGR/extranonce OP_RETURN) instead of copying node bytes. Product rule “pool never participates in consensus asset construction” is **not** implemented on the assembler path.
3. **Neither** should select RES winners or mint amounts; node template is SSOT. Pool winner-engine is preview-only (OK). `mint-engine` leftover is not linked (OK if unused).

No breach of W=32, T, RES schedule, OpuHash parameters, ORU subsidy constants, maturity=100, Genesis=0 ORU, ASERT, tx/sighash/witness/P2P rules in this inventory (those stay in `core/`).

### NEXT_IMPLEMENTATION_ORDER
1. **P041 Miner GBT copy-bytes** — SOLO: copy `default_coinbase_scriptsig` + `default_res_mint` + witness; grind nonce/time only; drop getwork/scrypt/RIGR-required; keep OpuHash. This is the blocker for a working external miner.
2. **P042 Pool SOLO-or-quarantine** — either keep pool out of test mining, or make it a true SOLO gateway: no PPLNS split, no coinbase rewrite, copy node template, submitblock only.
3. **P043 Explorer mint adapter** — index coinbase `OP_RES_MINT`; stop requiring PCORB body witness as mint proof.
4. **P044 Website/docs** — SOLO-first mining copy; drop PCORB carrier from architecture docs; `/mining/pool` not presented as the test path.

---

## SOLO as the unique test-phase mining path?

**No — not with the current miner/pool binaries against the current node.**

| Path | In SOLO release? | Produces ConnectBlock-valid blocks today? |
|------|------------------|-------------------------------------------|
| Node `generatetoaddress` / internal assembler | yes (node) | **yes** (node builds 3-push + mint) |
| `miner/` cpuminer / `rigor-miner.exe` GBT | **yes (shipped)** | **no** (2-push scriptSig, no mint copy) |
| `pool/` Stratum + PPLNS | **no (quarantined)** | **no** (2-push assembler) even if operated |
| `rigor-miner-agent` | pool stack only | depends on pool jobs (stale coinbase) |

Intended unique **external** test path is local SOLO GBT (`-solomining`, installer `rigor-miner.cmd`). It is the right topology, but the client is one consensus coinbase revision behind `rigord`. Until P041, the only reliable way to mine a valid tip is the **node assembler**, not the external miner or pool.
