import { describe, it } from "node:test";
import assert from "node:assert/strict";
import { buildConvergeProofPanel, CONVERGEPROOF_AUTHORITY_NOTICE } from "./convergeProofPanel";
import { buildTxConvergencePanel } from "./convergencePanel";
import { buildPcorbItems, buildTxWitnessPanel } from "./witnessPanel";

function pcorbStack(): string[] {
  return [
    "a".repeat(64),
    "0100000000000000",
    "b".repeat(64),
    "0200000000000000",
    "ffffffff",
    "c".repeat(64),
    "75757575757551",
  ];
}

describe("convergeProofPanel", () => {
  it("is visible for a body mint without a PCORB stack", () => {
    const txid = "coinbase";
    const convPanel = buildTxConvergencePanel([]);
    const witness = buildTxWitnessPanel(
      txid,
      [{ vin_index: 0, coinbase: true, txinwitness: [] }],
      [
        {
          mint_vout_index: 1,
          mint_amount: "10000",
          winner_convergence_txid: "winner",
          block_hash: "bh",
          block_height: 100,
          convergence_root: "c".repeat(64),
          proof_items: { source: "coinbase_op_res_mint" },
        },
      ],
      { block_hash: "bh", height: 100, rigor_winner: "", rigor_res_minted: 1 },
      convPanel,
      [],
    );

    const panel = buildConvergeProofPanel({
      txid,
      convergencePanel: convPanel,
      convergenceRows: [],
      resMintRows: [
        {
          mint_vout_index: 1,
          mint_amount: "10000",
          winner_convergence_txid: "winner",
          block_hash: "bh",
          block_height: 100,
          convergence_root: "c".repeat(64),
          proof_items: { source: "coinbase_op_res_mint" },
        },
      ],
      linkedMintRows: [],
      witnessPanel: witness,
      vouts: [{ vout_index: 1, asset_kind: "RES", value_atoms: "10000", address: null }],
      block: { block_hash: "bh", height: 100 },
      indexerTipHeight: 105,
    });

    assert.equal(panel.visible, true);
    assert.equal(panel.kind, "res_mint");
    assert.equal(panel.pcorb_witness, null);
    assert.equal(panel.res_mint_linkage.confirmations, 6);
    assert.equal(panel.res_mint_outputs.length, 1);
    assert.equal(panel.winner_txid, "winner");
  });

  it("is visible for winner convergence tx with sigma digest", () => {
    const txid = "winner";
    const convPanel = buildTxConvergencePanel([{ block_hash: "bh", block_height: 50, is_winner: true }]);
    const witness = buildTxWitnessPanel(txid, [], [], null, convPanel, [{ block_height: 50, convergence_root: "root" }]);

    const panel = buildConvergeProofPanel({
      txid,
      convergencePanel: convPanel,
      convergenceRows: [{ block_height: 50, convergence_root: "root", selection_digest_h: "ee".repeat(32) }],
      resMintRows: [],
      linkedMintRows: [{ mint_txid: "mint1", winner_convergence_txid: txid, block_height: 50, block_hash: "bh", mint_vout_index: 0, mint_amount: "10000" }],
      witnessPanel: witness,
      vouts: [],
      block: { block_hash: "bh", height: 50 },
      indexerTipHeight: 50,
    });

    assert.equal(panel.visible, true);
    assert.equal(panel.kind, "winner_convergence");
    assert.equal(panel.winner_txid, txid);
    assert.equal(panel.sigma_digest.hex, "ee".repeat(32));
    assert.equal(panel.res_mint_linkage.mint_txid, "mint1");
    assert.equal(panel.authority_notice, CONVERGEPROOF_AUTHORITY_NOTICE);
  });

  it("hidden for non-participating transactions", () => {
    const convPanel = buildTxConvergencePanel([]);
    const witness = buildTxWitnessPanel("x", [], [], null, convPanel, []);
    const panel = buildConvergeProofPanel({
      txid: "x",
      convergencePanel: convPanel,
      convergenceRows: [],
      resMintRows: [],
      linkedMintRows: [],
      witnessPanel: witness,
      vouts: [],
      block: null,
      indexerTipHeight: 0,
    });
    assert.equal(panel.visible, false);
  });

  it("buildPcorbItems produces 6 named elements", () => {
    assert.equal(buildPcorbItems(pcorbStack()).length, 6);
  });
});
