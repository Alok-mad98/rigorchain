# Mining: `getblocktemplate` and coinbase

How **Rigor Core** exposes block templates. Coinbase pays **ORU** (subsidy +
claimable ORU-domain fees) and may carry a Segwit witness commitment. It does
**not** mint RES.

**Supported path: SOLO mining** against a local full node (`generatetoaddress`,
GBT + local hasher, or `submitblock`). Do not write extranonce into the
coinbase `scriptSig` — that field is the BIP34 height push and has **zero free
bytes**.

Normative burn / mint rules: [convergence-consensus-v1.md](./convergence-consensus-v1.md).

---

## 1. RPC: `getblocktemplate`

- **Call shape:** pass `{"rules": ["segwit"]}` (and `"signet"` on signet).
- On mainnet-like chains the node refuses GBT with zero peers or during IBD.

### 1.1 Rigor-specific JSON fields

| Field | Meaning |
|--------|---------|
| `default_witness_commitment` | Hex script of the BIP 141 witness commitment, if present. |
| `default_coinbase_scriptsig` | Exact `vin[0].scriptSig` bytes: BIP34 height only. **Copy these bytes.** |
| `rigor_coinbase_immutable` | Always `true`. Do not mutate the coinbase; request a new template. |
| `mutable` | SOLO: **`["time"]` only**. `nNonce` is the grind. |
| `default_coinbase_tx` | Witness-serialized canonical coinbase. Only when `coinbase_address` was given. |
| `default_coinbase_txid` | txid of that coinbase. |
| `merkleroot` | Header merkle root of the unmodified template. |
| `coinbase_address` | Echo of the request payout address. |

There is no `default_res_mint`, no paid-winner field, and no convergence-root
push. Leftover `OP_RETURN` tags (CVGE / RIGR / PCORB) are not template fields.

### 1.2 `coinbasevalue`

`coinbasevalue` is the **ORU** value of coinbase `vout[0]`: block subsidy +
claimable ORU-domain fees (RFC-018: the RES-transfer fee burn share is **not**
claimed). RFC-020 subsidy is **0** at height 0 and **128 ORU**
(`12 800 000 000` atoms) for every height ≥ 1. There is no ORU halving and
no 1000 / 50 / 6.25 fallback. A fee-less height-1 template therefore has
`coinbasevalue = 12800000000`.

---

## 2. Coinbase layout

1. **`vin[0]`** — null prevout; **`scriptSig`** = BIP34 height push.
2. **`vout[0]`** — ORU payout:
   `(nFeesOru - nFeesOruResTransferBurn) + GetBlockSubsidy(height)`.
3. **Witness commitment** — BIP 141 `OP_RETURN`, appended when needed.

The coinbase must not carry a `0xbb` mint output. New RES is created only by
body transactions that burn ORU and satisfy `CheckResMintArithmetic`.

**Header grind:**

| Field | Mutable? |
|--------|----------|
| `nNonce` | **Yes — primary** |
| `nTime` | **Yes**, within MTP / future-time rules. `mutable` lists only `time`. |
| `nVersion` | Optional. Do not clobber consensus version bits. |
| coinbase `scriptSig` | **Never** |
| coinbase witness reserved value | **Never** — 32 zero bytes. |

A Bitcoin-style pool that writes `extranonce1 ‖ extranonce2` into `scriptSig`
will have every block rejected.

**Fail-fast:** copy `default_coinbase_scriptsig`. Prefer `default_coinbase_tx`
when present. Do not invent a RES mint output.

---

## 3. Leftover OP_RETURN tags (not consensus)

CVGE / RIGR / PCORB are not required. Extra `OP_RETURN` outputs are allowed
and ignored.

---

## 4. Recommended integration

1. `getblocktemplate '{"rules":["segwit"]}'`
2. Copy `default_coinbase_scriptsig` (or `default_coinbase_tx` when present).
3. Grind `nNonce`. The only advertised mutable header field is `time`.
4. `submitblock`.

`-miningresfeeaddr` is rejected on non-mockable chains. It is not a mint switch.

---

## 5. Code map

| Topic | Location |
|--------|-----------|
| GBT | `src/rpc/mining.cpp` |
| Coinbase assembly, ORU fees, RFC-018 burn | `src/node/miner.cpp` |
| Coinbase layout lock | `src/consensus/rigor_coinbase_layout.h` |
| RES mint (body tx) | `src/consensus/res_issuance.h`, `src/consensus/tx_verify.cpp` |
| `ConnectBlock` | `src/validation.cpp` |
