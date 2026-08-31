import { describe, it } from "node:test";
import assert from "node:assert/strict";
import {
  buildPcorbItems,
  buildResMintProofView,
  buildTxWitnessPanel,
  buildVinWitnessView,
  classifyWitnessPath,
} from "./witnessPanel";
import { buildTxConvergencePanel } from "./convergencePanel";
import { COINBASE_RES_MINT_SOURCE } from "./resMint";

const BOOTSTRAP_REDEEM = "75757575757551";

function proofFields(): string[] {
  return [
    "a".repeat(64),
    "0100000000000000",
    "b".repeat(64),
    "0200000000000000",
    "ffffffff",
    "c".repeat(64),
  ];
}

function pcorbStack(): string[] {
  return [...proofFields(), BOOTSTRAP_REDEEM];
}

describe("witnessPanel", () => {
  it("does not classify a PCORB-shaped stack as a mint path", () => {
    assert.equal(classifyWitnessPath(false, pcorbStack()), "other");
    assert.equal(classifyWitnessPath(false, proofFields()), "other");
    assert.equal(classifyWitnessPath(true, []), "coinbase");
  });

  it("classifies 2-item stack as RES key-path", () => {
    assert.equal(classifyWitnessPath(false, ["aa", "bb"]), "res_key_path");
  });

  it("classifies non-PCORB as ORU input", () => {
    assert.equal(classifyWitnessPath(false, ["deadbeef"]), "oru_input");
  });

  it("still parses historical 6-field stacks for display helpers", () => {
    const items = buildPcorbItems(pcorbStack());
    assert.equal(items.length, 6);
    assert.equal(items[0].field_name, "convergence_txid");
    assert.equal(items[5].field_name, "convergence_root");
  });

  it("maps coinbase vin as coinbase, not mint proof", () => {
    const cb = buildVinWitnessView({ vin_index: 0, coinbase: true, txinwitness: [] });
    const leftover = buildVinWitnessView({ vin_index: 1, coinbase: false, txinwitness: pcorbStack() });
    assert.equal(cb.path_kind, "coinbase");
    assert.equal(leftover.path_kind, "other");
    assert.equal(leftover.pcorb_items, null);
  });

  it("associates RES mint from indexed coinbase OP_RES_MINT row", () => {
    const txid = "coinbase";
    const panel = buildTxConvergencePanel([]);
    const proof = buildResMintProofView(
      txid,
      [
        {
          mint_vout_index: 1,
          mint_amount: "40000000",
          winner_convergence_txid: "parentwinner",
          block_hash: "blockhash",
          block_height: 100,
          convergence_root: "roothex",
          proof_items: { source: COINBASE_RES_MINT_SOURCE, script_pubkey: "00" + "14" + "11".repeat(20) + "bb" },
        },
      ],
      { block_hash: "blockhash", height: 100, rigor_winner: "", rigor_res_minted: 1 },
      panel,
      null,
    );
    assert.equal(proof.present, true);
    assert.equal(proof.confirmation_status, "Confirmed in block");
    assert.equal(proof.matches_winner_block, true);
    assert.equal(proof.mint_amount_display, "40.000000 RES");
    assert.equal(proof.proof_items, null);
  });

  it("does not invent a mint from a leftover PCORB witness", () => {
    const proof = buildResMintProofView(
      "bodytx",
      [],
      { block_hash: "bh", height: 10, rigor_winner: "w", rigor_res_minted: 1 },
      buildTxConvergencePanel([]),
      buildPcorbItems(pcorbStack()),
    );
    assert.equal(proof.present, false);
    assert.equal(proof.confirmation_status, "Not a RES mint");
  });

  it("builds full tx witness panel for coinbase mint", () => {
    const txid = "coinbase";
    const convPanel = buildTxConvergencePanel([]);
    const panel = buildTxWitnessPanel(
      txid,
      [{ vin_index: 0, coinbase: true, txinwitness: [] }],
      [
        {
          mint_vout_index: 1,
          mint_amount: "40000000",
          winner_convergence_txid: "parentwinner",
          block_hash: "bh",
          block_height: 100,
          convergence_root: "c".repeat(64),
          proof_items: { source: COINBASE_RES_MINT_SOURCE },
        },
      ],
      { block_hash: "bh", height: 100, rigor_winner: "", rigor_res_minted: 1 },
      convPanel,
      [{ block_height: 100, convergence_root: "c".repeat(64) }],
    );
    assert.equal(panel.inputs[0].path_kind, "coinbase");
    assert.equal(panel.res_mint_proof.present, true);
    assert.equal(panel.res_mint_proof.proof_items, null);
  });
});
