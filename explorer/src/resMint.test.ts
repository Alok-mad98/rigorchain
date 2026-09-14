import { describe, it } from "node:test";
import assert from "node:assert/strict";
import {
  RFC020_BODY_MINT_SOURCE,
  findBodyResMints,
  isCoinbaseMintProofItems,
  isBodyMintProofItems,
  rfc020MintProofItems,
} from "./resMint";

const OP_CONVERGE_HEX = "14" + "00".repeat(20) + "bd";
const P2WPKH = "00" + "14" + "22".repeat(20);
const RES_NATIVE_V1 = "5120" + "11".repeat(32);

describe("findBodyResMints", () => {
  it("indexes RFC-020 body mint: OP_CONVERGE + Witness v1 RES", () => {
    const found = findBodyResMints([
      {
        txid: "cb",
        is_coinbase: true,
        vouts: [{ vout_index: 0, value_atoms: 1000n * 100000000n, script_pubkey: P2WPKH, asset_kind: "ORU" }],
      },
      {
        txid: "mint",
        is_coinbase: false,
        vouts: [
          { vout_index: 0, value_atoms: 1024n * 100000000n, script_pubkey: OP_CONVERGE_HEX, asset_kind: "ORU" },
          { vout_index: 1, value_atoms: 100000000n, script_pubkey: RES_NATIVE_V1, asset_kind: "RES" },
        ],
      },
    ]);
    assert.equal(found.length, 1);
    assert.equal(found[0].mintTxid, "mint");
    assert.equal(found[0].burnAtoms, 1024n * 100000000n);
    assert.equal(found[0].amountAtoms, 100000000n);
  });

  it("does not treat coinbase or a plain burn as mint", () => {
    assert.equal(
      findBodyResMints([
        {
          txid: "cb",
          is_coinbase: true,
          vouts: [{ vout_index: 0, value_atoms: 1n, script_pubkey: RES_NATIVE_V1, asset_kind: "RES" }],
        },
      ]).length,
      0,
    );
    assert.equal(
      findBodyResMints([
        {
          txid: "burn",
          is_coinbase: false,
          vouts: [{ vout_index: 0, value_atoms: 1024n, script_pubkey: OP_CONVERGE_HEX, asset_kind: "ORU" }],
        },
      ]).length,
      0,
    );
  });

  it("stores RFC-020 mint proof metadata, not a witness stack", () => {
    const items = rfc020MintProofItems({ burnAtoms: 1024n * 100000000n, mintAtoms: 100000000n, height: 0 });
    assert.equal(items.source, RFC020_BODY_MINT_SOURCE);
    assert.equal(items.destroy_cost_atoms, (1024n * 100000000n).toString());
    assert.equal(isBodyMintProofItems(items), true);
    assert.equal("epoch" in items, false);
    assert.equal(isCoinbaseMintProofItems(items), false);
    assert.equal(isCoinbaseMintProofItems({ witness_stack: ["aa"] }), false);
  });
});
