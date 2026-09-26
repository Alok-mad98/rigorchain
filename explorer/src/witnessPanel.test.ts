import { describe, it } from "node:test";
import assert from "node:assert/strict";
import {
  buildResMintProofView,
  buildTxWitnessPanel,
  buildVinWitnessView,
  classifyWitnessPath,
} from "./witnessPanel";
import { buildTxConvergencePanel } from "./convergencePanel";

describe("witnessPanel", () => {
  it("classifies coinbase, key-path, and ordinary stacks", () => {
    assert.equal(classifyWitnessPath(true, []), "coinbase");
    assert.equal(classifyWitnessPath(false, ["aa", "bb"]), "res_key_path");
    assert.equal(classifyWitnessPath(false, ["deadbeef"]), "oru_input");
    assert.equal(classifyWitnessPath(false, []), "none");
  });

  it("does not treat a leftover 6-field stack as a mint path", () => {
    const leftover = [
      "a".repeat(64),
      "0100000000000000",
      "b".repeat(64),
      "0200000000000000",
      "ffffffff",
      "c".repeat(64),
    ];
    assert.equal(classifyWitnessPath(false, leftover), "oru_input");
    const view = buildVinWitnessView({ vin_index: 1, coinbase: false, txinwitness: leftover });
    assert.equal(view.path_kind, "oru_input");
  });

  it("associates RES mint from indexed RFC-020 body-tx row", () => {
    const txid = "minttx";
    const panel = buildTxConvergencePanel([]);
    const proof = buildResMintProofView(
      txid,
      [
        {
          mint_vout_index: 1,
          mint_amount: "100000000",
          winner_convergence_txid: "minttx",
          block_hash: "blockhash",
          block_height: 100,
          proof_items: { source: "rfc020_body_mint", burn_atoms: "102400000000", mint_atoms: "100000000" },
        },
      ],
      { block_hash: "blockhash", height: 100, rigor_winner: "", rigor_res_minted: 1 },
      panel,
    );
    assert.equal(proof.present, true);
    assert.equal(proof.confirmation_status, "Confirmed in block");
    assert.equal(proof.matches_winner_block, true);
    assert.equal(proof.mint_amount_display, "1.00000000 RES");
  });

  it("does not invent a mint from a leftover witness", () => {
    const proof = buildResMintProofView(
      "bodytx",
      [],
      { block_hash: "bh", height: 10, rigor_winner: "w", rigor_res_minted: 1 },
      buildTxConvergencePanel([]),
    );
    assert.equal(proof.present, false);
    assert.equal(proof.confirmation_status, "Not a RES mint");
  });

  it("builds full tx witness panel for RFC-020 body mint", () => {
    const txid = "minttx";
    const convPanel = buildTxConvergencePanel([]);
    const panel = buildTxWitnessPanel(
      txid,
      [{ vin_index: 0, coinbase: false, txinwitness: ["aa", "bb"] }],
      [
        {
          mint_vout_index: 1,
          mint_amount: "100000000",
          winner_convergence_txid: "minttx",
          block_hash: "bh",
          block_height: 100,
          proof_items: { source: "rfc020_body_mint" },
        },
      ],
      { block_hash: "bh", height: 100, rigor_winner: "", rigor_res_minted: 1 },
      convPanel,
    );
    assert.equal(panel.inputs[0].path_kind, "res_key_path");
    assert.equal(panel.res_mint_proof.present, true);
  });
});
