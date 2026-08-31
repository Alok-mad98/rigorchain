# RIGOR miner GBT copy-bytes (P0-041)

**Task:** `TASK-RIGOR-MINER-GBT-COPY-BYTES-SOLO-P0-041`  
**Consensus:** unchanged. W=32, T, RES, ORU, maturity, OpuHash, ASERT, genesis
are not modified.

## Boundary

```
--coinbase-addr → GBT coinbase_address → node CreateNewBlock
  → default_coinbase_tx (canonical bytes)
  → miner copy + OpuHash nonce/time
  → submitblock → ProcessNewBlock
```

Node is the only consensus assembler. Miner does not rebuild RES mint, ORU
subsidy, paid-winner, fee burn, scriptSig, or witness commitment.

Allowed miner fields: header `nNonce`, and `nTime` inside GBT `[mintime, maxtime]`.

## Node (`rpc/mining.cpp`)

- `template_request.coinbase_address` is classified as a destination
  (`std::holds_alternative<WitnessV0KeyHash>`), never by inspecting script bytes.
- `default_coinbase_tx`, `default_coinbase_txid`, `merkleroot` and the
  `coinbase_address` echo are emitted **only** when a payout address was given.
  Without one the template pays an all-zero witness key hash, and publishing
  those bytes would invite a copy-bytes miner to burn its own subsidy.
- The template memo lives in one `GbtTemplateCache` whose key names every input
  it depends on (tip, payout script, mempool cursor). A payout left out of the
  key would serve one miner a template paying another.

## Miner (`miner/gbt_solo.c`)

`parse_coinbase()` walks the node's coinbase structurally and records byte
spans; the txid is SHA256d over an exact slice of the node's own bytes rather
than a re-encode. Substring searches are gone — a byte appearing *somewhere*
in the transaction proves nothing about which output holds it.

Enforced before any work is spent:

| Field | Requirement |
| --- | --- |
| nVersion / nSequence / nLockTime | `2` / `0xffffffff` / `0` |
| serialization | BIP141 marker+flag mandatory; no trailing bytes |
| vin | exactly one, null prevout, scriptSig == `default_coinbase_scriptsig` |
| vout[0] | scriptPubKey == requested payout, value == `coinbasevalue` |
| witness commitment | present exactly once, value 0 |
| RES mint | present exactly once iff a winner was paid; script and value match `default_res_mint` |
| vout count | payout + optional mint + commitment, nothing else |
| witness stack | one item, 32 bytes, all zero |
| `default_coinbase_txid` | mandatory, must equal the parsed bytes |
| `merkleroot` | mandatory, must equal the miner's computed root |

Numeric handling: JSON amounts are accepted as integers only, transaction
count and cumulative size are bounded by `GBT_SOLO_MAX_TXS` /
`GBT_SOLO_MAX_BLOCK_BYTES`, and `nBits` is parsed as strict 8-digit hex rather
than `sscanf("%x")`, which accepts signs and padding.

A devtest `-reswalletbootstrap` template carries an extra coinbase output and
is therefore refused as a SOLO template; use `generatetoaddress` for it.

## Two blockers found while testing

Both were latent and are fixed here:

1. **`compat/jansson` used 32-bit integers.** `json_loads()` failed outright on
   any template with `coinbasevalue > INT_MAX`, i.e. every real one. The
   vendored library now uses a 64-bit `json_int_t`, matching jansson ≥ 2.0.
2. **`util.c:hack_json_numbers()` rewrote large integers as JSON reals** so the
   32-bit parser would accept them, pushing satoshi amounts through a `double`.
   Removed: the node's bytes are now parsed as-is, and an out-of-range amount
   fails the parse instead of being silently rounded.

## Tests

- `miner/test_gbt_solo`: copy-bytes, 3-push scriptSig, paid-winner, RES mint
  `0105…bb`, plus rejection of OP_5 mint, 2-push scriptSig, dropped mint,
  missing coinbase tx, and each structural/positional violation in the table
  above. Includes a **golden vector**: a verbatim regtest `getblocktemplate`
  response, so the hand-built fixtures cannot drift from real node output.
- `miner/replay_fuzz_corpus.sh` (in `make check`): replays
  `miner/fuzz_corpus/gbt_solo`, a structured seed corpus that starts the fuzzer
  inside the coinbase verifier rather than in the JSON parser. Regenerate with
  `./gen_fuzz_corpus.py`.
- `mining_solo_gbt_copy_bytes.py`: `submitblock` rejections pinned to exact
  node reasons — `high-hash`, `bad-diffbits`, `Block decode failed`,
  `bad-cb-convergence-root` (missing paid-winner and altered scriptSig),
  `bad-witness-merkle-match`. Continuous mining takes each GBT snapshot with
  the miner stopped and mines exactly that height, so verification no longer
  depends on how fast `minerd` runs.

## Remaining

- Empty-chain heights 1–3 have no parent-draw mint. Drop-mint submit is
  asserted when `default_res_mint` is present; miner unit tests refuse a
  coinbase that drops it.
- `CPP_REFINEMENT_PROVED` remains false (byte equality, not a Coq refinement).
