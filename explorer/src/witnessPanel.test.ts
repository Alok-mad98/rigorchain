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

// 6x OP_DROP + OP_TRUE, the devtest bootstrap redeem script (auto_res_mint.cpp).
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

/** As observed on a devtest mint: the six proof pushes plus the P2WSH redeem script. */
function pcorbStack(): string[] {
  return [...proofFields(), BOOTSTRAP_REDEEM];
}

describe("witnessPanel", () => {
  it("classifies a proof-shaped stack as RES mint PCORB regardless of trailing pushes", () => {
    assert.equal(classifyWitnessPath(false, pcorbStack()), "res_mint_pcorb");
    assert.equal(classifyWitnessPath(false, proofFields()), "res_mint_pcorb");
  });

  it("classifies 2-item stack as RES key-path", () => {
    assert.equal(classifyWitnessPath(false, ["aa", "bb"]), "res_key_path");
  });

  it("classifies non-PCORB as ORU input", () => {
    assert.equal(classifyWitnessPath(false, ["deadbeef"]), "oru_input");
  });

  it("rejects a stack whose proof fields have the wrong byte lengths", () => {
    const bad = pcorbStack();
    bad[4] = "ffff";
    assert.equal(classifyWitnessPath(false, bad), "oru_input");
  });

  it("builds 6 PCORB labeled items", () => {
    const items = buildPcorbItems(pcorbStack());
    assert.equal(items.length, 6);
    assert.equal(items[0].field_name, "convergence_txid");
    assert.equal(items[4].field_name, "target_prev");
    assert.equal(items[5].field_name, "convergence_root");
  });

  it("maps vin witness with ORU vs PCORB separation", () => {
    const oru = buildVinWitnessView({ vin_index: 0, coinbase: false, txinwitness: ["cafe"] });
    const mint = buildVinWitnessView({ vin_index: 1, coinbase: false, txinwitness: pcorbStack() });
    assert.equal(oru.path_kind, "oru_input");
    assert.equal(mint.path_kind, "res_mint_pcorb");
    assert.equal(mint.stack_depth, 7);
    assert.equal(mint.pcorb_items?.length, 6);
  });

  it("associates RES mint proof from indexed res_mints row", () => {
    const txid = "minttx";
    const panel = buildTxConvergencePanel([]);
    const proof = buildResMintProofView(
      txid,
      [
        {
          mint_vout_index: 0,
          mint_amount: "10000",
          winner_convergence_txid: "winner",
          block_hash: "blockhash",
          block_height: 100,
          convergence_root: "roothex",
          proof_items: { witness_stack: pcorbStack() },
        },
      ],
      { block_hash: "blockhash", height: 100, rigor_winner: "winner", rigor_res_minted: 1 },
      panel,
      null,
    );
    assert.equal(proof.present, true);
    assert.equal(proof.confirmation_status, "Confirmed in block");
    assert.equal(proof.matches_winner_block, true);
    assert.equal(proof.mint_amount_display, "100.00 RES");
    assert.equal(proof.proof_items?.length, 6);
  });

  it("marks convergence candidate without mint as pending", () => {
    const panel = buildTxConvergencePanel([
      { block_hash: "bh", block_height: 50, is_winner: false },
    ]);
    const proof = buildResMintProofView("cand", [], null, panel, null);
    assert.equal(proof.confirmation_status, "Pending candidate state");
    assert.equal(proof.block_height, 50);
  });

  it("builds full tx witness panel for winner mint tx", () => {
    const txid = "minttx";
    const convPanel = buildTxConvergencePanel([{ block_hash: "bh", block_height: 100, is_winner: true }]);
    const panel = buildTxWitnessPanel(
      txid,
      [{ vin_index: 0, coinbase: false, txinwitness: pcorbStack() }],
      [
        {
          mint_vout_index: 0,
          mint_amount: "10000",
          winner_convergence_txid: txid,
          block_hash: "bh",
          block_height: 100,
          convergence_root: "c".repeat(64),
          proof_items: { witness_stack: pcorbStack() },
        },
      ],
      { block_hash: "bh", height: 100, rigor_winner: txid, rigor_res_minted: 1 },
      convPanel,
      [{ block_height: 100, convergence_root: "c".repeat(64) }],
    );
    assert.equal(panel.inputs.length, 1);
    assert.equal(panel.res_mint_proof.present, true);
    assert.equal(panel.proof_status.winner_status, "Winner");
    assert.ok(panel.proof_status.convergence_root);
  });
});
