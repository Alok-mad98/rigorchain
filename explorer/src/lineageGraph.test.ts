import { describe, it } from "node:test";
import assert from "node:assert/strict";
import { buildConvergeProofPanel } from "./convergeProofPanel";
import { buildBlockConvergencePanel, buildTxConvergencePanel } from "./convergencePanel";
import {
  buildLineageGraph,
  LINEAGE_READONLY_NOTICE,
  LINEAGE_RPC_NOTICE,
} from "./lineageGraph";
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

describe("lineageGraph", () => {
  it("builds multi-candidate lineage with winner branch", () => {
    const candidates = [
      { convergence_txid: "cand1", draw_eligible: true, convergence_root: "root1" },
      { convergence_txid: "cand2", draw_eligible: true, convergence_root: "root1" },
      { convergence_txid: "winner", draw_eligible: true, convergence_root: "root1" },
      { convergence_txid: "cand4", draw_eligible: true, convergence_root: "root1" },
    ];
    const blockPanel = buildBlockConvergencePanel(
      { rigor_winner: "winner", committed_root: "root1" },
      candidates,
    );
    const convPanel = buildTxConvergencePanel([{ block_hash: "bh", block_height: 10, is_winner: true }]);
    const witness = buildTxWitnessPanel("winner", [], [], null, convPanel, [
      { block_height: 10, convergence_root: "root1" },
    ]);
    const convergeProof = buildConvergeProofPanel({
      txid: "winner",
      convergencePanel: convPanel,
      convergenceRows: [{ block_height: 10, convergence_root: "root1", selection_digest_h: "ee".repeat(32) }],
      resMintRows: [],
      linkedMintRows: [
        {
          mint_txid: "mint1",
          winner_convergence_txid: "winner",
          block_height: 10,
          block_hash: "bh",
          mint_vout_index: 0,
          mint_amount: "10000",
        },
      ],
      witnessPanel: witness,
      vouts: [],
      block: { block_hash: "bh", height: 10 },
      indexerTipHeight: 10,
    });

    const graph = buildLineageGraph({
      anchorTxid: "cand2",
      blockPanel,
      candidates,
      convergeProof,
      block: {
        block_hash: "bh",
        height: 10,
        rigor_winner: "winner",
        rigor_res_minted: 1,
        mismatch: false,
      },
      winnerConvergenceRow: { convergence_root: "root1", selection_digest_h: "ee".repeat(32) },
      mintResOutputs: [],
    });

    assert.equal(graph.visible, true);
    assert.equal(graph.f_set_size, 4);
    assert.equal(graph.candidate_set.length, 4);
    assert.equal(graph.winner.txid, "winner");
    assert.equal(graph.sigma.selection_digest_h, "ee".repeat(32));
    assert.equal(graph.mint.mint_txid, "mint1");
    assert.equal(graph.equivalence_claimed, false);

    const candidateNodes = graph.nodes.filter((n) => n.kind === "candidate");
    assert.equal(candidateNodes.length, 4);
    assert.ok(graph.edges.some((e) => e.from === "candidate:cand2" && e.to === "winner:winner"));
    assert.ok(graph.nodes.some((n) => n.kind === "pcorb" || n.kind === "sigma_digest"));
  });

  it("includes a PCORB node for RES mint lineage", () => {
    const txid = "minttx";
    const convPanel = buildTxConvergencePanel([]);
    const witness = buildTxWitnessPanel(
      txid,
      [{ vin_index: 0, coinbase: false, txinwitness: pcorbStack() }],
      [
        {
          mint_vout_index: 0,
          mint_amount: "10000",
          winner_convergence_txid: "winner",
          block_hash: "bh",
          block_height: 20,
          convergence_root: "c".repeat(64),
          proof_items: { witness_stack: pcorbStack() },
        },
      ],
      { block_hash: "bh", height: 20, rigor_winner: "winner", rigor_res_minted: 1 },
      convPanel,
      [],
    );
    const convergeProof = buildConvergeProofPanel({
      txid,
      convergencePanel: convPanel,
      convergenceRows: [],
      resMintRows: [
        {
          mint_vout_index: 0,
          mint_amount: "10000",
          winner_convergence_txid: "winner",
          block_hash: "bh",
          block_height: 20,
          convergence_root: "c".repeat(64),
          proof_items: { witness_stack: pcorbStack() },
        },
      ],
      linkedMintRows: [],
      witnessPanel: witness,
      vouts: [{ vout_index: 0, asset_kind: "RES", value_atoms: "10000", address: "trg1pabc" }],
      block: { block_hash: "bh", height: 20 },
      indexerTipHeight: 25,
    });

    const graph = buildLineageGraph({
      anchorTxid: txid,
      blockPanel: buildBlockConvergencePanel({ rigor_winner: "winner", committed_root: "c".repeat(64) }, [
        { convergence_txid: "winner", draw_eligible: true, convergence_root: "c".repeat(64) },
      ]),
      candidates: [{ convergence_txid: "winner", draw_eligible: true, convergence_root: "c".repeat(64) }],
      convergeProof,
      block: { block_hash: "bh", height: 20, rigor_winner: "winner", rigor_res_minted: 1 },
      winnerConvergenceRow: null,
      mintResOutputs: [],
    });

    assert.equal(graph.visible, true);
    assert.equal(graph.pcorb.present, true);
    assert.equal(graph.pcorb.item_count, 6);
    assert.equal(graph.mint.state, "Confirmed");
    assert.equal(buildPcorbItems(pcorbStack()).length, 6);
  });

  it("exposes read-only authority notices", () => {
    assert.match(LINEAGE_READONLY_NOTICE, /computed in the browser/);
    assert.match(LINEAGE_RPC_NOTICE, /rigord/);
  });
});
