import { describe, it } from "node:test";
import assert from "node:assert/strict";
import {
  COINBASE_RES_MINT_SOURCE,
  coinbaseMintProofItems,
  findCoinbaseOpResMint,
  isCoinbaseMintProofItems,
} from "./resMint";

/** GetResMintScript(0, 0x11…): OP_0 + PUSH20 + OP_RES_MINT */
const OP_RES_MINT_HEX = "00" + "14" + "11".repeat(20) + "bb";
const P2WPKH = "00" + "14" + "22".repeat(20);

describe("findCoinbaseOpResMint", () => {
  it("indexes coinbase OP_RES_MINT and ignores body RES", () => {
    const found = findCoinbaseOpResMint([
      {
        txid: "cb",
        is_coinbase: true,
        vouts: [
          { vout_index: 0, value_atoms: 1000n * 100000000n, script_pubkey: P2WPKH },
          { vout_index: 1, value_atoms: 40n * 1000000n, script_pubkey: OP_RES_MINT_HEX },
        ],
      },
      {
        txid: "body",
        is_coinbase: false,
        vouts: [{ vout_index: 0, value_atoms: 1n, script_pubkey: OP_RES_MINT_HEX }],
      },
    ]);
    assert.ok(found);
    assert.equal(found.mintTxid, "cb");
    assert.equal(found.outputs.length, 1);
    assert.equal(found.outputs[0].vout_index, 1);
    assert.equal(found.amountAtoms, 40n * 1000000n);
  });

  it("does not treat a body PCORB-shaped tx as mint", () => {
    assert.equal(
      findCoinbaseOpResMint([
        {
          txid: "body",
          is_coinbase: false,
          vouts: [{ vout_index: 0, value_atoms: 40n * 1000000n, script_pubkey: OP_RES_MINT_HEX }],
        },
      ]),
      null,
    );
  });

  it("returns null when coinbase has no OP_RES_MINT", () => {
    assert.equal(
      findCoinbaseOpResMint([
        {
          txid: "cb",
          is_coinbase: true,
          vouts: [{ vout_index: 0, value_atoms: 1n, script_pubkey: P2WPKH }],
        },
      ]),
      null,
    );
  });

  it("stores coinbase mint proof metadata, not a witness stack", () => {
    const items = coinbaseMintProofItems({ script_pubkey: OP_RES_MINT_HEX, paid_winner_txid: "winner" });
    assert.equal(items.source, COINBASE_RES_MINT_SOURCE);
    assert.equal(isCoinbaseMintProofItems(items), true);
    assert.equal(isCoinbaseMintProofItems({ witness_stack: ["aa"] }), false);
  });
});
